#include "cc2520.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "ioctl.h"
#include "cc2520.h"
#include "interface.h"
#include "radio.h"
#include "sack.h"
#include "csma.h"
#include "lpl.h"
#include "debug.h"

struct cc2520_interface *interface_bottom;

static unsigned int major;
static dev_t char_d_mm;
static struct cdev char_d_cdev;
static struct class* cl;
static struct device* de;

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

DECLARE_WAIT_QUEUE_HEAD(cc2520_interface_read_queue);

static void cc2520_interface_tx_done(u8 status);
static void cc2520_interface_rx_done(u8 *buf, u8 len);

static void interface_ioctl_set_channel(struct cc2520_set_channel_data *data);
static void interface_ioctl_set_address(struct cc2520_set_address_data *data);
static void interface_ioctl_set_txpower(struct cc2520_set_txpower_data *data);
static void interface_ioctl_set_ack(struct cc2520_set_ack_data *data);
static void interface_ioctl_set_lpl(struct cc2520_set_lpl_data *data);
static void interface_ioctl_set_csma(struct cc2520_set_csma_data *data);
static void interface_ioctl_set_print(struct cc2520_set_print_messages_data *data);


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

static void interface_print_to_log(char *buf, int len, bool is_write)
{
	char print_buf[641];
	char *print_buf_ptr;
	int i;

	print_buf_ptr = print_buf;

	for (i = 0; i < len && i < 128; i++) {
		print_buf_ptr += sprintf(print_buf_ptr, " 0x%02X", buf[i]);
	}
	*(print_buf_ptr) = '\0';

	if (is_write)
		INFO((KERN_INFO "[cc2520] - write: %s\n", print_buf));
	else
		INFO((KERN_INFO "[cc2520] - read: %s\n", print_buf));
}

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

	if (debug_print >= DEBUG_PRINT_DBG) {
		interface_print_to_log(tx_buf_c, pkt_len, true);
	}

	// Step 3: Launch off into sending this packet,
	// wait for an asynchronous callback to occur in
	// the form of a semaphore.
	interface_bottom->tx(tx_buf_c, pkt_len);
	down(&tx_done_sem);

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

	if (debug_print >= DEBUG_PRINT_DBG) {
		interface_print_to_log(rx_buf_c, rx_pkt_len, false);
	}

	return rx_pkt_len;
}

