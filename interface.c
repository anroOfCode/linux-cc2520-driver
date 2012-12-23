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
	// If BLOCK tx_semi_down
	// If NBLOCK try_tx_semi_down

	// Copy data to driver message buffer
	// Invoke next layer transmission 
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

	pkt_len = max(len, (size_t)127);

	if (copy_from_user(state.tx_buf_c, in_buf, pkt_len)) {
		result = -EFAULT;
		goto error;
	}

	return pkt_len;

	error:
		up(&state.tx_sem);
		return -EFAULT;
}

static ssize_t interface_read(struct file *filp, char __user *buff, size_t count,
			loff_t *offp)
{

	return 0;
}

static void interface_ioctl_set_channel(struct cc2520_set_channel_data * data)
{
	int result;
	struct cc2520_set_channel_data ldata;
	
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_channel_data));

	if (result) {
		printk(KERN_INFO "[cc2520] - an error occurred setting the channel");
		return;
	}

	printk(KERN_INFO "[cc2520] - Setting channel to %d\n", ldata.channel);
	cc2520_radio_set_channel(ldata.channel);
}

static void interface_ioctl_set_address(struct cc2520_set_address_data * data)
{
	int result;
	struct cc2520_set_address_data ldata;
	result = copy_from_user(&ldata, data, sizeof(struct cc2520_set_address_data));

	if (result) {
		printk(KERN_INFO "[cc2520] - an error occurred setting the address");
		return;
	}

	printk(KERN_INFO "[cc2520] - Setting addr: %d ext_addr: %lld pan_id: %d",
		ldata.short_addr, ldata.extended_addr, ldata.pan_id);
	cc2520_radio_set_address(ldata.short_addr, ldata.extended_addr, ldata.pan_id);
}

long interface_ioctl(struct file *file,
		 unsigned int ioctl_num,
		 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case CC2520_IO_RADIO_INIT:
			printk(KERN_INFO "[cc2520] - Radio Initializing\n");
			cc2520_radio_init();
			break;
		case CC2520_IO_RADIO_ON:
			printk(KERN_INFO "[cc2520] - Radio turning on\n");
			cc2520_radio_on();
			break;
		case CC2520_IO_RADIO_OFF:
			printk(KERN_INFO "[cc2520] - Radio turning off\n");
			cc2520_radio_off();
			break;
		case CC2520_IO_RADIO_SET_CHANNEL:
			interface_ioctl_set_channel((struct cc2520_set_channel_data *) ioctl_param);
			break;
		case CC2520_IO_RADIO_SET_ADDRESS:
			interface_ioctl_set_address((struct cc2520_set_address_data *) ioctl_param);
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

    state.tx_buf = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);
    if (!state.tx_buf) {
        result = -EFAULT;
        goto error;
    }
        
    state.rx_buf = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);    
    if (!state.rx_buf) {
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
	down(&state.tx_sem);
	down(&state.rx_sem);

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