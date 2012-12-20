#include <linux/module.h>  
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>

#include "cc2520.h"

//////////////////////////
// SPI Stuff
//////////////////////////

#define SPI_BUS 0
#define SPI_BUS_CS0 0
#define SPI_BUS_SPEED 500000

const char this_driver_name[] = "cc2520";

int cc2520_spi_addToBus(void)
{
    struct spi_master *spi_master;
    struct spi_device *spi_device;
    struct device *pdev;
    char buff[64];
    int status = 0;

    spi_master = spi_busnum_to_master(SPI_BUS);
    if (!spi_master) {
        printk(KERN_ALERT "spi_busnum_to_master(%d) returned NULL\n",
            SPI_BUS);
        printk(KERN_ALERT "Missing modprobe omap2_mcspi?\n");
        return -1;
    }

    spi_device = spi_alloc_device(spi_master);
    if (!spi_device) {
        put_device(&spi_master->dev);
        printk(KERN_ALERT "spi_alloc_device() failed\n");
        return -1;
    }

    spi_device->chip_select = SPI_BUS_CS0;

    /* Check whether this SPI bus.cs is already claimed */
    snprintf(buff, sizeof(buff), "%s.%u", 
            dev_name(&spi_device->master->dev),
            spi_device->chip_select);

    pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);
    if (pdev) {
        /* We are not going to use this spi_device, so free it */ 
        spi_dev_put(spi_device);

        /* 
         * There is already a device configured for this bus.cs  
         */
        printk(KERN_INFO
            "Driver [%s] already registered for %s. Nuking from orbit.\n",
            pdev->driver->name, buff);

        device_del(pdev);
    }

    spi_device->max_speed_hz = SPI_BUS_SPEED;
    spi_device->mode = SPI_MODE_0;
    spi_device->bits_per_word = 8;
    spi_device->irq = -1;
    spi_device->controller_state = NULL;
    spi_device->controller_data = NULL;
    strlcpy(spi_device->modalias, this_driver_name, SPI_NAME_SIZE);

    status = spi_add_device(spi_device);        
    if (status < 0) {   
        spi_dev_put(spi_device);
        printk(KERN_ALERT "spi_add_device() failed: %d\n", 
            status);        
    }               

    put_device(&spi_master->dev);
    return status;
}

static int cc2520_spi_probe(struct spi_device *spi_device)
{
    printk(KERN_INFO "PROBINGGGGG\n");
    return 0;
}

static int cc2520_spi_remove(struct spi_device *spi_device)
{

    return 0;
}

static struct spi_driver cc2520_spiDriver = {
        .driver = {
            .name = this_driver_name,
            .owner = THIS_MODULE,
        },
        .probe = cc2520_spi_probe,
        .remove = cc2520_spi_remove,
};

void cc2520_plat_setupSpi()
{
    spi_register_driver(&cc2520_spiDriver);
}

void cc2520_plat_freeSpi()
{
    spi_unregister_driver(&cc2520_spiDriver);
}

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
int cc2520_plat_setupGpioPins()
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
        cc2520_plat_freeGpioPins();
        return err;
}

void cc2520_plat_freeGpioPins()
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