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

**Instructions for installation**
These instructions assume you have almost nothing installed, if you're comfortable
and have a preexisting build environment then skip a lot of the preliminary stuff.

### Step One: Setup a 3.6 Kernel Source
First you'll need to get the Raspberry Pi 3.6 kernel source tree onto your computer.
To do this make some new folders 

```
mkdir rpi
cd rpi
```

Then clone the 3.6.y kernel. We use this kernel because it has a much better implementation
of the generic spi driver. We're going to use the code by msperl to get a really low-latency
SPI implementation. 

```
git clone git://github.com/raspberrypi/linux.git
git checkout rpi-3.6.y
```

Next replace the current SPI driver with the driver provided lovingly by msperl:

```
wget https://raw.github.com/msperl/linux/73afedc07e3b94d8b9d588912faf26081178d268/drivers/spi/spi-bcm2708.c
mv spi-bcm2708.c linux/drivers/spi
```

And patch a small part of the device configuration to let the SPI driver know
where it can find DMA-accessible memory. Open file
<code>arch/arm/mach-bcm2708/bcm2708.c</code> and scroll to line 582. Update
the struct to look like this:

```
static struct platform_device bcm2708_spi_device = {
  .name = "bcm2708_spi",
  .id = 0,
  .num_resources = ARRAY_SIZE(bcm2708_spi_resources),
  .resource = bcm2708_spi_resources,
  .dev = {
    .coherent_dma_mask = DMA_BIT_MASK(DMA_MASK_BITS_COMMON),
  },
};
```

I patched a small part of this driver to desert the chip-select line. Find the following lines 
in spi-bcm-2708.c and replace them.

**Find:**

```
	if (!(flags | FLAGS_LAST_TRANSFER)) {
		bs->cs|=SPI_CS_TA|SPI_CS_INTR|SPI_CS_INTD;
	}
```

**Replace With:**

```
	if (!(flags | FLAGS_LAST_TRANSFER)) {
		bs->cs|=SPI_CS_INTR|SPI_CS_INTD;
	}	
```

Not keeping the SPI_CS_TA flag active will have the SPI driver toggle
the CS pin between transmissions. The CC2520 radio requires a SPI
toggle to terminate command sequences that don't have a predetermined
length. 

Finally you're ready to compile the kernel for the Pi. First make sure you have the right build
environment. You'll need an ARM GCC cross-compiler installed (arm-linux-gnueabi-gcc).

Follow the instructions on the Raspberry Pi main site here, ignoring the part that checks out the
kernel from source, you already have the kernel on your machine. 

http://elinux.org/RPi_Kernel_Compilation

**Some Tips**
  * I found the best way to do things was to start with the Raspbian image for the 3.2 kernel,
    make sure it works on the Pi, and then mount the SD card on your computer and replace
    the kernel. To do this you'll need to replace the kernel on the boot partition, the modules
    directory, and the firmware. The tutorial above walks you through almost all of this. 
  * I would checkout the tools and firmware repositories into your rpi directory just because
    it's a little cleaner and keeps everything in the same place. 

When you're done, log into your pi and make sure you're now running the 3.6 kernel:

```
pi@raspberrypi ~ $ uname -a
Linux raspberrypi 3.6.11+ #3 PREEMPT Tue Dec 25 13:31:30 EST 2012 armv6l GNU/Linux
```

### Step Two: Enable the SPI driver at boot
Now that we've gotten the kernel installed we need to enable automatic loading
of the SPI driver at boot. Edit <code>/etc/modprobe.d/raspi-blacklist.conf</code>
and comment out the line that blacklists the SPI driver from boot. It should look
like this when you're done:

```
# blacklist spi and i2c by default (many users don't need them)

#blacklist spi-bcm2708
blacklist i2c-bcm2708
```

Reboot the Pi. Watch the boot messages, or consult /var/log/kern.log for
the following line to ensure the SPI driver is loaded:

