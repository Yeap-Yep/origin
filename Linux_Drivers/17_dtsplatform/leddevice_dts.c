#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/ide.h>
#include <linux/poll.h>
#include <linux/platform_device.h>

#define CCM_CCGR0_BASE              0X020C4068
#define SW_MUX_GPIO1_IO03_BASE      0X020E0068
#define SW_PAD_GPIO1_IO03_BASE      0X020E02F4
#define GPIO1_DR_BASE               0X0209C000
#define GPIO1_GDIR_BASE             0X0209C004

#define REGISTER_LENGTH             4   

void dev_release(struct device *dev)
{
    printk("呵呵哒\n");
}

static struct resource led_resources[]={
    [0]={
        .start = CCM_CCGR0_BASE,
        .end = (CCM_CCGR0_BASE + REGISTER_LENGTH - 1),
        .flags = IORESOURCE_MEM,
    },
    [1]={
        .start = SW_MUX_GPIO1_IO03_BASE,
        .end = (SW_MUX_GPIO1_IO03_BASE + REGISTER_LENGTH - 1),
        .flags = IORESOURCE_MEM,
    },
    [2]={
        .start = SW_PAD_GPIO1_IO03_BASE,
        .end = (SW_PAD_GPIO1_IO03_BASE + REGISTER_LENGTH - 1),
        .flags = IORESOURCE_MEM,
    },
    [3]={
        .start = GPIO1_DR_BASE,
        .end = (GPIO1_DR_BASE + REGISTER_LENGTH - 1),
        .flags = IORESOURCE_MEM,
    },
    [4]={
        .start = GPIO1_GDIR_BASE,
        .end = (GPIO1_GDIR_BASE + REGISTER_LENGTH - 1),
        .flags = IORESOURCE_MEM,
    },
};

static struct platform_device leddevice={
    .name = "imx6ull-led",
    .id = -1,
    .dev = {
        .release = dev_release,
    },
    .num_resources = ARRAY_SIZE(led_resources),
    .resource = led_resources,
};

static int __init platform_init(void)
{
    return platform_device_register(&leddevice);
}

static void __exit platform_exit(void)
{
    platform_device_unregister(&leddevice);
}


module_init(platform_init);
module_exit(platform_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");