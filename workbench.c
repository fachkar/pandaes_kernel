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
#include <linux/of.h>                   /* for DT parsing et al */
#include <linux/of_gpio.h>              /* for enum of_gpio_flags */

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is cache list kthread testbench ;/");
MODULE_AUTHOR("ferar aschkar");
MODULE_VERSION("03.08.2017");
MODULE_ALIAS("platform:gpo-61");

/*
 * root@arm:~# echo 1 > /sys/kernel/debug/tracing/tracing_on
 * root@arm:~# cat /sys/kernel/debug/tracing/trace
 * root@arm:~# echo 0 > /sys/kernel/debug/tracing/trace
 */

DECLARE_WAIT_QUEUE_HEAD(wait_queue_head);
static struct task_struct *ptr_my_task_struct1 = NULL;
static atomic_t atomic_thread1;

struct gpo_61_data {
    struct platform_device  *ptr_pdev;
    struct gpio_desc *ptr_gpiodesc;
    u8 can_sleep;
};

static int my_kthread_func1(void *my_arg)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
//     unsigned int cntro = 0;
//     u32 temp_idreg = 0x0;
    int rslt = -1;
    unsigned int rndm_uint = 0;

    struct device_node *node_gpo61 = NULL;

    struct gpo_61_data *ptr_gpo61_data = NULL;

    const char *compatible_string = NULL;
    const char *label_string = NULL;

    bool is_compatible = 0, active_low = 0;
    enum of_gpio_flags of_flags;

    u32 gpio_number = 0;

    entering = ktime_get();

    // NOTE: printing or ktime_get may sleep in interrupt context
    trace_printk(" >> entering %s\n", __func__);


    if (my_arg != NULL) {
        ptr_gpo61_data = (struct gpo_61_data *) my_arg;
        node_gpo61 = ptr_gpo61_data->ptr_pdev->dev.of_node;

        rndm_uint = get_random_int() >> 15;
//         trace_printk("%s:%d, rndm_uint:%u\n", __func__, __LINE__, rndm_uint);
        udelay(rndm_uint);

        trace_printk(" %s:%d, node name:%s\n", __func__, __LINE__, of_node_full_name(node_gpo61));

        rslt = of_property_read_string(node_gpo61, "compatible", &compatible_string);
        if (rslt == 0) {
            trace_printk(" %s:%d, string property:%s\n", __func__, __LINE__, compatible_string);
        } else {
            trace_printk(" %s:%d, warnign could not get string property rslt:%d\n", __func__, __LINE__, rslt);
        }

        rslt = of_property_read_string(node_gpo61, "label", &label_string); // perhaps check this string in probe func
        if (rslt == 0) {
            trace_printk(" %s:%d, string property:%s\n", __func__, __LINE__, label_string);
        } else {
            trace_printk(" %s:%d, warnign could not get string property rslt:%d\n", __func__, __LINE__, rslt);
        }

        is_compatible = of_device_is_compatible(node_gpo61, "panda-es-gpo61");
        if (is_compatible) {
            trace_printk(" %s:%d, device compatible with driver :/\n", __func__, __LINE__);
        }

        rslt = of_get_named_gpio_flags(node_gpo61, "outo-gpios", 0, &of_flags);
        if (rslt < 0) {
            trace_printk(" %s: %s warning can't get '%s' DT property\n", __func__, node_gpo61->name, "outo-gpios");
        }

        active_low = of_flags & OF_GPIO_ACTIVE_LOW;

        rslt = of_property_read_u32(node_gpo61, "gpio_number", &gpio_number);
        if (rslt == 0) {
            trace_printk(" %s:%d, gpio_number property:%u\n", __func__, __LINE__, gpio_number);

            if (gpio_is_valid(gpio_number)) {
                trace_printk(" %s: gpio_%u is valid\n", __func__, gpio_number);

                /* earlier fromprobe we know that we have a valid desc to the DT node */

                while (!kthread_should_stop()) {

                        gpiod_set_value_cansleep(ptr_gpo61_data->ptr_gpiodesc, 1);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(100));


                        gpiod_set_value_cansleep(ptr_gpo61_data->ptr_gpiodesc, 0);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(100));

                        gpiod_set_value_cansleep(ptr_gpo61_data->ptr_gpiodesc, 1);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(100));

                        gpiod_set_value_cansleep(ptr_gpo61_data->ptr_gpiodesc, 0);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(1000));
                    }



            } else {
                trace_printk("%s: warning gpio_%u is NOT valid\n", __func__, gpio_number);
            }

        } else {
            trace_printk(" %s:%d, warnign could not get gpio_number rslt:%d\n", __func__, __LINE__, rslt);
        }



        /* /home/ferar/Downloads/OMAP4460_ES.1x_PUBLIC_TRM_vM.pdf
         * page 3829
         * Table 18-387. CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0
         */


        /* drivers/gpio/gpiolib-legacy.c */
        /* All the same, a GPIO number passed to
                gpio_to_desc() must have been properly acquired, and usage of the returned GPIO
                descriptor is only possible after the GPIO number has been released. */
        /*
        rslt = gpio_request_one(61, GPIOF_OUT_INIT_LOW, "gpio-61");

        if (rslt == 0) {
            pgpio_desc =  gpio_to_desc(61);
            if (!IS_ERR(pgpio_desc)) {

                gpiod_direction_output(pgpio_desc, 1);
                CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0 = ioremap(0x4a100088, 0x020);
                if (CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0 != NULL) {
                    temp_idreg = ioread32(CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0);
                    trace_printk(" %s, temp_idreg : 0x%x\n", __func__, temp_idreg);

                    iowrite32(0x11b0003, CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0);

                    while (!kthread_should_stop() && cntro++ < 500) {

                        gpiod_set_value_cansleep(pgpio_desc, 1);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(100));


                        gpiod_set_value_cansleep(pgpio_desc, 0);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(100));

                        gpiod_set_value_cansleep(pgpio_desc, 1);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(100));

                        gpiod_set_value_cansleep(pgpio_desc, 0);
                        wait_event_interruptible_timeout(wait_queue_head, 0 == 1, msecs_to_jiffies(1000));
                    }



                    iounmap(CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0);
                }


            } else {
                trace_printk(" %s, error:%ld\n", __func__, PTR_ERR(pgpio_desc));
            }

            gpio_free(61);

        } else {
            trace_printk(" %s warning: could not ioremap CONTROL_CORE_PAD0_GPMC_NBE1_PAD1_GPMC_WAIT0=0x4a100088!\n", __func__);
        }

        */
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    atomic_set(&atomic_thread1, 1);
    do_exit(0);
}




