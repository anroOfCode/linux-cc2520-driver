#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>

#include "sack.h"
#include "cc2520.h"
#include "packet.h"

struct cc2520_interface *sack_top;
struct cc2520_interface *sack_bottom;

static int cc2520_sack_tx(u8 * buf, u8 len);
static void cc2520_sack_tx_done(u8 status);
static void cc2520_sack_rx_done(u8 *buf, u8 len);
static enum hrtimer_restart cc2520_sack_timer_cb(struct hrtimer *timer);

// Two pieces to software acknowledgements:
// 1 - Taking packets we're transmitting, setting an ACK flag
//     on them, and waiting for that ACK to be received before
//     calling tx_done.
//     Requires:
//     - A timeout equivalent to the ACK period
//     - Storing the DSN of the outgoing packet
//     - Interface to verify if an ACK is correct
// 2 - Examining packets we're receiving and sending an ACK if
//     needed.
//     Requires:
//     - Buffer to build ACK packet
//     - Concurrency mechanism to prevent transmission
//       during ACKing.

static u8 *ack_buf;
static u8 *cur_tx_buf;
static struct hrtimer timeout_timer;


enum cc2520_sack_state_enum {
	CC2520_SACK_IDLE,
	CC2520_SACK_TX, // Waiting for a tx to complete
	CC2520_SACK_TX_WAIT, // Waiting for an ack
	CC2520_SACK_TX_ACK, // Waiting for a sent ack to finish
};

static int sack_state;
static spinlock_t sack_sl;

int cc2520_sack_init()
{
	ktime_t kt;

	sack_top->tx = cc2520_sack_tx;
	sack_bottom->tx_done = cc2520_sack_tx_done;
	sack_bottom->rx_done = cc2520_sack_rx_done;

	cur_tx_buf = NULL;

	ack_buf = kmalloc(IEEE154_ACK_FRAME_LENGTH + 1, GFP_KERNEL);
	if (!ack_buf) {
		return -EFAULT;
	}

    // Create a 100uS time period.
    kt=ktime_set(10,100000);

	hrtimer_init(&timeout_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timeout_timer.function = &cc2520_sack_timer_cb; // callback
    //hrtimer_start(&timeout_timer, kt, HRTIMER_MODE_REL);

	spin_lock_init(&sack_sl);
	sack_state = CC2520_SACK_IDLE;

	return 0;
}

void cc2520_sack_free()
{
	if (ack_buf) {
		kfree(ack_buf);
	}

	hrtimer_cancel(&timeout_timer);
}

static int cc2520_sack_tx(u8 * buf, u8 len)
{
	spin_lock(&sack_sl);
	if (sack_state != CC2520_SACK_IDLE) {
		printk(KERN_INFO "[cc2520] - Ut oh! Tx spinlocking.\n");
	}
	while (sack_state != CC2520_SACK_IDLE) {
		spin_unlock(&sack_sl);
		spin_lock(&sack_sl);
	}
	sack_state = CC2520_SACK_TX;
	spin_unlock(&sack_sl);

	cur_tx_buf = buf;
	return sack_bottom->tx(buf, len);
}

static void cc2520_sack_tx_done(u8 status)
{
	spin_lock(&sack_sl);
	if (sack_state == CC2520_SACK_TX) {
		if (cc2520_packet_requires_ack_wait(cur_tx_buf)) {
			sack_state = CC2520_SACK_TX_WAIT;
			spin_unlock(&sack_sl);
		}
		else {
			// do we need to wait for an ACK
			sack_state = CC2520_SACK_IDLE;
			spin_unlock(&sack_sl);
			sack_top->tx_done(status);
		}
	}
	else if (sack_state == CC2520_SACK_TX_ACK) {
		sack_state = CC2520_SACK_IDLE;
		spin_unlock(&sack_sl);
	}
	else {
		printk(KERN_ALERT "[cc2520] - ERROR: tx_done state engine in impossible state.\n");
	}
}

static void cc2520_sack_rx_done(u8 *buf, u8 len)
{
	// if this packet we just received requires
	// an ACK, trasmit it.
	spin_lock(&sack_sl);

	// If IDLE this must be a new RX packet
	if (cc2520_packet_is_ack(buf)) {
		if (sack_state == CC2520_SACK_TX_WAIT && 
			cc2520_packet_is_ack_to(buf, cur_tx_buf)) {
			sack_state = CC2520_SACK_IDLE;
			spin_unlock(&sack_sl);
			sack_top->tx_done(0);
		}
		else {
			spin_unlock(&sack_sl);
			printk(KERN_INFO "[cc2520] - stray ack received.\n");
		}
	}
	else {
		if (cc2520_packet_requires_ack_reply(buf)) {
			if (sack_state == CC2520_SACK_IDLE) {
				cc2520_packet_create_ack(buf, ack_buf);
				sack_state = CC2520_SACK_TX_ACK;
				spin_unlock(&sack_sl);
				sack_bottom->tx(ack_buf, IEEE154_ACK_FRAME_LENGTH + 1);
				sack_top->rx_done(buf, len);
			}
			else {
				spin_unlock(&sack_sl);
				printk(KERN_INFO "[cc2520] - ACK skipped, soft-ack layer busy.\n");
			}
		}
		else {
			//if (sack_state != CC2520_SACK_IDLE) {
			//	printk(KERN_ALERT "[cc2520] - ERROR: Softack state is incorrect!\n");
			//}
			spin_unlock(&sack_sl);
			sack_top->rx_done(buf, len);
		}
	}
}

static enum hrtimer_restart cc2520_sack_timer_cb(struct hrtimer *timer)
{
	//ktime_t kt;

	//gpio_set_value(23, pinValue == 1);
	//pinValue = !pinValue;

	// Create a 100uS time period.
	//kt=ktime_set(0,100000);
	//hrtimer_forward_now(&utimer, kt);

	return HRTIMER_NORESTART;
}


// States:
// IDLE
// TX_WAIT