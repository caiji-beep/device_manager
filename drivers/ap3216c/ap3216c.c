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
#include <linux/i2c.h>
#include <linux/delay.h>


#include "ap3216creg.h"


#define AP3216C_NAME "ap3216c"
#define AP3216C_CNT 1



struct ap3216c_dev {
    struct miscdevice mdev;
    struct i2c_client *client;
    struct mutex lock;
    unsigned short ir, als, ps; /* 三个光传感器数据 */
};

/*从 ap3216c 读取多个寄存器数据*/
static int ap3216c_read_regs(struct ap3216c_dev *dev, u8 reg, void *val, int len)
{

    int ret = 0;
    struct i2c_msg msg[2];
    struct i2c_client *client = dev->client;
    /* msg[0]为发送要读取的首地址 */
    msg[0].addr = client->addr; /* ap3216c 地址 */
    msg[0].flags = 0;           /* 写标志 */
    msg[0].buf = &reg;          /* 读取的首地址 */
    msg[0].len = 1;             /* reg 长度 */
    /* msg[1]读取数据 */
    msg[1].addr = client->addr; /* ap3216c 地址 */
    msg[1].flags = I2C_M_RD;    /* 标记为读取数据 */
    msg[1].buf = val;           /* 读取数据缓冲区 */
    msg[1].len = len;           /* 要读取的数据长度 */

    ret = i2c_transfer(client->adapter, msg, 2);
    if (ret == 2)
    {
        ret = 0;
    }
    else
    {
        printk("i2c rd failed=%d reg=%06x len=%d\n", ret, reg, len);
        ret = -EREMOTEIO;
    }
    return ret;
}

/*向 ap3216c 多个寄存器写入数据*/
static s32 ap3216c_write_regs(struct ap3216c_dev *dev, u8 reg, u8 *buf, u8 len)
{
    u8 b[256];
    struct i2c_msg msg;
    struct i2c_client *client = dev->client;

    b[0] = reg;              /* 寄存器首地址 */
    memcpy(&b[1], buf, len); /* 将要写入的数据拷贝到数组 b 里面 */

    msg.addr = client->addr; /* ap3216c 地址 */
    msg.flags = 0;           /* 写标志 */
    msg.buf = b;          /* 读取的首地址 */
    msg.len = len + 1;             /* reg 长度 */
    return i2c_transfer(client->adapter, &msg, 1);
}

/*读取 ap3216c 指定寄存器值,读取一个寄存器*/
static unsigned char ap3216c_read_reg(struct ap3216c_dev *dev, u8 reg)
{
    unsigned char data;
    ap3216c_read_regs(dev, reg, &data, 1);
    return data;
}

/*向 ap3216c 指定寄存器写入指定的值,写一个寄存器*/
static void ap3216c_write_reg(struct ap3216c_dev *dev, u8 reg, u8 data)
{

    ap3216c_write_regs(dev, reg, &data, 1);
}

