#include <linux/types.h>
#include "sack.h"
#include "cc2520.h"

struct cc2520_interface *sack_top;
struct cc2520_interface *sack_bottom;

static int cc2520_sack_tx(u8 * buf, u8 len);
static void cc2520_sack_tx_done(u8 status);
static void cc2520_sack_rx_done(u8 *buf, u8 len);

int cc2520_sack_init()
{
	sack_top->tx = cc2520_sack_tx;
	sack_bottom->tx_done = cc2520_sack_tx_done;
	sack_bottom->rx_done = cc2520_sack_rx_done;

	return 0;
}

void cc2520_sack_free()
{

}

static int cc2520_sack_tx(u8 * buf, u8 len)
{
	return sack_bottom->tx(buf, len);
}

static void cc2520_sack_tx_done(u8 status)
{
	sack_top->tx_done(status);
}

static void cc2520_sack_rx_done(u8 *buf, u8 len)
{
	sack_top->rx_done(buf, len);
}