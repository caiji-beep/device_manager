#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "icm20608reg.h"

#define ICM20608_NAME "icm20608"
#define ICM20608_CNT 1

struct icm20608_dev
{
    struct miscdevice mdev;
    struct mutex lock;
    struct spi_device *spi;

    signed short accel_x_adc; /* 加速度计 X 轴原始值 */
    signed short accel_y_adc; /* 加速度计 Y 轴原始值 */
    signed short accel_z_adc; /* 加速度计 Z 轴原始值 */
    signed short temp_adc;    /* 温度原始值 */
    signed short gyro_x_adc;  /* 陀螺仪 X 轴原始值 */
    signed short gyro_y_adc;  /* 陀螺仪 Y 轴原始值 */
    signed short gyro_z_adc;  /* 陀螺仪 Z 轴原始值 */
};

static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{
    u8 data = 0;
    data = reg | 0x80; // 读置位
    spi_write_then_read(dev->spi, &data, 1, buf, len);
    return 0;
}

static int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, u8 len)
{
    unsigned char *txdata;
    txdata = kzalloc(len + 1, GFP_KERNEL);
    if (!txdata)
        return -ENOMEM;
    txdata[0] = reg & ~0x80; // 写清零
    memcpy(txdata + 1, buf, len);
    spi_write(dev->spi, txdata, len + 1);
    kfree(txdata);
    return 0;
}

static unsigned char icm20608_read_reg(struct icm20608_dev *dev, u8 reg)
{
    unsigned char data;
    icm20608_read_regs(dev, reg, &data, 1);
    return data;
}

static void icm20608_write_reg(struct icm20608_dev *dev, u8 reg, u8 data)
{
    icm20608_write_regs(dev, reg, &data, 1);
}

static void icm20608_readdata(struct icm20608_dev *dev)
{
    unsigned char data[14] = {0};
    icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);
    dev->accel_x_adc = (signed short)(data[0] << 8 | data[1]);
    dev->accel_y_adc = (signed short)(data[2] << 8 | data[3]);
    dev->accel_z_adc = (signed short)(data[4] << 8 | data[5]);
    dev->temp_adc = (signed short)(data[6] << 8 | data[7]);
    dev->gyro_x_adc = (signed short)(data[8] << 8 | data[9]);
    dev->gyro_y_adc = (signed short)(data[10] << 8 | data[11]);
    dev->gyro_z_adc = (signed short)(data[12] << 8 | data[13]);
}

/*icm20608初始化*/
static void icm20608reg_init(struct icm20608_dev *dev)
{
    unsigned char value = 0;
    /*复位设备*/
    icm20608_write_reg(dev, ICM20_PWR_MGMT_1, 0x80);
    mdelay(100);
    /*唤醒设备，自动选择时钟*/
    icm20608_write_reg(dev, ICM20_PWR_MGMT_1, 0x01);
    mdelay(100);
    value = icm20608_read_reg(dev, ICM20_WHO_AM_I); // 读取who am i寄存器，确认器件存在
    printk("ICM20608 ID = %#X\r\n", value);
    value = icm20608_read_reg(dev, ICM20_PWR_MGMT_1);
    printk("ICM20608 ICM20_PWR_MGMT_1 = %#X\r\n", value);

    icm20608_write_reg(dev, ICM20_SMPLRT_DIV, 0x00);    /* 输出速率是内部采样率					*/
    icm20608_write_reg(dev, ICM20_GYRO_CONFIG, 0x18);   /* 陀螺仪±2000dps量程 				*/
    icm20608_write_reg(dev, ICM20_ACCEL_CONFIG, 0x18);  /* 加速度计±16G量程 					*/
    icm20608_write_reg(dev, ICM20_CONFIG, 0x04);        /* 陀螺仪低通滤波BW=20Hz 				*/
    icm20608_write_reg(dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
    icm20608_write_reg(dev, ICM20_PWR_MGMT_2, 0x00);    /* 打开加速度计和陀螺仪所有轴 				*/
    icm20608_write_reg(dev, ICM20_LP_MODE_CFG, 0x00);   /* 关闭低功耗 						*/
    icm20608_write_reg(dev, ICM20_FIFO_EN, 0x00);       /* 关闭FIFO						*/
}

static int icm20608_open(struct inode *inode, struct file *filp)
{
    // 通过 miscdevice 指针，利用 container_of 反向提取出我们自定义的 icm20608_dev 结构体
    struct icm20608_dev *dev = container_of(filp->private_data, struct icm20608_dev, mdev);
    // 将提取出的结构体塞回私有数据，方便 read/write 函数直接使用
    filp->private_data = dev;

    return nonseekable_open(inode, filp); // 禁止文件指针移动
}
static int icm20608_release(struct inode *inode, struct file *filp)
{
    return 0;
}
static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int err = 0;
    signed short data[7] = {0}; // 7个数据：加速度计X/Y/Z，温度，陀螺仪X/Y/Z
    struct icm20608_dev *dev = filp->private_data;

    mutex_lock(&dev->lock);

    icm20608_readdata(dev);

    data[0] = dev->gyro_x_adc;
    data[1] = dev->gyro_y_adc;
    data[2] = dev->gyro_z_adc;
    data[3] = dev->accel_x_adc;
    data[4] = dev->accel_y_adc;
    data[5] = dev->accel_z_adc;
    data[6] = dev->temp_adc;

    mutex_unlock(&dev->lock);


    err = copy_to_user(buf, data, sizeof(data));
    if (err == 0)
    {
        return sizeof(data);
    }
    else
    {
        return -EFAULT;
    }
}

