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


#define NAME        "key"
#define ON               1
#define OFF              0

struct key_dev
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
struct key_dev key;

void status_switch(u8 status)
{
    if(status == OFF){
        gpio_set_value(key.gpio, 1);
    }
    else if(status == ON){
        gpio_set_value(key.gpio, 0);
    }
    else printk("wrong arguement\r\n");
}

static int open(struct inode *inode, struct file *filp)
{
	filp->private_data = &key; /* 设置私有数据 */
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
static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
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

static ssize_t read(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{       
    int val[1];
    val[0] = gpio_get_value(key.gpio);
    copy_to_user(buf, val , sizeof(val));
    return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int release(struct inode *inode, struct file *filp)
{
	return 0;
}

const struct file_operations fops={
    .owner = THIS_MODULE,
    .open = open,
    .write = write,
    .release = release,
    .read = read,
};


static int __init key_init(void)
{
    int ret;

    key.major = 0;

    /*获取节点*/
    key.nd = of_find_node_by_path("/key");
    if(key.nd == NULL) return -EINVAL;\

    /*获取led对应的gpio*/
    key.gpio = of_get_named_gpio(key.nd, "key-gpio", 0);
    if(key.gpio < 0){
        printk("Can't get key-gpio.\n");
        return -EINVAL;
    }
    printk("key-gpio num = %d\n", key.gpio);
    
    /*申请IO：不申请也没事，但如果申请的话，申请失败代表对应的IO已经给其它设备占用了*/
    ret = gpio_request(key.gpio, "key-gpio");
    if(ret){
        printk("Failed to request this gpio.\n");
        return ret;
    }
    /*设置gpio为输出，且默认输出低电平还是高电平*/
    ret = gpio_direction_input(key.gpio);
    if(ret < 0) printk("Can't set the gpio\n");

    /*注册设备号*/
    if(key.major){
        key.devid = MKDEV(key.major, 0);
        ret = register_chrdev_region(key.devid, 1, NAME);
        if(ret < 0) goto fail_dev;
    }
    else{
        ret = alloc_chrdev_region(&key.devid, 0 , 1, NAME);
        if(ret <0) goto fail_dev;
        key.major = MAJOR(key.devid);
        key.minor = MINOR(key.devid);
    }
    printk("major=%d, minor=%d\n", key.major, key.minor);

    /*添加字符设备*/
    cdev_init(&key.cdev, &fops);
    ret = cdev_add(&key.cdev, key.devid, 1);
    if(ret < 0) goto fail_cdev;

    /*创建类*/
    key.class = class_create(THIS_MODULE, NAME);
    if(IS_ERR(key.class)){
        ret = PTR_ERR(key.class);
        goto fail_class;
    }

    /*创建设备节点*/
    key.device = device_create(key.class, NULL, key.devid, NULL, NAME);
     if(IS_ERR(key.device)){
        ret = PTR_ERR(key.device);
        goto fail_device;   
    }

    return 0;

fail_device:
    class_destroy(key.class);
fail_class:
    cdev_del(&key.cdev);
fail_cdev:
    unregister_chrdev_region(key.devid, 1);
fail_dev:
    return ret;


}

static void __exit key_exit(void)
{
    /*释放申请的IO*/
    gpio_free(key.gpio);

    device_del(key.device);
    class_destroy(key.class);
    cdev_del(&key.cdev);
    unregister_chrdev_region(key.devid, 1);
}

module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");