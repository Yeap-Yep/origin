#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define chardevbase_major 200
#define chardevbase_name "chardevbase"

static char readbuf[100];
static char writebuf[100];
static char kerneldata[] = "kernel data!";


static int chardevbase_open(struct inode *inode, struct file *file)
{
    //printk("chardevbase_open\r\n");
    return 0;
}

static int chardevbase_release(struct inode *inode, struct file *file)
{
    //printk("chardevbase_release\r\n");
    return 0;
}

static ssize_t chardevbase_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
    int ret = 0;
    memcpy(readbuf, kerneldata, sizeof(kerneldata));
    ret = copy_to_user(buf, readbuf, count);
    // printk("have readen -- %s\r\n", readbuf);
}  

static ssize_t chardevbase_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    int ret = 0;
    copy_from_user(writebuf, buf, count);
    // if (ret == 0){
    //     printk("kernel receivadate: %s\r\n", writebuf);
    // }
}

static const struct file_operations chardevbase_fops =
{
    .owner = THIS_MODULE,
    .open = chardevbase_open,
    .release = chardevbase_release,
    .read =  chardevbase_read,
    .write = chardevbase_write,
};



static int __init chardevbase_init(void)
{
    
    int result = 0;
    printk("Chardevbase init!");
    /*注册字符设备*/
    result = register_chrdev(chardevbase_major, chardevbase_name, &chardevbase_fops);
    if (result < 0){
        printk("Chardevbase init failed!");
    } 
    return 0;
}

static void __exit chardevbase_exit(void)
{
    printk("Chardevbase break!");
    /*注销字符设备*/ 
    unregister_chrdev(chardevbase_major, chardevbase_name);
}




/*
模块入口与出口
*/
module_init(chardevbase_init);
module_exit(chardevbase_exit);

/*应该是添加信息，防止污染内核*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");