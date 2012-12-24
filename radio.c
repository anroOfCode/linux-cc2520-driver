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
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "cc2520.h"
#include "radio.h"
#include "radio_config.h"

static u16 short_addr;
static u64 extended_addr;
static u16 pan_id;
static u8 channel;

static struct spi_message msg;
static struct spi_transfer tsfer;
static struct spi_transfer tsfer1;
static struct spi_transfer tsfer2;
static struct spi_transfer tsfer3;

static u8 *tx_buf;
static u8 *rx_buf;

static u8 *tx_buf_r;
static u8 *rx_buf_r;

static u8 * cur_tx_buf;
static u8 cur_tx_len;

static u64 sfd_nanos_ts;

static spinlock_t radio_sl;
static int radio_state;

enum cc2520_radio_state_enum {
    CC2520_RADIO_STATE_IDLE,
    CC2520_RADIO_STATE_RX,
    CC2520_RADIO_STATE_TX,
};

static cc2520_status_t cc2520_radio_strobe(u8 cmd);
static void cc2520_radio_writeRegister(u8 reg, u8 value);
static void cc2520_radio_writeMemory(u16 mem_addr, u8 *value, u8 len);

static void cc2520_radio_beginRxRead(void);
static void cc2520_radio_continueRxRead(void *arg);
static void cc2520_radio_finishRxRead(void *arg);

static int cc2520_radio_tx(u8 *buf, u8 len);
static void cc2520_radio_continueTx(void *arg);
static void cc2520_radio_completeTx(void);


struct cc2520_interface *radio_top;

void cc2520_radio_lock(int state)
{
	spin_lock(&radio_sl);
	while (radio_state != CC2520_RADIO_STATE_IDLE) {
		spin_unlock(&radio_sl);
		spin_lock(&radio_sl);
	}
	radio_state = state;
	spin_unlock(&radio_sl);
}

int cc2520_radio_try_lock(int state)
{
	spin_lock(&radio_sl);
	if (radio_state != CC2520_RADIO_STATE_IDLE) {
		spin_unlock(&radio_sl);
		return -1;
	}
	radio_state = state;
	spin_unlock(&radio_sl);
	return 0;
}

void cc2520_radio_unlock()
{
	spin_lock(&radio_sl);
	radio_state = CC2520_RADIO_STATE_IDLE;
	spin_unlock(&radio_sl);
}

void cc2520_radio_idle_lock(int state)
{
	spin_lock(&radio_sl);
	if (radio_state == CC2520_RADIO_STATE_IDLE) {
		radio_state = state;
	}
	spin_unlock(&radio_sl);
}

int cc2520_radio_tx_unlock()
{
	spin_lock(&radio_sl);
	if (radio_state == CC2520_RADIO_STATE_TX) {
		radio_state = CC2520_RADIO_STATE_IDLE;
		spin_unlock(&radio_sl);
		return 1;
	}
	spin_unlock(&radio_sl);
	return 0;
}

//////////////////////////////
// Initialization & On/Off
/////////////////////////////

int cc2520_radio_init()
{
	int result;

	radio_top->tx = cc2520_radio_tx;

	short_addr = CC2520_DEF_SHORT_ADDR;
	extended_addr = CC2520_DEF_EXT_ADDR;
	pan_id = CC2520_DEF_PAN;
	channel = CC2520_DEF_CHANNEL;

	spin_lock_init(&radio_sl);
	radio_state = CC2520_RADIO_STATE_IDLE;

	tx_buf = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!tx_buf) {
		result = -EFAULT;
		goto error;
	}
		
	rx_buf = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);    
	if (!rx_buf) {
		result = -EFAULT;
		goto error;
	}

	tx_buf_r = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!tx_buf_r) {
		result = -EFAULT;
		goto error;
	}
		
	rx_buf_r = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);    
	if (!rx_buf_r) {
		result = -EFAULT;
		goto error;
	}

	return 0;

	error:
		if (rx_buf_r) {
			kfree(rx_buf_r);
			rx_buf_r = 0;     
		}

		if (tx_buf_r) {
			kfree(tx_buf_r);
			tx_buf_r = 0;
		}

		if (rx_buf) {
			kfree(rx_buf);
			rx_buf = 0;
		}

		if (tx_buf) {
			kfree(tx_buf);
			tx_buf = 0;
		}
		return result;
}

