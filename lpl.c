#include <linux/types.h>
#include "lpl.h"
#include "cc2520.h"

struct cc2520_interface *lpl_top;
struct cc2520_interface *lpl_bottom;

static int cc2520_lpl_tx(u8 * buf, u8 len);
static void cc2520_lpl_tx_done(u8 status);
static void cc2520_lpl_rx_done(u8 *buf, u8 len);

int cc2520_lpl_init()
{
	lpl_top->tx = cc2520_lpl_tx;
	lpl_bottom->tx_done = cc2520_lpl_tx_done;
	lpl_bottom->rx_done = cc2520_lpl_rx_done;

	return 0;
}

void cc2520_lpl_free()
{

}

static int cc2520_lpl_tx(u8 * buf, u8 len)
{
	return lpl_bottom->tx(buf, len);
}

static void cc2520_lpl_tx_done(u8 status)
{
	lpl_top->tx_done(status);
}

static void cc2520_lpl_rx_done(u8 *buf, u8 len)
{
	lpl_top->rx_done(buf, len);
}