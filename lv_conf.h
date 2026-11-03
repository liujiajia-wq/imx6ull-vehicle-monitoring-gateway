/**
 * @file lv_conf.h
 * @brief LVGL 8.3.x / 8.4.x 配置 —— 针对 imx6ull 800x480 + SquareLine Studio 1.5.3 导出工程
 *
 * 这不是完整版 lv_conf.h。完整版请从 lvgl/lv_conf_template.h 复制后，
 * 按下面的差异逐项修改；也可以直接用本文件替换工程根目录的 lv_conf.h。
 *
 * 关键前提：
 *   - SquareLine 导出的 ui.c 里有编译期断言，必须是 LV_COLOR_DEPTH 32 且 16_SWAP 0
 *   - 仪表盘用到 arc / bar，必须开启
 *   - 显示刷新周期 30ms ≈ 33FPS，imx6ull 单核 A7 的实际舒适值
 */

#if 1   /* 置 0 可整体禁用 LVGL 配置生效（调试用） */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ==================== 颜色设置 ==================== */

/* 必须 32：与 SquareLine 导出一致，也与 /dev/fb0 的 32bpp 对齐 */
#define LV_COLOR_DEPTH          32

/* 必须 0：同上 */
#define LV_COLOR_16_SWAP        0

/* 屏幕透明度支持关掉可以省一点渲染开销 */
#define LV_COLOR_SCREEN_TRANSP  0

/* ==================== 内存 ==================== */

/* imx6ull 有 512MB DDR，用内置堆 128KB 足够；
 * 若开了 CJK 全字库或图片多，可用 LV_MEM_CUSTOM 走 malloc。
 */
#define LV_MEM_CUSTOM           0
#define LV_MEM_SIZE             (256U * 1024U)
#define LV_MEM_ADR              0
#define LV_MEM_BUF_MAX_NUM      16

/* ==================== 显示 ==================== */

/* 30ms = 33FPS。imx6ull 用软件渲染，低于 20 会明显卡顿，
 * 低于 30 会让 CPU 一直满载，30~50 是平衡点。
 */
#define LV_DISP_DEF_REFR_PERIOD 30

/* 800x480 4.3~7 寸屏常用 130，决定默认字体和控件尺寸的缩放 */
#define LV_DPI_DEF              130

/* ==================== 输入设备 ==================== */

#define LV_INDEV_DEF_READ_PERIOD 30

/* ==================== 功能裁剪 ==================== */

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_ASSERT_STYLE     0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0

#define LV_USE_LOG              0
#define LV_USE_DEBUG            0

/* ==================== 字体 ==================== */

/* 工程自带 4 档中文字库 ui_font_cn_{14,18,25,32}（见 fonts/，
 * 用 lv_font_conv 从 STXIHEI.TTF 生成，覆盖界面全部 127 个汉字）。
 * 下面的 Montserrat 只用于 LV_SYMBOL_* 图标；若图标显示成方块，
 * 检查 LV_FONT_MONTSERRAT_14 是否打开。
 */
#define LV_FONT_MONTSERRAT_14   1       /* 顶栏/底栏/卡片副标题 */
#define LV_FONT_MONTSERRAT_16   1       /* 卡片标题 */
#define LV_FONT_MONTSERRAT_20   1       /* 顶栏主标题、姿态数值 */
#define LV_FONT_MONTSERRAT_28   1       /* 温度/烟雾/光照大号数值 */
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   0
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_30   0
#define LV_FONT_MONTSERRAT_32   0

#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* 字体压缩：SquareLine 生成的 CJK 字库用的是 no-compress，
 * 若你自己转的字体用了压缩，这里必须置 1
 */
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_SUBPX       0
#define LV_FONT_FMT_TXT_LARGE   0

