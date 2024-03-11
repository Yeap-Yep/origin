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
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include "icm20602.h"

#define NAME             "icm20602"

struct general_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
    dev_t devid;
    int major;
    int minor;
    void *target_dev;
};
struct general_dev mx6ull;


static struct spi_device_id icm20602_id[] = {
    {"Mini_Board,icm20602", 0},
    {}
};

//设备树驱动匹配结构体
static const struct of_device_id icm20602_of_match[] = {
    {.compatible = "Mini_Board,icm20602"},
    {}
};


static int open(struct inode *inode, struct file *filp)
{
    filp->private_data = &mx6ull;

    printk("icm20602 init\n");
	return 0;
}

static ssize_t read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    struct general_dev *dev = (struct general_dev*)filp->private_data; 

    return 0;
}

static int release(struct inode *inode, struct file *filp)
{
    printk("release\n");                                                      //删除异步通知
	return 0;
}

const struct file_operations fops={
    .owner = THIS_MODULE,
    .open = open,
    .release = release,
    .read = read,
};

static int icm20602_probe(struct spi_device *spi)
{   
    int ret =0;
    printk("icm20602_probe!\n");

    /*注册设备号*/
    if(mx6ull.major){
        mx6ull.devid = MKDEV(mx6ull.major, 0);
        ret = register_chrdev_region(mx6ull.devid, 1, NAME);
        if(ret < 0) goto fail_dev;
    }
    else{
        ret = alloc_chrdev_region(&mx6ull.devid, 0 , 1, NAME);
        if(ret <0) goto fail_dev;
        mx6ull.major = MAJOR(mx6ull.devid);
        mx6ull.minor = MINOR(mx6ull.devid);
    }
    printk("major=%d, minor=%d\n", mx6ull.major, mx6ull.minor);

    /*添加字符设备*/
    cdev_init(&mx6ull.cdev, &fops);
    ret = cdev_add(&mx6ull.cdev, mx6ull.devid, 1);
    if(ret < 0) goto fail_cdev;

    /*创建类*/
    mx6ull.class = class_create(THIS_MODULE, NAME);
    if(IS_ERR(mx6ull.class)){
        ret = PTR_ERR(mx6ull.class);
        goto fail_class;
    }

    /*创建设备节点*/
    mx6ull.device = device_create(mx6ull.class, NULL, mx6ull.devid, NULL, NAME);
     if(IS_ERR(mx6ull.device)){
        ret = PTR_ERR(mx6ull.device);
        goto fail_device;   
    }

    mx6ull.target_dev = spi;

    return 0;

fail_device:
    class_destroy(mx6ull.class);
fail_class:
    cdev_del(&mx6ull.cdev);
fail_cdev:
    unregister_chrdev_region(mx6ull.devid, 1);
fail_dev:
    return ret;

}

static int icm20602_remove(struct spi_device *spi)
{
    device_del(mx6ull.device);
    class_destroy(mx6ull.class);
    cdev_del(&mx6ull.cdev);
    unregister_chrdev_region(mx6ull.devid, 1);
    return 0;
}

struct spi_driver icm20602_driver = {
    .driver = {
        .name = "icm_20602",
        .owner = THIS_MODULE,
        .of_match_table = icm20602_of_match,
    },
    .probe = icm20602_probe,
    .remove = icm20602_remove,
    .id_table = icm20602_id,
};


static int __init icm20602_init(void)
{
    int ret = 0;
    ret = spi_register_driver(&icm20602_driver);
    return 0;
}


static void __exit icm20602_exit(void)
{
    spi_unregister_driver(&icm20602_driver);
}

module_init(icm20602_init);
module_exit(icm20602_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");