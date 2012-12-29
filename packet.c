#include <linux/types.h>
#include <linux/kernel.h>
#include "packet.h"

bool cc2520_packet_requires_ack_reply(u8 *buf)
{
	ieee154_simple_header_t *header;
	header = cc2520_packet_get_header(buf);
	printk(KERN_INFO "[cc2520] - fcf: %d", header->fcf);
	return ((header->fcf & (1 << IEEE154_FCF_ACK_REQ)) != 0) && 
		(header->dest != IEEE154_BROADCAST_ADDR);
}

bool cc2520_packet_requires_ack_wait(u8 *buf)
{
	ieee154_simple_header_t *header;
	header = cc2520_packet_get_header(buf);

	return ((header->fcf & (1 << IEEE154_FCF_ACK_REQ)) != 0) && 
		(header->dest != IEEE154_BROADCAST_ADDR);
}

void cc2520_packet_create_ack(u8 *pkt, u8 *buf)
{
	u8 *ptr;
	u8 dsn;
	ieee154_simple_header_t *header;

	header = cc2520_packet_get_header(pkt);
	dsn = header->dsn;

	header = cc2520_packet_get_header(buf);
	header->fcf = IEEE154_ACK_FRAME_VALUE;
	header->dsn = dsn;

	ptr = cc2520_packet_get_length_field(buf);
	*ptr = IEEE154_ACK_FRAME_LENGTH;
}

ieee154_simple_header_t* cc2520_packet_get_header(u8 *buf)
{
	// Ignore the length
	return (ieee154_simple_header_t*)(buf+1);
}

u8* cc2520_packet_get_length_field(u8 * buf)
{
	return buf;
}

u8* cc2520_packet_get_payload(u8 * buf)
{
	return buf + 3;
}

