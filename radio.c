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
#include "interface.h"
#include "debug.h"

static u16 short_addr;
static u64 extended_addr;
static u16 pan_id;
static u8 channel;

static struct spi_message msg;
static struct spi_transfer tsfer;
static struct spi_transfer tsfer1;
static struct spi_transfer tsfer2;
static struct spi_transfer tsfer3;
static struct spi_transfer tsfer4;

static struct spi_message rx_msg;
static struct spi_transfer rx_tsfer;

static u8 *tx_buf;
static u8 *rx_buf;

static u8 *rx_out_buf;
static u8 *rx_in_buf;

static u8 *tx_buf_r;
static u8 *rx_buf_r;
static u8 tx_buf_r_len;

static u64 sfd_nanos_ts;

static spinlock_t radio_sl;

static spinlock_t pending_rx_sl;
static bool pending_rx;

static spinlock_t rx_buf_sl;

static int radio_state;

static unsigned long flags;
static unsigned long flags1;

enum cc2520_radio_state_enum {
    CC2520_RADIO_STATE_IDLE,
    CC2520_RADIO_STATE_TX,
    CC2520_RADIO_STATE_TX_SFD_DONE,
    CC2520_RADIO_STATE_TX_SPI_DONE,
    CC2520_RADIO_STATE_TX_2_RX,
    CC2520_RADIO_STATE_CONFIG
};

static cc2520_status_t cc2520_radio_strobe(u8 cmd);
static void cc2520_radio_writeRegister(u8 reg, u8 value);
static void cc2520_radio_writeMemory(u16 mem_addr, u8 *value, u8 len);

static void cc2520_radio_beginRx(void);
static void cc2520_radio_continueRx(void *arg);
static void cc2520_radio_finishRx(void *arg);


static int cc2520_radio_tx(u8 *buf, u8 len);
static void cc2520_radio_beginTx(void);
static void cc2520_radio_continueTx_check(void *arg);
static void cc2520_radio_continueTx(void *arg);
static void cc2520_radio_completeTx(void);

static void cc2520_radio_flushRx(void);
static void cc2520_radio_continueFlushRx(void *arg);
static void cc2520_radio_completeFlushRx(void *arg);
static void cc2520_radio_flushTx(void);
static void cc2520_radio_completeFlushTx(void *arg);

struct cc2520_interface *radio_top;

// TODO: These methods are stupid
// and make things more confusing.
// Refactor them out.

void cc2520_radio_lock(int state)
{
	spin_lock_irqsave(&radio_sl, flags1);
	while (radio_state != CC2520_RADIO_STATE_IDLE) {
		spin_unlock_irqrestore(&radio_sl, flags1);
		spin_lock_irqsave(&radio_sl, flags1);
	}
	radio_state = state;
	spin_unlock_irqrestore(&radio_sl, flags1);
}

void cc2520_radio_unlock(void)
{
	spin_lock_irqsave(&radio_sl, flags1);
	radio_state = CC2520_RADIO_STATE_IDLE;
	spin_unlock_irqrestore(&radio_sl, flags1);
}

int cc2520_radio_tx_unlock_spi(void)
{
	spin_lock_irqsave(&radio_sl, flags1);
	if (radio_state == CC2520_RADIO_STATE_TX) {
		radio_state = CC2520_RADIO_STATE_TX_SPI_DONE;
		spin_unlock_irqrestore(&radio_sl, flags1);
		return 0;
	}
	else if (radio_state == CC2520_RADIO_STATE_TX_SFD_DONE) {
		radio_state = CC2520_RADIO_STATE_TX_2_RX;
		spin_unlock_irqrestore(&radio_sl, flags1);
		return 1;
	}
	spin_unlock_irqrestore(&radio_sl, flags1);
	return 0;
}

