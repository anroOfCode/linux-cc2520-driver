#include <linux/module.h>  
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#include "cc2520.h"

#define DRIVER_AUTHOR "Andrew Robinson <androbin@umich.edu>"
#define DRIVER_DESC   "A driver for the CC2520 radio. Be afraid."

struct cc2520_state state;
const char cc2520_name[] = "cc2520";

struct hrtimer utimer;
int irqNumber = 0;
int pinValue = 0;

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


int init_module()
{
    ktime_t kt;
    int err = 0;

    memset(&state, 0, sizeof(struct cc2520_state));
    state.short_addr = CC2520_DEF_SHORT_ADDR;
    state.extended_addr = CC2520_DEF_EXT_ADDR;
    state.pan_id = CC2520_DEF_PAN;
    state.channel = CC2520_DEF_CHANNEL;

    printk(KERN_INFO "Loading CC2520 Kernel Module v0.01...\n");

    sema_init(&state.radio_sem, 1);

    err = cc2520_plat_gpio_init();
    if (err) {
        printk(KERN_INFO "[CC2520] - Error setting up GPIO pins. Aborting.");
        return 1;
    }

    err = cc2520_plat_spi_init();
    if (err) {
        printk(KERN_ALERT "[cc2520] - Error setting up SPI. Aborting.");
        cc2520_plat_gpio_free();
        return 1;
    }

    err = cc2520_interface_init();
    if (err) {
        printk(KERN_ALERT "[cc2520] - Error setting up character driver. Aborting.");
        cc2520_plat_spi_free();
        cc2520_plat_gpio_free();
        return 1;        
    }

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
    cc2520_interface_free();
    cc2520_plat_gpio_free();
    cc2520_plat_spi_free();
    hrtimer_cancel(&utimer); //
    printk(KERN_INFO "Unloading CC2520 Kernel Module...\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);