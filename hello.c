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
MODULE_DESCRIPTION( "This is cmos ch5 ;/");
MODULE_AUTHOR("ferar aschkar");
MODULE_VERSION("0.0.1");

struct cmos_dev {
    unsigned short current_pointer;
    unsigned int size;
    int bank_number;
    struct cdev cdev;
    char name[32];
}*cmos_devp;

unsigned char port_data_in(unsigned char offset, int bank);
void port_data_out(unsigned char offset, unsigned char data, int bank);

int cmos_open(struct inode *inode, struct file *file);
int cmos_release(struct inode *inode, struct file *file);
ssize_t cmos_read(struct file *file, char *buf, size_t count, loff_t *ppos);
ssize_t cmos_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
static loff_t cmos_llseek(struct file *file, loff_t offset, int orig);
static int cmos_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

unsigned short adjust_cmos_crc(unsigned short a, unsigned short b)
{
    return a+b;
}

static struct file_operations cmos_fops = {
    .owner = THIS_MODULE,
    .open = cmos_open,
    .release = cmos_release,
    .read = cmos_read,
    .write = cmos_write,
    .llseek = cmos_llseek,
    .unlocked_ioctl = cmos_ioctl,
};

static dev_t cmos_dev_t_major_number;
struct class * cmos_class;

#define NUM_CMOS_BANKS          2
#define CMOS_BANK_SIZE          (0xFF*8)
#define DEVICE_NAME             "cmos"
#define CMOS_BANK0_INDEX_PORT   0x74
#define CMOS_BANK0_DATA_PORT    0x75
#define CMOS_BANK1_INDEX_PORT   0x76
#define CMOS_BANK1_DATA_PORT    0x77

struct cmos_dev *cmos_devs[2] ={ NULL, NULL};

unsigned char addrports[NUM_CMOS_BANKS] = {
    CMOS_BANK0_INDEX_PORT,
    CMOS_BANK1_INDEX_PORT,
};

unsigned char dataports[NUM_CMOS_BANKS] = {
    CMOS_BANK0_DATA_PORT,
    CMOS_BANK1_DATA_PORT,
};


/*
 * Open CMOS bank
 */
int
cmos_open(struct inode *inode, struct file *file)
{
    struct cmos_dev *cmos_devp;
    
    /* Get the per-device structure that contains this cdev */
    cmos_devp = container_of(inode->i_cdev, struct cmos_dev, cdev);
    
    /* Easy access to cmos_devp from rest of the entry points */
    file->private_data = cmos_devp;
    
    /* Initialize some fields */
    cmos_devp->size = CMOS_BANK_SIZE;
    cmos_devp->current_pointer = 0;
    
    return 0;
}

/*
 * Release CMOS bank
 */
int
cmos_release(struct inode *inode, struct file *file)
{
    struct cmos_dev *cmos_devp = file->private_data;
    
    /* Reset file pointer */
    cmos_devp->current_pointer = 0;
    
    return 0;
}

/*
 * Read from a CMOS Bank at bit-level granularity
 */
ssize_t
cmos_read(struct file *file, char *buf,
	  size_t count, loff_t *ppos)
{
    struct cmos_dev *cmos_devp = file->private_data;
    char data[CMOS_BANK_SIZE];
    unsigned char mask;
    int xferred = 0, i = 0, l, zero_out;
    int start_byte = cmos_devp->current_pointer/8;
    int start_bit  = cmos_devp->current_pointer%8;
    
    if (cmos_devp->current_pointer >= cmos_devp->size) {
	return 0; /*EOF*/
    }
    
    /* Adjust count if it edges past the end of the CMOS bank */
    if (cmos_devp->current_pointer + count > cmos_devp->size) {
	count = cmos_devp->size - cmos_devp->current_pointer;
    }
    
    /* Get the specified number of bits from the CMOS */
    while (xferred < count) {
	data[i] = port_data_in(start_byte, cmos_devp->bank_number)
	>> start_bit;
	xferred += (8 - start_bit);
	if ((start_bit) && (count + start_bit > 8)) {
	    data[i] |= (port_data_in (start_byte + 1,
				      cmos_devp->bank_number) << (8 - start_bit));
	    xferred += start_bit;
	}
	start_byte++;
	i++;
    }
    if (xferred > count) {
	/* Zero out (xferred-count) bits from the MSB
	 of the last dat*a byte */
	zero_out = xferred - count;
	mask = 1 << (8 - zero_out);
	for (l=0; l < zero_out; l++) {
	    data[i-1] &= ~mask;
	    mask <<= 1;
	}
	xferred = count;
    }
    
    if (!xferred) return -EIO;
    
    /* Copy the read bits to the user buffer */
    if (copy_to_user(buf, (void *)data, ((xferred/8)+1)) != 0) {
	return -EIO;
    }
    
    /* Increment the file pointer by the number of xferred bits */
    cmos_devp->current_pointer += xferred;
    return xferred; /* Number of bits read */
}


