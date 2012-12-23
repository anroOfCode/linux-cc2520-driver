#include "cc2520.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include "ioctl.h"
#include "cc2520.h"

// Should accept a 6LowPAN frame, no longer than 127 bytes.
static ssize_t interface_write(
	struct file *filp, const char *in_buf, size_t len, loff_t * off)
{
	int result;
	size_t pkt_len;

	printk(KERN_INFO "[cc2520] - beginning write\n");

	// Step 1: Get an exclusive lock on writing to the
	// radio.
	if (filp->f_flags & O_NONBLOCK) {
		result = down_trylock(&state.tx_sem);
		if (result)
			return -EAGAIN;
	}
	else {
		result = down_interruptible(&state.tx_sem);
		if (result)
			return -ERESTARTSYS;
	}
	printk(KERN_INFO "[cc2520] - write lock obtained.\n");

	// Step 2: Copy the packet to the incoming buffer.
	pkt_len = min(len, (size_t)127);
	if (copy_from_user(state.tx_buf_c, in_buf, pkt_len)) {
		result = -EFAULT;
		goto error;
	}
	state.tx_pkt_len = pkt_len;

	// Step 3: Launch off into sending this packet,
	// wait for an asynchronous callback to occur in
	// the form of a semaphore. 
	printk(KERN_INFO "[cc2520] - performing software ack test %d.\n", pkt_len);
	cc2520_sack_tx(state.tx_buf_c, pkt_len);
	result = down_interruptible(&state.tx_done_sem);
	if (result)
		return -ERESTARTSYS;

	// Step 4: Finally return and allow other callers to write
	// packets. 
	printk(KERN_INFO "[cc2520] - returned from sack cb.\n");
	up(&state.tx_sem);

	return state.tx_result ? state.tx_result : pkt_len;

	error:
		up(&state.tx_sem);
		return -EFAULT;
}

void cc2520_interface_write_cb(int result)
{
	state.tx_result = result;
	up(&state.tx_done_sem);
}

static ssize_t interface_read(struct file *filp, char __user *buff, size_t count,
			loff_t *offp)
{

	return 0;
}

static void interface_ioctl_set_channel(struct cc2520_set_channel_data *data)
{
	int result;
	struct cc2520_set_channel_data ldata;
	
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_channel_data));

	if (result) {
		printk(KERN_ALERT "[cc2520] - an error occurred setting the channel");
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
		printk(KERN_ALERT "[cc2520] - an error occurred setting the address");
		return;
	}

	printk(KERN_INFO "[cc2520] - setting addr: %d ext_addr: %lld pan_id: %d",
		ldata.short_addr, ldata.extended_addr, ldata.pan_id);
	cc2520_radio_set_address(ldata.short_addr, ldata.extended_addr, ldata.pan_id);
}

static void interface_ioctl_set_txpower(struct cc2520_set_txpower_data *data)
{
	int result;
	struct cc2520_set_txpower_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_txpower_data));

	if (result) {
		printk(KERN_ALERT "[cc2520] - an error occurred setting the txpower");
		return;
	}

	printk(KERN_INFO "[cc2520] - setting txpower: %d", ldata.txpower);
	cc2520_radio_set_txpower(ldata.txpower);
}

long interface_ioctl(struct file *file,
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

int cc2520_interface_init()
{
	int result; 

	sema_init(&state.tx_sem, 1);
	sema_init(&state.rx_sem, 1);

	sema_init(&state.tx_done_sem, 0);
	sema_init(&state.rx_done_sem, 0);

	state.tx_buf_c = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!state.tx_buf_c) {
		result = -EFAULT;
		goto error;
	}
		
	state.rx_buf_c = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);    
	if (!state.rx_buf_c) {
		result = -EFAULT;
		goto error;
	}

	state.major = register_chrdev(0, cc2520_name, &fops);
	printk(KERN_INFO "[cc2520] - Char interface registered on %d\n", state.major);
	return 0;

	error:

	if (state.rx_buf_c) {
		kfree(state.rx_buf_c);
		state.rx_buf_c = 0;		
	}

	if (state.tx_buf_c) {
		kfree(state.tx_buf_c);
		state.tx_buf_c = 0;
	}

	return result;
}

void cc2520_interface_free()
{
	int result;

	result = down_interruptible(&state.tx_sem);
	if (result) {
		printk("[cc2520] - critical error occurred on free.");
	}

	result = down_interruptible(&state.rx_sem);
	if (result) {
		printk("[cc2520] - critical error occurred on free.");
	}

	unregister_chrdev(state.major, cc2520_name);

	if (state.rx_buf_c) {
		kfree(state.rx_buf_c);
		state.rx_buf_c = 0;		
	}

	if (state.tx_buf_c) {
		kfree(state.tx_buf_c);
		state.tx_buf_c = 0;
	}
}