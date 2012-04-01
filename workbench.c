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
#include <linux/platform_device.h>		/* for platform device/driver */
#include <linux/random.h>               /* add random support */
#include <linux/rcupdate.h>             /* rcu related */

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("This is cache list kthread testbench ;/");
MODULE_AUTHOR("ferar aschkar");
MODULE_VERSION("20.5.0");


/*
 * root@arm:~# echo 1 > /sys/kernel/debug/tracing/tracing_on
 * root@arm:~# cat /sys/kernel/debug/tracing/trace
 * root@arm:~# echo 0 > /sys/kernel/debug/tracing/trace
 */


/* file:///home/ferar/pandaboard_es/linux-kernel-armv7-multiplatform/KERNEL/Documentation/DocBook/kernel-locking/efficiency-read-copy-update.html*/

#define MAX_CACHE_SIZE 1000

static struct task_struct *ptr_my_task_struct1 = NULL;
static struct task_struct *ptr_my_task_struct2 = NULL;
static atomic_t atomic_thread1;
static atomic_t atomic_thread2;
static atomic_t atomic_wait_condition;
DECLARE_WAIT_QUEUE_HEAD(wait_queue_head);
static struct task_struct *ptr_my_task_struct = NULL;
static int gid = 0;

typedef struct {
    atomic_t atomic_contr;
    struct work_struct wrk_strct;
    struct workqueue_struct *ptr_wrk_queue_strct;

} wait_work_struct;

static wait_work_struct *ptr_workstrct_wrkquestrct = NULL;


struct cache_node_struct {
    struct list_head list_node;
    struct rcu_head rcu;
    unsigned int id;
    char *name;
    unsigned int popularity;
};

static LIST_HEAD(head_of_mylist);
static struct cache_node_struct *ptr_cache_node = NULL;

void cache_printo(void)
{
    struct cache_node_struct *ptr_tmp_pos = NULL;

    trace_printk(" \t>>---------------------\n");
    list_for_each_entry_rcu(ptr_tmp_pos, &head_of_mylist, list_node) {
        if (ptr_tmp_pos != NULL) {
            trace_printk(" \tcache_obj:%s id:%u, popularity:%u\n", ptr_tmp_pos->name, ptr_tmp_pos->id, ptr_tmp_pos->popularity);
        }
    }
    trace_printk(" \t<<---------------------\n");
}


static void cache_delete_rcu_callback(struct rcu_head *argo)
{
    struct cache_node_struct *ptr_tmp_pos = container_of(argo, struct cache_node_struct, rcu);
    if (ptr_tmp_pos != NULL) {
        kfree(ptr_tmp_pos);
        ptr_tmp_pos = NULL;
    }
}

int cache_add(const char *nameo)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;

    unsigned int least_popularity = 99999;
    struct cache_node_struct *ptr_tmp_pos = NULL;

