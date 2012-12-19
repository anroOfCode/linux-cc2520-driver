#include <linux/module.h>  
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>

#include "cc2520.h"

#define DRIVER_AUTHOR "Andrew Robinson <androbin@umich.edu>"
#define DRIVER_DESC   "A driver for the CC2520 radio. Be afraid."

struct cc2520_state state;

int irqNumber = 0;
int pinValue = 0;

struct hrtimer utimer;

//////////////////////////
// Interrupt Handles
/////////////////////////

static irqreturn_t cc2520_sfdHandler(int irq, void *dev_id) 
{
    printk(KERN_INFO "[CC2520] - sfd interrupt occurred");
    return IRQ_HANDLED;
}

static irqreturn_t cc2520_fifopHandler(int irq, void *dev_id) 
{
    printk(KERN_INFO "[CC2520] - fifop interrupt occurred");
    return IRQ_HANDLED;
}

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

//////////////////////////////
// Interface Initialization
//////////////////////////////

// Sets up the GPIO pins needed for the CC2520
// and initializes any interrupt handlers needed.
int cc2520_setupGpioPins()
{
    int err = 0;
    int irq = 0;

    // Setup GPIO In/Out
    err = gpio_request_one(CC2520_FIFO, GPIOF_DIR_IN, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_FIFOP, GPIOF_DIR_IN, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_CCA, GPIOF_DIR_IN, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_SFD, GPIOF_DIR_IN, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_DEBUG_0, GPIOF_DIR_OUT, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_DEBUG_1, GPIOF_DIR_OUT, NULL);
    if (err)
        goto fail;

    gpio_set_value(CC2520_DEBUG_0, 0);
    gpio_set_value(CC2520_DEBUG_1, 0);

    // Setup FIFOP Interrupt
    irq = gpio_to_irq(CC2520_FIFOP);
    if (irq < 0) {
        err = irq;
        goto fail;
    }

    err = request_irq(
        irq, 
        cc2520_fifopHandler, 
        IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
        "fifopHandler", 
        NULL
    );
    if (err)
        goto fail;
    state.gpios.fifopIrq = irq;


    irq = gpio_to_irq(CC2520_SFD);
    if (irq < 0) {
        err = irq;
        goto fail;
    }

    // Setup SFD Interrupt
    err = request_irq(
        irq, 
        cc2520_sfdHandler, 
        IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, 
        "sfdHandler", 
        NULL
    );
    if (err)
        goto fail;
    state.gpios.sfdIrq = irq;

    return err;

    fail:
        printk(KERN_INFO "Failed to init GPIOs\n");
        cc2520_freeGpioPins();
        return err;
}

void cc2520_freeGpioPins()
{
    gpio_free(CC2520_CLOCK);
    gpio_free(CC2520_FIFO);
    gpio_free(CC2520_FIFOP);
    gpio_free(CC2520_CCA);
    gpio_free(CC2520_SFD);

    gpio_free(CC2520_DEBUG_0);
    gpio_free(CC2520_DEBUG_1);

    free_irq(state.gpios.fifopIrq, NULL);
    free_irq(state.gpios.sfdIrq, NULL);
}

//////////////////////////////
// Module Init/Cleanup
//////////////////////////////

int init_module()
{
    ktime_t kt;
    int err = 0;

    printk(KERN_INFO "Loading CC2520 Kernel Module v0.01...\n");

    err = cc2520_setupGpioPins();
    if (err) {
        printk(KERN_INFO "[CC2520] - Error setting up GPIO pins. Aborting.");
        return 1;
    }

    //////////////////////////
    // GPIO Interrupt Init
    //err = gpio_request_one(22, GPIOF_DIR_IN, NULL);
    //err = gpio_request_one(23, GPIOF_DIR_OUT, NULL);
    //printk(KERN_INFO "Requesting GPIO Pin 22: %d\n", err);

    //irqNumber = gpio_to_irq(22);
    //printk(KERN_INFO "Requesting IRQ: %d\n", irqNumber);

    //if (irqNumber < 0) {
    //    printk("Unable to get irq number.");
    //    return 1;
    //}

    //request_irq(irqNumber, handler, IRQF_TRIGGER_FALLING, "theInterrupt", NULL);

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
    cc2520_freeGpioPins();
    hrtimer_cancel(&utimer); //
    printk(KERN_INFO "Unloading CC2520 Kernel Module...\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
