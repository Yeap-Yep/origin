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

#define GPIOLED_NAME        "led_platform_dts"
#define LEDON               1
#define LEDOFF              0

struct gpioled_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *nd;
    dev_t devid;
    int major;
    int minor;
    int led_gpio;
};
struct gpioled_dev gpioled;

void led_switch(u8 status)
{
    if(status == LEDOFF){
        gpio_set_value(gpioled.led_gpio, 1);
    }
    else if(status == LEDON){
        gpio_set_value(gpioled.led_gpio, 0);
    }
    else printk("wrong arguement\r\n");
}

static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &gpioled; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
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

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

const struct file_operations fops={
    .owner = THIS_MODULE,
    .open = led_open,
    .write = led_write,
    .release = led_release,
};

static int led_probe(struct platform_device *dev)
{
    int ret;
    
    gpioled.major = 0;
    /*获取节点*/
#if 0
    gpioled.nd = of_find_node_by_path("/gpioled");
    if(gpioled.nd == NULL) return -EINVAL;
#endif
    gpioled.nd = dev->dev.of_node;

    /*获取led对应的gpio*/
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpios", 0);
    if(gpioled.led_gpio < 0){
        printk("Can't get led-gpiops.\n");
        return -EINVAL;
    }
    printk("led-gpios num = %d \n", gpioled.led_gpio);
    
    /*申请IO：不申请也没事，但如果申请的话，申请失败代表对应的IO已经给其它设备占用了*/
    ret = gpio_request(gpioled.led_gpio, "led-gpios");
    if(ret){
        printk("Failed to request the gpio\n");
        return ret;
    }

    /*设置gpio为输出，且默认输出低电平还是高电平*/
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if(ret < 0) printk("Can't set gpio \n");
    /*注册设备号*/
    if(gpioled.major){
        gpioled.devid = MKDEV(gpioled.major, 0);
        ret = register_chrdev_region(gpioled.devid, 1, GPIOLED_NAME);
        if(ret < 0) goto fail_dev;
    }
    else{
        ret = alloc_chrdev_region(&gpioled.devid, 0 , 1, GPIOLED_NAME);
        if(ret <0) goto fail_dev;
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    printk("major=%d, minor=%d\n", gpioled.major, gpioled.minor);

    /*添加字符设备*/
    cdev_init(&gpioled.cdev, &fops);
    ret = cdev_add(&gpioled.cdev, gpioled.devid, 1);
    if(ret < 0) goto fail_cdev;

    /*创建类*/
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if(IS_ERR(gpioled.class)){
        ret = PTR_ERR(gpioled.class);
        goto fail_class;
    }

    /*创建设备节点*/
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
     if(IS_ERR(gpioled.device)){
        ret = PTR_ERR(gpioled.device);
        goto fail_device;   
    }

    return 0;

fail_device:
    class_destroy(gpioled.class);
fail_class:
    cdev_del(&gpioled.cdev);
fail_cdev:
    unregister_chrdev_region(gpioled.devid, 1);
fail_dev:
    return ret;


}

static int led_remove(struct platform_device *dev)
{
    /*释放申请的IO*/
    gpio_free(gpioled.led_gpio);

    device_del(gpioled.device);
    class_destroy(gpioled.class);
    cdev_del(&gpioled.cdev);
    unregister_chrdev_region(gpioled.devid, 1);
    printk("ledplatform_dts remove\n");
    return 0;
}

struct of_device_id led_of_match[]={
    {.compatible = "Mini_Board,gpioled"},
    // {.compatible = "xxx"},                       //如果可以匹配多个compatible属性，那格式就这么写
    // {.compatible = "yyy"},
    {/*sentinel*/},
};

static struct platform_driver led_driver={
    .driver = {
        .name = "imx6ull-led",                      //无设备树的情况，驱动和设备的名字匹配，有设备树的情况，这就是单纯的名字，靠compatible属性匹配
        .of_match_table = led_of_match,
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