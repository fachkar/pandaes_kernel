#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
// #include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/parport.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/kthread.h>
#include <linux/err.h>

#include <linux/platform_device.h>

#include <asm/system.h>     /* cli(), *_flags */
#include <asm/uaccess.h>    /* copy_*_user */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>


MODULE_LICENSE ( "Dual BSD/GPL" );
MODULE_DESCRIPTION ( "This is leds fasl5 ;/" );
MODULE_AUTHOR ( "ferar aschkar" );
MODULE_VERSION ( "0.5.3" );


#define DEVICE_NAME     "ledso"

struct leds_dev
{
    char name[32];
    int led_value;
    struct cdev cdev;
    struct device *leds_device;
    struct parport_driver leds_parport_driver;
    struct pardevice *pdev;
    struct kobject kobj_leds;
} *leds_devp;

#define to_leds_dev(x) container_of(x, struct leds_dev, kobj_leds)

struct leds_attribute {
    struct attribute attr;
    ssize_t (*show)(struct leds_dev *ledsdevp, struct leds_attribute *attr, char *buf);
    ssize_t (*store)(struct leds_dev *ledsdevp, struct leds_attribute *attr, const char *buf, size_t count);
};
#define to_leds_attr(x) container_of(x, struct leds_attribute, attr)

static ssize_t leds_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct leds_attribute *ledsattribute;
    struct leds_dev *ledsdevp;
    printk ( "%s line: %d\n", __func__, __LINE__ );
    ledsattribute = to_leds_attr(attr);
    ledsdevp = to_leds_dev(kobj);

    if (!ledsattribute->show)
        return -EIO;

    return ledsattribute->show(ledsdevp, ledsattribute, buf);
}


static ssize_t leds_attr_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t len)
{
    struct leds_attribute *ledsattribute;
    struct leds_dev *ledsdevp;
    printk ( "%s line: %d\n", __func__, __LINE__ );
    ledsattribute = to_leds_attr(attr);
    ledsdevp = to_leds_dev(kobj);

    if (!ledsattribute->store)
        return -EIO;

    return ledsattribute->store(ledsdevp, ledsattribute, buf, len);
}

static const struct sysfs_ops leds_sysfs_ops = {
    .show = leds_attr_show,
    .store = leds_attr_store,
};

static void leds_kobj_release(struct kobject *kobj)
{
    struct leds_dev *ledsdevp;
    printk ( "%s line: %d\n", __func__, __LINE__ );
    ledsdevp = to_leds_dev(kobj);
    if (ledsdevp != NULL)kfree(ledsdevp);
}

static ssize_t leds_show(struct leds_dev *ledsdevp, struct leds_attribute *attr, char *buf)
{
    printk ( "%s line: %d\n", __func__, __LINE__ );
    return sprintf(buf, "%d\n", ledsdevp->led_value);
}

static ssize_t leds_store(struct leds_dev *ledsdevp, struct leds_attribute *attr, const char *buf, size_t count)
{
    unsigned char parportbuf;
    unsigned int number;
    printk ( "%s line: %d\n", __func__, __LINE__ );
    sscanf(buf, "%du", &ledsdevp->led_value);

    number = simple_strtol(attr->attr.name+3, NULL, 10);

    parport_claim_or_block(leds_devp->pdev);
    parportbuf = parport_read_data(leds_devp->pdev->port);
    if (ledsdevp->led_value) {
        parport_write_data(leds_devp->pdev->port, parportbuf | (1<<number));
    } else {
        parport_write_data(leds_devp->pdev->port, parportbuf & ~(1<<number));
    }
    parport_release(leds_devp->pdev);
    return count;
}

static struct leds_attribute led1_attribute = __ATTR(led1, 0666, leds_show, leds_store);
static struct leds_attribute led2_attribute = __ATTR(led2, 0666, leds_show, leds_store);

static struct attribute *leds_dev_default_attrs[] = {
    &led1_attribute.attr,
    &led2_attribute.attr,
    NULL,   /* need to NULL terminate the list of attributes */
};

static struct kobj_type leds_ktype = {
    .sysfs_ops = &leds_sysfs_ops,
    .release = leds_kobj_release,
    .default_attrs = leds_dev_default_attrs,
};

