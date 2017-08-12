#include <linux/kprobes.h> /* for kprobe dynamically inserting printks */
#include <linux/module.h>               /* Needed by all modules */
#include <linux/kernel.h>               /* Needed for printing .. */
#include <linux/slab.h>                 /* Needed for kmalloc .. */
#include <linux/time.h>                 /* Needed for wall time .. */
#include <linux/hrtimer.h>              /* Needed for hrtimer .. */
#include <linux/ktime.h>                /* Needed for ktime obj .. */
#include <linux/list.h>                 /* Needed for list_head obj .. */
#include <linux/wait.h>                 /* Needed for waitqueue .. */
#include <linux/sched.h>                /* Needed for tasklet scheduling .. */
#include <linux/kthread.h>              /* Needed for kthread */
#include <linux/delay.h>                /* Needed for msleep */
#include <linux/completion.h>           /* Needed for completion */
#include <linux/io.h>                   /* Needed for ioremap */
#include <linux/hashtable.h>            /* Needed for hlist */
#include <linux/kfifo.h>                /* Needed for kfifo */
#include <linux/log2.h>                 /* Needed for roundup_pow_of_two */
#include <linux/rbtree.h>               /* Needed for Red-Black tree */
#include <linux/gpio/consumer.h>        /* this is required to be gpiolib consumer */
#include <linux/gpio.h>
#include <linux/platform_device.h>		/* for platform device/driver */
#include <linux/random.h>               /* add random support */
#include <linux/rcupdate.h>             /* rcu related */
#include "../../drivers/gpio/gpiolib.h" /* ../../ from current location of kprobeo.c !*/
#include <linux/kallsyms.h>
#include "../../drivers/base/base.h"
#include <linux/async.h>
#include "../../drivers/pinctrl/core.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is  jprobe testbench ;/");
MODULE_AUTHOR("ferar aschkar");
MODULE_VERSION("10.4.0");


/* NOTE: http://www.ibm.com/developerworks/library/l-kprobes/index.html */
/* NOTE: http://www.tldp.org/HOWTO/KernelAnalysis-HOWTO-14.html */
static struct jprobe  *ptr_my_jp = NULL;
static struct jprobe  *ptr_my_jp2 = NULL;
static struct kretprobe *ptr_my_kretp = NULL;
static const char my_func_name[] = "driver_probe_device";
static const char my_func2_name[] = "__driver_attach";
struct gpio_regs {
    u32 irqenable1;
    u32 irqenable2;
    u32 wake_en;
    u32 ctrl;
    u32 oe;
    u32 leveldetect0;
    u32 leveldetect1;
    u32 risingdetect;
    u32 fallingdetect;
    u32 dataout;
    u32 debounce;
    u32 debounce_en;
};


struct gpio_bank {
    struct list_head node;
    void __iomem *base;
    int irq;
    u32 non_wakeup_gpios;
    u32 enabled_non_wakeup_gpios;
    struct gpio_regs context;
    u32 saved_datain;
    u32 level_mask;
    u32 toggle_mask;
    raw_spinlock_t lock;
    raw_spinlock_t wa_lock;
    struct gpio_chip chip;
    struct clk *dbck;
    u32 mod_usage;
    u32 irq_usage;
    u32 dbck_enable_mask;
    bool dbck_enabled;
    bool is_mpuio;
    bool dbck_flag;
    bool loses_context;
    bool context_valid;
    int stride;
    u32 width;
    int context_loss_count;
    int power_mode;
    bool workaround_enabled;

    void (*set_dataout)(struct gpio_bank *bank, unsigned gpio, int enable);
    int (*get_context_loss_count)(struct device *dev);

    struct omap_gpio_reg_offs *regs;
};

