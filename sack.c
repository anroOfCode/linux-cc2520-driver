#include <linux/types.h>
#include <linux/slab.h>
#include "sack.h"
#include "cc2520.h"
#include "packet.h"

struct cc2520_interface *sack_top;
struct cc2520_interface *sack_bottom;

static int cc2520_sack_tx(u8 * buf, u8 len);
static void cc2520_sack_tx_done(u8 status);
static void cc2520_sack_rx_done(u8 *buf, u8 len);

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
static u8 *curr_tx_buf;

enum cc2520_sack_state_enum {
	CC2520_SACK_IDLE,
	CC2520_SACK_TX,
	CC2520_SACK_TX_ACK
};

static int sack_state;


int cc2520_sack_init()
{
	sack_top->tx = cc2520_sack_tx;
	sack_bottom->tx_done = cc2520_sack_tx_done;
	sack_bottom->rx_done = cc2520_sack_rx_done;

	curr_tx_buf = NULL;

	ack_buf = kmalloc(IEEE154_ACK_FRAME_LENGTH + 1, GFP_KERNEL);
	if (!ack_buf) {
		return -EFAULT;
	}

	return 0;
}

void cc2520_sack_free()
{
	if (ack_buf) {
		kfree(ack_buf);
	}
}

static int cc2520_sack_tx(u8 * buf, u8 len)
{
	curr_tx_buf = buf;
	sack_state = CC2520_SACK_TX;

	// If previous packet pending, wait on it to
	// complete or timeout.

	// 1- Setup bookkeeping information on this
	//    tx frame. If it requires a software ack,
	//    then setup a timeout timer. 
	return sack_bottom->tx(buf, len);
}

static void cc2520_sack_tx_done(u8 status)
{
	if (sack_state == CC2520_SACK_TX) {
		sack_top->tx_done(status);
		sack_state = CC2520_SACK_IDLE;
	}
		

	// If in the middle of a transmit that requires an
	// ACK, retransmit, else call top send done.
}

static void cc2520_sack_rx_done(u8 *buf, u8 len)
{
	// if this packet we just received requires
	// an ACK, trasmit it.
	if (cc2520_packet_requires_ack_reply(buf)) {
		cc2520_packet_create_ack(buf, ack_buf);
		sack_state = CC2520_SACK_TX_ACK;
		sack_bottom->tx(ack_buf, IEEE154_ACK_FRAME_LENGTH + 1);
	}

	sack_top->rx_done(buf, len);
	// If in the middle of a transmit that requires
	// an ack, examine to see if this is the ack we are
	// looking for and let the upper layers know that tx is
	// complete, else just pass the message along. 
}

// States:
// IDLE
// TX_WAIT