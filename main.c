#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

#include "scull.h" 

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

struct scull_dev *scull_devices;

struct scull_qset* scull_follow(struct scull_dev *dev, int item)
{
    struct scull_qset *dptr = dev->data;
    int i;
    for (i = 0; i < item; i++) {
        dptr = dptr->next;
    }
    return dptr;
}

/* scull_read reads only quantum data. library read function
 * continuously calls scull_read unless count which must be read
 * is unsufficient.*/
ssize_t scull_read(struct file *filp, char __user *buff,
    size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;

    /* read size count check */
    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    if (*f_pos >= dev->size)
        goto out;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    /* find item position */
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum, q_pos = rest % quantum;

    dptr = scull_follow(dev, item);

    /* data check */
    if (!dptr || !dptr->data || !dptr->data[s_pos])
        goto out;

    /* read only up to the end of this quantum */
    if (q_pos + count > quantum)
        count = quantum - q_pos;

    /* copy data to userland */
    if (copy_to_user(buff, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

    /* invalid access */
    out:
        up(&dev->sem);
        return retval;
}

/* write data up to single quantum */
ssize_t scull_write(struct file *filp, char __user *buff,
    size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum;
    int qset = dev->qset;
    int itemsize = quantum * qset;
    int item, rest, s_pos, q_pos;
    ssize_t retval = ENOMEM;

    if (down_interruptible(&dev->sem)) {
        return -ERESTARTSYS;
    }

    /* find item */
    item = *f_pos / itemsize;
    rest = *f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;
    dptr = scull_follow(dev, item);

    /* data check */


    /* write data only upto single quantum */
    if (q_pos + count > quantum) {
        count -= quantum - q_pos;
    }
}

/* 
 * scull_trim is in charge of freeing the whole data
 * area and is invoked by scull_open when the file is
 * opened for writing
 */
int scull_trim(struct scull_dev *dev)
{
    struct scull_qset *next, *dptr;
    int qset = dev->qset;
    int i;

    for (dptr = dev->data; dptr; dptr = next) {
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    dev->size = 0;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->data = NULL;
    return 0;
}

int scull_open(struct inode *inode, struct file *filp)
{
    /* identify which device is being opened */
    struct scull_dev *dev;

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev; /* for other method */

    /* trim to 0 the length of the device if open was write only */
    if ((filp->f_mode & O_ACCMODE) == O_WRONLY) {
        
    }
}

int scull_release(struct inode *inode, struct file *filp)
{
    return 0;
}

struct file_operations scull_fops = {
    .owner =    THIS_MODULE,
    .llseek =   scull_llseek,
    .read =     scull_read,
    .write =    scull_write,
    .ioctl =    scull_ioctl,
    .open =     scull_open,
    .release =  scull_release,       
};

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
    int err, devno = MKDEV(scull_major, scull_minor + index);

    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_fops;
    err = cdev_add(&dev->cdev, devno, 1);

    if (err)
        printk(KERN_NOTICE "Error : %d adding scull%d", err, index);
}

static void scull_exit(void)
{
    dev_t dev = MKDEV(scull_major, scull_minor);
    unregister_chrdev_region(dev, scull_nr_devs);
}

static int scull_init(void)
{
    dev_t dev;
    int result, i;

    /* Step1 : allocate device number (major number and minor number) */
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

    /* Step2 : allocate and initilize device structure*/
    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
    if (!scull_devices) {
        result = -ENOMEM;
        scull_exit();
        return result;
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

    /* Step3 : allocate cdev structure and register device structure to kernel */
    for (i = 0; i < scull_nr_devs; i++) {
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
        /* init semaphore */
        /* implement */
        scull_setup_cdev(&scull_devices[i], i);
    }

    return 0;
}

module_init(scull_init);
module_exit(scull_exit);

MODULE_LICENSE("GPL");