/* Proxy routine having the same arguments as actual gpiod_request() routine */
static long jp_my_callback(struct device_driver *drv, struct device *dev)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", my_func_name);
    entering = ktime_get();

    if (drv->probe != NULL)
        trace_printk("    %s, probe:%pF @ %p\n", my_func_name, drv->probe, drv->probe);

    trace_printk("    %s, drivers_autoprobe:%d\n", my_func_name, drv->bus->p->drivers_autoprobe);

    trace_printk("    %s, name: %s, probe_type:%d\n", my_func_name, drv->name, drv->probe_type);

    if (dev->of_node != NULL)
        trace_printk("   of_node - name:%s, type:%s, full_name:%s\n", dev->of_node->name, dev->of_node->type, dev->of_node->full_name);


    if (dev != NULL)
        trace_printk("   device init_name:%s, state_in_sysfs:%d\n", dev->init_name, dev->kobj.state_in_sysfs);

    if (dev->type != NULL)
        trace_printk("          type-name:%s\n",   dev->type->name);

    if (dev->driver != NULL)
        trace_printk("          drv-name:%s\n", dev->driver->name);



    if (dev->pins != NULL)
        if (dev->pins->p != NULL)
            if (dev->pins->p->dev != NULL) {
                trace_printk("   pinctrl device init_name:%s\n", dev->pins->p->dev->init_name);

                if (dev->pins->p->dev->type != NULL)
                    trace_printk("           device type-name:%s\n",   dev->pins->p->dev->type->name);

                if (dev->pins->p->dev->driver != NULL)
                    trace_printk("           device drv-name:%s\n", dev->pins->p->dev->driver->name);
            }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", my_func_name, deltao);
    /* Always end with a call to jprobe_return(). */
    jprobe_return();
    return 0;
}

static long jp_my2_callback(struct device *dev, void *data)
{
    struct device_driver *drv = data;
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", my_func2_name);
    entering = ktime_get();


    if (drv->probe != NULL)
        trace_printk("    %s, probe:%pF @ %p\n", my_func2_name, drv->probe, drv->probe);

    if (drv->bus != NULL)
        trace_printk("    %s, match:%pF @ %p\n", my_func2_name, drv->bus->match, drv->bus->match);

    trace_printk("    %s, drivers_autoprobe:%d\n", my_func2_name, drv->bus->p->drivers_autoprobe);

    trace_printk("    %s, name: %s, probe_type:%d\n", my_func2_name, drv->name, drv->probe_type);

    if (dev->of_node != NULL)
        trace_printk("   of_node - name:%s, type:%s, full_name:%s\n", dev->of_node->name, dev->of_node->type, dev->of_node->full_name);

    if (dev->parent != NULL)
        trace_printk("   parent - init_name:%s, mutex:%p, locked:%d\n", dev->parent->init_name, &dev->parent->mutex, mutex_is_locked(&dev->parent->mutex));

    if (dev != NULL)
        trace_printk("   device init_name:%s, state_in_sysfs:%d, mutex:%p\n", dev->init_name, dev->kobj.state_in_sysfs, &dev->mutex);

    if (dev->type != NULL)
        trace_printk("          type-name:%s\n",   dev->type->name);

    if (dev->driver != NULL)
        trace_printk("          drv-name:%s\n", dev->driver->name);
    else
        trace_printk("     *** dev->driver = NULL should call driver_probe_device ***\n");



    if (dev->pins != NULL)
        if (dev->pins->p != NULL)
            if (dev->pins->p->dev != NULL) {
                trace_printk("   pinctrl device init_name:%s\n", dev->pins->p->dev->init_name);

                if (dev->pins->p->dev->type != NULL)
                    trace_printk("           device type-name:%s\n",   dev->pins->p->dev->type->name);

                if (dev->pins->p->dev->driver != NULL)
                    trace_printk("           device drv-name:%s\n", dev->pins->p->dev->driver->name);
            }

//     if (chip->request != NULL)
//         trace_printk("    %s, chip -- request:%pF @ %p\n", my_func2_name, chip->request, chip->request);
    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", my_func2_name, deltao);
    /* Always end with a call to jprobe_return(). */
    jprobe_return();
    return 0;
}


static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    unsigned long retval = regs_return_value(regs);
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    trace_printk("%s returned %lu\n", ri->rp->kp.symbol_name, retval);

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);


    return 0;
}



