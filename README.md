linux-cc2520-driver
===================

A kernel module that powers the CC2520 802.15.4 radio in Linux.

The general idea is to create a character driver that exposes 802.15.4 frames
to the end user, allowing them to test and layer different networking stacks
from user-land onto the CC2520 radio. We're specifically targeting running
the IPv6 TinyOS networking stack on a Raspberry Pi.


Installation for the Raspberry Pi
---------------------------------


To compile you'll need an ARM cross compiler and the source tree of a compiled
ARM kernel. Update the Makefile to point to your kernel source, and run the
following command:

    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-

Please note that we're cross-compiling from an x86 machine.



### Compile


Currently this process requires compiling against the 3.6 version of the Linux
kernel from the Raspberry Pi's GitHub.

#### Step One: Get the sources

Start by creating a folder to group all of the code needed to compile this
kernel module.

    mkdir rpi
    cd rpi

Now get the source. You'll need to get the Raspberry Pi 3.6 kernel source and
this repository.

    git clone git://github.com/raspberrypi/linux.git
    git clone git://github.com/ab500/linux-cc2520-driver.git


#### Step Two: Patch the linux source

Now you need to patch two files in the linux tree. The first changes a small
part of the device configuration to let the SPI driver know where it can find
DMA-accessible memory. The second updates the spi driver to msperl's version and
changes the spi driver to deassert the chip-select line. Not keeping the
`SPI_CS_TA` flag active will have the SPI driver toggle the CS pin between
transmissions. The CC2520 radio requires a SPI toggle to terminate command
sequences that don't have a predetermined length.

    cd linux
    git apply ../linux-cc2520-driver/patches/bcm2708.patch
    git apply ../linux-cc2520-driver/patches/spi-bcm2708.patch

#### Step Three: Compile the kernel

Finally you're ready to compile the kernel for the Pi. First make sure you have
the right build environment. You'll need an ARM GCC cross-compiler installed
(arm-linux-gnueabi-gcc).

Follow the instructions on the Raspberry Pi main site under the section
"Perform the compilation". You do not need to transfer all of the modules or
anything, but you do have to build them.

http://elinux.org/RPi_Kernel_Compilation#Perform_the_compilation


#### Step Four: Compile this module

To compile this module update the `Makefile` with the path of the RPi linux
source. Then:

    cd ~\rpi\linux-cc2520-driver
    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi-


### Install

To install you will want to start with an RPi already running Raspbian. Make
sure you are using version `2013-02-09-wheezy-raspbian` or later.

#### Step One: Copy files

You need to copy the recompiled kernel, the SPI kernel module, and the
CC2520 kernel module to the RPi.

    cd ~/rpi
    scp linux/arch/arm/boot/Image <RPI>:/boot/kernel.img
    scp linux/drivers/spi/spi-bcm2708.ko <RPI>:/lib/modules/3.6.11+/kernel/drivers/spi/
    scp linux-cc2520-driver/cc2520.ko <RPI>:/lib/modules/3.6.11+/kernel/drivers/misc/

#### Step Two: Enable the drivers

Now that we've gotten the kernel installed we need to enable automatic loading
of the SPI driver at boot. Edit `/etc/modprobe.d/raspi-blacklist.conf`
and comment out the line that blacklists the SPI driver from boot. It should look
like this when you're done:

    # blacklist spi and i2c by default (many users don't need them)
    #blacklist spi-bcm2708
    blacklist i2c-bcm2708

Now enable the `cc2520` driver at boot. Edit `/etc/modules` to look like:

    # /etc/modules: kernel modules to load at boot time.
    #
    # This file contains the names of kernel modules that should be loaded
    # at boot time, one per line. Lines beginning with "#" are ignored.
    # Parameters can be specified after the module name.

    snd-bcm2835
    cc2520

Lastly, force the system to find the new cc2520.ko driver:

    sudo depmod -a


### Test

Now that everything is setup you can run the driver and test that it works.


#### Step One: Move the tests to the RPi

To do this I use SFTP.

    sftp pi@your-pi-hostname

    sftp> put tests/read.c
    sftp> put tests/write.c


#### Step Two: Compile the tests

    gcc write.c -o write
    gcc read.c -o read


#### Step Three: Run the tests

First check the <code>kern.log</code> file for the debug output above. Also make sure your raspberry pi
hasn't frozen and paniced. You're doing pretty good.

Next you'll need another 802.15.4 mote. I've been using Epic-based devices. Use the SimpleSackTest
app in the shed and install it on a mote. It simply sends an 802.15.4 packet with an incrementing
counter in it, requesting an ACK, and will ACK any packets sent to it. On a standard mote the LEDs
are setup to indicate the following:

  * **Blue** - Toggles every time a packet is properly acknowledged by the radio.
  * **Green** - Toggles every time a packet is sent by the mote.
  * **Red** - Toggles every time the mote successfully receives a packet.

When the RPi radio is off only the green light will blink. Running the write and read apps
on the RPi will cause all the lights to blink.

The output will look something like this:

**read.c**

    Receiving a test message...
    result 14
    read  0x0D 0x61 0x88 0xD4 0x22 0x00 0x01 0x00 0x01 0x00 0x76 0xD5 0x13 0xEB
    Receiving a test message...
    result 14
    read  0x0D 0x61 0x88 0xD5 0x22 0x00 0x01 0x00 0x01 0x00 0x76 0xD6 0x13 0xEB
    Receiving a test message...
    result 14
    read  0x0D 0x61 0x88 0xD6 0x22 0x00 0x01 0x00 0x01 0x00 0x76 0xD7 0x13 0xEB
    Receiving a test message...
    result 14
    read  0x0D 0x61 0x88 0xD7 0x22 0x00 0x01 0x00 0x01 0x00 0x76 0xD8 0x13 0xEA

**write.c**

    Sending a test message...
    result 12
    Sending a test message...
    result 12
    Sending a test message...
    result 12
    Sending a test message...
    result 12
    Sending a test message...
    result 12
    Sending a test message...
    result 12


Current Status
---------------
The driver is starting to shape up to be feature-complete. Looking for obscure
timing bugs at the moment, but we support LPL, CSMA/CA, unique filtering, and
Soft-ACK features. It's basically a fully-featured radio implementing something
that's really close to the default TinyOS radio stack.

It runs more-or-less a generic, standard MAC layer. Nothing fancy.


