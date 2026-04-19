#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/atomic.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/input.h>

#define KEYINPUT_CNT 1
#define KEYINPUT_NAME "keyinput"

#define KEYNUM 1
#define KEY0VALUE 0x01
#define INVAKEY 0xff

struct irq_keydesc
{
    int gpio;
    int irqnum;          /*中断号*/
    unsigned char value; /**/
    char name[10];
    irqreturn_t (*handler)(int, void *); /*中断处理函数*/
};

/*设备结构体*/
struct keyinput_dev
{
    struct device_node *nd; /*节点*/
    struct irq_keydesc irqkey[KEYNUM];
    struct timer_list timer;

    struct input_dev *inputdev; /*输入设备*/
};
struct keyinput_dev keyinputdev;

/*定时器回调*/
static void timer_func(unsigned long arg)
{
    int value = 0;

    struct keyinput_dev *dev = (struct keyinput_dev *)arg;
    value = gpio_get_value(dev->irqkey[0].gpio);
    if (value == 0) /*按下*/
    {
        /*上报按键值*/
        input_event(dev->inputdev, EV_KEY, KEY_0, 1);
        input_sync(dev->inputdev);
    }
    else if (value == 1)
    {
        /*上报按键值*/
        input_event(dev->inputdev, EV_KEY, KEY_0, 0);
        input_sync(dev->inputdev);//同步
    }
}

static irqreturn_t key0_handler(int irq, void *dev_id)
{

    struct keyinput_dev *dev = (struct keyinput_dev *)dev_id;

    dev->timer.data = (volatile unsigned long)dev_id;
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10)); /**/
    return IRQ_HANDLED;
}

/*按键初始化*/
static int keyio_init(struct keyinput_dev *dev)
{
    int ret = 0, i = 0;
    /*获取设备节点*/
    dev->nd = of_find_node_by_path("/key");
    if (dev->nd == NULL)
    {
        ret = -EINVAL;
        goto fail_findnd;
    }
    /*获取key对应的gpio*/
    for (i = 0; i < KEYNUM; i++)
    {
        dev->irqkey[i].gpio = of_get_named_gpio(dev->nd, "key-gpios", i);
        printk("key_gpio num = %d\r\n", dev->irqkey[i].gpio);
    }
    for (i = 0; i < KEYNUM; i++)
    {
        /*申请gpio*/
        memset(dev->irqkey[i].name, 0, sizeof(dev->irqkey[i].name));
        sprintf(dev->irqkey[i].name, "KEY%d", i);
        ret = gpio_request(dev->irqkey[i].gpio, dev->irqkey[i].name);
        ret = gpio_direction_input(dev->irqkey[i].gpio); /*使用io,设置为输入*/
        /*获取中断号*/
        dev->irqkey[i].irqnum = gpio_to_irq(dev->irqkey[i].gpio);
#if 0
        dev->irqkey[i].irqnum = irq_of_parse_and_map(dev->nd,i);
#endif
    }
    /*按键中断初始化*/
    dev->irqkey[0].handler = key0_handler;
    dev->irqkey[0].value = KEY_0;
    for (i = 0; i < KEYNUM; i++)
    {
        ret = request_irq(dev->irqkey[i].irqnum, dev->irqkey[0].handler,
                          IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, dev->irqkey[i].name, &keyinputdev);
        if (ret)
        {
            printk("irq %d request fail\r\n", dev->irqkey[i].irqnum);
            goto fail_irq;
        }
    }

    /*初始化定时器*/
    init_timer(&keyinputdev.timer);
    keyinputdev.timer.function = timer_func;
    return 0;

fail_irq:
    for (i = 0; i < KEYNUM; i++)
        gpio_free(dev->irqkey[i].gpio); /*释放io*/
fail_findnd:
    return ret;
}

static int __init keyinput_init(void)
{
    int ret = 0;

    ret = keyio_init(&keyinputdev);
    if (ret < 0)
    {
        goto fail_keyinit;
    }

    /*申请 input_dev*/
    keyinputdev.inputdev = input_allocate_device();
    if (keyinputdev.inputdev == NULL)
    {
        ret = -EINVAL;
        goto fail_keyinit;
    }
    keyinputdev.inputdev->name = KEYINPUT_NAME;
    /* 初始化 input_dev,设置产生哪些事件 */
    __set_bit(EV_KEY, keyinputdev.inputdev->evbit); /*按键事件 */
    __set_bit(EV_REP, keyinputdev.inputdev->evbit); /* 重复事件 */
    __set_bit(KEY_0, keyinputdev.inputdev->keybit);    /* 初始化 input_dev,设置产生哪些按键 */
    ret = input_register_device(keyinputdev.inputdev);/* 注册输入设备 */
    if (ret)
    {
        ret = -EINVAL;
        printk("register input device failed!\r\n");
        goto fail_input_register;
    }

    return 0;
fail_input_register:
    input_free_device(keyinputdev.inputdev);/*释放input_dev*/
fail_keyinit:
    return ret;
}
static void __exit keyinput_exit(void)
{
    int i = 0;
    del_timer_sync(&keyinputdev.timer); /*删除定时器*/
    for (i = 0; i < KEYNUM; i++)
        free_irq(keyinputdev.irqkey[i].irqnum, &keyinputdev); /*释放中断*/
    for (i = 0; i < KEYNUM; i++)
        gpio_free(keyinputdev.irqkey[i].gpio); /*释放io*/

    /*注销input_dev*/
    input_unregister_device(keyinputdev.inputdev);
    input_free_device(keyinputdev.inputdev);
}

module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lsz");