linux-cc2520-driver
===================

A kernel module that will (one day) power the CC2520 802.15.4 radio in Linux. 

The general idea is to create a character driver that exposes 802.15.4 frames
to the end user, allowing them to test and layer different networking stacks
from user-land onto the CC2520 radio. We're specifically targeting running
the IPv6 TinyOS networking stack on a Raspberry Pi.

To compile you'll need an ARM cross compiler and the source tree of a compiled
ARM kernel. Update the Makefile to point to your kernel source, and run the
following command:

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-

Please note that we're cross-compiling from an x86 machine.

Installing
----------

The installation process for this has gotten a little more involved as I continue
to tweak things for performance. You'll need to be running the 3.6 kernel from
the Raspberry Pi's GitHub (not the 3.2 standard version).

This requires cross-compiling the kernel from source. To do this I started with
the standard Raspbian image and replaced the kernel, modules, and firmware using
the steps from the "How to Compile from Source" page. 

You need the 3.6 kernel because we're using a newer SPI driver. This driver
supports a real-time workqueue and is interrupt driven. You can find
it here:

https://raw.github.com/msperl/linux/73afedc07e3b94d8b9d588912faf26081178d268/drivers/spi/spi-bcm2708.c

You'll need to replace the current driver file, as well as modify the board
configuration to enable the DMA parts of it.

Finally this driver needs a small patch because nobody seems to implement
chip-select toggling behavior correctly:

	if (!(flags | FLAGS_LAST_TRANSFER)) {
		bs->cs|=SPI_CS_TA|SPI_CS_INTR|SPI_CS_INTD;
	}

will become

	if (!(flags | FLAGS_LAST_TRANSFER)) {
		bs->cs|=SPI_CS_INTR|SPI_CS_INTD;
	}	

Not keeping the SPI_CS_TA flag active will have the SPI driver toggle
the CS pin between transmissions. The CC2520 radio requires a SPI
toggle to terminate command sequences that don't have a predetermined
length. 

*Eventually I'll try to make this a easier once things stabilize a little
bit. Sorry it's so chaotic!*

Current Status
---------------
The driver currently sends and receives packets! We get close to the maximum
theoretical transmission rate supported by the CC2520, which is just fantastic.

Work is currently focused on implementing LPL and automatic software
acknowledgements. The goal is to push as much as possible into userland
but some things need to be implemented in the kernel to enable strict
timing guarantees. 

Some notes
----------
  * You'll need to load the spi-bcm2708 driver for this module to work. You can do this by commenting out the line in the blacklisted modules file found in /etc.
  * Make sure to update the directory for the kernel in the Makefile
  * Update the GPIO defs in cc2520.h