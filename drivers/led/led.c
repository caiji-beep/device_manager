#include <linux/kernel.h>      /* 包含内核基础宏和函数，如 printk, ARRAY_SIZE 等 */
#include <linux/module.h>      /* 模块化开发必备，提供 MODULE_LICENSE 等宏 */
#include <linux/fs.h>          /* 提供 file_operations, inode, file 等 VFS 数据结构 */
#include <linux/of.h>          /* 提供设备树 (Device Tree) 解析和匹配的相关接口 */
#include <linux/err.h>         /* 提供错误指针处理宏：IS_ERR, PTR_ERR 等 */
#include <linux/uaccess.h>     /* 提供内核与用户空间的数据拷贝：copy_to_user/from_user */
#include <linux/mutex.h>       /* 提供互斥锁机制，用于多线程并发保护 */
#include <linux/platform_device.h> /* 提供平台总线 (Platform Bus) 设备与驱动的结构体 */
#include <linux/miscdevice.h>  /* 提供杂项设备 (Misc Device) 注册框架 */
#include <linux/gpio/consumer.h> /* 现代 GPIO 描述符 (gpiod) 子系统核心头文件 */

/* 注册在 /dev 目录下的设备节点名称，即最终生成 /dev/led */
#define LED_NAME    "led"

/* 定义面向应用层的逻辑状态常量 */
#define LED_ON      1
#define LED_OFF     0

/* * 自定义设备结构体：将与该 LED 相关的资源“高内聚”封装在一起
 * 体现了 Linux 内核面向对象的设计思想
 */
struct led_dev {
    struct miscdevice mdev;        /* 杂项设备结构体，内嵌于此以便管理 */
    struct gpio_desc *gpiod;       /* 现代 GPIO 描述符指针，代表具体引脚 */
    struct mutex lock;             /* 互斥锁，防止多个应用同时操作引起硬件状态冲突 */
};

/*----------------------------------------------------------
 * 硬件操作层：设置 LED 状态
 * 参数 state: 期望的逻辑状态 (LED_ON 或 LED_OFF)
 *---------------------------------------------------------*/
static int led_set_state(struct led_dev *dev, u8 state)
{
    /* 严格校验参数合法性 */
    if (state != LED_ON && state != LED_OFF)
        return -EINVAL; /* 返回 Invalid Argument 错误码 */

    /* * gpiod_set_value_cansleep：设置 GPIO 逻辑电平。
     * 传入的 1/0 是“逻辑值”。如果设备树配置了 active_low，底层会自动反转为物理低电平。
     * 使用 cansleep 版本是因为 GPIO 控制器可能挂在 I2C/SPI 等会引起休眠的总线上。
     */
    gpiod_set_value_cansleep(dev->gpiod, state ? 1 : 0);
    return 0;
}

/*----------------------------------------------------------
 * 硬件操作层：获取 LED 当前状态
 *---------------------------------------------------------*/
static int led_get_state(struct led_dev *dev)
{
    int ret;

    /* 获取逻辑电平 */
    ret = gpiod_get_value_cansleep(dev->gpiod);
    if (ret < 0)
        return ret; /* 如果底层读取失败，直接向上透传错误码 */

    /* 将底层的 1/0 转换为我们定义的 LED_ON / LED_OFF */
    return ret ? LED_ON : LED_OFF;
}

/*----------------------------------------------------------
 * VFS 层：打开设备 (对应应用层的 open("/dev/led"))
 *---------------------------------------------------------*/
static int led_open(struct inode *inode, struct file *filp)
{
    struct led_dev *dev;

    /* * 神仙宏 container_of：
     * misc 框架在调用 open 时，会把内部的 mdev 指针藏在 filp->private_data 中。
     * 这里通过 mdev 的地址，反推出外层包裹它的自定义结构体 led_dev 的首地址。
     */
    dev = container_of(filp->private_data, struct led_dev, mdev);
    
    /* 将反推出来的顶层结构体指针存入 private_data，供后续 read/write 使用 */
    filp->private_data = dev;

    /* 明确宣告此设备不支持 lseek 操作 (改变文件读写指针对 LED 毫无意义) */
    return nonseekable_open(inode, filp);
}

