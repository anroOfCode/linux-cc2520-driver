#include "cc2520.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "ioctl.h"
#include "cc2520.h"
#include "interface.h"
#include "radio.h"

struct cc2520_interface *interface_bottom;

static unsigned int major;
static u8 *tx_buf_c;
static u8 *rx_buf_c;
static size_t tx_pkt_len;
static size_t rx_pkt_len;

// Allows for only a single rx or tx
// to occur simultaneously. 
static struct semaphore tx_sem;
static struct semaphore rx_sem;

// Used by the character driver
// to indicate when a blocking tx
// or rx has completed. 
static struct semaphore tx_done_sem;
static struct semaphore rx_done_sem;

// Results, stored by the callbacks
static int tx_result;
static int rx_result;

DECLARE_WAIT_QUEUE_HEAD(cc2520_interface_read_queue);

static void cc2520_interface_tx_done(u8 status);
static void cc2520_interface_rx_done(u8 *buf, u8 len);

static void interface_ioctl_set_channel(struct cc2520_set_channel_data *data);
static void interface_ioctl_set_address(struct cc2520_set_address_data *data);
static void interface_ioctl_set_txpower(struct cc2520_set_txpower_data *data);

static long interface_ioctl(struct file *file,
		 unsigned int ioctl_num,
		 unsigned long ioctl_param);

///////////////////////
// Interface callbacks
///////////////////////
void cc2520_interface_tx_done(u8 status)
{
	tx_result = status;
	up(&tx_done_sem);
}

void cc2520_interface_rx_done(u8 *buf, u8 len)
{
	rx_pkt_len = (size_t)len;
	memcpy(rx_buf_c, buf, len);
	wake_up(&cc2520_interface_read_queue);
}

////////////////////
// Implementation
////////////////////

// Should accept a 6LowPAN frame, no longer than 127 bytes.
static ssize_t interface_write(
	struct file *filp, const char *in_buf, size_t len, loff_t * off)
{
	int result;
	size_t pkt_len;

	DBG((KERN_INFO "[cc2520] - beginning write\n"));

	// Step 1: Get an exclusive lock on writing to the
	// radio.
	if (filp->f_flags & O_NONBLOCK) {
		result = down_trylock(&tx_sem);
		if (result)
			return -EAGAIN;
	}
	else {
		result = down_interruptible(&tx_sem);
		if (result)
			return -ERESTARTSYS;
	}
	DBG((KERN_INFO "[cc2520] - write lock obtained.\n"));

	// Step 2: Copy the packet to the incoming buffer.
	pkt_len = min(len, (size_t)128);
	if (copy_from_user(tx_buf_c, in_buf, pkt_len)) {
		result = -EFAULT;
		goto error;
	}
	tx_pkt_len = pkt_len;

	// Step 3: Launch off into sending this packet,
	// wait for an asynchronous callback to occur in
	// the form of a semaphore. 
	interface_bottom->tx(tx_buf_c, pkt_len);
	result = down_interruptible(&tx_done_sem);
	if (result)
		return -ERESTARTSYS;

	// Step 4: Finally return and allow other callers to write
	// packets. 
	DBG((KERN_INFO "[cc2520] - wrote %d bytes.\n", pkt_len));
	up(&tx_sem);

	return tx_result ? tx_result : pkt_len;

	error:
		up(&tx_sem);
		return -EFAULT;
}

static ssize_t interface_read(struct file *filp, char __user *buf, size_t count,
			loff_t *offp)
{
	interruptible_sleep_on(&cc2520_interface_read_queue);
	if (copy_to_user(buf, rx_buf_c, rx_pkt_len))
		return -EFAULT;

	return rx_pkt_len;
}

