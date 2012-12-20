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

void cc2520_radio_init()
{
    

}

void cc2520_radio_writeRegister(u8 reg, u8 value)
{
    struct spi_message msg;
    struct spi_transfer tsfer;
    int status;

    spi_message_init(&msg);

    tsfer.tx_buf = state.tx_buf;
    tsfer.rx_buf = state.rx_buf;
    tsfer.len = 3;

    state.tx_buf[0] = 0xAA;
    state.tx_buf[1] = 0xBB;
    state.tx_buf[2] = 0xCC;

    spi_message_add_tail(&tsfer, &msg);

    status = spi_sync(state.spi_device, &msg);
}