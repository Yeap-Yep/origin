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
#include <linux/input.h>

#define NAME             "key_input"


struct irq_key
{
    struct input_dev *inputdev;
    struct device_node *nd;
    struct timer_list timer;
    struct tasklet_struct tasklet;
    int gpio;
    int timeperiod;
    int irqnum;
    irqreturn_t (*handler)(int, void *);
};

struct irq_key irq_key;


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
    printk("外币把波\n");
    tasklet_schedule(&(dev->tasklet));
    return IRQ_HANDLED;
}

/*tasklet初始化函数*/
static void key_tasklet(unsigned long data)
{
    struct irq_key *dev = (struct irq_key *)data;
    mod_timer(&(dev->timer), jiffies + msecs_to_jiffies(15));
}

/*定时器处理函数*/
static void timer_func(unsigned long arg)
{
    static int val = 0;
    struct irq_key *dev = (struct irq_key*)arg;
    // static int val_last = 0;
    // val_last = val;

    // val = gpio_get_value(irq_key.gpio);
    // if(val == 1)
    //     if(val_last == 0)
    //         printk("外币吧波");

    val = gpio_get_value(irq_key.gpio);
    if(val == 0)
        input_event(dev->inputdev, EV_KEY, KEY_0, 0);               //结构体 上报事件类型 上报事件事件码 对应时间要上报的值
    else if(val == 1)
        input_event(dev->inputdev, EV_KEY, KEY_0, 1);       
    input_sync(dev->inputdev);                                      //上报同步事件，即告知这次上报事件结束
}

static int __init irq_key_init(void)
{
    int ret;
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

    /*注册input_dev*/
    irq_key.inputdev = input_allocate_device();                                     //申请一个input_dev结构体, 内部应该会存在某些东西的初始化
    if(irq_key.inputdev == NULL){
        printk("input_dev register failed");
        return -1;
    }
    irq_key.inputdev->name = NAME;

    /*设置input事件*/
    _set_bit(EV_KEY, irq_key.inputdev->evbit);                                      //按键事件
    _set_bit(EV_REP, irq_key.inputdev->evbit);                                      //设置重复事件，按下时会一直触发
    _set_bit(KEY_0, irq_key.inputdev->keybit);                                       //将板子上的按键与内核里面定义的虚拟按键联系起来

    ret = input_register_device(irq_key.inputdev);
    if(ret){
        goto register_fail;
    }

    return 0;


register_fail:
    input_free_device(irq_key.inputdev);
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

    /*注销input_dev*/
    input_unregister_device(irq_key.inputdev);
    input_free_device(irq_key.inputdev);                                        //把申请到的空间释放掉
}

module_init(irq_key_init);
module_exit(irq_key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");