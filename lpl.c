#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>

#include "lpl.h"
#include "packet.h"
#include "cc2520.h"
#include "debug.h"

struct cc2520_interface *lpl_top;
struct cc2520_interface *lpl_bottom;

static int cc2520_lpl_tx(u8 * buf, u8 len);
static void cc2520_lpl_tx_done(u8 status);
static void cc2520_lpl_rx_done(u8 *buf, u8 len);
static enum hrtimer_restart cc2520_lpl_timer_cb(struct hrtimer *timer);
static void cc2520_lpl_start_timer(void);

static int lpl_window;
static int lpl_interval;
static bool lpl_enabled;

static struct hrtimer lpl_timer;

static u8* cur_tx_buf;
static u8 cur_tx_len;

static spinlock_t state_sl;

static unsigned long flags;

enum cc2520_lpl_state_enum {
	CC2520_LPL_IDLE,
	CC2520_LPL_TX,
	CC2520_LPL_TIMER_EXPIRED
};

static int lpl_state;

int cc2520_lpl_init()
{
	lpl_top->tx = cc2520_lpl_tx;
	lpl_bottom->tx_done = cc2520_lpl_tx_done;
	lpl_bottom->rx_done = cc2520_lpl_rx_done;

	lpl_window = CC2520_DEF_LPL_LISTEN_WINDOW;
	lpl_interval = CC2520_DEF_LPL_WAKEUP_INTERVAL;
	lpl_enabled = CC2520_DEF_LPL_ENABLED;

	cur_tx_buf = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!cur_tx_buf) {
		goto error;
	}

	spin_lock_init(&state_sl);
	lpl_state = CC2520_LPL_IDLE;

	hrtimer_init(&lpl_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	lpl_timer.function = &cc2520_lpl_timer_cb;

	return 0;

	error:
		if (cur_tx_buf) {
			kfree(cur_tx_buf);
			cur_tx_buf = NULL;
		}

		return -EFAULT;
}

void cc2520_lpl_free()
{
	if (cur_tx_buf) {
		kfree(cur_tx_buf);
		cur_tx_buf = NULL;
	}

	hrtimer_cancel(&lpl_timer);
}

static int cc2520_lpl_tx(u8 * buf, u8 len)
{
	if (lpl_enabled) {
		spin_lock_irqsave(&state_sl, flags);
		if (lpl_state == CC2520_LPL_IDLE) {
			lpl_state = CC2520_LPL_TX;
			spin_unlock_irqrestore(&state_sl, flags);

			memcpy(cur_tx_buf, buf, len);
			cur_tx_len = len;

			lpl_bottom->tx(cur_tx_buf, cur_tx_len);
			cc2520_lpl_start_timer();
		}
		else {
			spin_unlock_irqrestore(&state_sl, flags);
			INFO(("[cc2520] - lpl tx busy.\n"));
			lpl_top->tx_done(-CC2520_TX_BUSY);
		}

		return 0;
	}
	else {
		return lpl_bottom->tx(buf, len);
	}
}

static void cc2520_lpl_tx_done(u8 status)
{
	if (lpl_enabled) {
		spin_lock_irqsave(&state_sl, flags);
		if (cc2520_packet_requires_ack_wait(cur_tx_buf)) {
			if (status == CC2520_TX_SUCCESS) {
				lpl_state = CC2520_LPL_IDLE;
				spin_unlock_irqrestore(&state_sl, flags);

				hrtimer_cancel(&lpl_timer);
				lpl_top->tx_done(status);
			}
			else if (lpl_state == CC2520_LPL_TIMER_EXPIRED) {
				lpl_state = CC2520_LPL_IDLE;
				spin_unlock_irqrestore(&state_sl, flags);
				lpl_top->tx_done(-CC2520_TX_FAILED);
			}
			else {
				spin_unlock_irqrestore(&state_sl, flags);
				DBG((KERN_INFO "[cc2520] - lpl retransmit.\n"));
				lpl_bottom->tx(cur_tx_buf, cur_tx_len);
			}
		}
		else {
			if (lpl_state == CC2520_LPL_TIMER_EXPIRED) {
				lpl_state = CC2520_LPL_IDLE;
				spin_unlock_irqrestore(&state_sl, flags);
				lpl_top->tx_done(CC2520_TX_SUCCESS);
			}
			else {
				spin_unlock_irqrestore(&state_sl, flags);
				lpl_bottom->tx(cur_tx_buf, cur_tx_len);
			}
		}
	}
	else {
		lpl_top->tx_done(status);
	}
	// if packet requires ack, examine status.
	//    if success terminate LPL window
	//    else if status != TIMER_EXPIRED resend
	// else resend
}

static void cc2520_lpl_rx_done(u8 *buf, u8 len)
{
	lpl_top->rx_done(buf, len);
}

static void cc2520_lpl_start_timer()
{
    ktime_t kt;
    kt = ktime_set(0, 1000 * (lpl_interval + 2 * lpl_window));
	hrtimer_start(&lpl_timer, kt, HRTIMER_MODE_REL);
}

static enum hrtimer_restart cc2520_lpl_timer_cb(struct hrtimer *timer)
{
	spin_lock_irqsave(&state_sl, flags);
	if (lpl_state == CC2520_LPL_TX) {
		lpl_state = CC2520_LPL_TIMER_EXPIRED;
		spin_unlock_irqrestore(&state_sl, flags);
	}
	else {
		spin_unlock_irqrestore(&state_sl, flags);
		INFO((KERN_INFO "[cc2520] - lpl timer in improbable state.\n"));
	}

	return HRTIMER_NORESTART;
}

void cc2520_lpl_set_enabled(bool enabled)
{
	lpl_enabled = enabled;
}

void cc2520_lpl_set_listen_length(int length)
{
	lpl_window = length;
}

void cc2520_lpl_set_wakeup_interval(int interval)
{
	lpl_interval = interval;
}
