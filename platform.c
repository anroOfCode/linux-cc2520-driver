#include <linux/module.h>  
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "cc2520.h"

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

    err = gpio_request_one(CC2520_RESET, GPIOF_DIR_IN, NULL);
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

    // Setup SFD Interrupt
    irq = gpio_to_irq(CC2520_SFD);
    if (irq < 0) {
        err = irq;
        goto fail;
    }

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
    gpio_free(CC2520_RESET);

    gpio_free(CC2520_DEBUG_0);
    gpio_free(CC2520_DEBUG_1);

    free_irq(state.gpios.fifopIrq, NULL);
    free_irq(state.gpios.sfdIrq, NULL);
}