int __init my_probe_init(void)
{
    int ret;
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    ptr_my_jp = kzalloc(sizeof(struct jprobe), GFP_KERNEL);
    if (IS_ERR(ptr_my_jp)) {
        trace_printk("warning %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_my_jp));
        return -ENOMEM; // or  PTR_ERR(ptr_my_jp)
    }

    ptr_my_jp->entry = jp_my_callback;
    /* specify the address/offset where you want to insert probe.
     * You can get the address using one of the methods described above.
     */
    ptr_my_jp->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name(my_func_name);

    /* check if the kallsyms_lookup_name() returned the correct value.
     */
    if (ptr_my_jp->kp.addr == NULL) {
        trace_printk("warning kallsyms_lookup_name could not find address for the specified symbol %s\n", my_func_name);
        if (ptr_my_jp != NULL) {
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }
        return -ENXIO;
    }

    /* All set to register with Kprobes
    */
    ret = register_jprobe(ptr_my_jp);
    if (ret < 0) {
        trace_printk("error register_kprobe failed, returned %d\n", ret);
        if (ptr_my_jp != NULL) {
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }
        return ret;
    }

    /// ptr_my_jp2


    ptr_my_jp2 = kzalloc(sizeof(struct jprobe), GFP_KERNEL);
    if (IS_ERR(ptr_my_jp2)) {
        trace_printk("warning %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_my_jp2));
        if (ptr_my_jp != NULL) {
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }
        return -ENOMEM; // or  PTR_ERR(ptr_my_jp2)
    }


    ptr_my_jp2->entry = jp_my2_callback;
    /* specify the address/offset where you want to insert probe.
     * You can get the address using one of the methods described above.
     */
    ptr_my_jp2->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name(my_func2_name);

    /* check if the kallsyms_lookup_name() returned the correct value.
     */
    if (ptr_my_jp2->kp.addr == NULL) {
        trace_printk("warning kallsyms_lookup_name could not find address for the specified symbol %s\n", my_func2_name);
        if (ptr_my_jp != NULL) {
            unregister_jprobe(ptr_my_jp);
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }

        if (ptr_my_jp2 != NULL) {
            kfree(ptr_my_jp2);
            ptr_my_jp2 = NULL;
        }
        return -ENXIO;
    }

    /* All set to register with Kprobes
    */
    ret = register_jprobe(ptr_my_jp2);
    if (ret < 0) {
        trace_printk("error register_kprobe failed, returned %d\n", ret);
        if (ptr_my_jp != NULL) {
            unregister_jprobe(ptr_my_jp);
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }

        if (ptr_my_jp2 != NULL) {
            kfree(ptr_my_jp2);
            ptr_my_jp2 = NULL;
        }
        return ret;
    }


    /// ptr_my_kretp

    ptr_my_kretp = kzalloc(sizeof(struct kretprobe), GFP_KERNEL);
    if (IS_ERR(ptr_my_kretp)) {
        trace_printk("warning %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_my_kretp));
        if (ptr_my_jp != NULL) {
            unregister_jprobe(ptr_my_jp);
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }

        if (ptr_my_jp2 != NULL) {
            unregister_jprobe(ptr_my_jp2);
            kfree(ptr_my_jp2);
            ptr_my_jp2 = NULL;
        }

        return -ENOMEM; // or  PTR_ERR(ptr_my_jp2)
    }

    ptr_my_kretp->kp.symbol_name = "platform_match";
    ptr_my_kretp->handler = ret_handler;
    ptr_my_kretp->maxactive = 5;
    ret = register_kretprobe(ptr_my_kretp);
    if (ret < 0) {
        trace_printk("warning %s:%d, register_kretprobe failed, returned %d\n", __func__, __LINE__, ret);
        if (ptr_my_jp != NULL) {
            unregister_jprobe(ptr_my_jp);
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }

        if (ptr_my_jp2 != NULL) {
            unregister_jprobe(ptr_my_jp2);
            kfree(ptr_my_jp2);
            ptr_my_jp2 = NULL;
        }

        if (ptr_my_kretp != NULL) {
            kfree(ptr_my_kretp);
            ptr_my_kretp = NULL;
        }
        return ret;
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    return 0;


}

void __exit my_probe_exit(void)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    entering = ktime_get();
    trace_printk("  >> entering %s\n", __func__);

    if (ptr_my_kretp != NULL) {
        unregister_kretprobe(ptr_my_kretp);
        kfree(ptr_my_kretp);
        ptr_my_kretp = NULL;
    }

    if (ptr_my_jp2 != NULL) {

        unregister_jprobe(ptr_my_jp2);
        kfree(ptr_my_jp2);
        ptr_my_jp2 = NULL;
    }


    if (ptr_my_jp != NULL) {

        unregister_jprobe(ptr_my_jp);
        kfree(ptr_my_jp);
        ptr_my_jp = NULL;
    }

    trace_printk("  << exiting %s\n", __func__);
    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    return;
}

module_init(my_probe_init);
module_exit(my_probe_exit);

