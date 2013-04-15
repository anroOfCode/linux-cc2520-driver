#include <linux/types.h>
#include <linux/kernel.h>
#include "packet.h"
#include "cc2520.h"
#include "debug.h"

bool cc2520_packet_requires_ack_reply(u8 *buf)
{
	ieee154_simple_header_t *header;
	header = cc2520_packet_get_header(buf);
	DBG((KERN_INFO "[cc2520] - fcf: %d\n", header->fcf));
	return ((header->fcf & (1 << IEEE154_FCF_ACK_REQ)) != 0);
}

bool cc2520_packet_requires_ack_wait(u8 *buf)
{
	ieee154_simple_header_t *header;
	header = cc2520_packet_get_header(buf);

	return (header->fcf & (1 << IEEE154_FCF_ACK_REQ)) != 0;
}

bool cc2520_packet_is_ack(u8* buf)
{
	ieee154_simple_header_t *header;
	header = cc2520_packet_get_header(buf);

	return (header->fcf & IEEE154_ACK_FRAME_MASK)
		== IEEE154_ACK_FRAME_VALUE;
}

bool cc2520_packet_is_ack_to(u8 *pending, u8 *buf)
{
	ieee154_simple_header_t *header;
	u8 dsn;

	header = cc2520_packet_get_header(buf);
	dsn = header->dsn;

	header = cc2520_packet_get_header(pending);
	return dsn == header->dsn;
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

u64 cc2520_packet_get_src(u8 *buf)
{
	// TODO: Make this a little smarter.
	// Right now we're assuming that the
	// PAN ID is always present. It's easy
	// to construct packets that don't
	// follow this contract.

	ieee154_simple_header_t *hdr;
	u64 ret;

	u8 src_addr_mode;
	u8 dest_addr_mode;
	bool pan_compression;

	u8 src_addr_offset;

	ret = 0;
	src_addr_offset = 4;
	hdr = cc2520_packet_get_header(buf);

	src_addr_mode = ((hdr->fcf >> IEEE154_FCF_SRC_ADDR_MODE) & 0x03);
	dest_addr_mode = ((hdr->fcf >> IEEE154_FCF_DEST_ADDR_MODE) & 0x03);
	pan_compression = ((hdr->fcf >> IEEE154_FCF_INTRAPAN) & 0x01) == 1;

	if (dest_addr_mode == IEEE154_ADDR_SHORT) {
		src_addr_offset += 4;
	}
	else if (dest_addr_mode == IEEE154_ADDR_EXT) {
		src_addr_offset += 10;
	}
	// WARNING: this is a weird edge case from the
	// 802.15.4 spec.
	// NOTE: Assuming we're on LE arch.
	else if (pan_compression) {
		src_addr_offset += 2;
	}

	if (src_addr_mode == IEEE154_ADDR_SHORT) {
		if (!pan_compression) {
			src_addr_offset += 2;
		}
		memcpy(&ret, buf + src_addr_offset, 2);
	}
	else if (src_addr_mode == IEEE154_ADDR_EXT) {
		if (!pan_compression) {
			src_addr_offset += 2;
		}
		memcpy(&ret, buf + src_addr_offset, 8);
	}

	DBG((KERN_INFO "[cc2520] - src_mode: %d dest_mode: %d pan_comp: %d src_addr_offset: %d\n",
		src_addr_mode, dest_addr_mode, pan_compression, src_addr_offset));

	return ret;
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