static int gpo_61_probe(struct platform_device *pdev)
{
    struct gpo_61_data *ptr_gpo61_data = NULL;
    static ktime_t entering, exiting;
    static s64 deltao = 0;


    entering = ktime_get();
    trace_printk(" >> entering %s\n", __func__);

    ptr_gpo61_data = kzalloc(sizeof(struct gpo_61_data), GFP_KERNEL);
    if (IS_ERR(ptr_gpo61_data)) {
        trace_printk("%s:%d, Error in kzalloc ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_gpo61_data));
        return -ENOMEM; // or  PTR_ERR(ptr_gpo61_data)
    }

    ptr_gpo61_data->ptr_gpiodesc = devm_gpiod_get(&pdev->dev, "outo", GPIOD_OUT_LOW); //  tek bir cagiriyla get gpio desc via its func name as per gpio-beeper.c (probe), more at Documentation gpio/board.txt, diger bir yol ( iki cagiriyla) as per leds-gpio.c (probe, create_gpio_led) */
    if (IS_ERR(ptr_gpo61_data->ptr_gpiodesc)) {
        trace_printk("%s:%d, Error in devm_gpiod_get ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_gpo61_data->ptr_gpiodesc));
        return PTR_ERR(ptr_gpo61_data->ptr_gpiodesc);

    }

    ptr_gpo61_data->ptr_pdev = pdev;

    platform_set_drvdata(pdev, ptr_gpo61_data);

    atomic_set(&atomic_thread1, 0);

    /* create a kthread in initial sleep mode */
    ptr_my_task_struct1 = kthread_create(my_kthread_func1, ptr_gpo61_data, "%s", "my_kthreado1");

    if (ptr_my_task_struct1 != NULL)
        wake_up_process(ptr_my_task_struct1);

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);

    return 0;
}

static int gpo_61_remove(struct platform_device *pdev)
{
    struct gpo_61_data *ptr_gpo61_data = platform_get_drvdata(pdev);

    static ktime_t entering, exiting;
    static s64 deltao = 0;
    entering = ktime_get();

    trace_printk("  >> entering %s\n", __func__);

    if (IS_ERR(ptr_gpo61_data)) {
        trace_printk("%s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_gpo61_data));
        return -EINVAL; // something went terribly wrong!
    }

    devm_gpiod_put(&pdev->dev, ptr_gpo61_data->ptr_gpiodesc);   /* freeing as per gpio/consumer.txt */

    kfree(ptr_gpo61_data);

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);

    return 0;


}


static const struct of_device_id of_gpo_61_match[] = {
    { .compatible = "panda-es-gpo61", },
    {},
};

MODULE_DEVICE_TABLE(of, of_gpo_61_match);

static struct platform_driver gpo_61_driver = {
    .probe		= gpo_61_probe,
    .remove	    = gpo_61_remove,
    .driver		= {
        .name	= "gpo-61-driver",
        .owner  = THIS_MODULE,
//         .probe_type = PROBE_PREFER_ASYNCHRONOUS,
        .of_match_table = of_gpo_61_match,
    },
};

int __init my_init(void)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    int ret = -1;

    entering = ktime_get();
    trace_printk(" >> entering %s\n", __func__);

    /* Registering platform driver with platform core bus */
    ret = platform_driver_register(&gpo_61_driver);
    if (ret != 0) {
        trace_printk(" %s, Error  registering platform driver!\n", __func__);
        return ret;

    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    return 0;
}

void __exit my_exit(void)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    entering = ktime_get();

    trace_printk("  >> entering %s\n", __func__);

    /* Unregistering platform driver from Kernel */
    platform_driver_unregister(&gpo_61_driver);


    if (ptr_my_task_struct1 != NULL)
        if (atomic_read(&atomic_thread1) == 0)
            kthread_stop(ptr_my_task_struct1);


    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);

    return;
}

module_init(my_init);
module_exit(my_exit);



