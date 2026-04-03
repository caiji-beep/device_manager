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

};
static int icm20608_spi_probe(struct spi_device *spi)
{
    printk("icm20608 spi probe success!\n");
    return 0;
}

static int icm20608_spi_remove(struct spi_device *spi)
{
    printk("icm20608 spi remove success!\n");
    return 0;
}


/*传统匹配*/
static const struct spi_device_id icm20608_spi_ids[] = {
    {"icm20608", 0},
    {}};

MODULE_DEVICE_TABLE(spi, icm20608_spi_ids);

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
    .id_table = icm20608_spi_ids,
};

MODULE_DEVICE_TABLE(of, icm20608_of_match);

module_spi_driver(icm20608_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lsz");
MODULE_DESCRIPTION("icm20608 SPI sensor driver");






