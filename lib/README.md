# lib/ —— SQLite 3.7.3（i.MX6ULL 交叉编译版）

这套东西是**已编译好的 ARM 版本**，给板子和交叉编译用，不是给 Ubuntu 本机用的。

```
lib/
├── include/   sqlite3.h  sqlite3ext.h  mysqlite.h      头文件
├── bin/       sqlite3                                  板子上直接用命令行查数据
├── share/     man/man1/sqlite3.1                       手册
└── lib/       libsqlite3.so*  libsqlite3.la  sqlite3.pc  ← 目前是空的，见下
```

## lib/lib 里应该有什么（现在缺）

| 文件 | 大小参考 | 来源 |
|---|---|---|
| `libsqlite3.so.0.8.6` | 约 700 KB | 真正的动态库 |
| `libsqlite3.so.0` | 符号链接 → `.so.0.8.6` | |
| `libsqlite3.so` | 符号链接 → `.so.0.8.6` | 链接时用这个 |
| `libsqlite3.la` | 1 KB | libtool 描述文件，可不要 |
| `sqlite3.pc` | 1 KB | pkg-config 描述，可不要 |

**关键**：必须是 **32 位小端 ARM（EABI5）** 版本，和 `lib/bin/sqlite3` 一致。
如果拷进来的是 x86-64 的 `.so`，链接时会报
`file in wrong format` / `skipping incompatible`，那就说明拿的是 Ubuntu 系统里 `/usr/lib/x86_64-linux-gnu/` 那份，不能用于交叉编译。

## 怎么把 .so 拷进来（拖拽已经失败 3 次）

VMware 拖拽对二进制 `.so` 和符号链接很不友好。改用打包：

```bash
# 虚拟机里，sqlite 安装前缀目录（你原来工程里那个 lib 目录）
cd ~/project/lv_port_linux-release-v8.4_timer/lib/lib
tar -czf ~/sqlite_lib.tar.gz libsqlite3.so* libsqlite3.la sqlite3.pc
ls -lh ~/sqlite_lib.tar.gz
```

把 `~/sqlite_lib.tar.gz` 拖出来（压缩包比散文件成功率高得多），放到本工程
`lib/lib/` 下解开：

```bash
cd lib/lib && tar -xzf ~/sqlite_lib.tar.gz
```

或者更省事——虚拟机里直接 SCP/共享目录拷进本工程的 `lib/lib/`。

## 编译时怎么用

主 Makefile 会**自动检测** `lib/lib/libsqlite3.so`：

- 有 → 自动加 `-Ilib/include -Llib/lib -lsqlite3`，`GW_WITH_SQLITE=1`，存储单元真正落库
- 没有 → 自动降级成 `GW_WITH_SQLITE=0`，存储单元只打日志不落库，**编译照样能过**

强制指定：

```bash
make WITH_SQLITE=0      # 不用 sqlite（先跑通界面时用）
make WITH_SQLITE=1      # 强制使用（会因为没有 .so 而链接失败，用于排查）
```

编译时可以直接看到判定结果：

```
  SQLite: 启用 (lib/lib/libsqlite3.so)
  CC units/storage_unit.c
```
或
```
  SQLite: 未检测到 lib/lib/libsqlite3.so，存储单元降级为只打日志
```

## 板子上部署

程序运行时要能加载到这个 .so，两种方式任选：

**① 拷到系统库目录（推荐）**

```sh
cd /root
wget -O sqlite_lib.tar.gz http://192.168.1.100:8000/sqlite_lib.tar.gz
tar -xzf sqlite_lib.tar.gz -C /tmp
cp /tmp/libsqlite3.so.0.8.6 /usr/lib/
ln -sf /usr/lib/libsqlite3.so.0.8.6 /usr/lib/libsqlite3.so.0
ln -sf /usr/lib/libsqlite3.so.0.8.6 /usr/lib/libsqlite3.so
/sbin/ldconfig 2>/dev/null || true
```

**② 放到程序同目录**（Makefile 已加 `-Wl,-rpath,$ORIGIN`）

```sh
cp libsqlite3.so.0.8.6 /root/       # 和 gateway_ui 放一起
```

验证板子上能不能用：

```sh
ldd ./gateway_ui | grep sqlite      # 应该指向 /usr/lib/libsqlite3.so.0
./lib/bin/sqlite3 /root/gwdata.db   # 能进 sqlite> 提示符就说明运行库 OK
```

## 数据库落在哪

默认 `/root/gwdata.db`，改 `units/storage_unit.c` 顶部的 `GW_DB_PATH` 即可。

板子上查数据：

```sh
/root/lib/bin/sqlite3 /root/gwdata.db
sqlite> .tables
sqlite> .schema sensor_log
sqlite> SELECT * FROM alarm_log ORDER BY id DESC LIMIT 10;
sqlite> SELECT datetime(ts,'unixepoch','localtime'), sensor, v1 FROM sensor_log
        ORDER BY id DESC LIMIT 20;
sqlite> .quit
```