static long interface_ioctl(struct file *file,
		 unsigned int ioctl_num,
		 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case CC2520_IO_RADIO_INIT:
			printk(KERN_INFO "[cc2520] - radio starting\n");
			cc2520_radio_start();
			break;
		case CC2520_IO_RADIO_ON:
			printk(KERN_INFO "[cc2520] - radio turning on\n");
			cc2520_radio_on();
			break;
		case CC2520_IO_RADIO_OFF:
			printk(KERN_INFO "[cc2520] - radio turning off\n");
			cc2520_radio_off();
			break;
		case CC2520_IO_RADIO_SET_CHANNEL:
			interface_ioctl_set_channel((struct cc2520_set_channel_data *) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_ADDRESS:
			interface_ioctl_set_address((struct cc2520_set_address_data *) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_TXPOWER:
			interface_ioctl_set_txpower((struct cc2520_set_txpower_data *) ioctl_param);
			break;
	}

	return 0;
}

struct file_operations fops = {
	.read = interface_read,
	.write = interface_write,
	.unlocked_ioctl = interface_ioctl,
	.open = NULL,
	.release = NULL
};

/////////////////
// IOCTL Handlers
///////////////////

static void interface_ioctl_set_channel(struct cc2520_set_channel_data *data)
{
	int result;
	struct cc2520_set_channel_data ldata;
	
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_channel_data));

	if (result) {
		printk(KERN_ALERT "[cc2520] - an error occurred setting the channel\n");
		return;
	}

	printk(KERN_INFO "[cc2520] - Setting channel to %d\n", ldata.channel);
	cc2520_radio_set_channel(ldata.channel);
}

static void interface_ioctl_set_address(struct cc2520_set_address_data *data)
{
	int result;
	struct cc2520_set_address_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_address_data));

	if (result) {
		printk(KERN_ALERT "[cc2520] - an error occurred setting the address\n");
		return;
	}

	printk(KERN_INFO "[cc2520] - setting addr: %d ext_addr: %lld pan_id: %d\n",
		ldata.short_addr, ldata.extended_addr, ldata.pan_id);
	cc2520_radio_set_address(ldata.short_addr, ldata.extended_addr, ldata.pan_id);
}

static void interface_ioctl_set_txpower(struct cc2520_set_txpower_data *data)
{
	int result;
	struct cc2520_set_txpower_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_txpower_data));

	if (result) {
		printk(KERN_ALERT "[cc2520] - an error occurred setting the txpower\n");
		return;
	}

	printk(KERN_INFO "[cc2520] - setting txpower: %d\n", ldata.txpower);
	cc2520_radio_set_txpower(ldata.txpower);
}

/////////////////
// init/free 
///////////////////

int cc2520_interface_init()
{
	int result; 

	interface_bottom->tx_done = cc2520_interface_tx_done;
	interface_bottom->rx_done = cc2520_interface_rx_done;

	sema_init(&tx_sem, 1);
	sema_init(&rx_sem, 1);

	sema_init(&tx_done_sem, 0);
	sema_init(&rx_done_sem, 0);

	tx_buf_c = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!tx_buf_c) {
		result = -EFAULT;
		goto error;
	}
		
	rx_buf_c = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);    
	if (!rx_buf_c) {
		result = -EFAULT;
		goto error;
	}

	major = register_chrdev(0, cc2520_name, &fops);
	printk(KERN_INFO "[cc2520] - Char interface registered on %d\n", major);
	return 0;

	error:

	if (rx_buf_c) {
		kfree(rx_buf_c);
		rx_buf_c = 0;		
	}

	if (tx_buf_c) {
		kfree(tx_buf_c);
		tx_buf_c = 0;
	}

	return result;
}

void cc2520_interface_free()
{
	int result;

	result = down_interruptible(&tx_sem);
	if (result) {
		printk("[cc2520] - critical error occurred on free.");
	}

	result = down_interruptible(&rx_sem);
	if (result) {
		printk("[cc2520] - critical error occurred on free.");
	}

	unregister_chrdev(major, cc2520_name);

	if (rx_buf_c) {
		kfree(rx_buf_c);
		rx_buf_c = 0;		
	}

	if (tx_buf_c) {
		kfree(tx_buf_c);
		tx_buf_c = 0;
	}
}