#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define NEWCHRLED_NAME  "NEWCHARLED"

#define CCM_CCGR0_BASE              0X020C4068
#define SW_MUX_GPIO1_IO03_BASE      0X020E0068
#define SW_PAD_GPIO1_IO03_BASE      0X020E02F4
#define GPIO1_DR_BASE               0X0209C000
#define GPIO1_GDIR_BASE             0X0209C004
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


static int __init newchrled_init(void)
{   
    int ret = 0;
    unsigned int val = 0;

    /*加载地址映射*/
    CCM_CCGR0 = ioremap(CCM_CCGR0_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

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
        ret = register_chrdev_region(newchrled.devid, 1, NEWCHRLED_NAME);
    }
    else{
        ret = alloc_chrdev_region(&newchrled.devid, 0, 1, NEWCHRLED_NAME);
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
        goto cedv_fail;
    /*自动创建设备节点*/
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.class)){
        goto class_fail;
    }

    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.device))
        goto device_fail;


cedv_fail:
    unregister_chrdev_region(newchrled.devid, 1);
class_fail:
    cdev_del(&newchrled.cdev); 
    unregister_chrdev_region(newchrled.devid, 1);
device_fail:
    class_destroy(newchrled.class);
    cdev_del(&newchrled.cdev); 
    unregister_chrdev_region(newchrled.devid, 1);

    return 0;
    
}

static void __exit newchrled_exit(void)
{
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


    printk("newchrled exit!\r\n");
    
}


module_init(newchrled_init);
module_exit(newchrled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");