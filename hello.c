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

#include <asm/system.h>         /* cli(), *_flags */
#include <asm/uaccess.h>        /* copy_*_user */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>


MODULE_LICENSE ( "Dual BSD/GPL" );
MODULE_DESCRIPTION( "This is leds ch5 ;/");
MODULE_AUTHOR("ferar aschkar");
MODULE_VERSION("0.5.2");

struct leds_dev {
    char  name[32];
    struct cdev cdev;
}*leds_devp;

unsigned char port_data_in(unsigned char offset, int bank);
void port_data_out(unsigned char offset, unsigned char data, int bank);

int leds_open(struct inode *inode, struct file *file);
int leds_release(struct inode *inode, struct file *file);
// ssize_t leds_read(struct file *file, char *buf, size_t count, loff_t *ppos);
ssize_t leds_write(struct file *file, const char *buf, size_t count, loff_t *ppos);

static struct file_operations leds_fops = {
    .owner = THIS_MODULE,
    .open = leds_open,
    .release = leds_release,
    .write = leds_write,
};

static dev_t leds_dev_t_major_number;
struct class * leds_class;

#define DEVICE_NAME	"leds"


int
leds_open(struct inode *inode, struct file *file)
{
    struct leds_dev *leds_devp;
    
    /* Get the per-device structure that contains this cdev */
    leds_devp = container_of(inode->i_cdev, struct leds_dev, cdev);
    
    /* Easy access to cmos_devp from rest of the entry points */
    file->private_data = leds_devp;
    
    return 0;
}


int
leds_release(struct inode *inode, struct file *file)
{
    /* following is dummy code but might be useful later on*/
    struct leds_dev *leds_devp = file->private_data;
    
    return 0;
}

// ssize_t
// leds_read(struct file *file, char *buf,
// 	  size_t count, loff_t *ppos)
// {
//     struct cmos_dev *cmos_devp = file->private_data;
//     char data[CMOS_BANK_SIZE];
//     unsigned char mask;
//     int xferred = 0, i = 0, l, zero_out;
//     int start_byte = cmos_devp->current_pointer/8;
//     int start_bit  = cmos_devp->current_pointer%8;
//     
//     if (cmos_devp->current_pointer >= cmos_devp->size) {
// 	return 0; /*EOF*/
//     }
//     
//     /* Adjust count if it edges past the end of the CMOS bank */
//     if (cmos_devp->current_pointer + count > cmos_devp->size) {
// 	count = cmos_devp->size - cmos_devp->current_pointer;
//     }
//     
//     /* Get the specified number of bits from the CMOS */
//     while (xferred < count) {
// 	data[i] = port_data_in(start_byte, cmos_devp->bank_number)
// 	>> start_bit;
// 	xferred += (8 - start_bit);
// 	if ((start_bit) && (count + start_bit > 8)) {
// 	    data[i] |= (port_data_in (start_byte + 1,
// 				      cmos_devp->bank_number) << (8 - start_bit));
// 	    xferred += start_bit;
// 	}
// 	start_byte++;
// 	i++;
//     }
//     if (xferred > count) {
// 	/* Zero out (xferred-count) bits from the MSB
// 	 of the last dat*a byte */
// 	zero_out = xferred - count;
// 	mask = 1 << (8 - zero_out);
// 	for (l=0; l < zero_out; l++) {
// 	    data[i-1] &= ~mask;
// 	    mask <<= 1;
// 	}
// 	xferred = count;
//     }
//     
//     if (!xferred) return -EIO;
//     
//     /* Copy the read bits to the user buffer */
//     if (copy_to_user(buf, (void *)data, ((xferred/8)+1)) != 0) {
// 	return -EIO;
//     }
//     
//     /* Increment the file pointer by the number of xferred bits */
//     cmos_devp->current_pointer += xferred;
//     return xferred; /* Number of bits read */
// }


ssize_t
cmos_write(struct file *file, const char *buf,
	   size_t count, loff_t *ppos)
{
    struct leds_dev *leds_devp = file->private_data;
    
    return xferred; /* Return the number of written bits */
}


int __init leds_init(void) {
    int i;
    if (alloc_chrdev_region(&leds_dev_t_major_number,0, 1, DEVICE_NAME) < 0) {
	printk(KERN_DEBUG "Can't register device\n");
	return -1;
    }
    
    leds_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(leds_class)) {
	printk("Bad class create\n");
	return -1;
    }

    leds_devp = kmalloc(sizeof(struct leds_dev), GFP_KERNEL);
    if (!leds_devp) {
	printk("failed to kmalloc device leds_devp!\n");
	return -1;
    }
    
    leds_devp->name = DEVICE_NAME;
    cdev_init(&leds_devp->cdev, &leds_fops);
    leds_devp->cdev.owner = THIS_MODULE;

    if (cdev_add(&leds_devp->cdev, leds_dev_t_major_number,1)) {
	printk("could't add cdev\n");
	
	if (leds_devp) {
	    cdev_del(&leds_devp->cdev);
	    kfree(leds_devp);
	}
	return -1;
    }
    
    device_create(leds_class, NULL, leds_dev_t_major_number , NULL, DEVICE_NAME);
    
    printk("LEDS module initialized!\n");
    
    return 0;
    
}

void __exit cmos_exit(void)
{
    if (leds_devp) {
	printk("LEDS module free-ing\n");
	cdev_del(&leds_devp->cdev);
	kfree(leds_devp);
    }
    
    printk("unregister_chrdev_region\n");
    unregister_chrdev_region(leds_dev_t_major_number, 1);
    
    printk("class_destroy\n");
    class_destroy(leds_class);
    return;
}

module_init(cmos_init);
module_exit(cmos_exit);