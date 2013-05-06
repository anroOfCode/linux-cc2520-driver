#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>

#include "sack.h"
#include "cc2520.h"
#include "packet.h"
#include "radio.h"
#include "debug.h"

struct cc2520_interface *sack_top;
struct cc2520_interface *sack_bottom;

static int cc2520_sack_tx(u8 * buf, u8 len);
static void cc2520_sack_tx_done(u8 status);
static void cc2520_sack_rx_done(u8 *buf, u8 len);
static enum hrtimer_restart cc2520_sack_timer_cb(struct hrtimer *timer);
static void cc2520_sack_start_timer(void);

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

static u8 *cur_rx_buf;
static u8 cur_rx_buf_len;

static struct hrtimer timeout_timer;
static int ack_timeout; //in microseconds
static int sack_state;
static spinlock_t sack_sl;

static unsigned long flags;

enum cc2520_sack_state_enum {
	CC2520_SACK_IDLE,
	CC2520_SACK_TX, // Waiting for a tx to complete
	CC2520_SACK_TX_WAIT, // Waiting for an ack to be received
	CC2520_SACK_TX_ACK, // Waiting for a sent ack to finish
};

int cc2520_sack_init()
{
	sack_top->tx = cc2520_sack_tx;
	sack_bottom->tx_done = cc2520_sack_tx_done;
	sack_bottom->rx_done = cc2520_sack_rx_done;

	ack_buf = kmalloc(IEEE154_ACK_FRAME_LENGTH + 1, GFP_KERNEL);
	if (!ack_buf) {
		goto error;
	}

	cur_tx_buf = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!cur_tx_buf) {
		goto error;
	}

	cur_rx_buf = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!cur_rx_buf) {
		goto error;
	}

	hrtimer_init(&timeout_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timeout_timer.function = &cc2520_sack_timer_cb;

	spin_lock_init(&sack_sl);
	sack_state = CC2520_SACK_IDLE;

	ack_timeout = CC2520_DEF_ACK_TIMEOUT;

	return 0;

	error:
		if (ack_buf) {
			kfree(ack_buf);
			ack_buf = NULL;
		}

		if (cur_tx_buf) {
			kfree(cur_tx_buf);
			cur_tx_buf = NULL;
		}

		return -EFAULT;
}

void cc2520_sack_free()
{
	if (ack_buf) {
		kfree(ack_buf);
	}

	if (cur_tx_buf) {
		kfree(cur_tx_buf);
	}

	hrtimer_cancel(&timeout_timer);
}

void cc2520_sack_set_timeout(int timeout)
{
	ack_timeout = timeout;
}

static void cc2520_sack_start_timer()
{
    ktime_t kt;
    kt = ktime_set(0, 1000 * ack_timeout);
	hrtimer_start(&timeout_timer, kt, HRTIMER_MODE_REL);
}

static int cc2520_sack_tx(u8 * buf, u8 len)
{
	spin_lock_irqsave(&sack_sl, flags);

	if (sack_state != CC2520_SACK_IDLE) {
		INFO((KERN_INFO "[cc2520] - Ut oh! Tx spinlocking.\n"));
	}

	while (sack_state != CC2520_SACK_IDLE) {
		spin_unlock_irqrestore(&sack_sl, flags);
		spin_lock_irqsave(&sack_sl, flags);
	}
	sack_state = CC2520_SACK_TX;
	spin_unlock_irqrestore(&sack_sl, flags);

	memcpy(cur_tx_buf, buf, len);
	return sack_bottom->tx(cur_tx_buf, len);
}

static void cc2520_sack_tx_done(u8 status)
{
	spin_lock_irqsave(&sack_sl, flags);
	if (sack_state == CC2520_SACK_TX) {
		if (cc2520_packet_requires_ack_wait(cur_tx_buf)) {
			DBG((KERN_INFO "[cc2520] - Entering TX wait state.\n"));
			sack_state = CC2520_SACK_TX_WAIT;
			cc2520_sack_start_timer();
			spin_unlock_irqrestore(&sack_sl, flags);
		}
		else {
			sack_state = CC2520_SACK_IDLE;
			spin_unlock_irqrestore(&sack_sl, flags);
			sack_top->tx_done(status);
		}
	}
	else if (sack_state == CC2520_SACK_TX_ACK) {
		sack_state = CC2520_SACK_IDLE;
		spin_unlock_irqrestore(&sack_sl, flags);
	}
	else {
		ERR((KERN_ALERT "[cc2520] - ERROR: tx_done state engine in impossible state.\n"));
	}
}

static void cc2520_sack_rx_done(u8 *buf, u8 len)
{
	// if this packet we just received requires
	// an ACK, trasmit it.
	memcpy(cur_rx_buf, buf, len);
	cur_rx_buf_len = len;

	// NOTE: this is a big hack right now,
	// and I'm not sure if it's even needed.
	// We introduce a strong coupling between
	// the sack layer and the radio layer here
	// by providing a mechanism to explicitly
	// release the buffer. When I was troubleshooting
	// a terrible concurrency bug I added this
	// as a possible solution, but I don't
	// think it's needed anymore.
	cc2520_radio_release_rx();

	spin_lock_irqsave(&sack_sl, flags);

	if (cc2520_packet_is_ack(cur_rx_buf)) {
		if (sack_state == CC2520_SACK_TX_WAIT &&
			cc2520_packet_is_ack_to(cur_rx_buf, cur_tx_buf)) {
			sack_state = CC2520_SACK_IDLE;
			spin_unlock_irqrestore(&sack_sl, flags);

			hrtimer_cancel(&timeout_timer);
			sack_top->tx_done(CC2520_TX_SUCCESS);
		}
		else {
			spin_unlock_irqrestore(&sack_sl, flags);
			INFO((KERN_INFO "[cc2520] - stray ack received.\n"));
		}
	}
	else {
		if (cc2520_packet_requires_ack_reply(cur_rx_buf)) {
			if (sack_state == CC2520_SACK_IDLE) {
				cc2520_packet_create_ack(cur_rx_buf, ack_buf);
				sack_state = CC2520_SACK_TX_ACK;
				spin_unlock_irqrestore(&sack_sl, flags);
				sack_bottom->tx(ack_buf, IEEE154_ACK_FRAME_LENGTH + 1);
				sack_top->rx_done(cur_rx_buf, cur_rx_buf_len);
			}
			else {
				spin_unlock_irqrestore(&sack_sl, flags);
				INFO((KERN_INFO "[cc2520] - ACK skipped, soft-ack layer busy. %d \n", sack_state));
			}
		}
		else {
			spin_unlock_irqrestore(&sack_sl, flags);
			sack_top->rx_done(cur_rx_buf, cur_rx_buf_len);
		}
	}
}

static enum hrtimer_restart cc2520_sack_timer_cb(struct hrtimer *timer)
{
	spin_lock_irqsave(&sack_sl, flags);

	if (sack_state == CC2520_SACK_TX_WAIT) {
		INFO((KERN_INFO "[cc2520] - tx ack timeout exceeded.\n"));
		sack_state = CC2520_SACK_IDLE;
		spin_unlock_irqrestore(&sack_sl, flags);

		sack_top->tx_done(-CC2520_TX_ACK_TIMEOUT);
	}
	else {
		spin_unlock_irqrestore(&sack_sl, flags);
	}

	return HRTIMER_NORESTART;
}