```
[   14.914109] bcm2708_spi bcm2708_spi.0: DMA channel 0 at address 0xf2007000 with irq 16
[   15.098289] bcm2708_spi bcm2708_spi.0: DMA channel 4 at address 0xf2007400 with irq 20
[   15.216224] spi_master spi0: will run message pump with realtime priority
[   15.279480] bcm2708_spi bcm2708_spi.0: SPI Controller at 0x20204000 (irq 80)
[   15.469266] bcm2708_spi bcm2708_spi.0: SPI Controller running in interrupt-driven mode
```

### Step Three: Checkout and Build This Module

Finally we're ready to compile/load this module and really get cooking. Checkout the
code and build it.

```
cd ~\rpi
git clone git@github.com:ab500/linux-cc2520-driver.git
cd linux-cc2520-driver

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- 
```

You should see some build output and end up with a file called cc2520.ko.

### Step Four: Move the module and support files to the pi

Now lets move this module to the Pi, as well as some support files to test and
do other things.

To do this I use SFTP. 

```
sftp pi@your-pi-hostname

sftp> put cc2520.ko
sftp> put tests/read.c
sftp> put tests/write.c
sftp> put ioctl.h
sftp> put setup.sh
sftp> put reload.sh

```

### Step Five: Load the Module and Build Test Utilities

We're ready to finally test things. Move to the Pi now, we'll compile
the test utilities on the Pi itself. They test the basic operation of the
Pi. I've also written two really really basic shell scripts to load the module
and set it up. 

**To Compile the Test Utilities**

``` 
gcc write.c -o write
gcc read.c -o read
```

**To Load the Module**

I've included two utilities to make things a little easier.
These utilities do really really basic stuff:

  * <code>setup.sh</code> - Installs a character driver interface and sets proper
    permissions for use.
  * <code>reload.sh</code> - Removes the module from the system if loaded, and reloads
    it.

**NOTE:** The setup.sh script assumes the character driver major number to be 251,
but that might be different on your system. After running <code>reload.sh</code>
check <code>/var/log/kern.log</code> for the following lines to determine what 
major number your driver has been assigned:

```
Jan 11 00:29:11 raspberrypi kernel: [ 6291.240366] loading CC2520 Kernel Module v0.01...
Jan 11 00:29:11 raspberrypi kernel: [ 6291.240491] [cc2520] - Driver [spidev] already registered for spi0.0. Nuking from orbit.
Jan 11 00:29:11 raspberrypi kernel: [ 6291.243147] spi spi0.0: setup: cd 0: 500000 Hz, bpw 8, mode 0x0 -> CS=00000000 CDIV=0200
Jan 11 00:29:11 raspberrypi kernel: [ 6291.243178] spi spi0.0: setup mode 0, 8 bits/w, 500000 Hz max --> 0
Jan 11 00:29:11 raspberrypi kernel: [ 6291.243306] bcm2708_spi bcm2708_spi.0: registered child spi0.0
Jan 11 00:29:11 raspberrypi kernel: [ 6291.243359] [cc2520] - Inserting SPI protocol driver.
Jan 11 00:29:11 raspberrypi kernel: [ 6291.243549] [cc2520] - Char interface registered on 251
```

So to get things up and running just go ahead and run them both:

```
sudo ./reload.sh
sudo ./setup.sh
```

**To Test the Driver**

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
```
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
```

**write.c**
```
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
```

Current Status
---------------
The driver currently sends and receives packets! We get close to the maximum
theoretical transmission rate supported by the CC2520, which is just fantastic.

We also support software acknowledgments.

Work is currently focused on implementing LPL. 
The goal is to push as much as possible into userland
but some things need to be implemented in the kernel to enable strict
timing guarantees. 

Some notes
----------
  * You'll need to load the spi-bcm2708 driver for this module to work. You can do this by commenting out the line in the blacklisted modules file found in /etc.
  * Make sure to update the directory for the kernel in the Makefile
  * Update the GPIO defs in cc2520.h