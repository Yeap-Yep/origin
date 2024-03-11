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
#include <linux/i2c.h>
#include <linux/delay.h>
#include "mpu6050.h"

#define NAME             "mpu6050"


struct general_dev
{
    struct cdev cdev;
    struct class *class;
    struct device *device;
    dev_t devid;
    int major;
    int minor;
    void *private_data;               //存放i2c_client
};
struct general_dev mx6ull;


/*不论读或写，可以理解为一个结构体就代表完成一次操作--读完成或写完成*/

/*从寄存器读数据，读取之前要把需要操作的寄存器地址先写入i2c外设*/
static int i2c_read_regs(struct general_dev *dev, u8 reg, void *val, int len)
{
    struct i2c_msg msg[2];                                                          //i2c_msg是个操作信息结构体，里面的成员包括你要操作的i2c对象的地址等信息
    /*i2c_client 是core层通过读取设备树结点信息而创建的描述i2c外设信息的结构体，所以里面包含外设地址等信息，取决于你的设备树*/
    struct i2c_client *client = (struct i2c_client*)dev->private_data;

    msg[0].addr = client->addr;
    msg[0].flags = 0;                   //表示写
    msg[0].buf = &reg;                  //表示要操作的数据--寄存器地址或数据，这里是地址  会将buf里的数据都操作完成，比如连读或者连写
    msg[0].len = 1;                     //i2c外设的寄存器字节长度

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;           //表示读
    msg[1].buf = val;                  //表示要操作的数据--这里是数据
    msg[1].len = len;                  //数据的长度

    /*i2c适配器     操作结构体    结构体长度*/
    i2c_transfer(client->adapter, msg, 2);
    return 0;
}

/*向寄存器写数据*/
static int i2c_write_regs(struct general_dev *dev, u8 reg, void *buf, int len)
{
    u8 b[128];
    struct i2c_msg msg;                                                          //i2c_msg是个操作信息结构体，里面的成员包括你要操作的i2c对象的地址等信息
    /*i2c_client 是core层通过读取设备树结点信息而创建的描述i2c外设信息的结构体，所以里面包含外设地址等信息，取决于你的设备树*/
    struct i2c_client *client = (struct i2c_client*)dev->private_data;

    /*把地址放到b[0]，把连写数据拷贝到b[0]后面*/
    b[0] = reg;
    memcpy(&b[1], buf, len);

    msg.addr = client->addr;
    msg.flags = 0;                       //表示写
    msg.buf = b;                         //连续写在这边本质就是，把buf的数据全写入，只不过第一个数据一定要是地址
    msg.len = len+1;                     //要操作的数据长度

    /*i2c适配器     操作结构体    结构体长度*/
    i2c_transfer(client->adapter, &msg, 1);
    return 0;
}

/*读取一个寄存器*/
static u8 i2c_read_one_reg(struct general_dev *dev, u8 reg)
{
    u8 val = 0;
    i2c_read_regs(dev, reg, &val, 1);
    return val;
}

/*往一个寄存器写值*/
static void i2c_write_one_reg(struct general_dev *dev, u8 reg, u8 val)
{
    i2c_write_regs(dev, reg, &val, 1);
}

static void mpu6050_register_init(void)
{
    int id = 0, test = 0;

    id = i2c_read_one_reg(&mx6ull, 0x75);
    printk("id = %#x\n", id);
    test = i2c_read_one_reg(&mx6ull, POWER_MANAGEMENT_1);
    printk("reset = %#x\n", test);


    i2c_write_one_reg(&mx6ull, POWER_MANAGEMENT_1, 0x80);
    mdelay(100);


    i2c_write_one_reg(&mx6ull, POWER_MANAGEMENT_1, 0x00);
    // i2c_write_one_reg(&mx6ull, INTERRUPT_ENABLE, 0x00);
    i2c_write_one_reg(&mx6ull, SAMPLE_RATE_DIVIDER, 0x07);
    i2c_write_one_reg(&mx6ull, CONFIGURATION, 0x00);
    i2c_write_one_reg(&mx6ull, GYROSCOPE_CONFIGURATION, 0x18);
    i2c_write_one_reg(&mx6ull, ACCELEROMETER_CONFIGURATION, 0x08);
    i2c_write_one_reg(&mx6ull, 0x23, 0x00);
    // i2c_write_one_reg(&mx6ull, BYPASS_ENABLE_CONFIGURATION, 0x02);
    i2c_write_one_reg(&mx6ull, POWER_MANAGEMENT_1, 0x00);                       //系统时钟源不能选择PPL， 使用x轴陀螺作为参考
    i2c_write_one_reg(&mx6ull, POWER_MANAGEMENT_2, 0x00);

    id = i2c_read_one_reg(&mx6ull, 0x75);
    printk("id_2 = %#x\n", id);
    test = i2c_read_one_reg(&mx6ull, POWER_MANAGEMENT_1);
    printk("reset_2 = %#x\n", test);

}


