#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>

#include "scull.h" 

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;

static int scull_init(void)
{
    dev_t dev;
    int result;

    if (scull_major) {
        dev = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(dev, scull_nr_devs, "scull");
    } else {
        result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
        scull_major = MAJOR(dev);
    }

    if (result < 0) {
        printk(KERN_WARNING "scull: can't get major");
        return result;
    }

    return 0;
}

static void scull_exit(void)
{
    dev_t dev = MKDEV(scull_major, scull_minor);
    unregister_chrdev_region(dev, scull_nr_devs);
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");