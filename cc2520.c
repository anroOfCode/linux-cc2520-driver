#include <linux/module.h>  
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>

#define DRIVER_AUTHOR "Andrew Robinson <androbin@umich.edu>"
#define DRIVER_DESC   "A driver for the CC2520 radio. Be afraid."


int irqNumber = 0;
int pinValue = 0;

struct hrtimer utimer;

static enum hrtimer_restart callbackFunc(struct hrtimer *timer)
{
    ktime_t kt;

    gpio_set_value(23, pinValue == 1);
    pinValue = !pinValue;

    // Create a 100uS time period.
    kt=ktime_set(0,100000);
    hrtimer_forward_now(&utimer, kt);

    return HRTIMER_RESTART;
}


static irqreturn_t handler(int irq, void *dev_id) 
{
    gpio_set_value(23, pinValue == 1);
    pinValue = !pinValue;
    printk(KERN_INFO "TIMER INTERRUPTED %d\n", pinValue);
    return IRQ_HANDLED;
}

int init_module()
{
    ktime_t kt;
    int err = 0;

    printk(KERN_INFO "Hello world 1.\n");

    //////////////////////////
    // GPIO Interrupt Init
    err = gpio_request_one(22, GPIOF_DIR_IN, NULL);
    err = gpio_request_one(23, GPIOF_DIR_OUT, NULL);
    printk(KERN_INFO "Requesting GPIO Pin 22: %d\n", err);

    irqNumber = gpio_to_irq(22);
    printk(KERN_INFO "Requesting IRQ: %d\n", irqNumber);

    if (irqNumber < 0) {
        printk("Unable to get irq number.");
        return 1;
    }

    request_irq(irqNumber, handler, IRQF_TRIGGER_FALLING, "theInterrupt", NULL);

    ////////////////////////
    // HFTimer Test
    // Create a 100uS time period.
    
    kt=ktime_set(10,100000);

    //printk(KERN_ALERT "HRTTIMER inserted\n");

    //utimer = (struct hrtimer *)kmalloc(sizeof(struct hrtimer),GFP_KERNEL);
    

    hrtimer_init(&utimer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);

    utimer.function = &callbackFunc; // callback

    hrtimer_start(&utimer, kt, HRTIMER_MODE_REL);

    //printk(KERN_ALERT "HRTTIMER STARTED\n");

    return 0;
}

void cleanup_module()
{
    free_irq(irqNumber, NULL);
    gpio_free(22);
    gpio_free(23);
    hrtimer_cancel(&utimer); //

    printk(KERN_INFO "See ya.\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