static int open(struct inode *inode, struct file *filp)
{
    filp->private_data = &mx6ull;
    mpu6050_register_init();
    printk("mpu6050 init\n");
	return 0;
}

static ssize_t read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    u8 data[14];
    signed short ax;
    int i = 0;
    int reg = 0x3B;
    int ret = 0;
    struct general_dev *dev = (struct general_dev*)filp->private_data; 



    for(i = 0; i < 14; i++)
    {
        data[i] = i2c_read_one_reg(dev, reg + i);
        mdelay(10);
    } 
    ax = (data[0] << 8) | data[1];
    printk("data = %d\n", ax);
    ret = copy_to_user(buf, data, sizeof(data));
    if(ret)
        printk("copy_to_user error!\n");
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

static int icm20602_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{   
    int ret =0;
    printk("mpu6050_probe!\n");

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

    mx6ull.private_data = i2c;

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

static int icm20602_remove(struct i2c_client *i2c)
{
    device_del(mx6ull.device);
    class_destroy(mx6ull.class);
    cdev_del(&mx6ull.cdev);
    unregister_chrdev_region(mx6ull.devid, 1);
    return 0;
}

/*传统匹配表*/
static struct i2c_device_id icm20602_id[] = {
    {"Mini_Board,mpu6050", 0},
    {}
};

/*设备树匹配表*/
static struct of_device_id icm20602_of_match[] = {
    {.compatible = "Mini_Board,mpu6050", },
    {}
};

// struct irq_key
// {
//     struct device_node *nd;
//     struct timer_list timer;
//     struct tasklet_struct tasklet;
//     int gpio;
//     int irqnum;
//     irqreturn_t (*handler)(int, void *);
// };

// static struct irq_key irq_key;


static struct i2c_driver icm20602_driver = {
    .probe = icm20602_probe,
    .remove = icm20602_remove,
    .id_table = icm20602_id,
    .driver = {
        .name = "mpu6050",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(icm20602_of_match),
    },
};

static int __init mpu6050_init(void)
{
    int ret = 0;
    ret = i2c_register_driver(THIS_MODULE, &icm20602_driver);
    return ret;
    /*物理设置是GPIO1_IO08是高点平，我们需要的也是高点平，就不需要去设置了*/



    // /*获取节点*/
    // irq_key.nd = of_find_node_by_path("/gpio");
    // if(irq_key.nd == NULL) return -EINVAL;

    // /*获取led对应的gpio*/
    // irq_key.gpio = of_get_named_gpio(irq_key.nd, "gpios", 0);
    // if(irq_key.gpio < 0){
    //     printk("Can't get gpio.\n");
    //     return -EINVAL;
    // }
    // printk("gpio num = %d\n", irq_key.gpio);
    
    // /*申请IO：不申请也没事，但如果申请的话，申请失败代表对应的IO已经给其它设备占用了*/
    // ret = gpio_request(irq_key.gpio, "gpios");
    // if(ret){
    //     printk("Failed to request this gpio.\n");
    //     return ret;
    // }
    // /*设置gpio为输出，且默认输出低电平还是高电平*/
    // ret = gpio_direction_output(irq_key.gpio, 0);
    // if(ret < 0) printk("Can't set the gpio\n");
    // // gpio_set_value(irq_key.gpio, 1);
    // ret = gpio_get_value(irq_key.gpio);
    // printk("ret = %d\n", ret);

}


static void __exit mpu6050_exit(void)
{
    i2c_del_driver(&icm20602_driver);
}

module_init(mpu6050_init);
module_exit(mpu6050_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");