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
#include <linux/delay.h>

#include "cc2520.h"

    struct spi_message msg;
    struct spi_transfer tsfer;

void cc2520_radio_init()
{
    // 200uS Reset Pulse.
    gpio_set_value(CC2520_RESET, 0);
    udelay(200);
    gpio_set_value(CC2520_RESET, 1);

    cc2520_radio_writeRegister(CC2520_TXPOWER, cc2520_txpower_default.value);
    cc2520_radio_writeRegister(CC2520_CCACTRL0, cc2520_ccactrl0_default.value);
    /*
    cc2520_radio_writeRegister(CC2520_MDMCTRL0, cc2520_mdmctrl0_default.value);
    cc2520_radio_writeRegister(CC2520_MDMCTRL1, cc2520_mdmctrl1_default.value);
    cc2520_radio_writeRegister(CC2520_RXCTRL, cc2520_rxctrl_default.value);
    cc2520_radio_writeRegister(CC2520_FSCTRL, cc2520_fsctrl_default.value);
    cc2520_radio_writeRegister(CC2520_FSCAL1, cc2520_fscal1_default.value);
    cc2520_radio_writeRegister(CC2520_AGCCTRL1, cc2520_agcctrl1_default.value);
    cc2520_radio_writeRegister(CC2520_ADCTEST0, cc2520_adctest0_default.value);
    cc2520_radio_writeRegister(CC2520_ADCTEST1, cc2520_adctest1_default.value);
    cc2520_radio_writeRegister(CC2520_ADCTEST2, cc2520_adctest2_default.value);
    */
    // setup fifop threshold
    //fifopctrl.f.fifop_thr = 127;
    //cc2520_radio_writeRegister(CC2520_FIFOPCTRL, fifopctrl.value);

    // FIXME: disable frame filtering for now
    //frmfilt0 = cc2520_frmfilt0_default;
    //frmfilt0.f.frame_filter_en = 0;
    //cc2520_radio_writeRegister(CC2520_FRMFILT0, frmfilt0.value);

    //frmctrl0 = cc2520_frmctrl0_default;
    //frmctrl0.f.autoack = 1;
    //writeRegister(CC2520_FRMCTRL0, frmctrl0.value);

    // accept reserved frames
    //frmfilt1 = cc2520_frmfilt1_default;
    //frmfilt1.f.accept_ft_4to7_reserved = 1;
    //cc2520_radio_writeRegister(CC2520_FRMFILT1, frmfilt1.value);

    // disable src address decoding
    //srcmatch = cc2520_srcmatch_default;
    //srcmatch.f.src_match_en = 0;
    //cc2520_radio_writeRegister(CC2520_SRCMATCH, srcmatch.value);   
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
    tsfer.len = 0;

    if (reg <= CC2520_FREG_MASK) {
        state.tx_buf[tsfer.len++] = CC2520_CMD_REGISTER_WRITE | reg;
    }
    else {
        state.tx_buf[tsfer.len++] = CC2520_CMD_MEMORY_WRITE;
        state.tx_buf[tsfer.len++] = reg;
    }

    state.tx_buf[tsfer.len++] = value;

    memset(state.rx_buf, 0, SPI_BUFF_SIZE);

    spi_message_init(&msg);
    msg.complete = spike_completion_handler;
    msg.context = NULL;
    spi_message_add_tail(&tsfer, &msg);

    status = spi_sync(state.spi_device, &msg);
}