int cc2520_radio_tx_unlock_sfd(void)
{
	spin_lock_irqsave(&radio_sl, flags1);
	if (radio_state == CC2520_RADIO_STATE_TX) {
		radio_state = CC2520_RADIO_STATE_TX_SFD_DONE;
		spin_unlock_irqrestore(&radio_sl, flags1);
		return 0;
	}
	else if (radio_state == CC2520_RADIO_STATE_TX_SPI_DONE) {
		radio_state = CC2520_RADIO_STATE_TX_2_RX;
		spin_unlock_irqrestore(&radio_sl, flags1);
		return 1;
	}
	spin_unlock_irqrestore(&radio_sl, flags1);
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
	spin_lock_init(&rx_buf_sl);
	spin_lock_init(&pending_rx_sl);

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

	rx_out_buf = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!rx_out_buf) {
		result = -EFAULT;
		goto error;
	}

	rx_in_buf = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!rx_in_buf) {
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
			rx_buf_r = NULL;
		}

		if (tx_buf_r) {
			kfree(tx_buf_r);
			tx_buf_r = NULL;
		}

		if (rx_buf) {
			kfree(rx_buf);
			rx_buf = NULL;
		}

		if (tx_buf) {
			kfree(tx_buf);
			tx_buf = NULL;
		}

		if (rx_in_buf) {
			kfree(rx_in_buf);
			rx_in_buf = NULL;
		}

		if (rx_out_buf) {
			kfree(rx_out_buf);
			rx_out_buf = NULL;
		}

		return result;
}

void cc2520_radio_free()
{
	if (rx_buf_r) {
		kfree(rx_buf_r);
		rx_buf_r = NULL;
	}

	if (tx_buf_r) {
		kfree(tx_buf_r);
		tx_buf_r = NULL;
	}

	if (rx_buf) {
		kfree(rx_buf);
		rx_buf = NULL;
	}

	if (tx_buf) {
		kfree(tx_buf);
		tx_buf = NULL;
	}

	if (rx_in_buf) {
		kfree(rx_in_buf);
		rx_in_buf = NULL;
	}

	if (rx_out_buf) {
		kfree(rx_out_buf);
		rx_out_buf = NULL;
	}
}

void cc2520_radio_start()
{
	cc2520_radio_lock(CC2520_RADIO_STATE_CONFIG);
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
	cc2520_radio_unlock();
}

void cc2520_radio_on()
{
	cc2520_radio_lock(CC2520_RADIO_STATE_CONFIG);
	cc2520_radio_set_channel(channel & CC2520_CHANNEL_MASK);
	cc2520_radio_set_address(short_addr, extended_addr, pan_id);
	cc2520_radio_strobe(CC2520_CMD_SRXON);
	cc2520_radio_unlock();
}

void cc2520_radio_off()
{
	cc2520_radio_lock(CC2520_RADIO_STATE_CONFIG);
	cc2520_radio_strobe(CC2520_CMD_SRFOFF);
	cc2520_radio_unlock();
}

//////////////////////////////
// Configuration Commands
/////////////////////////////

bool cc2520_radio_is_clear()
{
	return gpio_get_value(CC2520_CCA) == 1;
}

void cc2520_radio_set_channel(int new_channel)
{
	cc2520_freqctrl_t freqctrl;

	channel = new_channel;
	freqctrl = cc2520_freqctrl_default;

	freqctrl.f.freq = 11 + 5 * (channel - 11);

	cc2520_radio_writeRegister(CC2520_FREQCTRL, freqctrl.value);
}

// Sets the short address
void cc2520_radio_set_address(u16 new_short_addr, u64 new_extended_addr, u16 new_pan_id)
{
	char addr_mem[12];

	short_addr = new_short_addr;
	extended_addr = new_extended_addr;
	pan_id = new_pan_id;

	memcpy(addr_mem, &extended_addr, 8);

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
	// incoming/outgoing packets. To be used later...
	sfd_nanos_ts = nano_timestamp;

	if (!is_high) {
		// SFD falling indicates TX completion
		// if we're currently in TX mode, unlock.
		if (cc2520_radio_tx_unlock_sfd()) {
			cc2520_radio_completeTx();
		}
	}
}

// context: interrupt
void cc2520_radio_fifop_occurred()
{

	spin_lock_irqsave(&pending_rx_sl, flags);;

	if (pending_rx) {
		spin_unlock_irqrestore(&pending_rx_sl, flags);;
	}
	else {
		pending_rx = true;
		spin_unlock_irqrestore(&pending_rx_sl, flags);;
		cc2520_radio_beginRx();
	}
}

void cc2520_radio_reset(void)
{
	// TODO.
}

