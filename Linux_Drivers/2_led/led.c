#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/io.h>

#define LED_MAJOR 200
#define LED_NAME  "LED"

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

static int led_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
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

static int led_close(struct inode *inode, struct file *filp)
{
    return 0;
}

const struct file_operations led_fops={
    .owner = THIS_MODULE,
    .open = led_open,
    .write = led_write,
    .release = led_close,
};

static int __init led_init(void)
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

    printk("led_init\r\n");
    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if(ret < 0){
        return -EIO;
    }
    return 0;
}

static void __exit led_exit(void)
{
    unsigned val = 0;
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    /*取消地址映射*/
    iounmap(CCM_CCGR0);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    printk("exit_init\r\n");
    unregister_chrdev(LED_MAJOR, LED_NAME);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");