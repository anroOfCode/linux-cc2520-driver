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
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "cc2520.h"
#include "radio_config.h"


struct spi_message msg;
struct spi_transfer tsfer;

static cc2520_status_t cc2520_radio_strobe(u8 cmd);
static void cc2520_radio_writeRegister(u8 reg, u8 value);
static void cc2520_radio_writeMemory(u16 mem_addr, u8 *value, u8 len);

//static void cc2520_radio_download_packet(struct work_struct *work);

static void cc2520_radio_beginRxRead(void);
static void cc2520_radio_continueRxRead(void *arg);
static void cc2520_radio_finishRxRead(void *arg);

//////////////////////////////
// Initialization & On/Off
/////////////////////////////

void cc2520_radio_init()
{
    //INIT_WORK(&state.work, cc2520_radio_download_packet);
    tsfer.cs_change = 1;

    // 200uS Reset Pulse.
    gpio_set_value(CC2520_RESET, 0);
    udelay(200);
    gpio_set_value(CC2520_RESET, 1);
    udelay(200);

    cc2520_radio_writeRegister(CC2520_TXPOWER, cc2520_txpower_default.value);
    cc2520_radio_writeRegister(CC2520_CCACTRL0, cc2520_ccactrl0_default.value);
    cc2520_radio_writeRegister(CC2520_MDMCTRL0, cc2520_mdmctrl0_default.value);
    cc2520_radio_writeRegister(CC2520_MDMCTRL1, cc2520_mdmctrl1_default.value);
    cc2520_radio_writeRegister(CC2520_RXCTRL, cc2520_rxctrl_default.value);
    cc2520_radio_writeRegister(CC2520_FSCTRL, cc2520_fsctrl_default.value);
    cc2520_radio_writeRegister(CC2520_FSCAL1, cc2520_fscal1_default.value);
    cc2520_radio_writeRegister(CC2520_AGCCTRL1, cc2520_agcctrl1_default.value);
    cc2520_radio_writeRegister(CC2520_ADCTEST0, cc2520_adctest0_default.value);
    cc2520_radio_writeRegister(CC2520_ADCTEST1, cc2520_adctest1_default.value);
    cc2520_radio_writeRegister(CC2520_ADCTEST2, cc2520_adctest2_default.value);
    cc2520_radio_writeRegister(CC2520_FIFOPCTRL, cc2520_fifopctrl_default.value);
    cc2520_radio_writeRegister(CC2520_FRMCTRL0, cc2520_frmctrl0_default.value);
    cc2520_radio_writeRegister(CC2520_FRMFILT1, cc2520_frmfilt1_default.value);
    cc2520_radio_writeRegister(CC2520_SRCMATCH, cc2520_srcmatch_default.value);   
}

void cc2520_radio_on()
{
    cc2520_radio_set_channel(state.channel & CC2520_CHANNEL_MASK);
    cc2520_radio_set_address(state.short_addr, state.extended_addr, state.pan_id);
    cc2520_radio_strobe(CC2520_CMD_SRXON);
}

void cc2520_radio_off()
{
    cc2520_radio_strobe(CC2520_CMD_SRFOFF);
}

static void spike_completion_handler(void *arg)
{   
    printk(KERN_INFO "Spi Callback complete.");
}

//////////////////////////////
// Configuration Commands
/////////////////////////////

void cc2520_radio_set_channel(int channel)
{
    cc2520_freqctrl_t freqctrl;
    freqctrl = cc2520_freqctrl_default;

    freqctrl.f.freq = 11 + 5 * (channel - 11);

    cc2520_radio_writeRegister(CC2520_FREQCTRL, freqctrl.value);
}

// Sets the short address
void cc2520_radio_set_address(u16 short_addr, u64 extended_addr, u16 pan_id)
{
    char addr_mem[12];

    addr_mem[7] = (extended_addr >> 56) & 0xFF;
    addr_mem[6] = (extended_addr >> 48) & 0xFF;
    addr_mem[5] = (extended_addr >> 40) & 0xFF;
    addr_mem[4] = (extended_addr >> 32) & 0xFF;
    addr_mem[3] = (extended_addr >> 24) & 0xFF;
    addr_mem[2] = (extended_addr >> 16) & 0xFF;
    addr_mem[1] = (extended_addr >> 8) & 0xFF;
    addr_mem[0] = (extended_addr) & 0xFF;

    addr_mem[9] = (pan_id >> 8) & 0xFF;
    addr_mem[8] = (pan_id) & 0xFF;

    addr_mem[11] = (short_addr >> 8) & 0xFF;
    addr_mem[10] = (short_addr) & 0xFF;

    cc2520_radio_writeMemory(CC2520_MEM_ADDR_BASE, addr_mem, 12);
}