static long interface_ioctl(struct file *file,
		 unsigned int ioctl_num,
		 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case CC2520_IO_RADIO_INIT:
			INFO((KERN_INFO "[cc2520] - radio starting\n"));
			cc2520_radio_start();
			break;
		case CC2520_IO_RADIO_ON:
			INFO((KERN_INFO "[cc2520] - radio turning on\n"));
			cc2520_radio_on();
			break;
		case CC2520_IO_RADIO_OFF:
			INFO((KERN_INFO "[cc2520] - radio turning off\n"));
			cc2520_radio_off();
			break;
		case CC2520_IO_RADIO_SET_CHANNEL:
			interface_ioctl_set_channel((struct cc2520_set_channel_data*) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_ADDRESS:
			interface_ioctl_set_address((struct cc2520_set_address_data*) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_TXPOWER:
			interface_ioctl_set_txpower((struct cc2520_set_txpower_data*) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_ACK:
			interface_ioctl_set_ack((struct cc2520_set_ack_data*) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_LPL:
			interface_ioctl_set_lpl((struct cc2520_set_lpl_data*) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_CSMA:
			interface_ioctl_set_csma((struct cc2520_set_csma_data*) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_PRINT:
			interface_ioctl_set_print((struct cc2520_set_print_messages_data*) ioctl_param);
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
static void interface_ioctl_set_print(struct cc2520_set_print_messages_data *data)
{
	int result;
	struct cc2520_set_print_messages_data ldata;

	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_print_messages_data));

	if (result) {
		ERR((KERN_ALERT "[cc2520] - an error occurred setting print messages\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - setting debug message print: %i", ldata.debug_level));

	debug_print = ldata.debug_level;
}

static void interface_ioctl_set_channel(struct cc2520_set_channel_data *data)
{
	int result;
	struct cc2520_set_channel_data ldata;

	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_channel_data));

	if (result) {
		ERR((KERN_ALERT "[cc2520] - an error occurred setting the channel\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - Setting channel to %d\n", ldata.channel));
	cc2520_radio_set_channel(ldata.channel);
}

static void interface_ioctl_set_address(struct cc2520_set_address_data *data)
{
	int result;
	struct cc2520_set_address_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_address_data));

	if (result) {
		ERR((KERN_ALERT "[cc2520] - an error occurred setting the address\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - setting addr: %d ext_addr: %lld pan_id: %d\n",
		ldata.short_addr, ldata.extended_addr, ldata.pan_id));
	cc2520_radio_set_address(ldata.short_addr, ldata.extended_addr, ldata.pan_id);
}

static void interface_ioctl_set_txpower(struct cc2520_set_txpower_data *data)
{
	int result;
	struct cc2520_set_txpower_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_txpower_data));

	if (result) {
		ERR((KERN_ALERT "[cc2520] - an error occurred setting the txpower\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - setting txpower: %d\n", ldata.txpower));
	cc2520_radio_set_txpower(ldata.txpower);
}

static void interface_ioctl_set_ack(struct cc2520_set_ack_data *data)
{
	int result;
	struct cc2520_set_ack_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_ack_data));

	if (result) {
		ERR((KERN_INFO "[cc2520] - an error occurred setting soft ack\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - setting softack timeout: %d\n", ldata.timeout));
	cc2520_sack_set_timeout(ldata.timeout);
}

static void interface_ioctl_set_lpl(struct cc2520_set_lpl_data *data)
{
	int result;
	struct cc2520_set_lpl_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_lpl_data));

	if (result) {
		ERR((KERN_INFO "[cc2520] - an error occurred setting lpl\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - setting lpl enabled: %d, window: %d, interval: %d\n",
		ldata.enabled, ldata.window, ldata.interval));
	cc2520_lpl_set_enabled(ldata.enabled);
	cc2520_lpl_set_listen_length(ldata.window);
	cc2520_lpl_set_wakeup_interval(ldata.interval);
}

static void interface_ioctl_set_csma(struct cc2520_set_csma_data *data)
{
	int result;
	struct cc2520_set_csma_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_csma_data));

	if (result) {
		ERR((KERN_INFO "[cc2520] - an error occurred setting csma\n"));
		return;
	}

	INFO((KERN_INFO "[cc2520] - setting csma enabled: %d, min_backoff: %d, init_backoff: %d, cong_backoff_ %d\n",
		ldata.enabled, ldata.min_backoff, ldata.init_backoff, ldata.cong_backoff));
	cc2520_csma_set_enabled(ldata.enabled);
	cc2520_csma_set_min_backoff(ldata.min_backoff);
	cc2520_csma_set_init_backoff(ldata.init_backoff);
	cc2520_csma_set_cong_backoff(ldata.cong_backoff);
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

	// Allocate a major number for this device
	result = alloc_chrdev_region(&char_d_mm, 0, 1, cc2520_name);
	if (result < 0) {
		ERR((KERN_INFO "[cc2520] - Could not allocate a major number\n"));
		goto error;
	}
	major = MAJOR(char_d_mm);

	// Register the character device
	cdev_init(&char_d_cdev, &fops);
	char_d_cdev.owner = THIS_MODULE;
	result = cdev_add(&char_d_cdev, char_d_mm, 1);
	if (result < 0) {
		ERR((KERN_INFO "[cc2520] - Unable to register char dev\n"));
		goto error;
	}
	INFO((KERN_INFO "[cc2520] - Char interface registered on %d\n", major));

	cl = class_create(THIS_MODULE, "cc2520");
	if (cl == NULL) {
		ERR((KERN_INFO "[cc2520] - Could not create device class\n"));
		goto error;
	}

	// Create the device in /dev/radio
	de = device_create(cl, NULL, char_d_mm, NULL, "radio");
	if (de == NULL) {
		ERR((KERN_INFO "[cc2520] - Could not create device\n"));
		goto error;
	}

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
		ERR(("[cc2520] - critical error occurred on free."));
	}

	result = down_interruptible(&rx_sem);
	if (result) {
		ERR(("[cc2520] - critical error occurred on free."));
	}

	cdev_del(&char_d_cdev);
	unregister_chrdev(char_d_mm, cc2520_name);
	device_destroy(cl, char_d_mm);
	class_destroy(cl);


	INFO((KERN_INFO "[cc2520] - Removed character device\n"));

	if (rx_buf_c) {
		kfree(rx_buf_c);
		rx_buf_c = 0;
	}

	if (tx_buf_c) {
		kfree(tx_buf_c);
		tx_buf_c = 0;
	}
}