/*读取 AP3216C 的数据,读取原始数据,包括 ALS,PS 和 IR,同时打开 ALS,IR+PS 的话两次数据读取的间隔要大于 112.5ms*/
void ap3216c_readdata(struct ap3216c_dev *dev)
{
    unsigned char buf[6];
    //unsigned char i = 0;

    /*循环读取6个寄存器*/
    // for(i = 0; i < 6; i++)
    // {
    //     buf[i] = ap3216c_read_reg(dev,AP3216C_IRDATALOW + i);
    // }
    ap3216c_read_regs(dev, AP3216C_IRDATALOW, buf, 6);

    if(buf[0] & 0x80) /* IR_OF位为1,则数据无效 			*/
        dev->ir = 0;
    else /* 读取IR传感器的数据    */
        dev->ir = ((unsigned short)(buf[1] << 2)) | (buf[0] & 0X03);
    
    dev->als = ((unsigned short)buf[3] << 8) | buf[2]; /* 读取ALS传感器的数据 			 */

    if(buf[4] & 0x40) /* IR_OF位为1,则数据无效 			*/
        dev->ps = 0;
    else /* 读取PS传感器的数据    */
        dev->ps = (((unsigned short)buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);

}



static int ap3216c_open(struct inode *inode, struct file *filp)
{
    unsigned char value = 0;
    // 通过 miscdevice 指针，利用 container_of 反向提取出我们自定义的 ap3216c_dev 结构体
    struct ap3216c_dev *dev = container_of(filp->private_data, struct ap3216c_dev,mdev);
    // 将提取出的结构体塞回私有数据，方便 read/write 函数直接使用
    filp->private_data = dev;
    

    ap3216c_write_reg(dev, AP3216C_SYSTEMCONG, 0x04); /* 复位 */
    msleep(50);                                       /* AP3216C 复位最少 10ms */
    ap3216c_write_reg(dev, AP3216C_SYSTEMCONG, 0X03); /* 使能 ALS 和 PS+IR */
    value = ap3216c_read_reg(dev, AP3216C_SYSTEMCONG);
    printk("ap3216c SYSTEMCONG=0x%02x\r\n", value);

    return nonseekable_open(inode, filp);
}

static ssize_t ap3216c_read(struct file *filp, char __user *buf,
                         size_t count, loff_t *ppos)
{

    short data[3];
    int err = 0;
    struct ap3216c_dev *dev = filp->private_data;
    //struct i2c_client *client = dev->client;

     /* 读取传感器数据 */
    mutex_lock(&dev->lock);
    // 读取传感器数据
    ap3216c_readdata(dev);
    data[0] = dev->ir;
    data[1] = dev->als;
    data[2] = dev->ps;
    mutex_unlock(&dev->lock);
    printk("ap3216c_read: IR=%d, ALS=%d, PS=%d\n", data[0], data[1], data[2]);
     /* 将数据复制到用户空间 */
    err = copy_to_user(buf, data, sizeof(data));
     /* 错误处理 */
    if (err == 0)
    {
        return sizeof(data); // 成功！告诉 APP 我读到了 6 个字节
    }
    else
    {
        printk("copy_to_user failed, err=%d\n", err);
        return -EFAULT; // 返回错误码
    }

    

    return err;//0 = 全部拷贝成功
}
static int ap3216c_release(struct inode *inode, struct file *filp)
{

    return 0;
}



static const struct file_operations ap3216c_fops = {
    .owner   = THIS_MODULE,
    .open    = ap3216c_open,
    .read    = ap3216c_read,
    .release = ap3216c_release,
    .llseek  = no_llseek,
};

static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct ap3216c_dev *dev; // 使用局部指针
    printk("ID Name: %s\n", id->name);

    //动态分配内存
    dev = devm_kzalloc(&client->dev,sizeof(*dev),GFP_KERNEL);
    if (!dev) {
        dev_err(&client->dev, "Failed to allocate memory for ap3216c_dev\n");
        return -ENOMEM;
    }

    // 初始化结构体成员
    dev->client = client;
    mutex_init(&dev->lock);

    //将申请的结构体存入client的私有数据中，以便在remove函数中使用
    i2c_set_clientdata(client, dev);

    //注册Misc设备
    dev->mdev.minor = MISC_DYNAMIC_MINOR;
    dev->mdev.name = AP3216C_NAME;
    dev->mdev.fops = &ap3216c_fops;
    dev->mdev.parent = &client->dev;

    ret = misc_register(&dev->mdev);
    if (ret) {
        dev_err(&client->dev, "Failed to register misc device\n");
        return ret;
    }

    dev_info(&client->dev, "registered /dev/%s successfully\n", AP3216C_NAME);
    return 0;
}

static int ap3216c_remove(struct i2c_client *client)
{
    //提取在probe函数中保存的私有数据
    struct ap3216c_dev *dev = i2c_get_clientdata(client);
    
    if(!dev)
        return 0;
    
    //注销Misc设备
    misc_deregister(&dev->mdev);

    dev_info(&client->dev, "unregistered /dev/%s successfully\n", AP3216C_NAME);
    return 0;
}

/* 传统 I2C 匹配表 */
static const struct i2c_device_id ap3216c_id[] = {
    { "ap3216c", 0 },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, ap3216c_id);

/* 设备树匹配表 */
static const struct of_device_id ap3216c_of_match[] = {
    { .compatible = "lsz,ap3216c" },
    { }
};
MODULE_DEVICE_TABLE(of, ap3216c_of_match);

static struct i2c_driver ap3216c_driver = {
    .probe    = ap3216c_probe,
    .remove   = ap3216c_remove,
    .id_table = ap3216c_id,
    .driver = {
        .name           = "ap3216c",
        .of_match_table = ap3216c_of_match,
    },
};

module_i2c_driver(ap3216c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lsz");
MODULE_DESCRIPTION("AP3216C I2C sensor driver");

