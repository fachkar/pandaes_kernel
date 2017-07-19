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

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is  jprobe testbench ;/");
MODULE_AUTHOR("ferar aschkar");
MODULE_VERSION("10.4.0");


/* NOTE: http://www.ibm.com/developerworks/library/l-kprobes/index.html */
/* NOTE: http://www.tldp.org/HOWTO/KernelAnalysis-HOWTO-14.html */
static struct jprobe  *ptr_my_jp = NULL;
static struct jprobe  *ptr_my_jp2 = NULL;

/* Proxy routine having the same arguments as actual gpiod_request() routine */
static long jp_my_callback(struct gpio_chip *chip, unsigned offset, int value)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    trace_printk("    %s, offset:%u, value:%d\n", __func__, offset, value);
    trace_printk("    %s, chip -- label:%s, base:%d, ngpio:%u\n", __func__, chip->label, chip->base, chip->ngpio);

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    /* Always end with a call to jprobe_return(). */
    jprobe_return();
    return 0;
}


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

static long jp_my2_callback(struct gpio_bank *bank, unsigned offset, int enable)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    trace_printk("    %s, offset:%u, enable:%d\n", __func__, offset, enable);
    if (bank->base != NULL)
        trace_printk("    %s, bank->base:%p\n", __func__, bank->base);
    trace_printk("    %s, bank->irq:%d, bank->saved_datain:%u\n", __func__, bank->irq, bank->saved_datain);
    trace_printk("    %s, bank->level_mask:%u, bank->toggle_mask:%u\n", __func__, bank->level_mask, bank->toggle_mask);
    trace_printk("    %s, bank->mod_usage:%u, bank->irq_usage:%u, bank->dbck_enable_mask:%u\n", __func__, bank->mod_usage, bank->irq_usage, bank->dbck_enable_mask);

//     trace_printk("    %s, chip -- can sleep:%d\n", __func__, chip->can_sleep);
//     trace_printk("    %s, bgpio_data:%lu, bgpio_dir:%lu, bgpio_bits:%d  \n", __func__, chip->bgpio_data, chip->bgpio_dir, chip->bgpio_bits);
//     if (chip->request != NULL)
//         trace_printk("    %s, chip -- request:%pF @ %p\n", __func__, chip->request, chip->request);
    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    /* Always end with a call to jprobe_return(). */
    jprobe_return();
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


    ptr_my_jp2 = kzalloc(sizeof(struct jprobe), GFP_KERNEL);
    if (IS_ERR(ptr_my_jp2)) {
        trace_printk("warning %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_my_jp2));
        if (ptr_my_jp != NULL) {
            kfree(ptr_my_jp);
            ptr_my_jp = NULL;
        }
        return -ENOMEM; // or  PTR_ERR(ptr_my_jp2)
    }


    ptr_my_jp->entry = jp_my_callback;
    /* specify the address/offset where you want to insert probe.
     * You can get the address using one of the methods described above.
     */
    ptr_my_jp->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("omap_gpio_set");

    /* check if the kallsyms_lookup_name() returned the correct value.
     */
    if (ptr_my_jp->kp.addr == NULL) {
        trace_printk("warning kallsyms_lookup_name could not find address for the specified symbol %s\n", "omap_gpio_set");
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




    ptr_my_jp2->entry = jp_my2_callback;
    /* specify the address/offset where you want to insert probe.
     * You can get the address using one of the methods described above.
     */
    ptr_my_jp2->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("omap_set_gpio_dataout_reg");

    /* check if the kallsyms_lookup_name() returned the correct value.
     */
    if (ptr_my_jp2->kp.addr == NULL) {
        trace_printk("warning kallsyms_lookup_name could not find address for the specified symbol %s\n", "omap_set_gpio_dataout_reg");
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