/*----------------------------------------------------------
 * VFS 层：读取状态 (对应应用层的 read)
 *---------------------------------------------------------*/
static ssize_t led_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct led_dev *dev = filp->private_data; /* 提取 open 阶段存好的指针 */
    u8 value;
    int ret;

    /* 请求读取的字节数不能为 0 */
    if (count == 0)
        return 0;

    /* 字符设备防死循环机制：如果读写偏移量不为 0，说明已经读过一次，返回 0 表示文件结束 (EOF) */
    if (*ppos != 0)
        return 0;

    mutex_lock(&dev->lock);         /* 上锁保护硬件读取过程 */
    ret = led_get_state(dev);
    mutex_unlock(&dev->lock);       /* 解锁 */
    
    if (ret < 0)
        return ret;                 /* 读取失败则退出 */

    value = (u8)ret;

    /* 将内核空间得到的数据 (1字节) 拷贝给用户空间的 buf */
    if (copy_to_user(buf, &value, 1))
        return -EFAULT;             /* 如果指针异常导致拷贝失败，返回 Bad Address 错误 */

    *ppos += 1;                     /* 更新文件内部偏移量 */
    return 1;                       /* 返回成功读取到的字节数：1 字节 */
}

/*----------------------------------------------------------
 * VFS 层：写入控制指令 (对应应用层的 write)
 *---------------------------------------------------------*/
static ssize_t led_write(struct file *filp, const char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct led_dev *dev = filp->private_data;
    u8 value;
    int ret;

    /* 至少要写入 1 个字节的数据 */
    if (count < 1)
        return -EINVAL;

    /* 从用户空间拷贝 1 字节控制指令到内核变量 value 中 */
    if (copy_from_user(&value, buf, 1))
        return -EFAULT;

    /* 处理控制指令 */
    switch (value) {
    case LED_ON:
    case LED_OFF:
        /* 利用 switch-case 穿透特性，无论 ON 还是 OFF 都执行相同的底层调用流程 */
        mutex_lock(&dev->lock);
        ret = led_set_state(dev, value);
        mutex_unlock(&dev->lock);
        
        if (ret)
            return ret;             /* 如果底层设置出错，透传错误码 */
        return 1;                   /* 成功写入 1 字节 */
    default:
        return -EINVAL;             /* 收到了非 0/1 的无效指令 */
    }
}

/*----------------------------------------------------------
 * 文件操作函数集合 (File Operations)
 *---------------------------------------------------------*/
static const struct file_operations led_fops = {
    .owner  = THIS_MODULE,
    .open   = led_open,
    .read   = led_read,
    .write  = led_write,
    .llseek = no_llseek,            /* 配合 nonseekable_open 使用，彻底禁用 lseek */
};

/*----------------------------------------------------------
 * 驱动核心：Probe (设备探测与初始化)
 * 当平台总线发现设备树里的 compatible 属性与驱动匹配时，调用此函数
 *---------------------------------------------------------*/
