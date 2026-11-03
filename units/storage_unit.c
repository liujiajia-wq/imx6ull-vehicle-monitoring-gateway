// 数据存储单元 —— 邮箱任务实现
//
//   recv_mail_msg()  ← 采集单元投递的采样 / 异常单元投递的报警事件
//          ↓
//   按消息类型分流：采样 → 节流判定 → sensor_log
//                   报警 → 直接写 → alarm_log
//          ↓
//   批量事务提交（攒够 N 条或到时间），减少 SD 卡随机写
//
// 编译开关 GW_WITH_SQLITE（由 Makefile 根据 lib/lib/libsqlite3.so 是否存在自动决定）：
//   1 = 真正用 sqlite3 落库
//   0 = 降级：只统计、只打日志，不碰数据库 —— 保证没有 .so 时也能编译通过

#define _GNU_SOURCE

#include "storage_unit.h"
#include "gw_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/stat.h>

#if GW_WITH_SQLITE
#include <sqlite3.h>
#endif

/* ==========================================================================
 *  可调参数
 * ========================================================================== */

/* 数据库路径。板子根文件系统可能是只读的 ramfs，/root 一般可写。
 * 想放到 SD 卡/U 盘就改成 "/mnt/sdcard/gwdata.db" */
#ifndef GW_DB_PATH
#define GW_DB_PATH              "/root/gwdata.db"
#endif

/* 保留多少天的历史（启动时和每小时清理一次） */
#define GW_KEEP_DAYS            7

/* 攒够多少条 INSERT 就提交一次事务 */
#define GW_FLUSH_ROWS           16

/* 或者最多等多久提交一次（毫秒） */
#define GW_FLUSH_MS             2000

#define GW_DEBUG_LOG            1

/* ==========================================================================
 *  节流表 —— 每类传感器一条
 *
 *  min_ms : 最小落库间隔（毫秒）
 *  delta  : 数值变化超过这个量就立刻落库（跳过间隔限制）
 *
 *  调法：嫌数据少就把 min_ms 调小，嫌 SD 卡写得多就调大。
 * ========================================================================== */

typedef struct {
    uint32_t min_ms;
    float    delta;
    uint32_t last_ms;
    float    last_a;
    float    last_b;
    bool     has;
} gw_throt_t;

/* 下标 = msgdata.id - 1：舱温 / 机温 / 姿态 / 倒车 */
static gw_throt_t s_throt[4] = {
    { 30000, 0.5f,  0, 0.0f, 0.0f, false },   /* 驾驶舱：30 秒一条，变化 >0.5℃ 立即存 */
    { 10000, 0.5f,  0, 0.0f, 0.0f, false },   /* 发动机：10 秒一条 */
    {  2000, 2.0f,  0, 0.0f, 0.0f, false },   /* 姿态  ：2 秒一条，倾角变化 >2° 立即存 */
    {   500, 0.05f, 0, 0.0f, 0.0f, false },   /* 倒车  ：0.5 秒一条（只在挂倒挡时存） */
};

/* ==========================================================================
 *  统计
 * ========================================================================== */

static volatile uint32_t s_rows = 0;
static volatile uint32_t s_alarms = 0;
static volatile uint32_t s_dropped = 0;

uint32_t storage_unit_rows(void)    { return s_rows; }
uint32_t storage_unit_alarms(void)  { return s_alarms; }
uint32_t storage_unit_dropped(void) { return s_dropped; }

static bool s_active = false;

bool storage_unit_active(void) { return s_active; }

/* ==========================================================================
 *  小工具
 * ========================================================================== */

static uint32_t _now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000);
}

/* 用消息里带的时间戳（采集那一刻），拿不到就用当前时间。
 * 未启用 SQLite 时这个函数没人调用，加 unused 免得 -Wall 报警告。 */
__attribute__((unused))
static int64_t _msg_ts(const struct msgdata * d)
{
    struct tm t = d->tim;
    time_t    s;

    s = mktime(&t);
    if(s <= 0) s = time(NULL);
    return (int64_t)s;
}

