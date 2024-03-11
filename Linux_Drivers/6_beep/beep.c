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

#define GPIOLED_NAME        "beep"
#define ON               1
#define OFF              0

struct beep_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *nd;
    dev_t devid;
    int major;
    int minor;
    int gpio;
};
struct beep_dev beep;

void status_switch(u8 status)
{
    if(status == OFF){
        gpio_set_value(beep.gpio, 1);
    }
    else if(status == ON){
        gpio_set_value(beep.gpio, 0);
    }
    else printk("wrong arguement\r\n");
}

static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &beep; /* 设置私有数据 */
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
        status_switch(status_buf[0]);
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


static int __init gpioled_init(void)
{
    int ret;

    beep.major = 0;

    /*获取节点*/
    beep.nd = of_find_node_by_path("/beep");
    if(beep.nd == NULL) return -EINVAL;\

    /*获取led对应的gpio*/
    beep.gpio = of_get_named_gpio(beep.nd, "beep-gpio", 0);
    if(beep.gpio < 0){
        printk("Can't get beep-gpio.\n");
        return -EINVAL;
    }
    printk("beep-gpio num = %d\n", beep.gpio);
    
    /*申请IO：不申请也没事，但如果申请的话，申请失败代表对应的IO已经给其它设备占用了*/
    ret = gpio_request(beep.gpio, "beep-gpio");
    if(ret){
        printk("Failed to request this gpio.\n");
        return ret;
    }
    /*设置gpio为输出，且默认输出低电平还是高电平*/
    ret = gpio_direction_output(beep.gpio, 1);
    if(ret < 0) printk("Can't set the gpio\n");

    /*注册设备号*/
    if(beep.major){
        beep.devid = MKDEV(beep.major, 0);
        ret = register_chrdev_region(beep.devid, 1, GPIOLED_NAME);
        if(ret < 0) goto fail_dev;
    }
    else{
        ret = alloc_chrdev_region(&beep.devid, 0 , 1, GPIOLED_NAME);
        if(ret <0) goto fail_dev;
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }
    printk("major=%d, minor=%d\n", beep.major, beep.minor);

    /*添加字符设备*/
    cdev_init(&beep.cdev, &fops);
    ret = cdev_add(&beep.cdev, beep.devid, 1);
    if(ret < 0) goto fail_cdev;

    /*创建类*/
    beep.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if(IS_ERR(beep.class)){
        ret = PTR_ERR(beep.class);
        goto fail_class;
    }

    /*创建设备节点*/
    beep.device = device_create(beep.class, NULL, beep.devid, NULL, GPIOLED_NAME);
     if(IS_ERR(beep.device)){
        ret = PTR_ERR(beep.device);
        goto fail_device;   
    }

    return 0;

fail_device:
    class_destroy(beep.class);
fail_class:
    cdev_del(&beep.cdev);
fail_cdev:
    unregister_chrdev_region(beep.devid, 1);
fail_dev:
    return ret;


}

static void __exit gpioled_exit(void)
{
    /*释放申请的IO*/
    gpio_free(beep.gpio);

    device_del(beep.device);
    class_destroy(beep.class);
    cdev_del(&beep.cdev);
    unregister_chrdev_region(beep.devid, 1);
}

module_init(gpioled_init);
module_exit(gpioled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");