//     trace_printk(" >> entering %s nameo:%s\n", __func__, nameo);
    entering = ktime_get();

    ptr_cache_node = kzalloc(sizeof(struct cache_node_struct), GFP_ATOMIC);
    if (IS_ERR(ptr_cache_node)) {
        trace_printk("Error! %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_cache_node));
        return -ENOMEM;
    }

    if (++gid > MAX_CACHE_SIZE) {
        struct cache_node_struct *ptr_cache_node_leastpopular = NULL;

        list_for_each_entry_rcu(ptr_tmp_pos, &head_of_mylist, list_node) {
            if (ptr_tmp_pos != NULL) {
                if (ptr_tmp_pos->popularity < least_popularity) {
                    least_popularity = ptr_tmp_pos->popularity;
                    ptr_cache_node_leastpopular = ptr_tmp_pos;
                }
            }
        }

        --gid;
        ptr_cache_node->id = ptr_cache_node_leastpopular->id;
        ptr_cache_node->popularity++;
        ptr_cache_node->name = (char *)kzalloc(strlen(nameo) + 1, GFP_ATOMIC);
        memset(ptr_cache_node->name, '\0', strlen(nameo) + 1);
        if (IS_ERR(ptr_cache_node->name)) {
            trace_printk("Error! %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_cache_node->name));
            if (ptr_cache_node != NULL) {
                kfree(ptr_cache_node);
                ptr_cache_node = NULL;
            }

            return -ENOMEM;
        }

//         cache_printo();

        list_del_rcu(&ptr_cache_node_leastpopular->list_node);
        call_rcu(&ptr_cache_node_leastpopular->rcu, cache_delete_rcu_callback);

        memcpy(ptr_cache_node->name, nameo, strlen(nameo));

        list_add_tail_rcu(&ptr_cache_node->list_node, &head_of_mylist);

//         cache_printo();


    } else {
        ptr_cache_node->id = gid;
        ptr_cache_node->popularity++;
        ptr_cache_node->name = (char *)kzalloc(strlen(nameo) + 1, GFP_ATOMIC);
        memset(ptr_cache_node->name, '\0', strlen(nameo) + 1);
        if (IS_ERR(ptr_cache_node->name)) {
            trace_printk("Error! %s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_cache_node->name));
            if (ptr_cache_node != NULL) {
                kfree(ptr_cache_node);
                ptr_cache_node = NULL;
            }

            return -ENOMEM;
        }

        memcpy(ptr_cache_node->name, nameo, strlen(nameo));


//         cache_printo();

        list_add_tail_rcu(&ptr_cache_node->list_node, &head_of_mylist);

//         cache_printo();
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
//     trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    return 0;
}

int cache_delete(unsigned int ido)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    int reto = -1;
    struct cache_node_struct *ptr_tmp_pos = NULL;

//     trace_printk(" >> entering %s ido:%u\n", __func__,ido);
    entering = ktime_get();

//     cache_printo();
    list_for_each_entry_rcu(ptr_tmp_pos, &head_of_mylist, list_node) {
        if (ptr_tmp_pos != NULL) {
            if (ptr_tmp_pos->id == ido) {
                list_del_rcu(&ptr_tmp_pos->list_node);
                call_rcu(&ptr_tmp_pos->rcu, cache_delete_rcu_callback);
                reto = 0;
                break;

            }
        }
    }

//     cache_printo();

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
//     trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);

    return reto;
}

struct cache_node_struct *cache_find(unsigned int ido)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    struct cache_node_struct *ptr_tmp_pos = NULL;

//     trace_printk(" >> entering %s ido:%u\n", __func__, ido);
    entering = ktime_get();

    list_for_each_entry_rcu(ptr_tmp_pos, &head_of_mylist, list_node) {
        if (ptr_tmp_pos != NULL) {
            if (ptr_tmp_pos->id == ido) {
                ptr_tmp_pos->popularity++;
                exiting = ktime_get();
                deltao = ktime_to_ns(ktime_sub(exiting, entering));
//                 trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
                return ptr_tmp_pos;
            }
        }
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
//     trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);

    return ptr_tmp_pos;
}


static int my_kthread_func1(void *my_arg)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    unsigned int cntro = 0;
    unsigned int rndm_uint = 0;
    char namebuf[20] = {'\0'};

    // NOTE: printing or ktime_get may sleep in interrupt context
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    while (!kthread_should_stop() && cntro++ < 150) {
        rndm_uint = get_random_int() >> 15;
//         trace_printk("%s:%d, rndm_uint:%u\n", __func__, __LINE__, rndm_uint);
        udelay(rndm_uint);

        /* no waiting for tasklet like kthreads */
//         wait_event_interruptible_timeout(*ptr_wait_queue_head, atomic_read(&atomic_wait_condition)== 1, usecs_to_jiffies(100000));
//         atomic_set(&atomic_wait_condition,0);


        sprintf(namebuf, "%s_%u", "cache_obj1", cntro);
        rcu_read_lock();
        cache_add(namebuf);
        rcu_read_unlock();
    }

    cntro = 0;
    while (!kthread_should_stop() && cntro++ < 130) {
        rndm_uint = get_random_int() & 0x0E;
//         trace_printk("%s:%d, rndm_uint:%u\n", __func__, __LINE__, rndm_uint);
        udelay(rndm_uint);
        sprintf(namebuf, "%s_%u%c", "cache_obj1", cntro, '\0');
        cache_delete(rndm_uint);
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    atomic_set(&atomic_thread1, 1);
    do_exit(0);
}


static int my_kthread_func2(void *my_arg)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    unsigned int cntro = 0;
    unsigned int rndm_uint = 0;
    char namebuf[20] = {'\0'};

    // NOTE: ktime_get may sleep in interrupt context
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();


    while (!kthread_should_stop() && cntro++ < 100) {
        rndm_uint = get_random_int() >> 15;
//         trace_printk("%s:%d, rndm_uint:%u\n", __func__, __LINE__, rndm_uint);
        udelay(rndm_uint);
        sprintf(namebuf, "%s_%u%c", "cache_obj2", cntro, '\0');
        rcu_read_lock();
        cache_add(namebuf);
        rcu_read_unlock();

    }

    cntro = 0;
    while (!kthread_should_stop() && cntro++ < 160) {
        rndm_uint = get_random_int() & 0x0E;
//         trace_printk("%s:%d, rndm_uint:%u\n", __func__, __LINE__, rndm_uint);
        udelay(rndm_uint);
        sprintf(namebuf, "%s_%u", "cache_obj1", cntro);
        rcu_read_lock();
        cache_find(rndm_uint);
        rcu_read_unlock();
    }


    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    atomic_set(&atomic_thread2, 1);
    do_exit(0);
}


static int my_task_func(void *my_arg)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;

    // NOTE: printing or ktime_get may sleep in interrupt context
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    while (!kthread_should_stop()) {
        atomic_set(&atomic_wait_condition, 1);  /* make sure to set condition or it is in vain sending a wakeup signal*/
        wake_up_all(&wait_queue_head);
        msleep(100);
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    return 0;
}


static void work_stuct_functor(struct work_struct *argo_wrk_strct)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    wait_work_struct *ptr_workstrct_wrkquestrct = container_of(argo_wrk_strct, wait_work_struct, wrk_strct);

    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();


    if (ptr_workstrct_wrkquestrct != NULL) {
        while (atomic_read(&ptr_workstrct_wrkquestrct->atomic_contr) < 5) {

            /* check remaining time/signal_pending */
            wait_event_interruptible_timeout(wait_queue_head, atomic_read(&atomic_wait_condition) == 1, usecs_to_jiffies(100000));  /* since kthreads have higher priority work_stuct_functor runs ! */
            atomic_set(&atomic_wait_condition, 0);
            trace_printk("%s:%d, counter:%d\n", __func__, __LINE__, atomic_read(&ptr_workstrct_wrkquestrct->atomic_contr));
            atomic_inc(&ptr_workstrct_wrkquestrct->atomic_contr);
        }
    }

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
}


