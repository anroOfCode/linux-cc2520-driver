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
#include <linux/time.h>

#include "cc2520.h"
#include "radio.h"
#include "platform.h"
#include "debug.h"

//////////////////////////
// SPI Stuff
//////////////////////////

static int cc2520_spi_add_to_bus(void)
{
    struct spi_master *spi_master;
    struct spi_device *spi_device;
    struct device *pdev;
    char buff[64];
    int status = 0;

    spi_master = spi_busnum_to_master(SPI_BUS);
    if (!spi_master) {
        ERR((KERN_ALERT "[cc2520] - spi_busnum_to_master(%d) returned NULL\n",
            SPI_BUS));
        ERR((KERN_ALERT "[cc2520] - Missing modprobe spi-bcm2708?\n"));
        return -1;
    }

    spi_device = spi_alloc_device(spi_master);
    if (!spi_device) {
        put_device(&spi_master->dev);
        ERR((KERN_ALERT "[cc2520] - spi_alloc_device() failed\n"));
        return -1;
    }

    spi_device->chip_select = SPI_BUS_CS0;

    /* Check whether this SPI bus.cs is already claimed */
    snprintf(buff, sizeof(buff), "%s.%u",
            dev_name(&spi_device->master->dev),
            spi_device->chip_select);

    pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);

    if (pdev) {
        if (pdev->driver != NULL) {
            ERR((KERN_INFO
                "[cc2520] - Driver [%s] already registered for %s. \
Nuking from orbit.\n",
                pdev->driver->name, buff));
        }
        else {
            ERR((KERN_INFO
                "[cc2520] - Previous driver registered with no loaded module. \
Nuking from orbit.\n"));
        }

        device_unregister(pdev);
    }

    spi_device->max_speed_hz = SPI_BUS_SPEED;
    spi_device->mode = SPI_MODE_0;
    spi_device->bits_per_word = 8;
    spi_device->irq = -1;

    spi_device->controller_state = NULL;
    spi_device->controller_data = NULL;
    strlcpy(spi_device->modalias, cc2520_name, SPI_NAME_SIZE);

    status = spi_add_device(spi_device);
    if (status < 0) {
        spi_dev_put(spi_device);
        ERR((KERN_ALERT "[cc2520] - spi_add_device() failed: %d\n",
            status));
    }

    put_device(&spi_master->dev);
    return status;
}

static int cc2520_spi_probe(struct spi_device *spi_device)
{
    ERR((KERN_INFO "[cc2520] - Inserting SPI protocol driver.\n"));
    state.spi_device = spi_device;
    return 0;
}

static int cc2520_spi_remove(struct spi_device *spi_device)
{
    ERR((KERN_INFO "[cc2520] - Removing SPI protocol driver."));
    state.spi_device = NULL;
    return 0;
}

static struct spi_driver cc2520_spi_driver = {
        .driver = {
            .name = cc2520_name,
            .owner = THIS_MODULE,
        },
        .probe = cc2520_spi_probe,
        .remove = cc2520_spi_remove,
};

int cc2520_plat_spi_init()
{
    int result;

    result = cc2520_spi_add_to_bus();
    if (result < 0)
        goto error;

    result = spi_register_driver(&cc2520_spi_driver);
    if (result < 0)
        goto error;

    return 0;

    error:
        spi_unregister_driver(&cc2520_spi_driver);
        return result;
}

void cc2520_plat_spi_free()
{
    if (state.spi_device)
        spi_unregister_device(state.spi_device);

    spi_unregister_driver(&cc2520_spi_driver);
}

//////////////////////////
// Interrupt Handles
/////////////////////////

static irqreturn_t cc2520_sfd_handler(int irq, void *dev_id)
{
    int gpio_val;
    struct timespec ts;
    s64 nanos;

    // NOTE: For now we're assuming no delay between SFD called
    // and actual SFD received. The TinyOS implementations call
    // for a few uS of delay, but it's likely not needed.
    getrawmonotonic(&ts);
    nanos = timespec_to_ns(&ts);
    gpio_val = gpio_get_value(CC2520_SFD);

    //DBG((KERN_INFO "[cc2520] - sfd interrupt occurred at %lld, %d\n", (long long int)nanos, gpio_val));

    cc2520_radio_sfd_occurred(nanos, gpio_val);
    return IRQ_HANDLED;
}

static irqreturn_t cc2520_fifop_handler(int irq, void *dev_id)
{
    if (gpio_get_value(CC2520_FIFOP) == 1) {
        DBG((KERN_INFO "[cc2520] - fifop interrupt occurred\n"));
        cc2520_radio_fifop_occurred();
    }
    return IRQ_HANDLED;
}

//////////////////////////////
// Interface Initialization
//////////////////////////////

// Sets up the GPIO pins needed for the CC2520
// and initializes any interrupt handlers needed.
int cc2520_plat_gpio_init()
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

    err = gpio_request_one(CC2520_RESET, GPIOF_DIR_OUT, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_DEBUG_0, GPIOF_DIR_OUT, NULL);
    if (err)
        goto fail;

    err = gpio_request_one(CC2520_DEBUG_1, GPIOF_DIR_OUT, NULL);
    if (err)
        goto fail;

    gpio_set_value(CC2520_DEBUG_0, 0);

    // Setup FIFOP Interrupt
    irq = gpio_to_irq(CC2520_FIFOP);
    if (irq < 0) {
        err = irq;
        goto fail;
    }

    err = request_irq(
        irq,
        cc2520_fifop_handler,
        IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
        "fifopHandler",
        NULL
    );
    if (err)
        goto fail;
    state.gpios.fifop_irq = irq;

    // Setup SFD Interrupt
    irq = gpio_to_irq(CC2520_SFD);
    if (irq < 0) {
        err = irq;
        goto fail;
    }

    err = request_irq(
        irq,
        cc2520_sfd_handler,
        IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
        "sfdHandler",
        NULL
    );
    if (err)
        goto fail;
    state.gpios.sfd_irq = irq;

    return err;

    fail:
        ERR((KERN_ALERT "[cc2520] - failed to init GPIOs\n"));
        cc2520_plat_gpio_free();
        return err;
}

void cc2520_plat_gpio_free()
{
    gpio_free(CC2520_CLOCK);
    gpio_free(CC2520_FIFO);
    gpio_free(CC2520_FIFOP);
    gpio_free(CC2520_CCA);
    gpio_free(CC2520_SFD);
    gpio_free(CC2520_RESET);

    gpio_free(CC2520_DEBUG_0);
    gpio_free(CC2520_DEBUG_1);

    free_irq(state.gpios.fifop_irq, NULL);
    free_irq(state.gpios.sfd_irq, NULL);
}
