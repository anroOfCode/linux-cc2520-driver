#include <asm/ioctl.h>
#include <linux/types.h>
#define BASE 0xCC

#ifndef __KERNEL__
#include <inttypes.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

struct cc2520_set_channel_data {
	u8 channel;
};

struct cc2520_set_address_data {
	u16 short_addr;
	u64 extended_addr;
	u16 pan_id;
};

struct cc2520_set_ack_data {
	u32 timeout;
};

struct cc2520_set_lpl_data {
	u32 window;
	u32 interval;
	bool enabled;
};

struct cc2520_set_csma_data {
	u32 min_backoff;
	u32 init_backoff;
	u32 cong_backoff;
	bool enabled;
};

struct cc2520_set_print_messages_data {
	u8 debug_level;
};

// Possible TX Powers:
#define CC2520_TXPOWER_5DBM 0xF7
#define CC2520_TXPOWER_3DBM 0xF2
#define CC2520_TXPOWER_2DBM 0xAB
#define CC2520_TXPOWER_1DBM 0x13
#define CC2520_TXPOWER_0DBM 0x32
#define CC2520_TXPOWER_N2DBM 0x81
#define CC2520_TXPOWER_N4DBM 0x88
#define CC2520_TXPOWER_N7DBM 0x2C
#define CC2520_TXPOWER_N18DBM 0x03

struct cc2520_set_txpower_data {
	u8 txpower;
};

#define CC2520_IO_RADIO_INIT _IO(BASE, 0)
#define CC2520_IO_RADIO_ON _IO(BASE, 1)
#define CC2520_IO_RADIO_OFF _IO(BASE, 2)
#define CC2520_IO_RADIO_SET_CHANNEL _IOW(BASE, 3, struct cc2520_set_channel_data)
#define CC2520_IO_RADIO_SET_ADDRESS _IOW(BASE, 4, struct cc2520_set_address_data)
#define CC2520_IO_RADIO_SET_TXPOWER _IOW(BASE, 5, struct cc2520_set_txpower_data)
#define CC2520_IO_RADIO_SET_ACK _IOW(BASE, 6, struct cc2520_set_ack_data)
#define CC2520_IO_RADIO_SET_LPL _IOW(BASE, 7, struct cc2520_set_lpl_data)
#define CC2520_IO_RADIO_SET_CSMA _IOW(BASE, 8, struct cc2520_set_csma_data)
#define CC2520_IO_RADIO_SET_PRINT _IOW(BASE, 9, struct cc2520_set_print_messages_data)
