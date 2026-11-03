# =====================================================================
#  大型车辆车载网关监测系统 —— LVGL 8.3 工程 Makefile
#
#  用法：
#    make                # 交叉编译到 i.MX6ULL（默认）
#    make NATIVE=1       # 在 PC 上本地编译，先跑通逻辑
#    make clean
#
#  目录约定：
#    lvgl/                  LVGL 8.3 源码（git clone 到这里）
#    include/ src/          你的线程邮箱框架（linklist / linkqueue / mailbox）
#    units/                 五个单元：alarm(预警) net(上传) storage(存储，可选)
#    bridge/                采集单元（读 /dev 节点 + 解码 + 姿态融合）
#    lib/                   SQLite 交叉编译版（include/bin/share/lib 四层）
#    screens/               页面构建（SquareLine 导出）
#    components/            面板构建（SquareLine 导出）
#    fonts/                 中文字库
#    ui.c / ui.h            入口
#    ui_sensor.c            数据桥接层
#    main.c                 fb 驱动 + 采集线程 + 主循环
# =====================================================================

# ---------- 工具链 ----------
ifeq ($(NATIVE),1)
  CC           := gcc
  CFLAGS_ARCH  := -O2 -g
else
  CROSS        ?= arm-linux-gnueabihf-
  CC           := $(CROSS)gcc
  # i.MX6ULL 是 Cortex-A7，开 NEON 能显著加速 LVGL 的混合运算
  CFLAGS_ARCH  := -O2 -march=armv7-a -mtune=cortex-a7 -mfpu=neon -mfloat-abi=hard
endif

# ---------- 头文件路径 ----------
INCS := -I. -Iinclude -Isrc -Iunits -Ibridge -Iscreens -Icomponents -Ifonts -Ilib/include -Ilvgl

CFLAGS := $(CFLAGS_ARCH) -std=gnu11 -Wall -Wextra -Wno-unused-parameter $(INCS)
LDLIBS := -lm -lpthread -lrt

# ---------- SQLite（存储单元用，自动查找） ----------
# 按顺序找 libsqlite3.so：工程自带 lib/lib → $SQLITE_PREFIX/lib → 上一层 lib/lib。
# 想让程序真正落库，把虚拟机上那套 sqlite 的 .so 拷进 lib/lib/ 即可（见 lib/README.md）；
# 也可以在命令行直接指定安装前缀，不必拷文件：
#     make SQLITE_PREFIX=/home/linux/sqlite_install
SQLITE_PREFIX ?=
SQLITE_DIRS   := lib/lib $(if $(SQLITE_PREFIX),$(SQLITE_PREFIX)/lib) ../lib/lib ../lib/sqlite_install/lib
SQLITE_SO     := $(firstword $(foreach d,$(SQLITE_DIRS),$(wildcard $(d)/libsqlite3.so $(d)/libsqlite3.so.0)))

ifeq ($(origin WITH_SQLITE), undefined)
  ifeq ($(SQLITE_SO),)
    WITH_SQLITE := 0
  else
    WITH_SQLITE := 1
  endif
endif

ifeq ($(WITH_SQLITE),1)
  ifeq ($(SQLITE_SO),)
    $(error 指定了 WITH_SQLITE=1 但找不到 libsqlite3.so，见 lib/README.md)
  endif
  CFLAGS += -DGW_WITH_SQLITE=1 $(if $(SQLITE_PREFIX),-I$(SQLITE_PREFIX)/include)
  # rpath 两个都写：$ORIGIN 让运行时先从程序所在目录找，
  # /usr/lib 是板子上常规安装位置，随你放哪边都能起来。
  LDLIBS += -L$(dir $(SQLITE_SO)) -lsqlite3 -Wl,-rpath-link,$(dir $(SQLITE_SO)) -Wl,-rpath,'$$ORIGIN' -Wl,-rpath,/usr/lib
  SQLITE_MSG := 启用（$(SQLITE_SO)）
else
  CFLAGS += -DGW_WITH_SQLITE=0
  SQLITE_MSG := 未找到 libsqlite3.so → 存储单元只统计不落库（放进 lib/lib/ 后重新 make 即启用）
endif

# ---------- 源文件 ----------
APP_SRC := main.c \
           ui.c \
           ui_helpers.c \
           ui_sensor.c \
           src/linklist.c \
           src/linkqueue.c \
           src/mailbox.c \
           bridge/sensor_bridge.c \
           units/display_unit.c \
           units/alarm_unit.c \
           units/alarm_hw.c \
           units/net_unit.c \
           units/storage_unit.c \
           screens/ui_Gateway.c \
           components/ui_comp_gateway.c \
           components/ui_comp_hook.c \
           fonts/ui_font_cn_14.c \
           fonts/ui_font_cn_18.c \
           fonts/ui_font_cn_25.c \
           fonts/ui_font_cn_32.c

# LVGL 核心源码：自动收集，不用手写一长串
LVGL_SRC := $(shell find lvgl/src -name '*.c' 2>/dev/null)

SRC := $(APP_SRC) $(LVGL_SRC)
APP_OBJ := $(APP_SRC:.c=.o)
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

TARGET := gateway_ui

# 说明：工程如果放在 D 盘 / 共享文件夹 / 网盘同步目录里，文件时间戳往往不可靠，
# make 会误判"源码比 .o 旧"而跳过重编 —— 改了宏（比如 DEMO_MODE）却链接到旧的
# main.o，界面上就会一直跑旧逻辑，非常难查。
# 所以这里每次都先把应用层那 16 个 .o 删掉重编（几秒钟），LVGL 的 .o 保留。
all: check
	@echo "  SQLite: $(SQLITE_MSG)"
	@rm -f $(APP_OBJ)
	@$(MAKE) --no-print-directory $(TARGET)

check:
	@if [ -z "$(LVGL_SRC)" ]; then \
	  echo "!! 找不到 lvgl/src"; \
	  echo "   先执行：git clone -b release/v8.3 --depth 1 https://github.com/lvgl/lvgl.git lvgl"; \
	  exit 1; \
	fi

$(TARGET): $(OBJ)
	@echo "  LD $@"
	@$(CC) $(OBJ) -o $@ $(LDLIBS)
	@du -h $@ | cut -f1

%.o: %.c
	@echo "  CC $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

# ---------- 部署 ----------
BOARD_IP   ?= 192.168.1.100
BOARD_USER ?= root
deploy: all
	scp $(TARGET) $(BOARD_USER)@$(BOARD_IP):/usr/bin/
	@echo "  已上传，板子上执行：/usr/bin/$(TARGET)"

.PHONY: all clean check deploy

-include $(DEP)