/*
 * Write to a CMOS bank at bit-level granularity. 'count' holds the
 * number of bits to be written.
 */
ssize_t
cmos_write(struct file *file, const char *buf,
	   size_t count, loff_t *ppos)
{
    struct cmos_dev *cmos_devp = file->private_data;
    int xferred = 0, i = 0, l, end_l, start_l;
    char *kbuf, tmp_kbuf;
    unsigned char tmp_data = 0, mask;
    int start_byte = cmos_devp->current_pointer/8;
    int start_bit  = cmos_devp->current_pointer%8;
    
    if (cmos_devp->current_pointer >= cmos_devp->size) {
	return 0; /* EOF */
    }
    /* Adjust count if it edges past the end of the CMOS bank */
    if (cmos_devp->current_pointer + count > cmos_devp->size) {
	count = cmos_devp->size - cmos_devp->current_pointer;
    }
    
    kbuf = kmalloc((count/8)+1,GFP_KERNEL);
    if (kbuf==NULL)
	return -ENOMEM;
    
    /* Get the bits from the user buffer */
    if (copy_from_user(kbuf,buf,(count/8)+1)) {
	kfree(kbuf);
	return -EFAULT;
    }
    
    /* Write the specified number of bits to the CMOS bank */
    while (xferred < count) {
	tmp_data = port_data_in(start_byte, cmos_devp->bank_number);
	mask = 1 << start_bit;
	end_l = 8;
	if ((count-xferred) < (8 - start_bit)) {
	    end_l = (count - xferred) + start_bit;
	}
	
	for (l = start_bit; l < end_l; l++) {
	    tmp_data &= ~mask;
	    mask <<= 1;
	}
	tmp_kbuf = kbuf[i];
	mask = 1 << end_l;
	for (l = end_l; l < 8; l++) {
	    tmp_kbuf &= ~mask;
	    mask <<= 1;
	}
	
	port_data_out(start_byte,
		      tmp_data |(tmp_kbuf << start_bit),
		      cmos_devp->bank_number);
	xferred += (end_l - start_bit);
	
	if ((xferred < count) && (start_bit) &&
	    (count + start_bit > 8)) {
	    tmp_data = port_data_in(start_byte+1,
				    cmos_devp->bank_number);
	    start_l = ((start_bit + count) % 8);
	mask = 1 << start_l;
	for (l=0; l < start_l; l++) {
	    mask >>= 1;
	    tmp_data &= ~mask;
	}
	port_data_out((start_byte+1),
		      tmp_data |(kbuf[i] >> (8 - start_bit)),
		      cmos_devp->bank_number);
	xferred += start_l;
	    }
	    
	    start_byte++;
	    i++;
    }
    
    if (!xferred) return -EIO;
    
    /* Push the offset pointer forward */
    cmos_devp->current_pointer += xferred;
    return xferred; /* Return the number of written bits */
}


/*
 * Read data from specified CMOS bank
 */
unsigned char
port_data_in(unsigned char offset, int bank)
{
    unsigned char data;
    
    if (unlikely(bank >= NUM_CMOS_BANKS)) {
	printk("Unknown CMOS Bank\n");
	return 0;
    } else {
	outb(offset, addrports[bank]); /* Read a byte */
	data = inb(dataports[bank]);
    }
    return data;
    
    
}
/*
 * Write data to specified CMOS bank
 */
void
port_data_out(unsigned char offset, unsigned char data,
	      int bank)
{
    if (unlikely(bank >= NUM_CMOS_BANKS)) {
	printk("Unknown CMOS Bank\n");
	return;
    } else {
	outb(offset, addrports[bank]); /* Output a byte */
	outb(data, dataports[bank]);
    }
    return;
}


/*
 * Seek to a bit offset within a CMOS bank
 */
static loff_t
cmos_llseek(struct file *file, loff_t offset,
	    int orig)
{
    struct cmos_dev *cmos_devp = file->private_data;
    
    switch (orig) {
	case 0: /* SEEK_SET */
	    if (offset >= cmos_devp->size) {
		return -EINVAL;
	    }
	    cmos_devp->current_pointer = offset; /* Bit Offset */
	    break;
	    
	case 1: /* SEEK_CURR */
	    if ((cmos_devp->current_pointer + offset) >=
		cmos_devp->size) {
		return -EINVAL;
		}
		cmos_devp->current_pointer = offset; /* Bit Offset */
		break;
	    
	case 2: /* SEEK_END - Not supported */
	    return -EINVAL;
	    
	default:
	    return -EINVAL;
    }
    
    return(cmos_devp->current_pointer);
}


