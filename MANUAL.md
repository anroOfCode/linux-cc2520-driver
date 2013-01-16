CC2520 Linux Kernel Module Manual
=================================
The purpose of this manual is to describe the basic operations of the
CC2520 linux kernel module. It should give you an idea of how to use it,
the current caveats, and maybe how to extend it.

Introduction
------------
The motivations behind the development of this driver are to enable an
embedded linux system to run a full 802.15.4 radio stack without the
addition of a dedicated coprocessor to handle timing critical operations.
The idea is that this will give us the maximal flexibility, while still
being mindful of things like keeping the component count low. We chose
to use the cc2520 radio because it's reasonably new, offering small
performance and interface improvements over it's predecessor, the cc2420, 
which has enjoyed a long run of successful applications.

The reason that coprocessors have traditionally been employed in the use
of low-power 802.15.4 radios has been to meet the strict timing guarantees
and to allow for reuse of code that runs on the motes. If the basestation
is running the same exact networking stack as the motes they are
likely to work together. The coprocessor has taken the form of an additional
wireless sensor mote attached via a serial link to the Linux-powered base
station. 

During the development of this module we discovered that many of the
functions traditionally required to be performed by a coprocessor can
instead be performed within a kernel module. We can achieve
microsecond-resolution timing using the latest hr_timers and the kernel
APIs for gpio interrupts and asynchronous SPI transactions have reached
a point of maturity. 

Overview
--------

Configuration
-------------

Character Driver Interface
--------------------------

Frame Format
------------

Carrier Sense Multi-Access/Collision Avoidance (CSMA/CA)
--------------------------------------------------------
CSMA/CA is a feature that allows for the driver to sense the current channel
energy level, and intelligently wait to send a packet until the channel is clear,
avoiding potential collisions.

It is configured using three parameters via the <code>CC2520_IO_RADIO_SET_CSMA</code> 
ioctl. It should only be configured when the radio is off. 

  * <code>min_backoff</code> - The minimum back off period, in microseconds, the radio
will use.
  * <code>init_backoff</code> - The maximum back off period, in microseconds, the radio
will use when initially sending a message. 
  * <code>cong_backoff</code> - The maximum back off period, in microseconds, the radio
will use when it has detected congestion in the channel.
  * <code>enabled</code> - Whether CSMA is enabled or disabled. 

The way CSMA operates is by introducing a random amount of delay into each transmitted
packet. It does this both when congestion is detected, and more unintuitively, before
the first transmission as well. It introduces random delay in the first transmission
in order to prevent multiple motes from transmitting simultaneously in situations
where they are all responding to a single broadcast message. 

CSMA will choose a random backoff period bounded between the <code>min_backoff</code>
and <code>init_backoff</code> values for initial transmission, and between the 
<code>min_backoff</code> and <code>cong_backoff</code> values for a second transmission
when the driver detects congestion.

The initial backoff period should always be smaller than the congestion backoff period.
This give priority to motes that aren't continuously transmitting packets.

Software Acknowledgment (Soft-ACK)
----------------------------------

Low Power Listening (LPL)
-------------------------


Portability
------------

**Platform Portability**
This module directly interfaces a CC2520 radio with linux. It is tested
on the Raspberry Pi board, but uses standard kernel abstractions
(specifically gpios, hr_timers, and spi_devices) so it should be easily
ported to any other platform. The biggest problem will probably be maintaining
the strict timing guarantees this module implicitly requires of the underlying
system.

For example it relies on the SPI driver executing with a real-time queue,
any looser timing will result in potentially out of order execution of SPI
commands, or interweaving with interrupt handles that are implicitly required
to occur after certain bus transactions have completed. 

For help getting this code working on a different platform feel free to
shoot me an e-mail.

**Radio Portability**
Most of this code should also be reasonably radio portable. We intentionally
avoided employing too many abstractions with regards to the low-level
implementation of the radio itself. This keeps in line with the philosophy
seen in other linux kernel modules. We focus on a concise implementation
that is easy to understand, avoiding the alternative which is an extremely
generic system that only the original author can extend. 

That being said almost all radio-specific functionality is contained within
<code>radio.c</code>. As long as your radio can generate interrupts upon
the start of a transmission, and the reception of packet, you should be good
to go. 

This code could easily be modified to support other radios out there, including
the CC2420 (trivially), and the RF230 by Atmel. 

License
-------
To be determined. If you want to work with the code right now feel free
to. Licensing information will be added soon. 