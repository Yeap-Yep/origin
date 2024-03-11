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


static int __init dtsof_init(void)
{
    struct device_node *bl_nd;
    bl_nd = of_find_node_by_path("/blacklight");
    retunn 0;
}

static void __exit dtsof_exit(void)
{


}


module_init(dtsof_init);
module_exit(dtsof_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeap");