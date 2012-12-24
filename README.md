linux-cc2520-driver
===================

A kernel module that will (one day) power the CC2520 802.15.4 radio in Linux. 

The general idea is to create a character driver that exposes 802.15.4 frames
to the end user, allowing them to test and layer different networking stacks
from user-land onto the CC2520 radio. We're specifically targetting running
the IPv6 TinyOS networking stack on a Raspberry Pi.

To compile you'll need an ARM cross compiler and the source tree of a compiled
ARM kernel. Update the Makefile to point to your kernel source, and run the
following command:

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-

Please note that we're cross-compiling from an x86 machine.

Some notes
----------
  * You'll need to load the spi-bcm2708 driver for this module to work. You can do this with modprobe spi-bcm2708.
  * I made a small patch to the spi-bcm2708 driver. It previously didn't allow leaving the CS line high after a spi_message completed:
  	spi-bcm2709.c:315:
	if (xfer->cs_change) {
	//if (list_is_last(&xfer->transfer_list, &msg->transfers) ||
	//		xfer->cs_change) {
  * Make sure to update the directory for the kernel in the Makefile
  * Update the GPIO defs in cc2520.h