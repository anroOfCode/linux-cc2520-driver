#include <linux/types.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>

#include "csma.h"
#include "cc2520.h"

struct cc2520_interface *csma_top;
struct cc2520_interface *csma_bottom;

static int backoff_min;
static int backoff_max_init;
static int backoff_max_cong;

static struct hrtimer backoff_timer;

static u8* cur_tx_buf;

static int cc2520_csma_tx(u8 * buf, u8 len);
static void cc2520_csma_tx_done(u8 status);
static void cc2520_csma_rx_done(u8 *buf, u8 len);
static enum hrtimer_restart cc2520_csma_timer_cb(struct hrtimer *timer);
static void cc2520_csma_start_timer(int us_period);

int cc2520_csma_init()
{
	csma_top->tx = cc2520_csma_tx;
	csma_bottom->tx_done = cc2520_csma_tx_done;
	csma_bottom->rx_done = cc2520_csma_rx_done;

	backoff_min = CC2520_DEF_MIN_BACKOFF;
	backoff_max_init = CC2520_DEF_INIT_BACKOFF;
	backoff_max_cong = CC2520_DEF_CONG_BACKOFF;

	cur_tx_buf = kmalloc(PKT_BUFF_SIZE, GFP_KERNEL);
	if (!cur_tx_buf) {
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

		return -EFAULT;
}

void cc2520_csma_free()
{
	if (cur_tx_buf) {
		kfree(cur_tx_buf);
		cur_tx_buf = NULL;
	}

	hrtimer_cancel(&backoff_timer);
}

static void cc2520_csma_start_timer(int us_period)
{
    ktime_t kt;
    kt = ktime_set(0, 1000 * us_period);
	hrtimer_start(&backoff_timer, kt, HRTIMER_MODE_REL);
}

static enum hrtimer_restart cc2520_csma_timer_cb(struct hrtimer *timer)
{

	return HRTIMER_NORESTART;
}



static int cc2520_csma_tx(u8 * buf, u8 len)
{
	return csma_bottom->tx(buf, len);
}

static void cc2520_csma_tx_done(u8 status)
{
	csma_top->tx_done(status);
}

static void cc2520_csma_rx_done(u8 *buf, u8 len)
{
	csma_top->rx_done(buf, len);
}