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


#define NAME        "timer"
#define ON               0
#define OFF              1

#define CLOSE_CMD        _IO(0XEF, 1)       //关闭命令
#define OPEN_CMD         _IO(0XEF, 2)       //打开命令
#define WRITE_CMD        _IOW(0XEF, 3, int) //设置周期


struct timer_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *nd;
    dev_t devid;
    int major;
    int minor;
    int gpio;
    int timeperiod;
    struct timer_list timer;
};
struct timer_dev timer;



static int open(struct inode *inode, struct file *filp)
{
	filp->private_data = &timer; /* 设置私有数据 */
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
// static ssize_t write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
// {
//     // int ret = 0;
//     // unsigned char status_buf[1];
//     // ret = copy_from_user(status_buf, buf, count);
//     // if (ret < 0){
//     //     printk("write to kernel failed!\r\n");
//     //     return -EFAULT;
//     // }
//     // else{
//     //     status_switch(status_buf[0]);
//     // }

// 	return 0;
// }

// static ssize_t read(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
// {       
//     // int val[1];
//     // val[0] = gpio_get_value(timer.gpio);
//     // copy_to_user(buf, val , sizeof(val));
//     return 0;
// }




static long timer_ioctl_unlocked(struct file *file,
		  unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev*)file->private_data;
    unsigned int val = 0;
    int ret = 0;
	switch (cmd)
    {
    case CLOSE_CMD:
        del_timer(&(dev->timer));
        break;
    case OPEN_CMD:
        mod_timer(&(dev->timer), jiffies + msecs_to_jiffies(dev->timeperiod));
        break;
    case WRITE_CMD:
        ret = copy_from_user(&val, (int *)arg, sizeof(int));
        if(ret < 0) return -EFAULT;
        dev->timeperiod = val;
        mod_timer(&(dev->timer), jiffies + msecs_to_jiffies(dev->timeperiod));
        break;
    }

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
    .unlocked_ioctl = timer_ioctl_unlocked,
    .release = release,
};


/*定时器处理函数*/
static void timer_func(unsigned long arg)
{
    struct timer_dev dev = *(struct timer_dev*)arg;
    static int val = 0;
    if(val == 0){
        gpio_set_value(dev.gpio, ON);
        val += 1;
    }
    else{
        gpio_set_value(dev.gpio, OFF);
        val = 0;
    }
    mod_timer(&timer.timer, jiffies + msecs_to_jiffies(dev.timeperiod));   /*重启定时器，并给出下一次周期，定时器执行一次后会自动关闭，只是关闭，初始化的结构体还在*/
}

static int __init timer_init(void)
{
    int ret;
    timer.major = 0;
    timer.timeperiod = 500;

    /*获取节点*/
    timer.nd = of_find_node_by_path("/gpioled");
    if(timer.nd == NULL) return -EINVAL;\

    /*获取led对应的gpio*/
    timer.gpio = of_get_named_gpio(timer.nd, "led-gpios", 0);
    if(timer.gpio < 0){
        printk("Can't get timer-gpio.\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d\n", timer.gpio);
    
    /*申请IO：不申请也没事，但如果申请的话，申请失败代表对应的IO已经给其它设备占用了*/
    ret = gpio_request(timer.gpio, "led-gpios");
    if(ret){
        printk("Failed to request this gpio.\n");
        return ret;
    }
    /*设置gpio为输出，且默认输出低电平还是高电平*/
    ret = gpio_direction_output(timer.gpio, 1);
    if(ret < 0) printk("Can't set the gpio\n");


    /*注册设备号*/
    if(timer.major){
        timer.devid = MKDEV(timer.major, 0);
        ret = register_chrdev_region(timer.devid, 1, NAME);
        if(ret < 0) goto fail_dev;
    }
    else{
        ret = alloc_chrdev_region(&timer.devid, 0 , 1, NAME);
        if(ret <0) goto fail_dev;
        timer.major = MAJOR(timer.devid);
        timer.minor = MINOR(timer.devid);
    }
    printk("major=%d, minor=%d\n", timer.major, timer.minor);

    /*添加字符设备*/
    cdev_init(&timer.cdev, &fops);
    ret = cdev_add(&timer.cdev, timer.devid, 1);
    if(ret < 0) goto fail_cdev;

    /*创建类*/
    timer.class = class_create(THIS_MODULE, NAME);
    if(IS_ERR(timer.class)){
        ret = PTR_ERR(timer.class);
        goto fail_class;
    }

    /*创建设备节点*/
    timer.device = device_create(timer.class, NULL, timer.devid, NULL, NAME);
     if(IS_ERR(timer.device)){
        ret = PTR_ERR(timer.device);
        goto fail_device;   
    }

    /*初始化定时器*/
    init_timer(&timer.timer);
    timer.timer.expires = jiffies + msecs_to_jiffies(timer.timeperiod);      /*定时器周期一到，timer_func--timer.timer.function*/
    timer.timer.function = timer_func;
    timer.timer.data = (unsigned long)&timer;                   /*这个参数会作为timer_func函数的参数传入*/
    add_timer(&timer.timer);

    return 0;

fail_device:
    class_destroy(timer.class);
fail_class:
    cdev_del(&timer.cdev);
fail_cdev:
    unregister_chrdev_region(timer.devid, 1);
fail_dev:
    return ret;


}

static void __exit timer_exit(void)
{
     gpio_set_value(timer.gpio, OFF);

    /*释放申请的IO,不释放的话这个gpio可能无法再次申请*/
    gpio_free(timer.gpio);

    /*删除定时器*/
    del_timer(&timer.timer);

    device_del(timer.device);
    class_destroy(timer.class);
    cdev_del(&timer.cdev);
    unregister_chrdev_region(timer.devid, 1);
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");