#define CMOS_ADJUST_CHECKSUM 1
#define CMOS_VERIFY_CHECKSUM 2

#define CMOS_BANK1_CRC_OFFSET 0x1E


/*
 * Ioctls to adjust and verify CRC16s.
 */
static int
cmos_ioctl(struct file *file,
	   unsigned int cmd, unsigned long arg)
{
    unsigned short crc = 0;
    unsigned char buf;
    
    switch (cmd) {
	case CMOS_ADJUST_CHECKSUM:
	    /* Calculate the CRC of bank0 using a seed of 0 */
	    crc = adjust_cmos_crc(0, 0);
	    
	    /* Seed bank1 with CRC of bank0 */
	    crc = adjust_cmos_crc(1, crc);
	    
	    /* Store calculated CRC */
	    port_data_out(CMOS_BANK1_CRC_OFFSET,
			  (unsigned char)(crc & 0xFF), 1);
	    port_data_out((CMOS_BANK1_CRC_OFFSET + 1),
			  (unsigned char) (crc >> 8), 1);
	    break;
	    
	case CMOS_VERIFY_CHECKSUM:
	    /* Calculate the CRC of bank0 using a seed of 0 */
	    crc = adjust_cmos_crc(0, 0);
	    
	    /* Seed bank1 with CRC of bank0 */
	    crc = adjust_cmos_crc(1, crc);
	    
	    /* Compare the calculated CRC with the stored CRC */
	    buf = port_data_in(CMOS_BANK1_CRC_OFFSET, 1);
	    if (buf != (unsigned char) (crc & 0xFF)) return -EINVAL;
	       
	       buf = port_data_in((CMOS_BANK1_CRC_OFFSET+1), 1);
	    if (buf != (unsigned char)(crc >> 8)) return -EINVAL;
	       break;
	default:
	    return -EIO;
    }
    
    return 0;
}

int __init cmos_init(void) {
    int i;
    if (alloc_chrdev_region(&cmos_dev_t_major_number,0, NUM_CMOS_BANKS, DEVICE_NAME) < 0) {
	printk(KERN_DEBUG "Can't register device\n");
	return -1;
    }
    
    cmos_class = class_create(THIS_MODULE, DEVICE_NAME);
    
    for (i=0;i<NUM_CMOS_BANKS;++i) {
	cmos_devs[i] = kmalloc(sizeof(struct cmos_dev), GFP_KERNEL);
	if (!cmos_devs[i]) {
	    printk("failed to kmalloc device: %d!\n",i);
	    for (;i > -1; --i) {
		if (cmos_devs[i])
		    kfree(cmos_devs[i]);
	    }
	    
	    return -1;
	}
	
	sprintf(cmos_devs[i]->name, "cmos%d",i);
	
	// request ioports
	if (!(request_region(addrports[i],2, cmos_devs[i]->name))) {
	    printk("cmos: I/O port 0x%x is not free!\n", addrports[i]);
	    for (;i > -1; --i) {
		if (cmos_devs[i])
		    kfree(cmos_devs[i]);
	    }
	    return -EIO;
	}
	
	cmos_devs[i]->bank_number = i;
	
	cdev_init(&cmos_devs[i]->cdev, &cmos_fops);
	
	cmos_devs[i]->cdev.owner = THIS_MODULE;
	
	if (cdev_add(&cmos_devs[i]->cdev, (cmos_dev_t_major_number+i),1)) {
	    printk("could't add cdev\n");
	    
	    for (;i > -1; --i) {
		if (cmos_devs[i]) {
		    cdev_del(&cmos_devs[i]->cdev);
		    kfree(cmos_devs[i]);
		}
	    }
	    return -1;
	}
	
	device_create(cmos_class, NULL, (cmos_dev_t_major_number + i), NULL, "cmos%d",i);
    }
    
    printk("CMOS module initialized!\n");
    
    return 0;
    
}

void __exit cmos_exit(void)
{
    int i;
    
    for (i=NUM_CMOS_BANKS-1;i > -1; --i) {
	if (cmos_devs[i]) {
	    printk("CMOS module free-ing: %d\n",i);
	    cdev_del(&cmos_devs[i]->cdev);
	    kfree(cmos_devs[i]);
	}
    }
    
    printk("unregister_chrdev_region\n");
    unregister_chrdev_region(cmos_dev_t_major_number, NUM_CMOS_BANKS);
    
    
    for (i=0; i<NUM_CMOS_BANKS;++i) {
	printk("release_region: %d\n",i);
	release_region(addrports[i],2);
    }
    
    printk("class_destroy\n");
    class_destroy(cmos_class);
    return;
}

module_init(cmos_init);
module_exit(cmos_exit);