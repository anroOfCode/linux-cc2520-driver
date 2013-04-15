#include <linux/types.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <linux/workqueue.h>

#include "csma.h"
#include "cc2520.h"
#include "radio.h"
#include "debug.h"

struct cc2520_interface *csma_top;
struct cc2520_interface *csma_bottom;

static int backoff_min;
static int backoff_max_init;
static int backoff_max_cong;
static bool csma_enabled;

static struct hrtimer backoff_timer;

static u8* cur_tx_buf;
static u8 cur_tx_len;

static spinlock_t state_sl;

static struct workqueue_struct *wq;
static struct work_struct work;

enum cc2520_csma_state_enum {
	CC2520_CSMA_IDLE,
	CC2520_CSMA_TX,
	CC2520_CSMA_CONG
};

static int csma_state;

static unsigned long flags;

static int cc2520_csma_tx(u8 * buf, u8 len);
static void cc2520_csma_tx_done(u8 status);
static void cc2520_csma_rx_done(u8 *buf, u8 len);
static enum hrtimer_restart cc2520_csma_timer_cb(struct hrtimer *timer);
static void cc2520_csma_start_timer(int us_period);
static int cc2520_csma_get_backoff(int min, int max);
static void cc2520_csma_wq(struct work_struct *work);

int cc2520_csma_init()
{
	csma_top->tx = cc2520_csma_tx;
	csma_bottom->tx_done = cc2520_csma_tx_done;
	csma_bottom->rx_done = cc2520_csma_rx_done;

	backoff_min = CC2520_DEF_MIN_BACKOFF;
	backoff_max_init = CC2520_DEF_INIT_BACKOFF;
	backoff_max_cong = CC2520_DEF_CONG_BACKOFF;
	csma_enabled = CC2520_DEF_CSMA_ENABLED;

	spin_lock_init(&state_sl);
	csma_state = CC2520_CSMA_IDLE;

	cur_tx_buf = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!cur_tx_buf) {
		goto error;
	}

	wq = alloc_workqueue("csma_wq", WQ_HIGHPRI, 128);
	if (!wq) {
		goto error;
	}

	hrtimer_init(&backoff_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	backoff_timer.function = &cc2520_csma_timer_cb;

	return 0;

	error:
		if (cur_tx_buf) {
			kfree(cur_tx_buf);
			cur_tx_buf = NULL;
		}

		if (wq) {
			destroy_workqueue(wq);
		}

		return -EFAULT;
}

void cc2520_csma_free()
{
	if (cur_tx_buf) {
		kfree(cur_tx_buf);
		cur_tx_buf = NULL;
	}

	if (wq) {
		destroy_workqueue(wq);
	}

	hrtimer_cancel(&backoff_timer);
}

static int cc2520_csma_get_backoff(int min, int max)
{
	uint rand_num;
	int span;

	span = max - min;
	get_random_bytes(&rand_num, 4);
	return min + (rand_num % span);
}

static void cc2520_csma_start_timer(int us_period)
{
    ktime_t kt;
    kt = ktime_set(0, 1000 * us_period);
	hrtimer_start(&backoff_timer, kt, HRTIMER_MODE_REL);
}

static enum hrtimer_restart cc2520_csma_timer_cb(struct hrtimer *timer)
{
	ktime_t kt;
	int new_backoff;

	if (cc2520_radio_is_clear()) {
		// NOTE: We can absolutely not send from
		// interrupt context, there's a few places
		// where we spin lock and assume we can be
		// preempted. If we're running in atomic mode
		// that promise is broken. We use a work queue.

		// The workqueue adds about 30uS of latency.
		INIT_WORK(&work, cc2520_csma_wq);
		queue_work(wq, &work);
		return HRTIMER_NORESTART;
	}
	else {
		spin_lock_irqsave(&state_sl, flags);
		if (csma_state == CC2520_CSMA_TX) {
			csma_state = CC2520_CSMA_CONG;
			spin_unlock_irqrestore(&state_sl, flags);

			new_backoff =
				cc2520_csma_get_backoff(backoff_min, backoff_max_cong);

			INFO((KERN_INFO "[cc2520] - channel still busy, waiting %d uS\n", new_backoff));
			kt = ktime_set(0,1000 * new_backoff);
			hrtimer_forward_now(&backoff_timer, kt);
			return HRTIMER_RESTART;
		}
		else {
			csma_state = CC2520_CSMA_IDLE;
			spin_unlock_irqrestore(&state_sl, flags);

			csma_top->tx_done(-CC2520_TX_BUSY);
			return HRTIMER_NORESTART;
		}
	}
}

static void cc2520_csma_wq(struct work_struct *work)
{
	csma_bottom->tx(cur_tx_buf, cur_tx_len);
}

static int cc2520_csma_tx(u8 * buf, u8 len)
{
	int backoff;

	if (!csma_enabled) {
		return csma_bottom->tx(buf, len);
	}

	spin_lock_irqsave(&state_sl, flags);
	if (csma_state == CC2520_CSMA_IDLE) {
		csma_state = CC2520_CSMA_TX;
		spin_unlock_irqrestore(&state_sl, flags);

		memcpy(cur_tx_buf, buf, len);
		cur_tx_len = len;

		backoff = cc2520_csma_get_backoff(backoff_min, backoff_max_init);

		DBG((KERN_INFO "[cc2520] - waiting %d uS to send.\n", backoff));
		cc2520_csma_start_timer(backoff);
	}
	else {
		spin_unlock_irqrestore(&state_sl, flags);
		DBG((KERN_INFO "[cc2520] - csma layer busy.\n"));
		csma_top->tx_done(-CC2520_TX_BUSY);
	}

	return 0;
}

static void cc2520_csma_tx_done(u8 status)
{
	if (csma_enabled) {
		spin_lock_irqsave(&state_sl, flags);
		csma_state = CC2520_CSMA_IDLE;
		spin_unlock_irqrestore(&state_sl, flags);
	}

	csma_top->tx_done(status);
}

static void cc2520_csma_rx_done(u8 *buf, u8 len)
{
	csma_top->rx_done(buf, len);
}

void cc2520_csma_set_enabled(bool enabled)
{
	csma_enabled = enabled;
}

void cc2520_csma_set_min_backoff(int backoff)
{
	backoff_min = backoff;
}

void cc2520_csma_set_init_backoff(int backoff)
{
	backoff_max_init = backoff;
}

void cc2520_csma_set_cong_backoff(int backoff)
{
	backoff_max_cong = backoff;
}
