#ifndef PACKET_H
#define PACKET_H

#include <linux/types.h>

typedef u16 ieee154_addr_t;
typedef u16 ieee154_panid_t;

typedef struct cc2520_header_t
{
	u8 length;
} cc2520_header_t;

typedef struct ieee154_simple_header_t
{
	u16 fcf;
	u8 dsn;
	u16 destpan;
	u16 dest;
	u16 src;
} ieee154_simple_header_t;


typedef struct cc2520packet_header_t
{
	cc2520_header_t cc2520;
	ieee154_simple_header_t ieee154;
} cc2520packet_header_t;

enum {
	IEEE154_BROADCAST_ADDR = 0xffff,
	IEEE154_BROADCAST_PAN  = 0xffff,
	IEEE154_LINK_MTU       = 127,
};

struct ieee154_frame_addr {
	ieee154_addr_t  ieee_src;
	ieee154_addr_t  ieee_dst;
	ieee154_panid_t ieee_dstpan;
};

enum ieee154_fcf_enums {
	IEEE154_FCF_FRAME_TYPE = 0,
	IEEE154_FCF_SECURITY_ENABLED = 3,
	IEEE154_FCF_FRAME_PENDING = 4,
	IEEE154_FCF_ACK_REQ = 5,
	IEEE154_FCF_INTRAPAN = 6,
	IEEE154_FCF_DEST_ADDR_MODE = 10,
	IEEE154_FCF_SRC_ADDR_MODE = 14,
};

enum ieee154_fcf_type_enums {
	IEEE154_TYPE_BEACON = 0,
	IEEE154_TYPE_DATA = 1,
	IEEE154_TYPE_ACK = 2,
	IEEE154_TYPE_MAC_CMD = 3,
	IEEE154_TYPE_MASK = 7,
};

enum ieee154_fcf_addr_mode_enums {
	IEEE154_ADDR_NONE = 0,
	IEEE154_ADDR_SHORT = 2,
	IEEE154_ADDR_EXT = 3,
	IEEE154_ADDR_MASK = 3,
};

enum
{
	IEEE154_DATA_FRAME_MASK = (IEEE154_TYPE_MASK << IEEE154_FCF_FRAME_TYPE) 
		| (1 << IEEE154_FCF_INTRAPAN) 
		| (IEEE154_ADDR_MASK << IEEE154_FCF_DEST_ADDR_MODE) 
		| (IEEE154_ADDR_MASK << IEEE154_FCF_SRC_ADDR_MODE),

	IEEE154_DATA_FRAME_VALUE = (IEEE154_TYPE_DATA << IEEE154_FCF_FRAME_TYPE) 
		| (1 << IEEE154_FCF_INTRAPAN) 
		| (IEEE154_ADDR_SHORT << IEEE154_FCF_DEST_ADDR_MODE) 
		| (IEEE154_ADDR_SHORT << IEEE154_FCF_SRC_ADDR_MODE),

	IEEE154_DATA_FRAME_PRESERVE = (1 << IEEE154_FCF_ACK_REQ) 
		| (1 << IEEE154_FCF_FRAME_PENDING),

	IEEE154_ACK_FRAME_LENGTH = 3,	// includes the FCF, DSN
	IEEE154_ACK_FRAME_MASK = (IEEE154_TYPE_MASK << IEEE154_FCF_FRAME_TYPE), 
	IEEE154_ACK_FRAME_VALUE = (IEEE154_TYPE_ACK << IEEE154_FCF_FRAME_TYPE),
};

ieee154_simple_header_t* cc2520_packet_get_header(u8 *buf);
u8* cc2520_packet_get_length_field(u8 * buf);
u8* cc2520_packet_get_payload(u8 * buf);

bool cc2520_packet_requires_ack_reply(u8 *buf);
bool cc2520_packet_requires_ack_wait(u8 *buf);
void cc2520_packet_create_ack(u8 *pkt, u8 *buf);


#endif