static int led_probe(struct platform_device *pdev)
{
    struct led_dev *led;
    int ret;

    /* 1. 分配受设备生命周期管理的内存 (设备拔出/驱动卸载时自动释放，防内存泄漏) */
    led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
    if (!led)
        return -ENOMEM;

    /* 2. 初始化并发保护锁 */
    mutex_init(&led->lock);

    /* * 3. 获取 GPIO 资源
     * "led" 会让框架去设备树中寻找名为 "led-gpios" 的属性。
     * GPIOD_OUT_LOW 意味着获取到 GPIO 的瞬间，就将其默认设置为逻辑低电平 (默认熄灭)。
     */
    led->gpiod = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
    
    /* 错误指针校验 */
    if (IS_ERR(led->gpiod)) {
        ret = PTR_ERR(led->gpiod); /* 将错误指针转换为整数错误码 */
        
        /* * 兼容旧版内核的延迟探测 (Deferred Probing) 处理：
         * 如果返回 -EPROBE_DEFER (意味着依赖的 GPIO 控制器还没准备好)，
         * 属于正常重试机制，不打印报错日志，保持 dmesg 整洁。
         */
        if (ret != -EPROBE_DEFER)
            dev_err(&pdev->dev, "failed to get led-gpios: %d\n", ret);
        return ret;
    }

    /* 4. 填充杂项设备 (Misc Device) 结构体 */
    led->mdev.minor  = MISC_DYNAMIC_MINOR;  /* 内核自动分配次设备号 */
    led->mdev.name   = LED_NAME;            /* /dev/ 节点名称 */
    led->mdev.fops   = &led_fops;           /* 绑定刚才写好的 VFS 操作函数 */
    led->mdev.parent = &pdev->dev;          /* 建立设备模型父子拓扑关系，在 /sys 中归类 */

    /* 将 led_dev 指针藏入平台设备中，方便 remove 函数取用清理 */
    platform_set_drvdata(pdev, led);

    /* 5. 正式向内核注册节点 */
    ret = misc_register(&led->mdev);
    if (ret) {
        dev_err(&pdev->dev, "failed to register misc device\n");
        return ret;
    }

    /* 打印 Probe 成功信息，方便调试 */
    dev_info(&pdev->dev, "registered /dev/%s\n", LED_NAME);
    return 0;
}

/*----------------------------------------------------------
 * 驱动核心：Remove (设备移除与清理)
 * 驱动被 rmmod 卸载，或者底层硬件被摘除时调用
 *---------------------------------------------------------*/
static int led_remove(struct platform_device *pdev)
{
    /* 取回 probe 时藏好的结构体指针 */
    struct led_dev *led = platform_get_drvdata(pdev);

    if (!led)
        return 0;

    /* 良好的习惯：卸载驱动前，确保硬件处于安全关闭状态 */
    mutex_lock(&led->lock);
    led_set_state(led, LED_OFF);
    mutex_unlock(&led->lock);

    /* 注销 /dev/led 节点 */
    misc_deregister(&led->mdev);

    /* * 注意：由于我们前面使用的是 devm_gpiod_get 和 devm_kzalloc，
     * 所以这里不需要手动 gpiod_put 和 kfree，内核设备模型会自动帮我们清理！
     */

    dev_info(&pdev->dev, "removed\n");
    return 0;
}

/*----------------------------------------------------------
 * 设备树匹配表 (OF Match Table)
 * 内核依靠这个表里的字符串，与设备树 (.dts) 节点进行"相亲"匹配
 *---------------------------------------------------------*/
static const struct of_device_id led_of_match[] = {
    { .compatible = "lsz,atkalpha-led" }, /* 核心暗号，必须与 dts 里的一字不差 */
    { }                                   /* 必须以空元素结尾，作为遍历结束符 */
};
MODULE_DEVICE_TABLE(of, led_of_match);    /* 导出表，支持 udev 等热插拔机制 */

/*----------------------------------------------------------
 * 平台驱动结构体 (Platform Driver)
 *---------------------------------------------------------*/
static struct platform_driver led_driver = {
    .probe  = led_probe,
    .remove = led_remove,
    .driver = {
        .name           = "led",          /* sysfs 中的驱动目录名 */
        .of_match_table = led_of_match,   /* 绑定上面的匹配表 */
    },
};

/* * 神仙宏 module_platform_driver：
 * 自动帮你生成 module_init() 和 module_exit() 的模板代码，
 * 并自动调用 platform_driver_register / unregister。极大简化代码。
 */
module_platform_driver(led_driver);

/* 模块基础信息描述 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lsz");
MODULE_DESCRIPTION("LED driver based on platform + miscdevice + gpiod");