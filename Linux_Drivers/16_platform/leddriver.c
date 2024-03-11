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


#define PLATFORM_NAME  "platform_led"
#define LEDON                       1
#define LEDOFF                      0

void __iomem *  CCM_CCGR0;
void __iomem *  SW_MUX_GPIO1_IO03;
void __iomem *  SW_PAD_GPIO1_IO03;
void __iomem *  GPIO1_DR;
void __iomem *  GPIO1_GDIR;

/*LED设备结构体*/
struct newchrled_dev
{
    struct cdev cdev;           /*字符设备*/
    struct class *class;;       /*类*/
    struct device *device;      /*设备节点*/
    dev_t devid;                /*设备号*/
    int major;                  /*主设备号*/
    int minor;                  /*次设备号*/
};
struct newchrled_dev newchrled;

void led_switch(u8 status)
{
    unsigned val = 0;
    if(status == LEDOFF){
        val = readl(GPIO1_DR);
        val |= (1 << 3);
        writel(val, GPIO1_DR);
    }
    else if(status == LEDON){
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    }
    else printk("wrong arguement\r\n");
}

static int newchrled_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;
    return 0;
}

static ssize_t newchrled_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    int ret = 0;
    unsigned char status_buf[1];
    ret = copy_from_user(status_buf, buf, count);
    if (ret < 0){
        printk("write to kernel failed!\r\n");
        return -EFAULT;
    }
    else{
        led_switch(status_buf[0]);
    }

    return 0;
}

static int newchrled_close(struct inode *inode, struct file *filp)
{
    return 0;
}

const struct file_operations newchrled_fops={
    .owner = THIS_MODULE,
    .open = newchrled_open,
    .write = newchrled_write,
    .release = newchrled_close,
};

static int led_probe(struct platform_device *dev)
{
    // printk("leddriver probe\n");
    /*初始化一些东西，当驱动和设备匹配后，会调用probe也就是此函数*/
    struct resource *ledresources[5];
    int i, ret = 0, val = 0;
    for(i = 0; i < 5; i++){
        ledresources[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
        if(ledresources[i] == NULL)
            return -EINVAL;
    }

        /*加载地址映射*/
    CCM_CCGR0 = ioremap(ledresources[0]->start, resource_size(ledresources[0]));
    SW_MUX_GPIO1_IO03 = ioremap(ledresources[1]->start, resource_size(ledresources[1]));
    SW_PAD_GPIO1_IO03 = ioremap(ledresources[2]->start, resource_size(ledresources[2]));
    GPIO1_DR = ioremap(ledresources[3]->start, resource_size(ledresources[3]));
    GPIO1_GDIR = ioremap(ledresources[4]->start, resource_size(ledresources[4]));

    /*初始化*/
    val = readl(CCM_CCGR0);
    val &= ~(3 << 26);
    val |= 3 << 26;
    writel(val, CCM_CCGR0);

    writel(0x5, SW_MUX_GPIO1_IO03);
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    val = readl(GPIO1_GDIR);
    val |= 1 << 3;
    writel(val, GPIO1_GDIR);

    val = readl(GPIO1_DR);
    val &= ~(1 << 3);
    writel(val, GPIO1_DR);

    /*注册字符设备*/
    if(newchrled.major){
        newchrled.devid = MKDEV(newchrled.major, 0);
        ret = register_chrdev_region(newchrled.devid, 1, PLATFORM_NAME);
    }
    else{
        ret = alloc_chrdev_region(&newchrled.devid, 0, 1, PLATFORM_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    if(ret < 0){
        printk("newchrled chrdev_region failed!\r\n");
        return -1;
    }
    printk("newchrled major=%d minor=%d\r\n", newchrled.major, newchrled.minor);

    /*初始化cdev结构体*/
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &newchrled_fops);
    ret = cdev_add(&newchrled.cdev, newchrled.devid, 1);      /* 添加字符设备 */
    if (ret < 0)
        // goto cedv_fail;
        return -1;
    /*自动创建设备节点*/
    newchrled.class = class_create(THIS_MODULE, PLATFORM_NAME);
    if(IS_ERR(newchrled.class)){
        // goto class_fail;
        return -1;
    }

    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, PLATFORM_NAME);
    if(IS_ERR(newchrled.device))
        // goto device_fail;
        return -1;

    return 0;

// device_fail:
//     class_destroy(newchrled.class);
// class_fail:
//     cdev_del(&newchrled.cdev); 
// cedv_fail:
//     unregister_chrdev_region(newchrled.devid, 1);
//     return -1;

}

static int led_remove(struct platform_device *dev)
{
    /*当对应的驱动和设备其中一个从内核太卸下时候，remove即此函数就会运行*/
    unsigned val = 0;
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    /*卸载地址映射*/
    iounmap(CCM_CCGR0);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /*摧毁设备节点*/
    device_destroy(newchrled.class, newchrled.devid);
    /*摧毁类*/
    class_destroy(newchrled.class);
    /* 删除 cdev */
    cdev_del(&newchrled.cdev); 
    /*卸载字符设备*/
    unregister_chrdev_region(newchrled.devid, 1);
    
    printk("leddriver removed\n");
    
    return 0;
}

static struct platform_driver led_driver={
    .driver = {
        .name = "imx6ull-led",                      //驱动名字要和设备名字一样才能匹配
    },
    .probe = led_probe,
    .remove = led_remove,
};

static int __init platform_init(void)
{
    return platform_driver_register(&led_driver);
}

static void __exit platform_exit(void)
{
    platform_driver_unregister(&led_driver);
}


module_init(platform_init);
module_exit(platform_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");