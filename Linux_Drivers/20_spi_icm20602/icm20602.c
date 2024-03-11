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
    int cs_gpio;
    struct device_node *nd;
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

static int spi_reg_write(struct general_dev *dev, u8 reg, void *buf, int len)
{
    int ret = 0;
    unsigned char txdata[len];
    struct spi_message m;
    struct spi_transfer *t;
    struct spi_device *spi = (struct spi_device*)dev->target_dev;

    /*spi通信操作开始，片选拉低，操作完成，片选拉高*/
    gpio_set_value(dev->cs_gpio, 0);

    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);                      /*从内存分配一个结构体大小的空间，并返回这个空间的首地址， GFP_KERNEL这个指分配内存的行为，具体什么我也不清楚*/

    /* 第一步，发送要读的寄存器地址*/
    txdata[0] = reg & 0x7f;             //最高为置一表示要读，置0表示要写
    t->tx_buf = txdata;
    t->len = 1;
    spi_message_init(&m);                 //每次用之前一定要初始化spi_message消息结构体，估计是为了清空数据
    spi_message_add_tail(t, &m);         //将数据添加到消息结构体
    ret = spi_sync(spi, &m);                 //发送数据


    /*第二步，写入数据*/
    t->tx_buf = buf;             //因为全双工，没用也要填
    t->len = len;
    spi_message_init(&m);                 //每次用之前一定要初始化spi_message消息结构体
    spi_message_add_tail(t, &m);         //将数据添加到消息结构体
    ret = spi_sync(spi, &m);                 //发送数据

    kfree(t);

    gpio_set_value(dev->cs_gpio, 1);

    return ret;
}


static int spi_reg_read(struct general_dev *dev, u8 reg, void *buf, int len)
{
    int ret = 0;
    unsigned char txdata[len];
    struct spi_message m;
    struct spi_transfer *t;
    struct spi_device *spi = (struct spi_device*)dev->target_dev;

    /*spi通信操作开始，片选拉低，操作完成，片选拉高*/
    gpio_set_value(dev->cs_gpio, 0);

    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);                      /*从内存分配一个结构体大小的空间，并返回这个空间的首地址， GFP_KERNEL这个指分配内存的行为，具体什么我也不清楚*/

    /* 第一步，发送要读的寄存器地址*/
    txdata[0] = reg | 0x80;             //最高为置一表示要读，置0表示要写
    t->tx_buf = txdata;
    t->len = 1;
    spi_message_init(&m);                 //每次用之前一定要初始化spi_message消息结构体，估计是为了清空数据
    spi_message_add_tail(t, &m);         //将数据添加到消息结构体
    ret = spi_sync(spi, &m);                 //发送数据


    /*第二步，读取数据*/
    txdata[0] = 0xff;             //因为全双工，没用也要填
    t->rx_buf = buf;
    t->len = len;
    spi_message_init(&m);                 //每次用之前一定要初始化spi_message消息结构体
    spi_message_add_tail(t, &m);         //将数据添加到消息结构体
    ret = spi_sync(spi, &m);                 //发送数据

    kfree(t);

    gpio_set_value(dev->cs_gpio, 1);

    return ret;

}

static u8 spi_reg_one_read(struct general_dev *dev, u8 reg)
{
    int ret = 0;
    u8 data = 0;
    ret = spi_reg_read(dev, reg, &data, 1);
    return data;
}

static void spi_reg_one_write(struct general_dev *dev, u8 reg, u8 data)
{
    int ret = 0;
    ret = spi_reg_write(dev, reg, &data, 1);
}

static int open(struct inode *inode, struct file *filp)
{
    u8 id = 0;
    filp->private_data = &mx6ull;
    
    /*icm20602初始化*/
    spi_reg_one_write(&mx6ull, ICM20_I2C_IF, 0x40);          //禁止i2c通讯
    spi_reg_one_write(&mx6ull, ICM20_PWR_MGMT_1, 0x80);     //复位
    mdelay(50);
    id = spi_reg_one_read(&mx6ull, ICM20_WHO_AM_I);         //获取id，为了检验上面的复位有没有成功
    printk("icm20602-id = %d\n", id);
    spi_reg_one_write(&mx6ull, ICM20_PWR_MGMT_1, 0x01);
    spi_reg_one_write(&mx6ull, ICM20_PWR_MGMT_2, 0x00);
    spi_reg_one_write(&mx6ull, ICM20_CONFIG, 0x01);
    spi_reg_one_write(&mx6ull, ICM20_SMPLRT_DIV, 0x07);
    spi_reg_one_write(&mx6ull, ICM20_GYRO_CONFIG, 0x18);
    spi_reg_one_write(&mx6ull, ICM20_ACCEL_CONFIG, 0x10);
    spi_reg_one_write(&mx6ull, ICM20_ACCEL_CONFIG2, 0x03);

    printk("icm20602 init\n");
	return 0;
}

static ssize_t read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    struct general_dev *dev = (struct general_dev*)filp->private_data; 
    int i =  0;
    u8 data[12];
    u8 accel_start_address = 0x3B;
    u8 gyro_start_address = 0x43;

    for(i = 0; i < 6; i++){
        data[i] = spi_reg_one_read(dev, accel_start_address + i);
    }
    
   for(i = 0; i < 6; i++){
        data[i+6] = spi_reg_one_read(dev, gyro_start_address + i);
    }
    copy_to_user(buf, data, 12);

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

    /*初始化spi*/
    spi->mode = SPI_MODE_0;                                     //极性选择 
    spi_setup(spi);

    mx6ull.target_dev = spi;

    mx6ull.nd = of_get_parent(spi->dev.of_node);
    mx6ull.cs_gpio = of_get_named_gpio(mx6ull.nd, "cs-gpio", 0);
    if(mx6ull.cs_gpio < 0){
        printk("Can't get the cs_gpio!");
        return -1;
    }
    ret = gpio_request(mx6ull.cs_gpio, "cs_gpio");
    if(ret < 0){
        printk("cs_gpio request failed!");
        return -1;
    }
    ret = gpio_direction_output(mx6ull.cs_gpio, 1);

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
    gpio_free(mx6ull.cs_gpio);

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