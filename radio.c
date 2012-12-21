#include <linux/module.h>  
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/spi/spi.h>

#include "cc2520.h"

struct spi_message msg;
struct spi_transfer tsfer;

void cc2520_radio_init()
{


}

static void spike_completion_handler(void *arg)
{   
    printk(KERN_INFO "Spi Callback complete.");
}

void cc2520_radio_writeRegister(u8 reg, u8 value)
{
    int status;

    tsfer.tx_buf = state.tx_buf;
    tsfer.rx_buf = state.rx_buf;
    tsfer.len = 4;

    state.tx_buf[0] = 0xAA;
    state.tx_buf[1] = 0xBB;
    state.tx_buf[2] = 0xCC;
    state.tx_buf[3] = 0xDD;

    memset(state.rx_buf, 0, SPI_BUFF_SIZE);

    spi_message_init(&msg);

    msg.complete = spike_completion_handler;
    msg.context = NULL;

    spi_message_add_tail(&tsfer, &msg);

    status = spi_sync(state.spi_device, &msg);
}