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

Default Configuration
---------------------

Character Driver Interface
--------------------------
The radio presents itself using a standard Linux character driver. This decision
was made because character drivers are conceptually simple, reasonably performant,
and easy to program against. We do bend the rules of the character driver
interface however. Deviations are as follows:

  * We require that only entire packets are written or read from the 
character driver. You may not write partial packets to the driver, and
you should always call read with a buffer that is 128 bytes or large to
allow for the maximum possible frame size, and length byte. 
  * The read call will always try to write exactly a single packet to
the user buffers. If you provide inadequate  buffer space for the maximum
packet size it will overrun.
  * We define some custom error codes to indicate a busy channel and
other radio specific error codes.
  * **Our driver is not fully thread-safe.** Although you can certainly
send and receive packets simultaneously (this is recommended), you may
not send multiple packets from separate threads. Only a single packet
may be sent or received at any time from all processes. 
  * Our driver does not implement nonblocking IO currently. The low
data rates really don't make this something we need to do, although
it might be supported in the future. 

**Error Codes**

The following error codes can be returned from calls to <code>write</code>:

  * **CC2520_TX_SUCCESS**- The packet was transmitted successfully.
  * **CC2520_TX_BUSY**- The channel was busy and the packet was unable to
be transmitted.
  * **CC2520_TX_ACK_TIMEOUT** - The packet was sent but the receive did not
send an ACK within the timeout period.
  * **CC2520_TX_FAILED** - A general error has occurred.

Frame Format
------------

Carrier Sense Multi-Access/Collision Avoidance (CSMA/CA)
--------------------------------------------------------
CSMA/CA is a feature that allows for the driver to sense the current channel
energy level, and intelligently wait to send a packet until the channel is clear,
avoiding potential collisions.

It is configured using three parameters via the <code>CC2520_IO_RADIO_SET_CSMA</code> 
ioctl. It should only be configured when the radio is off. 

  * <code>min_backoff</code>- The minimum back off period, in microseconds, the radio
will use.
  * <code>init_backoff</code>- The maximum back off period, in microseconds, the radio
will use when initially sending a message. 
  * <code>cong_backoff</code>- The maximum back off period, in microseconds, the radio
will use when it has detected congestion in the channel.
  * <code>enabled</code>- Whether CSMA is enabled or disabled. 

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

In cases where CSMA is unable to transmit the packet due to congestion within two retries
it will return an error code, which bubbles up to the system write call. The error is
defined in cc2520.h as <code>-CC2520_TX_BUSY</code>

Software Acknowledgment (Soft-ACK)
----------------------------------
Soft-ACK allows the radio to acknowledge packets from within the software stack,
instead of traditional hardware acknowledgment. This is useful because it gives
the acknowledgment a slightly better guarantee than hardware acknowledgments do.
It allows for the sender to confirm that the packet has made it through the
software layers as well as the hardware layers of the radio. This is important
because in some edge cases the software will have to drop a received packet
although the radio has successfully received it. 

Software acknowledgments are controlled by a single ioctl parameter. This
parameter is the timeout, in microseconds, that the driver will wait for
other radios to send an acknowledgment. It defaults to 2.5ms. 

Soft-ACK is always enabled, but acknowledgment is controlled on a per-packet
basis. Check the 802.15.4 MAC frame control sequence (FCS) header for more
information on how to request software acknowledgments on an individual packet.
The driver will always acknowledge packets received requesting an acknowledgment.

**TODO:** In the future it would be wise to modify the default behavior to only
accept packets if they are successfully read by the character driver <code>read</code>
function call.

Low Power Listening (LPL)
-------------------------
In order to extend the battery life of motes with limited amounts of energy available
a technique called low power listening has been employed. This technique effectively
duty-cycles receiving radios, periodically waking them up for a fraction of the time
they would typically be awake for, checking for active transmissions, and then going
back to sleep.

This technique is effective because idly listening for a message consumes almost as
much power as sending does, but happens much more often. By reducing the time
the mote spends listening for a packet to a small percentage of the total time
significant power savings are realized, with only a marginal loss of perceived
responsiveness. 

The tradeoff involves shifting the energy burden to the transmitting side of the
radio, by continuously retransmitting the packet over the entire period the receiver
radio could be sleeping.

Our radio does support LPL. An ioctl supports enabling/disabling this feature as
well as setting a number of key parameters that determine how long the mote will
send for. These parameters are documented below:

  * <code>window</code>- The amount of time, in microseconds, that the receiving
mote will wake up for and listen for data. The mote will most likely wake up and
sample for channel energy repeatedly during this interval. It should be longer
than the software-ack timeout period.
  * <code>interval</code>- The amount of time, in microseconds, that the receiving
mote will sleep between wakeup windows. Together with the window these two 
parameters make up the duty-cycle of the receiving mote.
  * <code>enabled</code>- Whether LPL is enabled or not. 

Keep in mind that the parameters you set here should match those set in the motes.
They are only used to determine the length of time that the radio should attempt to
retransmit the packet for a single LPL period in order to ensure with a reasonable
certainty that a receiving mote will wake up and receive it. 

LPL can have a significant impact on performance in certain situations. When used
with software acknowledgments, which are enabled on all non-broadcast packets, and
sent to a mote that is not configured with LPL it will have negligible performance
impact. The mote will immediately ACK the first received packet and this will
conclude the LPL sending session, causing no resends. However, when sending packets
to the broadcast address LPL will significantly reduce packet rate. Because there
is no mechanism to ACK broadcast packets, the packet will be resent for the entire
LPL send period. In these situations it is recommended to either switch to a
non-broadcast address, or to disable LPL. 

Sending/Receiving Data
----------------------

Turning the Radio On/Off
------------------------

Portability
------------

**Platform Portability:**
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

**Radio Portability:**
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