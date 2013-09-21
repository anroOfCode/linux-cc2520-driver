#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>

#include "cc2520.h"
#include "radio.h"
#include "platform.h"
#include "lpl.h"
#include "interface.h"
#include "sack.h"
#include "csma.h"
#include "unique.h"
#include "debug.h"

#define DRIVER_AUTHOR  "Andrew Robinson <androbin@umich.edu>"
#define DRIVER_DESC    "A driver for the CC2520 radio."
#define DRIVER_VERSION "0.5"

uint8_t debug_print;

struct cc2520_state state;
const char cc2520_name[] = "cc2520";

struct cc2520_interface interface_to_unique;
struct cc2520_interface unique_to_lpl;
struct cc2520_interface lpl_to_csma;
struct cc2520_interface csma_to_sack;
struct cc2520_interface sack_to_radio;

void setup_bindings(void)
{
	radio_top = &sack_to_radio;
	sack_bottom = &sack_to_radio;
	sack_top = &csma_to_sack;
	csma_bottom = &csma_to_sack;
	csma_top = &lpl_to_csma;
	lpl_bottom = &lpl_to_csma;
	lpl_top = &unique_to_lpl;
	unique_bottom = &unique_to_lpl;
	unique_top = &interface_to_unique;
	interface_bottom = &interface_to_unique;
}

int init_module()
{
	int err = 0;

	debug_print = DEBUG_PRINT_INFO;

	setup_bindings();

	memset(&state, 0, sizeof(struct cc2520_state));

	INFO((KERN_INFO "[CC2520] - Loading kernel module v%s\n", DRIVER_VERSION));

	err = cc2520_plat_gpio_init();
	if (err) {
		ERR((KERN_ALERT "[CC2520] - gpio driver error. aborting.\n"));
		goto error7;
	}

	err = cc2520_plat_spi_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - spi driver error. aborting.\n"));
		goto error6;
	}

	err = cc2520_interface_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - char driver error. aborting.\n"));
		goto error5;
	}

	err = cc2520_radio_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - radio init error. aborting.\n"));
		goto error4;
	}

	err = cc2520_lpl_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - lpl init error. aborting.\n"));
		goto error3;
	}

	err = cc2520_sack_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - sack init error. aborting.\n"));
		goto error2;
	}

	err = cc2520_csma_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - csma init error. aborting.\n"));
		goto error1;
	}

	err = cc2520_unique_init();
	if (err) {
		ERR((KERN_ALERT "[cc2520] - unique init error. aborting.\n"));
		goto error0;
	}

	state.wq = create_singlethread_workqueue(cc2520_name);

	return 0;

	error0:
		cc2520_csma_free();
	error1:
		cc2520_sack_free();
	error2:
		cc2520_lpl_free();
	error3:
		cc2520_radio_free();
	error4:
		cc2520_interface_free();
	error5:
		cc2520_plat_spi_free();
	error6:
		cc2520_plat_gpio_free();
	error7:
		return -1;
}

void cleanup_module()
{
	destroy_workqueue(state.wq);
	cc2520_interface_free();
	cc2520_plat_gpio_free();
	cc2520_plat_spi_free();
	INFO((KERN_INFO "[cc2520] - Unloading kernel module\n"));
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