/* 这一拍要不要落库 */
static bool _should_store(const struct msgdata * d, float a, float b)
{
    gw_throt_t * t;
    uint32_t     now;

    if(d->id < GW_ID_CABIN || d->id > GW_ID_REAR) return false;

    t = &s_throt[d->id - GW_ID_CABIN];

    /* 倒车：只在挂倒挡时记录，否则不产生行 */
    if(d->id == GW_ID_REAR && d->reverse == 0) {
        s_dropped++;
        return false;
    }

    /* 离线数据没意义，不存 */
    if(d->status != GW_ST_OK) {
        s_dropped++;
        return false;
    }

    now = _now_ms();

    if(!t->has) {
        t->has    = true;
        t->last_ms = now;
        t->last_a = a;
        t->last_b = b;
        return true;
    }

    /* 变化够大 → 立刻存，不受间隔限制（避免把"突变"平滑掉） */
    if(t->delta > 0.0f &&
       (fabsf(a - t->last_a) >= t->delta || fabsf(b - t->last_b) >= t->delta)) {
        t->last_ms = now;
        t->last_a  = a;
        t->last_b  = b;
        return true;
    }

    /* 变化不大，按最小间隔存一条，让曲线仍有底噪 */
    if((uint32_t)(now - t->last_ms) >= t->min_ms) {
        t->last_ms = now;
        t->last_a  = a;
        t->last_b  = b;
        return true;
    }

    s_dropped++;
    return false;
}

/* 从消息里取出 (v1, v2) */
static void _unpack(const struct msgdata * d, float * a, float * b)
{
    switch(d->id) {
    case GW_ID_CABIN:   *a = d->temp;  *b = d->hum;   break;
    case GW_ID_ENGINE:  *a = d->temp;  *b = 0.0f;     break;
    case GW_ID_ATTITUDE:*a = d->pitch; *b = d->roll;  break;
    case GW_ID_REAR:    *a = d->dist;  *b = 0.0f;     break;
    default:            *a = 0.0f;     *b = 0.0f;     break;
    }
}

/* ==========================================================================
 *  —— 以下为真正落库部分 ——
 * ========================================================================== */

#if GW_WITH_SQLITE

static sqlite3      * s_db       = NULL;
static sqlite3_stmt * s_ins_sen  = NULL;
static sqlite3_stmt * s_ins_alarm= NULL;
static bool           s_in_tx    = false;
static uint32_t       s_pending  = 0;
static uint32_t       s_last_flush = 0;
static uint32_t       s_last_prune = 0;

static const char * s_dbpath = GW_DB_PATH;

const char * storage_unit_dbpath(void) { return s_dbpath; }

static void _exec(const char * sql)
{
    char * err = NULL;

    if(sqlite3_exec(s_db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "[store] SQL 失败：%s  （%s）\n", sql, err ? err : "?");
        if(err) sqlite3_free(err);
    }
}

/* 建库、建表、预编译语句 */
static bool _db_open(void)
{
    int rc;

    rc = sqlite3_open_v2(GW_DB_PATH, &s_db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "[store] 打开 %s 失败：%s\n",
                GW_DB_PATH, s_db ? sqlite3_errmsg(s_db) : "?");
        if(s_db) { sqlite3_close(s_db); s_db = NULL; }
        return false;
    }

    sqlite3_busy_timeout(s_db, 3000);

    /* SD 卡 / NAND 上最怕大量小随机写，这三条能把写放大压下来。
     * journal_mode=MEMORY 的代价：掉电会丢最近一次未提交的事务，
     * 对"历史记录"这种数据可以接受；报警事件是每条单独提交的，丢不了。 */
    _exec("PRAGMA journal_mode = MEMORY");
    _exec("PRAGMA synchronous  = NORMAL");
    _exec("PRAGMA temp_store   = MEMORY");

    /* 采样表：一行就是一次观测 */
    _exec("CREATE TABLE IF NOT EXISTS sensor_log ("
          "  id     INTEGER PRIMARY KEY AUTOINCREMENT,"
          "  ts     INTEGER NOT NULL,"              /* unix 秒 */
          "  sensor INTEGER NOT NULL,"              /* 1舱温 2机温 3姿态 4倒车 */
          "  v1     REAL,"                          /* 温度 / pitch / 距离 */
          "  v2     REAL,"                          /* 湿度 / roll */
          "  status INTEGER NOT NULL DEFAULT 0,"
          "  extra  INTEGER NOT NULL DEFAULT 0)");

    _exec("CREATE INDEX IF NOT EXISTS idx_sensor_ts ON sensor_log(ts)");

    /* 报警表：一行是一次等级跳变 */
    _exec("CREATE TABLE IF NOT EXISTS alarm_log ("
          "  id     INTEGER PRIMARY KEY AUTOINCREMENT,"
          "  ts     INTEGER NOT NULL,"
          "  sensor INTEGER NOT NULL,"
          "  level  INTEGER NOT NULL,"              /* 2=预警 3=超限 */
          "  value  REAL,"
          "  acked  INTEGER NOT NULL DEFAULT 0)");

    _exec("CREATE INDEX IF NOT EXISTS idx_alarm_ts ON alarm_log(ts)");

    if(sqlite3_prepare_v2(s_db,
            "INSERT INTO sensor_log(ts,sensor,v1,v2,status,extra)"
            " VALUES(?,?,?,?,?,?)", -1, &s_ins_sen, NULL) != SQLITE_OK) {
        fprintf(stderr, "[store] 预编译 sensor_log 失败：%s\n", sqlite3_errmsg(s_db));
        return false;
    }

    if(sqlite3_prepare_v2(s_db,
            "INSERT INTO alarm_log(ts,sensor,level,value,acked)"
            " VALUES(?,?,?,?,0)", -1, &s_ins_alarm, NULL) != SQLITE_OK) {
        fprintf(stderr, "[store] 预编译 alarm_log 失败：%s\n", sqlite3_errmsg(s_db));
        return false;
    }

    s_active = true;
    return true;
}