static int
leds_preempt ( void *handle )
{
    printk ( "%s line: %d\n", __func__, __LINE__ );
    return 1;
}

/* Parport attach method */
static void
leds_attach ( struct parport *port )
{
    printk ( ">> %s line: %d\n", __func__, __LINE__ );
    /* Register the parallel LED device with parport */
    leds_devp->pdev = parport_register_device ( port, DEVICE_NAME,
                      leds_preempt, NULL,
                      NULL, 0, NULL );
    if ( leds_devp->pdev == NULL )
        printk ( "Bad register\n" );
    printk ( "<< %s line: %d\n", __func__, __LINE__ );
}

/* Parport detach method */
static void
leds_detach ( struct parport *port )
{
    printk ( "%s line: %d\n", __func__, __LINE__ );
    /* Do nothing */
}

unsigned char port_data_in ( unsigned char offset, int bank );
void port_data_out ( unsigned char offset, unsigned char data, int bank );

int leds_open ( struct inode *inode, struct file *file );
int leds_release ( struct inode *inode, struct file *file );
// ssize_t leds_read(struct file *file, char *buf, size_t count, loff_t *ppos);
ssize_t leds_write ( struct file *file, const char *buf, size_t count,
                     loff_t * ppos );

static struct file_operations leds_fops =
{
    .owner = THIS_MODULE,
    .open = leds_open,
    .release = leds_release,
    .write = leds_write,
};

static dev_t leds_dev_t_major_number;
struct class *leds_class;




int
leds_open ( struct inode *inode, struct file *file )
{
    struct leds_dev *leds_devp = NULL;

    printk ( ">> %s line: %d\n", __func__, __LINE__ );

    /* Get the per-device structure that contains this cdev */
    leds_devp = container_of ( inode->i_cdev, struct leds_dev, cdev );
    if ( !leds_devp )
    {
        printk ( KERN_ERR "leds_open %s failed to get container_of line: %d\n",
                 DEVICE_NAME, __LINE__ );
        return -1;
    }

    /* Easy access to cmos_devp from rest of the entry points */
    file->private_data = leds_devp;
    printk ( "<< %s line: %d\n", __func__, __LINE__ );
    return 0;
}


int
leds_release ( struct inode *inode, struct file *file )
{
    /* following is dummy code but might be useful later on */
    struct leds_dev *leds_devp = file->private_data;

    printk ( KERN_DEBUG "leds_release %s line: %d\n", DEVICE_NAME, __LINE__ );
    return 0;
}

