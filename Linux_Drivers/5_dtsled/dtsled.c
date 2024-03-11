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

#define DTSLED_NAME     "DTSLED"
#define LEDON           1
#define LEDOFF          0

void __iomem *  CCM_CCGR0;
void __iomem *  SW_MUX_GPIO1_IO03;
void __iomem *  SW_PAD_GPIO1_IO03;
void __iomem *  GPIO1_DR;
void __iomem *  GPIO1_GDIR;

struct dtsled_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    struct device_node *nd;
    int major;
    int minor;
};
struct dtsled_dev dtsled;

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

static int dtsled_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &dtsled; /* 设置私有数据 */
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
static ssize_t dtsled_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
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
static int dtsled_release(struct inode *inode, struct file *filp)
{
	return 0;
}

const struct file_operations fops={
    .owner = THIS_MODULE,
    .open = dtsled_open,
    .write = dtsled_write,
    .release = dtsled_release,
};

static int __init dtsled_init(void)
{
    int ret = 0;
    u32 val = 0;
    u32 regdata[14];
    struct property *proper;
    const char *str;
    dtsled.major = 0;

    /*从设备树获取信息*/
    dtsled.nd = of_find_node_by_path("/Mini_Board");
    if(dtsled.nd == NULL){
		printk("Mini_Board node not find!\n");
		return -EINVAL;        
    }
    else{
        printk("Mini_Board node find!\n");
    }

    proper = of_find_property(dtsled.nd, "compatible", NULL);
	if(proper == NULL) {
		printk("compatible property find failed\r\n");
	} else {
		printk("compatible = %s\n", (char*)proper->value);
	}

	ret = of_property_read_string(dtsled.nd, "status", &str);
	if(ret < 0){
		printk("status read failed!\r\n");
	} else {
		printk("status = %s\n",str);
	}
#if 0
    ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 10);
	if(ret < 0) {
		printk("reg property read failed!\r\n");
	} else {
		u8 i = 0;
		printk("reg data:\r\n");
		for(i = 0; i < 10; i++)
			printk("%#X ", regdata[i]);
		printk("\r\n");
    }

    /*获取MMU的虚拟映射地址*/

    CCM_CCGR0 = ioremap(regdata[0],regdata[1]);
    SW_MUX_GPIO1_IO03 = ioremap(regdata[2],)regdata[3];
    SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
    GPIO1_DR = ioremap(regdata[6], regdata[7]);
    PIO1_GDIR = ioremap(regdata[8], regdata[9]);
#else
    /*这个函数直接获取设备树节点reg的值，并完成地址映射*/
    CCM_CCGR0 = of_iomap(dtsled.nd, 0);
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd, 1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd, 2);
    GPIO1_DR = of_iomap(dtsled.nd, 3);
    GPIO1_GDIR = of_iomap(dtsled.nd, 4);
#endif

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


    /*注册设备号*/
    if(dtsled.major){
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, 1, DTSLED_NAME);
    }
    else{
        ret = alloc_chrdev_region(&dtsled.devid, 0, 1, DTSLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    if(ret < 0){
        goto fail_dev;
    }

    /*添加字符设备*/
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &fops);
    ret = cdev_add(&dtsled.cdev, dtsled.devid, 1);
    if(ret < 0) goto fail_cdev;

    /*创建类*/
    dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
    if(IS_ERR(dtsled.class)){
        ret = PTR_ERR(dtsled.class);
        goto fail_class;
    }

    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
    if(IS_ERR(dtsled.device)){
        ret = PTR_ERR(dtsled.device);
        goto fail_device;
    }
    
    return 0;
    
fail_device:
    class_destroy(dtsled.class);
fail_class:
    cdev_del(&dtsled.cdev);
fail_cdev:
    unregister_chrdev_region(dtsled.devid, 1);
fail_dev:
    return ret;

//    return 0;
}


static void __exit dtsled_exit(void)
{
    /*关灯*/
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

    device_destroy(dtsled.class, dtsled.devid);
    class_destroy(dtsled.class);
    cdev_del(&dtsled.cdev);
    unregister_chrdev_region(dtsled.devid, 1);
}

module_init(dtsled_init);
module_exit(dtsled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");