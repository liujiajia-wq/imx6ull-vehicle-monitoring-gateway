#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>

extern void msleep(unsigned int msecs);

/* ========== MPU6050 寄存器 ========== */
#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_WHO_AM_I        0x75

#define MPU6050_ACCEL_FS_4G         0x08
#define MPU6050_GYRO_FS_500         0x08
#define MPU6050_CHIP_ID             0x68

/* ========== 全局数据 ========== */
static struct i2c_client *mpu6050_client = NULL;
static DEFINE_MUTEX(mpu6050_lock);   /* I2C 读需要睡眠，用 mutex 而非 spinlock */

/* ========== I2C 寄存器读写（用子系统 API） ========== */
static int mpu6050_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
    return i2c_smbus_write_byte_data(client, reg, val);
}

static int mpu6050_read_regs(struct i2c_client *client, u8 reg, u8 *pdata, int len)
{
    return i2c_smbus_read_i2c_block_data(client, reg, len, pdata);
}

/* ========== 芯片初始化 ========== */
static int mpu6050_chip_init(struct i2c_client *client)
{
    int ret = 0;

    /* 复位 + 唤醒，选 PLL 时钟 */
    mpu6050_write_reg(client, MPU6050_REG_PWR_MGMT_1, 0x80);
    msleep(100);
    mpu6050_write_reg(client, MPU6050_REG_PWR_MGMT_1, 0x01);
    msleep(10);

    /* 校验 WHO_AM_I */
    ret = i2c_smbus_read_byte_data(client, MPU6050_REG_WHO_AM_I);
    if (ret < 0) {
        pr_info("read who_am_i failed: %d\n", ret);
        return -1;
    }
    if (ret != MPU6050_CHIP_ID) {
        pr_info("unexpected chip id 0x%02x\n", ret);
        return -1;
    }
    pr_info("mpu6050 detected, id = 0x%02x\n", ret);

    /* 采样率 100Hz */
    mpu6050_write_reg(client, MPU6050_REG_SMPLRT_DIV, 0x09);
    /* DLPF 约 44Hz */
    mpu6050_write_reg(client, MPU6050_REG_CONFIG, 0x03);
    /* 陀螺 ±500°/s，加速度 ±4g */
    mpu6050_write_reg(client, MPU6050_REG_GYRO_CONFIG, MPU6050_GYRO_FS_500);
    mpu6050_write_reg(client, MPU6050_REG_ACCEL_CONFIG, MPU6050_ACCEL_FS_4G);

    msleep(50);
    return 0;
}

/* ========== 读取一次完整数据（14 字节原始值） ========== */
static int mpu6050_read_once(struct i2c_client *client, unsigned char *pdata)
{
    return mpu6050_read_regs(client, MPU6050_REG_ACCEL_XOUT_H, pdata, 14);
}

/* ========== 文件操作 ========== */
static ssize_t mpu6050_read(struct file *fp, char __user *puser, size_t n, loff_t *off)
{
    int ret = 0;
    unsigned char data[14] = {0};
    unsigned long nret = 0;

    if (!mpu6050_client)
        return -ENODEV;

    /* I2C 传输会睡眠，必须用 mutex 保护，不能关中断 */
    if (mutex_lock_interruptible(&mpu6050_lock))
        return -ERESTARTSYS;

    ret = mpu6050_read_once(mpu6050_client, data);
    mutex_unlock(&mpu6050_lock);

    if (ret < 0) {
        pr_info("mpu6050 read failed: %d\n", ret);
        return -EIO;
    }

    nret = copy_to_user(puser, data, sizeof(unsigned char) * 14);
    if (nret) {
        pr_info("copy_to_user failed\n");
        return -EFAULT;
    }

    return (sizeof(unsigned char) * 14);
}

static int mpu6050_open(struct inode *inode, struct file *fp)
{
    return 0;
}

static int mpu6050_release(struct inode *inode, struct file *fp)
{
    return 0;
}

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = mpu6050_open,
    .release = mpu6050_release,
    .read    = mpu6050_read,
};

static struct miscdevice mpu6050_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "mpu6050_misc",
    .fops  = &fops,
};

/* ========== probe / remove ========== */
static int mpu6050_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    int ret = 0;

    pr_info("mpu6050 probe, addr = 0x%02x\n", client->addr);

    /* 检查适配器是否支持 SMBus 块读 */
    if (!i2c_check_functionality(client->adapter,
                                 I2C_FUNC_SMBUS_BYTE_DATA |
                                 I2C_FUNC_SMBUS_I2C_BLOCK)) {
        pr_info("i2c adapter not support needed functionality\n");
        return -EOPNOTSUPP;
    }

    ret = mpu6050_chip_init(client);
    if (ret) {
        pr_info("mpu6050_chip_init failed\n");
        return -ENODEV;
    }

    mpu6050_client = client;

    ret = misc_register(&mpu6050_misc);
    if (ret) {
        pr_info("misc_register failed\n");
        mpu6050_client = NULL;
        return -1;
    }

    i2c_set_clientdata(client, &mpu6050_misc);
    return 0;
}

static int mpu6050_remove(struct i2c_client *client)
{
    misc_deregister(&mpu6050_misc);
    mpu6050_client = NULL;
    return 0;
}

static const struct i2c_device_id mpu6050_id_tables[] = {
    { "putempu6050", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_id_tables);

static const struct of_device_id mpu6050_of_match_tables[] = {
    { .compatible = "pute,putempu6050" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu6050_of_match_tables);

static struct i2c_driver mpu6050_drv = {
    .probe    = mpu6050_probe,
    .remove   = mpu6050_remove,
    .id_table = mpu6050_id_tables,
    .driver = {
        .name           = "putempu6050",
        .of_match_table = mpu6050_of_match_tables,
    },
};

module_i2c_driver(mpu6050_drv);

MODULE_LICENSE("GPL");