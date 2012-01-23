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


MODULE_LICENSE ( "Dual BSD/GPL" );
MODULE_DESCRIPTION ( "This is leds fasl5 ;/" );
MODULE_AUTHOR ( "ferar aschkar" );
MODULE_VERSION ( "0.5.2" );


#define DEVICE_NAME     "ledso"

struct led_attr
{
    struct attribute attr;
    ssize_t ( *show ) ( char * );
    ssize_t ( *store ) ( const char *, size_t count );
};


struct leds_dev
{
    char name[32];
    struct cdev cdev;
    struct device *leds_device;
    struct parport_driver leds_parport_driver;
    struct pardevice *pdev;
    struct attribute **leds_attrbts;
    struct kobj_type ktype_leds;
    struct led_attr *leds_attr;
    int lednum;
} *leds_devp;

static ssize_t
store_led ( const char *buffer, size_t count )
{
    unsigned char buf;
    int value;
    sscanf ( buffer, "%d", &value );
    if ( leds_devp != NULL )
    {
        if ( leds_devp->pdev != NULL )
        {
            parport_claim_or_block ( leds_devp->pdev );
            buf = parport_read_data ( leds_devp->pdev->port );
            if ( value )
            {
                parport_write_data ( leds_devp->pdev->port, buf | ( 1 << leds_devp->lednum ) );
            }
            else
            {
                parport_write_data ( leds_devp->pdev->port, buf & ~ ( 1 << leds_devp->lednum ) );
            }
            parport_release ( leds_devp->pdev );
            return count;
        }
    }
    return count;
}

static ssize_t
show_led ( char *buffer )
{
    unsigned char buf;
    if ( leds_devp != NULL )
    {
        if ( leds_devp->pdev != NULL )
        {
            parport_claim_or_block ( leds_devp->pdev );
            buf = parport_read_data ( leds_devp->pdev->port );
            parport_release ( leds_devp->pdev );
            if ( buf & ( 1 << 0 ) )
            {
                return sprintf ( buffer, "ON\n" );
            }
            else
            {
                return sprintf ( buffer, "OFF\n" );
            }
        }
    }
    return sprintf ( buffer, "unknown!\n" );
}

static ssize_t
l_show ( struct kobject *kobj, struct attribute *a, char *buf )
{
    int ret;
    struct led_attr *ledattr = container_of ( a, struct led_attr, attr );
    ret = ledattr->show ? ledattr->show ( buf ) : -EIO;
    return ret;
}

static ssize_t
l_store ( struct kobject *kobj, struct attribute *a, const char *buf,
          size_t count )
{
    int ret;
    struct led_attr *ledattr = container_of ( a, struct led_attr, attr );
    ret = ledattr->store ? ledattr->store ( buf, count ) : -EIO;
    return ret;
}

static struct sysfs_ops leds_sysfs_ops =
{
    .show = l_show,
    .store = l_store,
};


static int
leds_preempt ( void *handle )
{
    return 1;
}

/* Parport attach method */
static void
leds_attach ( struct parport *port )
{
    printk ( KERN_DEBUG "leds_attach %s line: %d\n", DEVICE_NAME, __LINE__ );
    /* Register the parallel LED device with parport */
    leds_devp->pdev = parport_register_device ( port, DEVICE_NAME,
                      leds_preempt, NULL,
                      NULL, 0, NULL );
    if ( leds_devp->pdev == NULL )
        printk ( "Bad register\n" );
    printk ( KERN_DEBUG "leds_attach %s line: %d\n", DEVICE_NAME, __LINE__ );
}

/* Parport detach method */
static void
leds_detach ( struct parport *port )
{
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

    printk ( KERN_DEBUG "leds_open %s line: %d\n", DEVICE_NAME, __LINE__ );

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
    printk ( KERN_DEBUG "leds_open %s line: %d\n", DEVICE_NAME, __LINE__ );
    return 0;
}


int
leds_release ( struct inode *inode, struct file *file )
{
    printk ( KERN_DEBUG "leds_release %s line: %d\n", DEVICE_NAME, __LINE__ );
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
    int i = 0;
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

    leds_devp = kmalloc ( sizeof ( struct leds_dev ), GFP_KERNEL );
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

    leds_devp->leds_attrbts = kmalloc ( sizeof ( struct attribute* ) *9, GFP_KERNEL );
    if ( !leds_devp->leds_attrbts )
    {
        printk ( KERN_ERR "kmalloc failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        kfree ( leds_devp );
        leds_devp = NULL;
        return -ENOMEM;
    }

    leds_devp->leds_attr = kmalloc ( sizeof ( struct led_attr ) * 8, GFP_KERNEL );
    if ( !leds_devp->leds_attr )
    {
        printk ( KERN_ERR "kmalloc failed for %s line: %d\n", DEVICE_NAME,
                 __LINE__ );
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        kfree ( leds_devp->leds_attrbts );
        kfree ( leds_devp );
        leds_devp = NULL;
        return -ENOMEM;
    }

    for ( i=0;i<8;++i )
    {
        leds_devp->leds_attr[i].attr.name = __stringify ( led##i);
        leds_devp->leds_attr[i].attr.mode = 0666;
        leds_devp->leds_attr[i].show = show_led;
        leds_devp->leds_attr[i].store = store_led;
        leds_devp->leds_attrbts[i] = & leds_devp->leds_attr[i].attr;
    }

    leds_devp->leds_attrbts[9] = NULL;

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
            kfree ( leds_devp->leds_attrbts );
            kfree ( leds_devp->leds_attr );
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
            kfree ( leds_devp->leds_attrbts );
            kfree ( leds_devp->leds_attr );
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
            kfree ( leds_devp->leds_attrbts );
            kfree ( leds_devp->leds_attr );
            kfree ( leds_devp );
            leds_devp = NULL;
        }
        unregister_chrdev_region ( leds_dev_t_major_number, 1 );
        class_destroy ( leds_class );
        return ret;
    }

    printk ( "LEDS module initialized!\n" );

    return 0;

}

void __exit
leds_exit ( void )
{
    printk ( KERN_DEBUG "parport_unregister_driver\n" );
    parport_unregister_driver ( &leds_devp->leds_parport_driver );
    if ( leds_devp )
    {
        printk ( "LEDS module free-ing\n" );
        cdev_del ( &leds_devp->cdev );
        device_del ( leds_devp->leds_device );
        kfree ( leds_devp->leds_attrbts );
        kfree ( leds_devp->leds_attr );
        kfree ( leds_devp );
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
