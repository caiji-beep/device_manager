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

#define BEEP_NAME   "beep"

#define BEEP_ON     1
#define BEEP_OFF    0

struct beep_dev {
    struct miscdevice mdev;
    struct gpio_desc *gpiod;
    struct mutex lock;
};

static int beep_set_state(struct beep_dev *dev, u8 state)
{
    if (state != BEEP_ON && state != BEEP_OFF)
        return -EINVAL;

    gpiod_set_value_cansleep(dev->gpiod, state ? 1 : 0);
    return 0;
}

static int beep_get_state(struct beep_dev *dev)
{
    int ret;

    ret = gpiod_get_value_cansleep(dev->gpiod);
    if (ret < 0)
        return ret;

    return ret ? BEEP_ON : BEEP_OFF;
}

static int beep_open(struct inode *inode, struct file *filp)
{
    struct beep_dev *dev;

    dev = container_of(filp->private_data, struct beep_dev, mdev);
    filp->private_data = dev;

    return nonseekable_open(inode, filp);
}

static ssize_t beep_read(struct file *filp, char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct beep_dev *dev = filp->private_data;
    u8 value;
    int ret;

    if (count == 0)
        return 0;

    if (*ppos != 0)
        return 0;

    mutex_lock(&dev->lock);
    ret = beep_get_state(dev);
    mutex_unlock(&dev->lock);
    if (ret < 0)
        return ret;

    value = (u8)ret;

    if (copy_to_user(buf, &value, 1))
        return -EFAULT;

    *ppos += 1;
    return 1;
}

static ssize_t beep_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    struct beep_dev *dev = filp->private_data;
    u8 value;
    int ret;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(&value, buf, 1))
        return -EFAULT;

    switch (value) {
    case BEEP_ON:
    case BEEP_OFF:
        mutex_lock(&dev->lock);
        ret = beep_set_state(dev, value);
        mutex_unlock(&dev->lock);
        if (ret)
            return ret;
        return 1;
    default:
        return -EINVAL;
    }
}

static const struct file_operations beep_fops = {
    .owner  = THIS_MODULE,
    .open   = beep_open,
    .read   = beep_read,
    .write  = beep_write,
    .llseek = no_llseek,
};

static int beep_probe(struct platform_device *pdev)
{
    struct beep_dev *beep;
    int ret;

    beep = devm_kzalloc(&pdev->dev, sizeof(*beep), GFP_KERNEL);
    if (!beep)
        return -ENOMEM;

    mutex_init(&beep->lock);

    beep->gpiod = devm_gpiod_get(&pdev->dev, "beep", GPIOD_OUT_LOW);
    if (IS_ERR(beep->gpiod)) {
        ret = PTR_ERR(beep->gpiod);
        if (ret != -EPROBE_DEFER)
            dev_err(&pdev->dev, "failed to get beep-gpios: %d\n", ret);
        return ret;
    }

    beep->mdev.minor  = MISC_DYNAMIC_MINOR;
    beep->mdev.name   = BEEP_NAME;
    beep->mdev.fops   = &beep_fops;
    beep->mdev.parent = &pdev->dev;

    platform_set_drvdata(pdev, beep);

    ret = misc_register(&beep->mdev);
    if (ret) {
        dev_err(&pdev->dev, "failed to register misc device\n");
        return ret;
    }

    dev_info(&pdev->dev, "registered /dev/%s\n", BEEP_NAME);
    return 0;
}

static int beep_remove(struct platform_device *pdev)
{
    struct beep_dev *beep = platform_get_drvdata(pdev);

    if (!beep)
        return 0;

    mutex_lock(&beep->lock);
    beep_set_state(beep, BEEP_OFF);
    mutex_unlock(&beep->lock);

    misc_deregister(&beep->mdev);

    dev_info(&pdev->dev, "removed\n");
    return 0;
}

static const struct of_device_id beep_of_match[] = {
    { .compatible = "lsz,atkalpha-beep" },
    { }
};
MODULE_DEVICE_TABLE(of, beep_of_match);

static struct platform_driver beep_driver = {
    .probe  = beep_probe,
    .remove = beep_remove,
    .driver = {
        .name           = "beep",
        .of_match_table = beep_of_match,
    },
};

module_platform_driver(beep_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lsz");
MODULE_DESCRIPTION("BEEP driver based on platform + miscdevice + gpiod");