//////////////////////////////
// Transmit Engine
/////////////////////////////

// context: process?
static int cc2520_radio_tx(u8 *buf, u8 len)
{
	DBG((KERN_INFO "[cc2520] - beginning write op.\n"));
	// capture exclusive radio rights to send
	// build the transmit command seq
	// write that packet!

	// 1- Stop Receiving
	// 2- Write Packet Header
	// 3- Turn on TX
	// 4- Write Rest of Packet
	// 5- On SFD falling edge give up lock

	// Beginning of TX critical section
	cc2520_radio_lock(CC2520_RADIO_STATE_TX);

	memcpy(tx_buf_r, buf, len);
	tx_buf_r_len = len;

	cc2520_radio_beginTx();
	return 0;
}

// Tx Part 1: Turn off the RF engine.
static void cc2520_radio_beginTx()
{
	int status;

	tsfer1.tx_buf = tx_buf;
	tsfer1.rx_buf = rx_buf;
	tsfer1.len = 0;
	tsfer1.cs_change = 1;
	tx_buf[tsfer1.len++] = CC2520_CMD_SRFOFF;

	spi_message_init(&msg);
	msg.complete = cc2520_radio_continueTx_check;
	msg.context = NULL;

	spi_message_add_tail(&tsfer1, &msg);

	status = spi_async(state.spi_device, &msg);
}

// Tx Part 2: Check for missed RX transmission
// and flush the buffer, actually write the data.
static void cc2520_radio_continueTx_check(void *arg)
{
	int status;
	int buf_offset;
	int i;

	buf_offset = 0;

	tsfer1.tx_buf = tx_buf + buf_offset;
	tsfer1.rx_buf = rx_buf + buf_offset;
	tsfer1.len = 0;
	tsfer1.cs_change = 1;

	if (gpio_get_value(CC2520_FIFO) == 1) {
		INFO((KERN_INFO "[cc2520] - tx/rx race condition adverted.\n"));
		tx_buf[buf_offset + tsfer1.len++] = CC2520_CMD_SFLUSHRX;
	}

	tx_buf[buf_offset + tsfer1.len++] = CC2520_CMD_TXBUF;

	// Length + FCF
	for (i = 0; i < 3; i++)
		tx_buf[buf_offset + tsfer1.len++] = tx_buf_r[i];
	buf_offset += tsfer1.len;

	tsfer2.tx_buf = tx_buf + buf_offset;
	tsfer2.rx_buf = rx_buf + buf_offset;
	tsfer2.len = 0;
	tsfer2.cs_change = 1;
	tx_buf[buf_offset + tsfer2.len++] = CC2520_CMD_STXON;
	buf_offset += tsfer2.len;

	// We're keeping these two SPI transactions separated
	// in case we later want to encode timestamp
	// information in the packet itself after seeing SFD
	// flag.
	if (tx_buf_r_len > 3) {
		tsfer3.tx_buf = tx_buf + buf_offset;
		tsfer3.rx_buf = rx_buf + buf_offset;
		tsfer3.len = 0;
		tsfer3.cs_change = 1;
		tx_buf[buf_offset + tsfer3.len++] = CC2520_CMD_TXBUF;
		for (i = 3; i < tx_buf_r_len; i++)
			tx_buf[buf_offset + tsfer3.len++] = tx_buf_r[i];

		buf_offset += tsfer3.len;
	}

	tsfer4.tx_buf = tx_buf + buf_offset;
	tsfer4.rx_buf = rx_buf + buf_offset;
	tsfer4.len = 0;
	tsfer4.cs_change = 1;
	tx_buf[buf_offset + tsfer4.len++] = CC2520_CMD_REGISTER_READ | CC2520_EXCFLAG0;
	tx_buf[buf_offset + tsfer4.len++] = 0;

	spi_message_init(&msg);
	msg.complete = cc2520_radio_continueTx;
	msg.context = NULL;

	spi_message_add_tail(&tsfer1, &msg);
	spi_message_add_tail(&tsfer2, &msg);

	if (tx_buf_r_len > 3)
		spi_message_add_tail(&tsfer3, &msg);

	spi_message_add_tail(&tsfer4, &msg);

	status = spi_async(state.spi_device, &msg);
}