int __init my_init(void)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    trace_printk(" >> entering %s\n", __func__);
    entering = ktime_get();

    atomic_set(&atomic_thread1, 0);
    atomic_set(&atomic_thread2, 0);



    ptr_workstrct_wrkquestrct = kzalloc(sizeof(wait_work_struct), GFP_KERNEL);
    if (IS_ERR(ptr_workstrct_wrkquestrct)) {
        pr_err("%s:%d, kzalloc error ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_workstrct_wrkquestrct));
        return PTR_ERR(ptr_workstrct_wrkquestrct);
    }

    atomic_set(&ptr_workstrct_wrkquestrct->atomic_contr, 0);

    INIT_WORK(&ptr_workstrct_wrkquestrct->wrk_strct, work_stuct_functor);

    ptr_my_task_struct = kthread_create(my_task_func, NULL, "%s", "my_task_strcto");
    if (ptr_my_task_struct != NULL) {
        wake_up_process(ptr_my_task_struct);
    }


    ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct = create_singlethread_workqueue("my_wrkque_strct");
    if (IS_ERR(ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct)) {
        pr_err("%s:%d, ret:%ld\n", __func__, __LINE__, PTR_ERR(ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct));
        if (ptr_workstrct_wrkquestrct != NULL) {
            kfree(ptr_workstrct_wrkquestrct);
            ptr_workstrct_wrkquestrct = NULL;
        }

        return PTR_ERR(ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct);

    }

    queue_work(ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct, &ptr_workstrct_wrkquestrct->wrk_strct);


    /* create a kthread in initial sleep mode */
    ptr_my_task_struct1 = kthread_create(my_kthread_func1, NULL, "%s", "my_kthreado1");
    if (ptr_my_task_struct1 != NULL)
        wake_up_process(ptr_my_task_struct1);

    /* create a kthread in initial sleep mode */
    ptr_my_task_struct2 = kthread_create(my_kthread_func2, NULL, "%s", "my_kthreado2");
    if (ptr_my_task_struct2 != NULL)
        wake_up_process(ptr_my_task_struct2);




    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);
    return 0;
}

void __exit my_exit(void)
{
    static ktime_t entering, exiting;
    static s64 deltao = 0;
    struct cache_node_struct *ptr_tmp_pos = NULL;
    entering = ktime_get();

    trace_printk("  >> entering %s\n", __func__);


    if (ptr_my_task_struct2 != NULL)
        if (atomic_read(&atomic_thread2) == 0)
            kthread_stop(ptr_my_task_struct2);


    if (ptr_my_task_struct1 != NULL)
        if (atomic_read(&atomic_thread1) == 0)
            kthread_stop(ptr_my_task_struct1);



    while (!list_empty(&head_of_mylist)) {
        ptr_tmp_pos = list_first_entry(&head_of_mylist, struct cache_node_struct, list_node);
        if (ptr_tmp_pos != NULL) {
            list_del_init(&ptr_tmp_pos->list_node);
            kfree(ptr_tmp_pos);
            ptr_tmp_pos = NULL;
        }
    }

    if (ptr_workstrct_wrkquestrct != NULL) {
        cancel_work_sync(&ptr_workstrct_wrkquestrct->wrk_strct);
        if (ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct != NULL) {
            flush_workqueue(ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct);
            destroy_workqueue(ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct);      // calls kfree(ptr_wrk_queue_strct);
            ptr_workstrct_wrkquestrct->ptr_wrk_queue_strct = NULL;
        }

        kfree(ptr_workstrct_wrkquestrct);
        ptr_workstrct_wrkquestrct = NULL;
    }

    kthread_stop(ptr_my_task_struct);

    exiting = ktime_get();
    deltao = ktime_to_ns(ktime_sub(exiting, entering));
    trace_printk(" << exiting %s, time took : %lld ns\n", __func__, deltao);

    return;
}

module_init(my_init);
module_exit(my_exit);