static const struct file_operations icm20608_fops = {
    .owner = THIS_MODULE,
    .open = icm20608_open,
    .read = icm20608_read,
    .release = icm20608_release,
};

static int icm20608_spi_probe(struct spi_device *spi)
{
    int ret = 0;
    struct icm20608_dev *dev;
    // printk("icm20608 spi probe success!\n");
    dev = devm_kzalloc(&spi->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
    {
        dev_err(&spi->dev, "Failed to allocate memory for icm20608_dev\n");
        return -ENOMEM;
    }

    // 初始化结构体成员
    dev->spi = spi;
    mutex_init(&dev->lock);

    // 将申请的结构体存入spi的私有数据中，以便在remove函数中使用
    spi_set_drvdata(spi, dev);

    // 注册misc设备
    dev->mdev.minor = MISC_DYNAMIC_MINOR;
    dev->mdev.name = ICM20608_NAME;
    dev->mdev.fops = &icm20608_fops;
    dev->mdev.parent = &spi->dev;

    ret = misc_register(&dev->mdev);
    if (ret)
    {
        dev_err(&spi->dev, "Failed to register misc device\n");
        return ret;
    }

    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    // 初始化icm20608
    icm20608reg_init(dev);

    dev_info(&spi->dev, "registered /dev/%s successfully\n", ICM20608_NAME);

    return 0;
}

static int icm20608_spi_remove(struct spi_device *spi)
{
    struct icm20608_dev *dev = spi_get_drvdata(spi); /* 从 spi 中取出 probe 时保存的结构体指针 */

    if (!dev)
        return 0;

    misc_deregister(&dev->mdev); /* 注销 misc 设备 */
    dev_info(&spi->dev, "unregistered /dev/%s successfully\n", ICM20608_NAME);

    return 0;
}

// /*传统匹配*/
// static const struct spi_device_id icm20608_spi_ids[] = {
//     {"icm20608", 0},
//     {}};

// MODULE_DEVICE_TABLE(spi, icm20608_spi_ids);

/*设备树匹配*/
static const struct of_device_id icm20608_of_match[] = {
    {.compatible = "lsz,icm20608"},
    {},
};
/*spi_driver*/
static struct spi_driver icm20608_spi_driver = {
    .driver = {
        .name = "icm20608",
        .owner = THIS_MODULE,
        .of_match_table = icm20608_of_match,
    },
    .probe = icm20608_spi_probe,
    .remove = icm20608_spi_remove,
    //.id_table = icm20608_spi_ids,
};

MODULE_DEVICE_TABLE(of, icm20608_of_match);

module_spi_driver(icm20608_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lsz");
MODULE_DESCRIPTION("icm20608 SPI sensor driver");