static void cc2520_radio_continueTx(void *arg)
{
	DBG((KERN_INFO "[cc2520] - tx spi write callback complete.\n"));

	if ((((u8*)tsfer4.rx_buf)[1] & CC2520_TX_UNDERFLOW) > 0) {
		cc2520_radio_flushTx();
	}
	else if (cc2520_radio_tx_unlock_spi()) {
		// To prevent race conditions between the SPI engine and the
		// SFD interrupt we unlock in two stages. If this is the last
		// thing to complete we signal TX complete.
		cc2520_radio_completeTx();
	}
}

static void cc2520_radio_flushTx()
{
	int status;
	INFO((KERN_INFO "[cc2520] - tx underrun occurred.\n"));

	tsfer1.tx_buf = tx_buf;
	tsfer1.rx_buf = rx_buf;
	tsfer1.len = 0;
	tsfer1.cs_change = 1;
	tx_buf[tsfer1.len++] = CC2520_CMD_SFLUSHTX;
	tx_buf[tsfer1.len++] = CC2520_CMD_REGISTER_WRITE | CC2520_EXCFLAG0;
	tx_buf[tsfer1.len++] = 0;

	spi_message_init(&msg);
	msg.complete = cc2520_radio_completeFlushTx;
	msg.context = NULL;

	spi_message_add_tail(&tsfer1, &msg);

	status = spi_async(state.spi_device, &msg);
}

static void cc2520_radio_completeFlushTx(void *arg)
{
	cc2520_radio_unlock();
	DBG((KERN_INFO "[cc2520] - write op complete.\n"));
	radio_top->tx_done(-CC2520_TX_FAILED);
}

static void cc2520_radio_completeTx()
{
	cc2520_radio_unlock();
	DBG((KERN_INFO "[cc2520] - write op complete.\n"));
	radio_top->tx_done(CC2520_TX_SUCCESS);
}

//////////////////////////////
// Receiver Engine
/////////////////////////////

static void cc2520_radio_beginRx()
{
	int status;

	rx_tsfer.tx_buf = rx_out_buf;
	rx_tsfer.rx_buf = rx_in_buf;
	rx_tsfer.len = 0;
	rx_out_buf[rx_tsfer.len++] = CC2520_CMD_RXBUF;
	rx_out_buf[rx_tsfer.len++] = 0;

	rx_tsfer.cs_change = 1;

	memset(rx_in_buf, 0, SPI_BUFF_SIZE);

	spi_message_init(&rx_msg);
	rx_msg.complete = cc2520_radio_continueRx;
	rx_msg.context = NULL;
	spi_message_add_tail(&rx_tsfer, &rx_msg);

	status = spi_async(state.spi_device, &rx_msg);
}

static void cc2520_radio_continueRx(void *arg)
{
	int status;
	int i;
	int len;

	// Length of what we're reading is stored
	// in the received spi buffer, read from the
	// async operation called in beginRxRead.
	len = rx_in_buf[1];

	if (len > 127) {
		cc2520_radio_flushRx();
	}
	else {
		rx_tsfer.len = 0;
		rx_out_buf[rx_tsfer.len++] = CC2520_CMD_RXBUF;
		for (i = 0; i < len; i++)
			rx_out_buf[rx_tsfer.len++] = 0;

		rx_tsfer.cs_change = 1;

		spi_message_init(&rx_msg);
		rx_msg.complete = cc2520_radio_finishRx;
		// Platform dependent?
		rx_msg.context = (void*)len;
		spi_message_add_tail(&rx_tsfer, &rx_msg);

		status = spi_async(state.spi_device, &rx_msg);
	}
}

static void cc2520_radio_flushRx()
{

	int status;

	INFO((KERN_INFO "[cc2520] - flush RX FIFO (part 1).\n"));

	rx_tsfer.len = 0;
	rx_tsfer.cs_change = 1;
	rx_out_buf[rx_tsfer.len++] = CC2520_CMD_SFLUSHRX;

	spi_message_init(&rx_msg);
	rx_msg.complete = cc2520_radio_continueFlushRx;
	rx_msg.context = NULL;

	spi_message_add_tail(&rx_tsfer, &rx_msg);

	status = spi_async(state.spi_device, &rx_msg);
}

