#!/bin/sh
# =====================================================================
#  板子上一键自检：定位"界面数据不对"到底出在哪一层
#
#  用法（板子上执行）：
#      sh board_check.sh
#  如果报 ": not found" 之类的怪错，是行尾符问题，先执行：
#      sed -i 's/\r$//' board_check.sh
#
#  它做七件事：
#    1. 找出板子上所有 gateway_ui，看有没有跑错目录
#    2. 检查每个二进制里还带不带"模拟线程"（带了就是在跑 DEMO 波形）
#    3. 检查 /dev 节点和 .ko 有没有加载
#    4. 直接读 DHT11 的 4 个原始字节（跳过所有上层代码）
#    5. 直接读 MPU6050 的 14 个原始字节
#    6. 列出声光报警可用的设备（LED class / /dev/beep / PWM）
#    7. dmesg 最后 15 行
#
#  第 4、5 步是关键：这两步稳，说明驱动没问题，问题在上层；
#  这两步就乱，说明是驱动 / 接线 / 供电，跟 LVGL 一点关系都没有。
#  第 6 步用来确认报警灯 / 蜂鸣器的真实路径（自动探测失败时手工指定它）。
# =====================================================================

HR="------------------------------------------------------------"

echo "########## 1. 板子上所有 gateway_ui ##########"
find / -xdev -name 'gateway_ui' -type f 2>/dev/null | while read -r f; do
    echo "--- $f"
    ls -l "$f" 2>/dev/null
    if command -v md5sum >/dev/null 2>&1; then
        echo "    md5: $(md5sum "$f" 2>/dev/null | cut -d' ' -f1)"
    fi
done
echo "（如果这里出现两个以上，说明可能跑的是旧目录里那份）"

echo
echo "########## 2. 每个二进制里还带不带模拟线程 ##########"
for f in $(find / -xdev -name 'gateway_ui' -type f 2>/dev/null); do
    if command -v strings >/dev/null 2>&1; then
        demo=$(strings "$f" 2>/dev/null | grep -c '模拟采集线程创建失败')
        real=$(strings "$f" 2>/dev/null | grep -c '数据源 = ')
    else
        demo=$(grep -a -c '模拟采集线程创建失败' "$f" 2>/dev/null)
        real=$(grep -a -c '数据源 = ' "$f" 2>/dev/null)
    fi
    echo "$f"
    echo "    模拟线程字符串 = $demo   （0 = 好，>=1 = 这份是 DEMO 版）"
    echo "    新版数据源标记 = $real   （1 = 是刚编译的新版）"
done

echo
echo "########## 3. 设备节点与驱动模块 ##########"
ls -l /dev/dht11_misc /dev/mpu6050_misc 2>&1
echo "--- lsmod ---"
lsmod 2>/dev/null | grep -Ei 'dht11|mpu6050|mpu' || echo "（没看到 dht11/mpu6050 模块）"

echo
echo "########## 4. DHT11 原始 4 字节（连读 6 次，间隔 1 秒）##########"
echo "格式：湿度整数 湿度小数 温度整数 温度小数"
i=0
while [ $i -lt 6 ]; do
    printf "  [%d] " $i
    dd if=/dev/dht11_misc bs=4 count=1 2>/dev/null | od -An -tu1 | tr -s ' '
    i=$((i + 1))
    sleep 1
done
echo "（这 6 行如果每行都一样或只差 1，说明 DHT11 是稳的；"
echo "  如果完全随机，是接线/上拉/时序问题，不是界面问题）"

echo
echo "########## 5. MPU6050 原始 14 字节（连读 5 次，间隔 1 秒）##########"
echo "格式：axH axL ayH ayL azH azL TH TL gxH gxL gyH gyL gzH gzL（大端）"
i=0
while [ $i -lt 5 ]; do
    printf "  [%d] " $i
    dd if=/dev/mpu6050_misc bs=14 count=1 2>/dev/null | od -An -tx1 | tr -s ' '
    i=$((i + 1))
    sleep 1
done
echo "（静止时 ay 两字节应该稳定在 FD 20 附近，az 在 0C 70 附近）"

echo
echo "########## 6. 声光报警设备（LED / 蜂鸣器）##########"
echo "--- /sys/class/leds（出厂 LED 一般在这里，注意 trigger 是不是 [heartbeat]）"
if [ -d /sys/class/leds ]; then
    ls /sys/class/leds
    for d in /sys/class/leds/*; do
        [ -d "$d" ] || continue
        printf "  %-24s max_brightness=%s trigger=%s\n" \
            "$(basename "$d")" \
            "$(cat "$d/max_brightness" 2>/dev/null)" \
            "$(cat "$d/trigger" 2>/dev/null)"
    done
else
    echo "  （没有 led class —— LED 不是走 leds 子系统的）"
fi

echo "--- 蜂鸣器可能的节点"
ls -l /dev/beep /dev/buzzer /dev/led 2>/dev/null || echo "  （没有 /dev/beep、/dev/buzzer、/dev/led）"
ls -d /sys/class/pwm/pwmchip* 2>/dev/null || echo "  （没有 pwm 子系统，说明蜂鸣器不是 PWM 驱动）"

echo "--- 已经 export 的 GPIO"
ls /sys/class/gpio 2>/dev/null | grep -E '^gpio[0-9]+$' | tr '\n' ' '
echo

echo
echo "########## 7. 内核最近 15 行 ##########"
dmesg 2>/dev/null | tail -15

echo
echo "$HR"
echo "把上面全部内容贴给我。"
echo "另外再跑一次程序，把开头 3 行也贴出来："
echo "    ./gateway_ui"
echo "    → [ui] gateway_ui build ..."
echo "    → [ui] 数据源 = ..."
echo "    → [bridge] ..."
echo "$HR"
