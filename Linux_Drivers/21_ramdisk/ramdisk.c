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
#include <linux/blkdev.h>


/*ramdisk 是一种模拟磁盘，其数据实际上存储在 RAM 中。它通过 kzalloc() 分配出来的内存空间来模拟出一个磁盘，以块设备的方式访问这片内存。*/

/*定义磁盘大小，内存模式*/
#define RAMDISK_SIZE        (2 * 1024 * 1024)       //2MB的空间
#define RAMDISK_NAME        "ramdisk"
#define RAMDISK_MINOR       3                       //分区数量

struct ramdisk_dev{
    int major;                      //主设备号
    unsigned char *ramdiskbuf;      //内存空间，模拟磁盘空间
    struct gendisk *gendisk;
    struct request_queue *queue;
    spinlock_t lock;
};
struct ramdisk_dev ramdisk;

static int ramdisk_open(struct block_device *bdev, fmode_t mode)
{
    printk("ramdisk open!\n");
    return 0;
}

static void ramdisk_release(struct gendisk *gendisk, fmode_t mode)
{
    printk("ramdisk release!\n");
}

static int ramdisk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    /*用于获取硬盘信息，在hd_geometry这个结构体中*/
    return 0;
}

static const struct block_device_operations fops = {
    .owner = THIS_MODULE,
    .open = ramdisk_open,
    .release = ramdisk_release,
    .getgeo = ramdisk_getgeo,
};


static int ramdisk_transfer(struct request *req)
{
    /**
     * 数据三要素： 源，     目的，        长度
     *            内存地址，块设备地址从， 长度
    */
    unsigned long start = blk_rq_pos(req) << 9;          //块设备起始地址，这里获取的是一个相对的地址(基于有效地址的偏移。只是单位看能不是字节)，而不是绝对的物理地址或映射的虚拟地址
    unsigned long len = blk_rq_cur_bytes(req);           //数据长度
    void *buffer = bio_data(req->bio);                   //bio中的缓存区
    if(start + len > RAMDISK_SIZE){
        printk("too much data!\n");
        return -1;
    }


    /**
     * 获取bio里面的缓存区;
     * 读：磁盘读到的数据保存在缓存区里面
     * 写：此缓存区保存要写入磁盘的数据
    */

    if(rq_data_dir(req) == READ)
        memcpy(buffer, ramdisk.ramdiskbuf + start, len);             //内存模拟用的是memcpy,sd卡、emmc那些用的是对应驱动
    else 
        memcpy(ramdisk.ramdiskbuf + start, buffer, len);

    return 0;
}

static void ramdisk_request_fn(struct request_queue *queue)
{
    int ret,err = 0;
    struct request *req;

    req = blk_fetch_request(queue);
    /*一直取出请求，知道没有请求需要处理*/
    while(req){
        ret = ramdisk_transfer(req);
        if(ret < 0)
            break;
        if(!__blk_end_request_cur(req, err))
            req = blk_fetch_request(queue);
    }
}   


static int __init ramdisk_init(void)
{
    int ret = 0;
    printk("ramdisk_init!\n");

    /*1.申请内存*/
    ramdisk.ramdiskbuf = kzalloc(RAMDISK_SIZE, GFP_KERNEL);
    if(ramdisk.ramdiskbuf == NULL){
        ret = -EINVAL;
        goto ramalloc_fail;
    }

    /*2.注册块设备*/
    ramdisk.major = register_blkdev(0, RAMDISK_NAME);       //为0代表由系统自动分配主设备号
    if(ramdisk.major == 0){
        ret = -EINVAL;
        goto ram_register_blkdev_fail;
    }

    /*3.申请磁盘，参数为次设备号数量，也就是 gendisk 对应的分区数量。*/
    ramdisk.gendisk = alloc_disk(RAMDISK_MINOR);
    if(!ramdisk.gendisk){
        ret = -EINVAL;
        goto gendisk_alloc_fail;
    }

    /*4.申请自旋锁，用来锁住资源*/
    spin_lock_init(&ramdisk.lock);

    /*5.申请请求队列，对于磁盘是需要的，与电梯算法有关*/
    /*对块设备读取的请求都集合到这个结构体，相关读写操作也在这个结构体中*/
    ramdisk.queue = blk_init_queue(ramdisk_request_fn, &ramdisk.lock);
    if(!ramdisk.queue){
        ret = -EINVAL;
        goto blk_queue_fail;
    }

    /*6.初始化gendisk*/
    ramdisk.gendisk->major = ramdisk.major;
    ramdisk.gendisk->first_minor = 0;
    ramdisk.gendisk->fops = &fops;
    //ramdisk.gendisk->private_data = ;                 //没有用到，所以我们不设置，其实就是一个空指针
    ramdisk.gendisk->queue = ramdisk.queue;
    sprintf(ramdisk.gendisk->disk_name, RAMDISK_NAME);  //将后者的字符串内容拷贝到前者指针指向的空间
    set_capacity(ramdisk.gendisk, RAMDISK_SIZE/512);    //设置gendisk容量，单位扇区， 一个扇区是512字节
    printk("bdev-major = %d\n", ramdisk.major);
    add_disk(ramdisk.gendisk);                          //添加到内核里面去


    return 0;

blk_queue_fail:
    del_gendisk(ramdisk.gendisk);
    put_disk(ramdisk.gendisk);
gendisk_alloc_fail:
    unregister_blkdev(ramdisk.major, RAMDISK_NAME);
ram_register_blkdev_fail:
    kfree(ramdisk.ramdiskbuf);
ramalloc_fail:
    return ret;
}

static void __exit ramdisk_exit(void)
{
    del_gendisk(ramdisk.gendisk);
    put_disk(ramdisk.gendisk);                         //删除gendisk后，通知内核gendisk的数量少了一个
    blk_cleanup_queue(ramdisk.queue);
    unregister_blkdev(ramdisk.major, RAMDISK_NAME);
    kfree(ramdisk.ramdiskbuf);
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");