ssize_t
leds_read ( struct file * file, char *buf, size_t count, loff_t * ppos )
{
    unsigned char byte;
    struct leds_dev *leds_devp = file->private_data;
    parport_claim_or_block ( leds_devp->pdev );
    byte = parport_read_data ( leds_devp->pdev->port );
    parport_release ( leds_devp->pdev );

    if ( copy_to_user ( buf, &byte, 1 ) )
    {
        printk ( KERN_ERR "copy_from_user failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        return -EFAULT;
    }
    printk ( KERN_DEBUG "byte: 0x%x\n", byte );
    return count;
}


ssize_t
leds_write ( struct file * file, const char *buf, size_t count, loff_t * ppos )
{
    struct leds_dev *leds_devp = file->private_data;
    char kbuf;
    printk ( KERN_DEBUG "leds_write %s line: %d\n", DEVICE_NAME, __LINE__ );
    if ( copy_from_user ( &kbuf, buf, 1 ) )
    {
        printk ( KERN_ERR "copy_from_user failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        return -EFAULT;
    }
    printk ( KERN_DEBUG "leds_write %s line: %d\n", DEVICE_NAME, __LINE__ );
    parport_claim_or_block ( leds_devp->pdev );
    parport_write_data ( leds_devp->pdev->port, kbuf );
    parport_release ( leds_devp->pdev );
    printk ( KERN_DEBUG "leds_write %s line: %d\n", DEVICE_NAME, __LINE__ );
    return count;
}


int __init
leds_init ( void )
{

    int ret = -ENODEV;
    ret = alloc_chrdev_region ( &leds_dev_t_major_number, 0, 1, DEVICE_NAME );
    if ( ret < 0 )
    {
        printk ( KERN_ERR "alloc_chrdev_region failed for %s line: %d\n",
                 DEVICE_NAME, __LINE__ );
        return ret;
    }

    leds_class = class_create ( THIS_MODULE, DEVICE_NAME );
    if ( IS_ERR ( leds_class ) )
    {
        printk ( KERN_ERR "class_create failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        return PTR_ERR ( leds_class );
    }

    leds_devp = kzalloc ( sizeof ( struct leds_dev ), GFP_KERNEL );
    if ( !leds_devp )
    {
        printk ( KERN_ERR "kmalloc failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        leds_devp = NULL;
        return -ENOMEM;
    }

    leds_devp->leds_parport_driver.name = DEVICE_NAME;
    leds_devp->leds_parport_driver.attach = leds_attach;
    leds_devp->leds_parport_driver.detach = leds_detach;

    sprintf ( leds_devp->name, "%s", DEVICE_NAME );
    cdev_init ( &leds_devp->cdev, &leds_fops );
    leds_devp->cdev.owner = THIS_MODULE;

    ret = cdev_add ( &leds_devp->cdev, leds_dev_t_major_number, 1 );
    if ( ret < 0 )
    {
        printk ( KERN_ERR "cdev_add failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        if ( leds_devp )
        {
            cdev_del ( &leds_devp->cdev );
            kfree ( leds_devp );
            leds_devp = NULL;
        }
        return ret;
    }

    leds_devp->leds_device =
        device_create ( leds_class, NULL, leds_dev_t_major_number, NULL,
                        DEVICE_NAME );
    if ( IS_ERR ( leds_devp->leds_device ) )
    {
        printk ( KERN_ERR "device_create failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        if ( leds_devp )
        {
            cdev_del ( &leds_devp->cdev );
            kfree ( leds_devp );
            leds_devp = NULL;
        }
        return PTR_ERR ( leds_devp->leds_device );
    }

    ret = parport_register_driver ( &leds_devp->leds_parport_driver );
    if ( ret )
    {
        printk ( KERN_ERR "parport_register_driver failed for %s line: %d\n",
                 DEVICE_NAME, __LINE__ );
        if ( leds_devp )
        {
            cdev_del ( &leds_devp->cdev );
            device_del ( leds_devp->leds_device );
            kfree ( leds_devp );
            leds_devp = NULL;
        }
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        return ret;
    }

    ret = kobject_init_and_add(&leds_devp->kobj_leds, &leds_ktype, NULL, "%s", "led1");
    if (ret) {
        printk ( KERN_ERR "kobject_create_and_add failed for %s line: %d\n", DEVICE_NAME, __LINE__ );

        if ( leds_devp )
        {
            kobject_put(&leds_devp->kobj_leds);
            parport_unregister_driver ( &leds_devp->leds_parport_driver );
            cdev_del ( &leds_devp->cdev );
            device_del ( leds_devp->leds_device );
            kfree ( leds_devp );
            leds_devp = NULL;
        }
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        return -ENOMEM;
    }
    kobject_uevent(&leds_devp->kobj_leds, KOBJ_ADD);

    printk ( "LEDS module initialized!\n" );

    return ret;

}

void __exit
leds_exit ( void )
{
    printk ( KERN_DEBUG "parport_unregister_driver\n" );
    if ( leds_devp )
    {
        printk ( "LEDS parport_unregister_driver\n" );
        parport_unregister_driver ( &leds_devp->leds_parport_driver );
        printk ( "LEDS char dev delete\n" );
        cdev_del ( &leds_devp->cdev );
        device_del ( leds_devp->leds_device );
        printk ( "LEDS kobject putting\n" );
        kobject_put(&leds_devp->kobj_leds); //kfree ( leds_devp );
        leds_devp = NULL;
    }

    printk ( "unregister_chrdev_region\n" );
    unregister_chrdev_region ( leds_dev_t_major_number, 1 );

    printk ( "class_destroy\n" );
    class_destroy ( leds_class );
    return;
}

module_init ( leds_init );
module_exit ( leds_exit );