void cc2520_radio_free()
{
	if (rx_buf_r) {
		kfree(rx_buf_r);
		rx_buf_r = 0;
	}

	if (tx_buf_r) {
		kfree(tx_buf_r);
		tx_buf_r = 0;
	}

	if (rx_buf) {
		kfree(rx_buf);
		rx_buf = 0;
	}

	if (tx_buf) {
		kfree(tx_buf);
		tx_buf = 0;
	}
}

void cc2520_radio_start()
{
	//INIT_WORK(&work, cc2520_radio_download_packet);

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
	cc2520_radio_set_channel(channel & CC2520_CHANNEL_MASK);
	cc2520_radio_set_address(short_addr, extended_addr, pan_id);
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

void cc2520_radio_set_txpower(u8 power)
{
	cc2520_txpower_t txpower;
	txpower = cc2520_txpower_default;

	txpower.f.pa_power = power;

	cc2520_radio_writeRegister(CC2520_TXPOWER, txpower.value);
}
//////////////////////////////
// Callback Hooks
/////////////////////////////

// context: interrupt
void cc2520_radio_sfd_occurred(u64 nano_timestamp, u8 is_high)
{
	// Store the SFD time for use later in timestamping
	// incoming/outgoing packets.
	sfd_nanos_ts = nano_timestamp;
	if (is_high)
		// Lock in a transmit if idle
		cc2520_radio_idle_lock(CC2520_RADIO_STATE_RX);
	else {
		// SFD falling indicates TX completion.
		if (cc2520_radio_tx_unlock()) {
			cc2520_radio_completeTx();				
		}
	
	}
}

// context: interrupt
void cc2520_radio_fifop_occurred()
{
	cc2520_radio_beginRxRead();
	//queue_work(wq, &work);
}

void cc2520_radio_reset(void)
{

}

//////////////////////////////
// Transmit Engine
/////////////////////////////

// context: process?
static int cc2520_radio_tx(u8 *buf, u8 len)
{
	printk(KERN_INFO "[cc2520] - beginning write op.\n");
	// capture exclusive radio rights to send
	// build the transmit command seq
	// write that packet!

	// 1- Stop Receiving
	// 2- Write Packet Header
	// 3- Turn on TX
	// 4- Write Rest of Packet
	// 5- On SFD falling edge give up lock

	int status;
	int buf_offset;
	int i;

	buf_offset = 0;
	cc2520_radio_lock(CC2520_RADIO_STATE_TX);

	// Turn off the radio
	tsfer.len = 0;
	tx_buf[buf_offset + tsfer.len++] = CC2520_CMD_SRFOFF;
	buf_offset += tsfer.len;
	tsfer.cs_change = 1;

	tsfer1.tx_buf = tx_buf + buf_offset;
	tsfer1.rx_buf = rx_buf + buf_offset;
	tsfer1.len = 0;
	tsfer1.cs_change = 1;
	tx_buf[buf_offset + tsfer1.len++] = CC2520_CMD_TXBUF;
	// Data + 2 bytes for FCS
	tx_buf[buf_offset + tsfer1.len++] = len + 2;
	for (i = 0; i < 8; i++)
		tx_buf[buf_offset + tsfer1.len++] = buf[i];
	buf_offset += tsfer1.len;

	tsfer2.tx_buf = tx_buf + buf_offset;
	tsfer2.rx_buf = rx_buf + buf_offset; 
	tsfer2.len = 0;
	tsfer2.cs_change = 1;
	tx_buf[buf_offset + tsfer2.len++] = CC2520_CMD_STXON;
	buf_offset += tsfer2.len;

	tsfer3.tx_buf = tx_buf + buf_offset;
	tsfer3.rx_buf = rx_buf + buf_offset;
	tsfer3.len = 0;
	tsfer3.cs_change = 1;
	tx_buf[buf_offset + tsfer3.len++] = CC2520_CMD_TXBUF;
	for (i = 8; i < len; i++)
		tx_buf[buf_offset + tsfer3.len++] = buf[i];

	spi_message_init(&msg);
	msg.complete = cc2520_radio_continueTx;
	msg.context = NULL;
	spi_message_add_tail(&tsfer, &msg);
	spi_message_add_tail(&tsfer1, &msg);
	spi_message_add_tail(&tsfer2, &msg);
	spi_message_add_tail(&tsfer3, &msg);

	status = spi_async(state.spi_device, &msg); 

	radio_top->tx_done(0);
	return 0;
}

static void cc2520_radio_continueTx(void *arg)
{   
	printk(KERN_INFO "Spi Callback complete.\n");
}

static void cc2520_radio_completeTx()
{
	printk(KERN_INFO "[cc2520] - write op complete.\n");
	radio_top->tx_done(0);
}

//////////////////////////////
// Receiver Engine
/////////////////////////////

static void cc2520_radio_beginRxRead()
{
	int status;

	tsfer.len = 0;
	tx_buf[tsfer.len++] = CC2520_CMD_RXBUF;
	tx_buf[tsfer.len++] = 0;

	tsfer.cs_change = 0;

	memset(rx_buf, 0, SPI_BUFF_SIZE);

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

	len = rx_buf[1];

	tsfer.len = 0;
	for (i = 0; i < len; i++) 
		tx_buf[tsfer.len++] = 0;

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

	cc2520_radio_unlock();
	// At this point we should schedule the system to move the
	// RX into a different buffer. For now just print it. 
	buff = kmalloc(len*5 + 1, GFP_ATOMIC);
	if (buff) {
		buff_ptr = buff;
		for (i = 0; i < len; i++)
		{
			buff_ptr += sprintf(buff_ptr, " 0x%02X", rx_buf[i]);
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
	tx_buf[tsfer.len++] = CC2520_CMD_MEMORY_WRITE | ((mem_addr >> 8) & 0xFF);
	tx_buf[tsfer.len++] = mem_addr & 0xFF;

	for (i=0; i<len; i++) {
		tx_buf[tsfer.len++] = value[i];
	}

	memset(rx_buf, 0, SPI_BUFF_SIZE);

	spi_message_init(&msg);
	msg.complete = spike_completion_handler;
	msg.context = NULL;
	spi_message_add_tail(&tsfer, &msg);

	status = spi_sync(state.spi_device, &msg);   
}

static void cc2520_radio_writeRegister(u8 reg, u8 value)
{
	int status;

	tsfer.tx_buf = tx_buf;
	tsfer.rx_buf = rx_buf;
	tsfer.len = 0;

	if (reg <= CC2520_FREG_MASK) {
		tx_buf[tsfer.len++] = CC2520_CMD_REGISTER_WRITE | reg;
	}
	else {
		tx_buf[tsfer.len++] = CC2520_CMD_MEMORY_WRITE;
		tx_buf[tsfer.len++] = reg;
	}

	tx_buf[tsfer.len++] = value;

	memset(rx_buf, 0, SPI_BUFF_SIZE);

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

	tx_buf[0] = cmd;
	tsfer.len = 1;

	memset(rx_buf, 0, SPI_BUFF_SIZE);

	spi_message_init(&msg);
	msg.complete = spike_completion_handler;
	msg.context = NULL;
	spi_message_add_tail(&tsfer, &msg);    

	status = spi_sync(state.spi_device, &msg);

	ret.value = rx_buf[0];
	return ret;
}