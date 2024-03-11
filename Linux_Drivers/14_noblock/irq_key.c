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


#define NAME             "irq_key"
#define ON               0
#define OFF              1

#define CLOSE_CMD        _IO(0XEF, 1)       //关闭命令
#define OPEN_CMD         _IO(0XEF, 2)       //打开命令
#define WRITE_CMD        _IOW(0XEF, 3, int) //设置周期


struct general_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
    dev_t devid;
    int major;
    int minor;
};

struct irq_key
{
    struct device_node *nd;
    struct timer_list timer;
    struct tasklet_struct tasklet;
    int gpio;
    int timeperiod;
    int irqnum;
    unsigned char flag;
    irqreturn_t (*handler)(int, void *);
    wait_queue_head_t rwait;
};

struct general_dev mx6ull;
struct irq_key irq_key;

static int open(struct inode *inode, struct file *filp)
{
	filp->private_data = &irq_key; /* 设置私有数据 */
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

static ssize_t read(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{       
    struct irq_key *dev = (struct irq_key*)filp->private_data;
    int val[1];
    int ret = 0;



/*非阻塞方式*/
    if(filp->f_flags & O_NONBLOCK){
        if(dev->flag == 0){
            return -EAGAIN;
        }
        dev->flag = 0;
        ret = copy_to_user(buf, val , sizeof(val));
        if(ret < 0){
            printk("read from kernel failed!\n");
            return -EFAULT;
        }
    }

    // ret = wait_event_interruptible(dev->rwait, dev->flag);
    // if(ret) return ret;



    return 0;

/*阻塞方式*/
#if 0
    wait_event_interruptible(dev->rwait, dev->flag);
    dev->flag = 0;


    DECLARE_WAITQUEUE(wait, current);               //用宏创建一个等待队列项，名字为wait
    add_wait_queue(&(dev->rwait), &wait);           //将等待队列项添加到等待队列头
    __set_current_state(TASK_INTERRUPTIBLE);        //当前进程设置成可被打断状态
    schedule();`                                    //但前状态被主动设置成TASK_INTERRUPTIBLE，所以这个函数会切换到其它可执行进程，等到该进程被唤醒会切换回来
    /*唤醒等到队列头*/
    if(signal_pending(current)){                    //被信号打断：如 ctrl+c 等
        ret = -ERESTARTSYS; 
        goto data_error;
    }
    else{
        __set_current_state(TASK_RUNNING);              //正常唤醒
        remove_wait_queue(&(dev->rwait), &wait);
    }

    val[0] = gpio_get_value(dev->gpio);
    ret = copy_to_user(buf, val , sizeof(val));
    if(ret < 0){
        printk("read from kernel failed!\n");
        return -EFAULT;
    }
    return 0;

data_error:
    __set_current_state(TASK_RUNNING);
    remove_wait_queue(&(dev->rwait), &wait);
    return ret;
#endif



}


static unsigned int poll(struct file * filp, struct poll_table_struct * wait)
{
    int mask = 0;

    struct irq_key *dev = (struct irq_key *)filp->private_data;
    poll_wait(filp, &dev->rwait, wait);                                                     //poll_wait()函数将相应的等待队列注册到poll_table.
    if(dev->flag){
        mask = POLLIN | POLLRDNORM;                                                         //POLLIN代表可读，POLLIN代表普通或优先数据可读， POLLRDNORM代表普通数据可读
    }

    return mask;
}

static long timer_ioctl_unlocked(struct file *file,
		  unsigned int cmd, unsigned long arg)
{
    struct irq_key *dev = (struct irq_key*)file->private_data;
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
    .read = read,
    .poll = poll,
};


/*中断处理函数*/
static irqreturn_t key_irq(int irq, void *dev_id)
{
    struct irq_key *dev = (struct irq_key *)dev_id;
#if 0
    static int val = 0;
    val = gpio_get_value(dev->gpio);
    printk("val = %d\n", val);
    mod_timer(&(dev->timer), msecs_to_jiffies(5));
    if(val == 0){
        printk("KEY0 Pushed!\n");
    }
    else if(val == 1){
        printk("KEY0 Released!\n");
    }
#endif
    tasklet_schedule(&(dev->tasklet));
    return IRQ_HANDLED;
}

/*tasklet初始化函数*/
static void key_tasklet(unsigned long data)
{
    struct irq_key *dev = (struct irq_key *)data;
    static int val = 0;
    static int val_last = 0;
    val_last = val;
    val = gpio_get_value(dev->gpio);
    mod_timer(&(dev->timer), msecs_to_jiffies(15));
    // if(val == 0){
    //     // printk("KEY0 Pushed!\n");
    //     dev->flag = 0;
    // }
    // else if(val == 1){
    //     // printk("KEY0 Released!\n");
    //     dev->flag = 1;
    // }
    if(val == 1)
        if(val_last == 0)
            dev->flag = 1;
    //     else
    //         dev->flag = 0;
    // else if(val == 0)
    //     dev->flag = 0;
    
    if(dev->flag == 1) wake_up(&(dev->rwait));
}

/*定时器处理函数*/
static void timer_func(unsigned long arg)
{
    // struct irq_key *dev = (struct irq_key*)arg;
    // static int val = 0;
    // val = gpio_get_value(dev->gpio);
    // if(val == 0){
    //     printk("KEY0 Pushed!\n");
    // }
    // else if(val == 1){
    //     printk("KEY0 Released!\n");
    // }
}

static int __init irq_key_init(void)
{
    int ret;
    mx6ull.major = 0;
    irq_key.timeperiod = 20;

    /*获取节点*/
    irq_key.nd = of_find_node_by_path("/key");
    if(irq_key.nd == NULL) return -EINVAL;\

    /*获取led对应的gpio*/
    irq_key.gpio = of_get_named_gpio(irq_key.nd, "key-gpio", 0);
    if(irq_key.gpio < 0){
        printk("Can't get irq_key-gpio.\n");
        return -EINVAL;
    }
    printk("key-gpio num = %d\n", irq_key.gpio);
    
    /*申请IO：不申请也没事，但如果申请的话，申请失败代表对应的IO已经给其它设备占用了*/
    ret = gpio_request(irq_key.gpio, "key-gpio");
    if(ret){
        printk("Failed to request this gpio.\n");
        return ret;
    }
    /*设置gpio为输出，且默认输出低电平还是高电平*/
    ret = gpio_direction_input(irq_key.gpio);
    if(ret < 0) printk("Can't set the gpio\n");


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


    /*初始化定时器*/
    init_timer(&irq_key.timer);
    irq_key.timer.expires = jiffies + msecs_to_jiffies(irq_key.timeperiod);      /*定时器周期一到，timer_func--timer.timer.function*/
    irq_key.timer.function = timer_func;
    irq_key.timer.data = (unsigned long)&irq_key;                   /*这个参数会作为timer_func函数的参数传入*/


    /*获取中断号*/
    irq_key.irqnum = gpio_to_irq(irq_key.gpio);                     /*这两个都可以*/
    printk("irqnum = %d\n", irq_key.irqnum);
    // irq_key.irqnum = irq_of_parse_and_map(irq_key.nd, 0);
    // printk("irqnum = %d\n", irq_key.gpio);

    /*申请中断*/
    irq_key.handler = key_irq;
    ret = request_irq(irq_key.irqnum, irq_key.handler, IRQF_TRIGGER_FALLING|IRQF_TRIGGER_FALLING, NAME, &irq_key);
    if(ret) printk("IRQ %d request failed!", irq_key.irqnum);

    /*下文部分tasklet初始化等*/
    tasklet_init(&(irq_key.tasklet), key_tasklet, (unsigned long)&irq_key);

    /* 初始化等待队列头*/
    init_waitqueue_head(&irq_key.rwait);

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

static void __exit irq_key_exit(void)
{
    //gpio_set_value(irq_key.gpio, OFF);

    /*释放中断*/
    free_irq(irq_key.irqnum, &irq_key);

    /*释放申请的IO,不释放的话这个gpio可能无法再次申请*/
    gpio_free(irq_key.gpio);

    /*删除定时器*/
    del_timer(&irq_key.timer);

    device_del(mx6ull.device);
    class_destroy(mx6ull.class);
    cdev_del(&mx6ull.cdev);
    unregister_chrdev_region(mx6ull.devid, 1);
}

module_init(irq_key_init);
module_exit(irq_key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");