// Flush RX twice. This is due to Errata Bug 1 and to try to fix an issue where
// the radio goes into a state where it no longer receives any packets after
// clearing the RX FIFO when a packet arrives at the same time a packet
// is being processed.
// Also, both the TinyOS and Contiki implementations do this.
static void cc2520_radio_continueFlushRx(void* arg)
{
	int status;

	INFO((KERN_INFO "[cc2520] - flush RX FIFO (part 2).\n"));

	rx_tsfer.len = 0;
	rx_tsfer.cs_change = 1;
	rx_out_buf[rx_tsfer.len++] = CC2520_CMD_SFLUSHRX;

	spi_message_init(&rx_msg);
	rx_msg.complete = cc2520_radio_completeFlushRx;
	rx_msg.context = NULL;

	spi_message_add_tail(&rx_tsfer, &rx_msg);

	status = spi_async(state.spi_device, &rx_msg);
}

static void cc2520_radio_completeFlushRx(void *arg)
{
	spin_lock_irqsave(&pending_rx_sl, flags);
	pending_rx = false;
	spin_unlock_irqrestore(&pending_rx_sl, flags);
}

static void cc2520_radio_finishRx(void *arg)
{
	int len;

	len = (int)arg;

	// we keep a lock on the RX buffer separately
	// to allow for another rx packet to pile up
	// behind the current one.
	spin_lock(&rx_buf_sl);

	// Note: we place the len at the beginning
	// of the packet to make the interface symmetric
	// with the TX interface.
	rx_buf_r[0] = len;

	// Make sure to ignore the command return byte.
	memcpy(rx_buf_r + 1, rx_in_buf + 1, len);

	// Pass length of entire buffer to
	// upper layers.
	radio_top->rx_done(rx_buf_r, len + 1);

	DBG((KERN_INFO "[cc2520] - Read %d bytes from radio.\n", len));

	// For now if we received more than one RX packet we simply
	// clear the buffer, in the future we can move back to the scheme
	// where pending_rx is actually a FIFOP toggle counter and continue
	// to receive another packet. Only do this if it becomes a problem.
	if (gpio_get_value(CC2520_FIFO) == 1) {
		INFO((KERN_INFO "[cc2520] - more than one RX packet received, flushing buffer\n"));
		cc2520_radio_flushRx();
	}
	else {
		// Allow for subsequent FIFOP
		spin_lock_irqsave(&pending_rx_sl, flags);
		pending_rx = false;
		spin_unlock_irqrestore(&pending_rx_sl, flags);
	}
}

void cc2520_radio_release_rx()
{
	spin_unlock(&rx_buf_sl);
}

//////////////////////////////
// Helper Routines
/////////////////////////////

// Memory address MUST be >= 200.
static void cc2520_radio_writeMemory(u16 mem_addr, u8 *value, u8 len)
{
	int status;
	int i;

	tsfer.tx_buf = tx_buf;
	tsfer.rx_buf = rx_buf;
	tsfer.len = 0;

	tx_buf[tsfer.len++] = CC2520_CMD_MEMORY_WRITE | ((mem_addr >> 8) & 0xFF);
	tx_buf[tsfer.len++] = mem_addr & 0xFF;

	for (i=0; i<len; i++) {
		tx_buf[tsfer.len++] = value[i];
	}

	memset(rx_buf, 0, SPI_BUFF_SIZE);

	spi_message_init(&msg);
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
	msg.context = NULL;
	spi_message_add_tail(&tsfer, &msg);

	status = spi_sync(state.spi_device, &msg);
}

static cc2520_status_t cc2520_radio_strobe(u8 cmd)
{
	int status;
	cc2520_status_t ret;

	tsfer.tx_buf = tx_buf;
	tsfer.rx_buf = rx_buf;
	tsfer.len = 0;

	tx_buf[0] = cmd;
	tsfer.len = 1;

	memset(rx_buf, 0, SPI_BUFF_SIZE);

	spi_message_init(&msg);
	msg.context = NULL;
	spi_message_add_tail(&tsfer, &msg);

	status = spi_sync(state.spi_device, &msg);

	ret.value = rx_buf[0];
	return ret;
}
