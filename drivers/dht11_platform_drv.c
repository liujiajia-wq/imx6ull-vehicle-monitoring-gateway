// DHT11 —— platform + misc 驱动（已修复原版的两处隐患）
//
// 与原版 dht11_platform_drv.c 的差异（对外接口、设备名、compatible 全部不变，
// 直接替换原文件重新编译 .ko 即可）：
//
//   1) 【致命】原版 dht11_read() 里 master_recv_dht11_respone() 返回 -1 时
//      直接 return，spin_unlock_irqrestore() 没执行，中断一直处于关闭状态。
//      后果：传感器没接/线松/接触不良时，读一次就把整个系统卡死（UI 也冻住）。
//      现在所有失败分支都走统一出口解锁。
//
//   2) 原版 master_recv_dht11_respone() 里
//          while (!gpio_get_value(...));
//          while (gpio_get_value(...));
//      以及 master_recv_dht11_data() 里的两个 while 都是死等，没有超时。
//      DHT11 掉线时引脚电平不会变，内核态无限循环（而且是在关中断的自旋锁里）。
//      现在每个等待都加了 us 级超时，超时就返回错误，用户态 read() 拿到 -1。
//
//   3) 数据校验失败时（-2）现在也会先把引脚拉回输出高电平再由上层重试，
//      避免总线一直被从机拉着。
//
// 数据格式（没变，用户态照旧）：
//    read() 成功返回 4，缓冲区 4 字节：
//      [0] 湿度整数  [1] 湿度小数  [2] 温度整数  [3] 温度小数
//    read() 失败返回 -1（无应答/超时） / -2（校验错） / -3（拷贝失败）

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <asm/delay.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>

extern void msleep(unsigned int msecs);

/* 超时上限：udelay(10) 一次，1000 次 = 10ms。
 * DHT11 一个电平最长 80us 左右，10ms 足够宽松，真掉线也不会卡住内核。 */
#define DHT11_WAIT_TIMEOUT_US   1000

static int dht11gpiono = 0;
static spinlock_t lock;

void master_send_dht11_reset(void)
{
    gpio_set_value(dht11gpiono, 0);
    msleep(20);                     /* 主机拉低 >=18ms */
    gpio_set_value(dht11gpiono, 1);
    udelay(30);                     /* 拉高 20~40us 后释放总线 */
}

/* 等 DHT11 的应答：先 80us 低，再 80us 高。返回 0 正常，-1 超时 */
static int master_recv_dht11_respone(void)
{
    int timeout = 0;

    gpio_direction_input(dht11gpiono);

    /* 等待从机把总线拉低（最长 20*10us = 200us） */
    while (timeout < 20 && gpio_get_value(dht11gpiono)) {
        udelay(10);
        timeout++;
    }
    if (timeout >= 20)
        return -1;

    /* 等待低电平结束 */
    timeout = 0;
    while (!gpio_get_value(dht11gpiono)) {
        udelay(10);
        if (++timeout > DHT11_WAIT_TIMEOUT_US)
            return -1;
    }

    /* 等待高电平结束 */
    timeout = 0;
    while (gpio_get_value(dht11gpiono)) {
        udelay(10);
        if (++timeout > DHT11_WAIT_TIMEOUT_US)
            return -1;
    }

    return 0;
}

/* 读 len 字节。返回 0 正常，-1 超时（引脚电平不动） */
static int master_recv_dht11_data(unsigned char *pdata, int len)
{
    int i = 0;
    int n = 0;
    int cnt = 0;
    int timeout = 0;

    for (i = 0; i < len; i++) {
        for (n = 7; n >= 0; n--) {
            /* 每 bit 开头是 50us 低电平 */
            timeout = 0;
            while (!gpio_get_value(dht11gpiono)) {
                udelay(10);
                if (++timeout > DHT11_WAIT_TIMEOUT_US)
                    goto out_timeout;
            }

            /* 高电平时长决定 0 还是 1：26~28us = 0，70us = 1 */
            cnt = 0;
            while (gpio_get_value(dht11gpiono)) {
                udelay(10);
                cnt++;
                if (cnt > DHT11_WAIT_TIMEOUT_US)
                    goto out_timeout;
            }

            if (cnt > 5)
                pdata[i] |= (0x1 << n);
        }
    }

    gpio_direction_output(dht11gpiono, 1);
    return 0;

out_timeout:
    /* 无论怎么退出，都要把总线交还成输出高电平，否则下次读不到应答 */
    gpio_direction_output(dht11gpiono, 1);
    return -1;
}

static int master_check_dht11_data(unsigned char *pdata, int len)
{
    if (pdata[4] == (unsigned char)(pdata[0] + pdata[1] + pdata[2] + pdata[3]))
        return 0;
    return -2;
}

static ssize_t dht11_read(struct file *fp, char __user *puser, size_t n, loff_t *off)
{
    int ret = 0;
    unsigned char data[5] = {0};
    unsigned long nret = 0;
    unsigned long flags = 0;

    master_send_dht11_reset();

    spin_lock_irqsave(&lock, flags);

    ret = master_recv_dht11_respone();
    if (ret) {
        spin_unlock_irqrestore(&lock, flags);     /* ← 原版这里漏了解锁 */
        gpio_direction_output(dht11gpiono, 1);
        return -1;
    }

    ret = master_recv_dht11_data(data, 5);
    spin_unlock_irqrestore(&lock, flags);         /* 先解锁，后面不再碰 GPIO */

    if (ret) {
        gpio_direction_output(dht11gpiono, 1);
        return -1;
    }

    ret = master_check_dht11_data(data, 5);
    if (ret) {
        gpio_direction_output(dht11gpiono, 1);
        return ret;
    }

    nret = copy_to_user(puser, data, sizeof(unsigned char) * 4);
    if (nret) {
        pr_info("copy_to_user failed\n");
        return -3;
    }

    return (sizeof(unsigned char) * 4);
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dht11_read,
};

static struct miscdevice dht11_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "dht11_misc",
    .fops  = &fops,
};

static int dht11_probe(struct platform_device *pdevice)
{
    int ret = 0;

    dht11gpiono = of_get_named_gpio(pdevice->dev.of_node, "gpio-dht11", 0);
    if (dht11gpiono < 0) {
        pr_info("of_get_named_gpio failed\n");
        return -1;
    }

    spin_lock_init(&lock);

    ret = misc_register(&dht11_misc);
    if (ret) {
        pr_info("misc_register failed\n");
        return -1;
    }

    ret = devm_gpio_request(dht11_misc.this_device, dht11gpiono, "dht11-platform-drv");
    if (ret) {
        pr_info("devm_gpio_request failed\n");
        return -1;
    }

    gpio_direction_output(dht11gpiono, 1);

    pr_info("dht11 platform driver probed, gpio = %d\n", dht11gpiono);
    return 0;
}

static int dht11_remove(struct platform_device *pdevice)
{
    int ret = 0;

    ret = misc_deregister(&dht11_misc);
    if (ret) {
        pr_info("misc_deregister failed\n");
        return -1;
    }

    return 0;
}

static struct platform_device_id dht11_id_tables[] = {
    {.name = "putedht11"},
    {},
};

static struct of_device_id dht11_of_match_tables[] = {
    {.compatible = "pute,putedht11"},
    {},
};

struct platform_driver dht11_drv = {
    .probe  = dht11_probe,
    .remove = dht11_remove,
    .driver = {
        .name           = "putedht11",
        .of_match_table = dht11_of_match_tables,
    },
    .id_table = dht11_id_tables,
};

module_platform_driver(dht11_drv);

MODULE_LICENSE("GPL");
