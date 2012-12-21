#include "cc2520.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>

#include "ioctl.h"
#include "cc2520.h"

static ssize_t interface_write(
	struct file *filp, const char *in_buf, size_t len, loff_t * off)
{
	
	printk(KERN_INFO "Data received\n");
	return len;
}

static ssize_t interface_read(struct file *filp, char __user *buff, size_t count,
			loff_t *offp)
{

	return 0;
}

long interface_ioctl(struct file *file,
		 unsigned int ioctl_num,
		 unsigned long ioctl_param)
{
	switch (ioctl_num) {
		case CC2520_IO_RADIO_INIT:
			printk(KERN_INFO "[cc2520] - Radio Initializing...\n");
			cc2520_radio_init();
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
	state.major = register_chrdev(0, cc2520_name, &fops);
	printk(KERN_INFO "[cc2520] - Char interface registered on %d\n", state.major);
	return 0;
}

void cc2520_interface_free()
{
	unregister_chrdev(state.major, cc2520_name);
}