/* ==================== 控件 ==================== */
/*
 * 这里全部打开。关掉任何一个都会在【链接阶段】失败，原因有两个：
 *
 *  1) SquareLine 导出的 ui_helpers.c 是"通用工具箱"，给每种控件都写了
 *     set/get 函数。即使界面只用 obj/label/arc/bar，它照样调用
 *     lv_slider_set_value()、lv_textarea_cursor_up()、lv_img_set_src()...
 *  2) lv_theme_default.c 的 theme_apply() 直接引用了 lv_line_class、
 *     lv_btnmatrix_class、lv_table_class、lv_checkbox_class 等符号，
 *     没有 #if 保护。
 *
 * 被关掉的控件，其 .c 正文被 #if 整段跳过，符号就不存在了。症状是：
 * 编译全部通过、无 warning，只在 LD 阶段报 undefined reference。
 *
 * 代价：全开比最小集合多约 40KB 代码，imx6ull 上无所谓。
 * 真要裁剪，请连 ui_helpers.c 一起裁（删掉用不到的那几个函数）。
 */

#define LV_USE_ARC              1       /* 温度 / 姿态圆弧仪表 */
#define LV_USE_BAR              1       /* 进度条 */
#define LV_USE_LABEL            1       /* 全部文字 */
#define LV_USE_OBJ              1
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        1       /* theme_default / keyboard 依赖 */
#define LV_USE_CANVAS           1
#define LV_USE_CHECKBOX         1       /* theme_default 依赖 */
#define LV_USE_DROPDOWN         1       /* theme_default + ui_helpers 依赖 */
#define LV_USE_IMG              1       /* ui_helpers 依赖 */
#define LV_USE_LINE             1       /* theme_default 依赖 */
#define LV_USE_ROLLER           1       /* theme_default + ui_helpers 依赖 */
#define LV_USE_SLIDER           1       /* theme_default + ui_helpers 依赖 */
#define LV_USE_SWITCH           1       /* theme_default 依赖 */
#define LV_USE_TEXTAREA         1       /* theme_default + ui_helpers 依赖 */
#define LV_USE_TABLE            1       /* theme_default 依赖 */
#define LV_USE_CHART            1
#define LV_USE_METER            1
#define LV_USE_SPAN             1
#define LV_USE_COLORWHEEL       1
#define LV_USE_IMGBTN           1
#define LV_USE_LED              1

/* ---- 扩展控件（extra） ---- */
#define LV_USE_SPINBOX          1       /* ui_helpers 依赖 */
#define LV_USE_KEYBOARD         1       /* ui_helpers 依赖 */
#define LV_USE_TABVIEW          1
#define LV_USE_TILEVIEW         1
#define LV_USE_WIN              1
#define LV_USE_LIST             1
#define LV_USE_MENU             1
#define LV_USE_MSGBOX           1
#define LV_USE_SPINNER          1
#define LV_USE_CALENDAR         1
#define LV_USE_ANIMIMG          1

/* ==================== 主题 ==================== */

/* ui_init() 里调用了 lv_theme_default_init()，必须开 */
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1       /* 深色主题，与仪表盘配色一致 */
#define LV_THEME_DEFAULT_GROW   1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_THEME_DEFAULT_COLOR_PRIMARY      lv_palette_main(LV_PALETTE_BLUE)
#define LV_THEME_DEFAULT_COLOR_SECONDARY    lv_palette_main(LV_PALETTE_RED)
#define LV_USE_THEME_BASIC      1
#define LV_USE_THEME_MONO       0

/* ==================== 布局 ==================== */

/* 我们的界面全用绝对坐标，不依赖 flex；但库里的 tabview / menu / msgbox /
 * keyboard 等控件内部调用了 lv_obj_set_flex_flow()，关掉会再次出现链接错误。
 */
#define LV_USE_FLEX             1
#define LV_USE_GRID             1

/* ==================== 其他 ==================== */

#define LV_USE_ANIM             1
#define LV_USE_SNAPSHOT         0
#define LV_USE_GROUP            1
#define LV_USE_GC               1
#define LV_USE_USER_DATA        1
#define LV_USE_PERF_MONITOR     0       /* 若想左上角看 FPS/CPU，改成 1 */
#define LV_USE_MEM_MONITOR      0

/* 日志：LV_USE_LOG 为 1 时才需要 */
#if LV_USE_LOG
    #define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF   1
#endif

#endif /* LV_CONF_H */
#endif /* 文件末尾的 #if 1 */
