#include <linux/types.h>
#include "csma.h"
#include "cc2520.h"

struct cc2520_interface *csma_top;
struct cc2520_interface *csma_bottom;

static int cc2520_csma_tx(u8 * buf, u8 len);
static void cc2520_csma_tx_done(u8 status);
static void cc2520_csma_rx_done(u8 *buf, u8 len);

int cc2520_csma_init()
{
	csma_top->tx = cc2520_csma_tx;
	csma_bottom->tx_done = cc2520_csma_tx_done;
	csma_bottom->rx_done = cc2520_csma_rx_done;

	return 0;
}

void cc2520_csma_free()
{

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