//////////////////////////////
// Callback Hooks
/////////////////////////////

void cc2520_radio_sfd_occurred(u64 nano_timestamp)
{
    // Store the SFD time for use later in timestamping
    // incoming/outgoing packets.
    state.sfd_nanos_ts = nano_timestamp;
}

void cc2520_radio_fifop_occurred()
{
    cc2520_radio_beginRxRead();
    //queue_work(state.wq, &state.work);
}

//////////////////////////////
// Receiver Engine
/////////////////////////////

void cc2520_radio_reset()
{

}

static void cc2520_radio_beginRxRead()
{
    int status;

    tsfer.len = 0;
    state.tx_buf[tsfer.len++] = CC2520_CMD_RXBUF;
    state.tx_buf[tsfer.len++] = 0;

    tsfer.cs_change = 0;

    memset(state.rx_buf, 0, SPI_BUFF_SIZE);

    spi_message_init(&msg);
    msg.complete = cc2520_radio_continueRxRead;
    msg.context = NULL;
    spi_message_add_tail(&tsfer, &msg);

    status = spi_async(state.spi_device, &msg);   
}

static void cc2520_radio_continueRxRead(void *arg)
{
    int status;
    int i;
    // Length of what we're reading is stored
    // in the received spi buffer, read from the
    // async operation called in beginRxRead.
    int len;

    len = state.rx_buf[1];

    tsfer.len = 0;
    for (i = 0; i < len; i++) 
        state.tx_buf[tsfer.len++] = 0;

    tsfer.cs_change = 1;

    spi_message_init(&msg);
    msg.complete = cc2520_radio_finishRxRead;
    // Platform dependent? 
    msg.context = (void*)len;
    spi_message_add_tail(&tsfer, &msg);

    status = spi_async(state.spi_device, &msg);  
}

static void cc2520_radio_finishRxRead(void *arg)
{
    int len;
    int i;
    char *buff;
    char *buff_ptr;

    len = (int)arg;

    printk(KERN_INFO "[cc2520] - Read %d bytes from radio.", len);

    // At this point we should schedule the system to move the
    // RX into a different buffer. For now just print it. 
    buff = kmalloc(len*5 + 1, GFP_ATOMIC);
    if (buff) {
        buff_ptr = buff;
        for (i = 0; i < len; i++)
        {
            buff_ptr += sprintf(buff_ptr, " 0x%02X", state.rx_buf[i]);
        }
        sprintf(buff_ptr,"\n");
        *(buff_ptr + 1) = '\0';
        printk(KERN_INFO "[cc2520] - %s\n", buff);
        kfree(buff);
    }  
}

//////////////////////////////
// Helper Routines
/////////////////////////////

// Memory address MUST be >= 200.
static void cc2520_radio_writeMemory(u16 mem_addr, u8 *value, u8 len)
{
    int status;
    int i;

    tsfer.len = 0;
    state.tx_buf[tsfer.len++] = CC2520_CMD_MEMORY_WRITE | ((mem_addr >> 8) & 0xFF);
    state.tx_buf[tsfer.len++] = mem_addr & 0xFF;

    for (i=0; i<len; i++) {
        state.tx_buf[tsfer.len++] = value[i];
    }

    memset(state.rx_buf, 0, SPI_BUFF_SIZE);

    spi_message_init(&msg);
    msg.complete = spike_completion_handler;
    msg.context = NULL;
    spi_message_add_tail(&tsfer, &msg);

    status = spi_sync(state.spi_device, &msg);   
}

static void cc2520_radio_writeRegister(u8 reg, u8 value)
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

static cc2520_status_t cc2520_radio_strobe(u8 cmd)
{
    int status;
    cc2520_status_t ret;

    state.tx_buf[0] = cmd;
    tsfer.len = 1;

    memset(state.rx_buf, 0, SPI_BUFF_SIZE);

    spi_message_init(&msg);
    msg.complete = spike_completion_handler;
    msg.context = NULL;
    spi_message_add_tail(&tsfer, &msg);    

    status = spi_sync(state.spi_device, &msg);

    ret.value = state.rx_buf[0];
    return ret;
}