static void _db_close(void)
{
    if(s_db == NULL) return;

    if(s_in_tx) {
        sqlite3_exec(s_db, "COMMIT", NULL, NULL, NULL);
        s_in_tx = false;
    }
    if(s_ins_sen)   { sqlite3_finalize(s_ins_sen);   s_ins_sen   = NULL; }
    if(s_ins_alarm) { sqlite3_finalize(s_ins_alarm); s_ins_alarm = NULL; }

    sqlite3_close(s_db);
    s_db = NULL;
    s_active = false;
}

static bool _begin(void)
{
    if(s_in_tx) return true;
    if(sqlite3_exec(s_db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "[store] BEGIN 失败：%s\n", sqlite3_errmsg(s_db));
        return false;
    }
    s_in_tx = true;
    return true;
}

static void _commit(void)
{
    if(!s_in_tx) return;
    if(sqlite3_exec(s_db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
        fprintf(stderr, "[store] COMMIT 失败：%s\n", sqlite3_errmsg(s_db));
    }
    s_in_tx      = false;
    s_pending    = 0;
    s_last_flush = _now_ms();
}

/* 攒够条数或超时就提交 */
static void _maybe_flush(bool force)
{
    uint32_t now = _now_ms();

    if(!s_in_tx) return;

    if(force ||
       s_pending >= GW_FLUSH_ROWS ||
       (uint32_t)(now - s_last_flush) >= GW_FLUSH_MS) {
        _commit();
    }
}

static bool _insert_sample(const struct msgdata * d)
{
    float a, b;

    _unpack(d, &a, &b);

    if(!_begin()) return false;

    sqlite3_reset(s_ins_sen);
    sqlite3_bind_int64(s_ins_sen, 1, _msg_ts(d));
    sqlite3_bind_int  (s_ins_sen, 2, d->id);
    sqlite3_bind_double(s_ins_sen, 3, (double)a);
    sqlite3_bind_double(s_ins_sen, 4, (double)b);
    sqlite3_bind_int  (s_ins_sen, 5, d->status);
    sqlite3_bind_int  (s_ins_sen, 6, d->reverse);

    if(sqlite3_step(s_ins_sen) != SQLITE_DONE) {
        fprintf(stderr, "[store] INSERT 失败：%s\n", sqlite3_errmsg(s_db));
        return false;
    }

    s_pending++;
    s_rows++;
    _maybe_flush(false);
    return true;
}

/* 报警：单独事务、立即提交，掉电也不会丢 */
static bool _insert_alarm(const struct msgdata * d)
{
    bool ok = true;

    /* 先把采样事务收了，避免报警和采样混在一个事务里被一起丢 */
    _commit();

    if(sqlite3_exec(s_db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) return false;

    sqlite3_reset(s_ins_alarm);
    sqlite3_bind_int64(s_ins_alarm, 1, _msg_ts(d));
    sqlite3_bind_int  (s_ins_alarm, 2, d->id - GW_ID_ALARM_BASE);
    sqlite3_bind_int  (s_ins_alarm, 3, d->status);
    sqlite3_bind_double(s_ins_alarm, 4, (double)d->temp);

    if(sqlite3_step(s_ins_alarm) == SQLITE_DONE) {
        s_alarms++;
    }
    else {
        fprintf(stderr, "[store] 报警 INSERT 失败：%s\n", sqlite3_errmsg(s_db));
        ok = false;
    }

    if(sqlite3_exec(s_db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) ok = false;
    return ok;
}

static void _prune(void)
{
    time_t  cutoff;
    char    sql[160];
    uint32_t now = _now_ms();

    /* 每小时清一次就够 */
    if((uint32_t)(now - s_last_prune) < 3600u * 1000u) return;
    s_last_prune = now;

    cutoff = time(NULL) - (time_t)GW_KEEP_DAYS * 86400;

    snprintf(sql, sizeof(sql), "DELETE FROM sensor_log WHERE ts < %lld",
             (long long)cutoff);
    _exec(sql);

    snprintf(sql, sizeof(sql), "DELETE FROM alarm_log WHERE ts < %lld",
             (long long)cutoff);
    _exec(sql);
}

#else   /* ---------------- GW_WITH_SQLITE == 0：降级实现 ---------------- */

const char * storage_unit_dbpath(void) { return "（未启用，见 lib/README.md）"; }

static void _maybe_flush(bool force) { (void)force; }
static void _prune(void)    { }

static bool _insert_sample(const struct msgdata * d)
{
    s_rows++;
    return true;
}

static bool _insert_alarm(const struct msgdata * d)
{
    s_alarms++;
    (void)d;
    return true;
}

#endif

/* ==========================================================================
 *  处理一条消息
 * ========================================================================== */

static void _handle(const struct msgdata * d)
{
    /* 报警事件：id = 100 + 传感器号 */
    if(d->id > GW_ID_ALARM_BASE && d->id <= GW_ID_ALARM_BASE + GW_ID_REAR) {
        _insert_alarm(d);
#if GW_DEBUG_LOG
        printf("[store] 报警记录 sensor=%d level=%d value=%.2f\n",
               d->id - GW_ID_ALARM_BASE, d->status, (double)d->temp);
        fflush(stdout);
#endif
        return;
    }

    if(d->id < GW_ID_CABIN || d->id > GW_ID_REAR) return;

    {
        float a, b;

        _unpack(d, &a, &b);
        if(_should_store(d, a, b)) {
            _insert_sample(d);
        }
    }
}

/* ==========================================================================
 *  任务线程
 * ========================================================================== */

void * storage_unit_task(void * arg)
{
    struct msgdata d;
    char           from[32];
    uint32_t       last_report = 0;

    (void)arg;

#if GW_WITH_SQLITE
    if(_db_open()) {
        printf("[store] 数据存储单元已启动（SQLite %s → %s，保留 %d 天）\n",
               sqlite3_libversion(), GW_DB_PATH, GW_KEEP_DAYS);
    }
    else {
        printf("[store] 数据存储单元启动，但数据库打开失败 —— 只统计不落库\n");
    }
#else
    printf("[store] 数据存储单元已启动（编译时未启用 SQLite，只统计不落库）\n");
    printf("[store]   想让数据真正存下来：把 libsqlite3.so 放进 lib/lib/ 后重新 make\n");
#endif
    fflush(stdout);

    last_report = _now_ms();

    while(1) {
        memset(&d, 0, sizeof(d));

        if(recv_mail_msg(&d, from) != 0) {
            /* 没消息也要把攒着的行提交掉，否则低频时段要等很久才落盘 */
            _maybe_flush(false);

            /* 每秒报一次统计，方便确认存储单元还活着 */
            if((uint32_t)(_now_ms() - last_report) >= 1000u) {
                last_report = _now_ms();
                _prune();
            }
            usleep(2000);
            continue;
        }

        if(d.id == MSG_ID_QUIT) break;

        _handle(&d);
    }

    _maybe_flush(true);
#if GW_WITH_SQLITE
    _db_close();
#endif

    printf("[store] 数据存储单元已退出（采样 %u 行，报警 %u 行，节流跳过 %u 行）\n",
           s_rows, s_alarms, s_dropped);
    fflush(stdout);
    return NULL;
}
