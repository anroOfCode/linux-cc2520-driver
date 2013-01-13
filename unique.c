#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>

#include "unique.h"
#include "packet.h"
#include "cc2520.h"

struct cc2520_interface *unique_top;
struct cc2520_interface *unique_bottom;

static int cc2520_unique_tx(u8 * buf, u8 len);
static void cc2520_unique_tx_done(u8 status);
static void cc2520_unique_rx_done(u8 *buf, u8 len);

int cc2520_unique_init()
{
	unique_top->tx = cc2520_unique_tx;
	unique_bottom->tx_done = cc2520_unique_tx_done;
	unique_bottom->rx_done = cc2520_unique_rx_done;

	return 0;
}

void cc2520_unique_free()
{

}

static int cc2520_unique_tx(u8 * buf, u8 len)
{
	return unique_bottom->tx(buf, len);
}

static void cc2520_unique_tx_done(u8 status)
{
	unique_top->tx_done(status);
}

static void cc2520_unique_rx_done(u8 *buf, u8 len)
{
	unique_top->rx_done(buf, len);
}