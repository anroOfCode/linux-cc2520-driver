#include <asm/ioctl.h>
#include <linux/types.h>
#define BASE 0xCC

#ifndef __KERNEL__
#include <inttypes.h>
typedef uint8_t u8;
typedef uint16_t u16;
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

#define CC2520_IO_RADIO_INIT _IO(BASE, 0)
#define CC2520_IO_RADIO_ON _IO(BASE, 1)
#define CC2520_IO_RADIO_OFF _IO(BASE, 2)
#define CC2520_IO_RADIO_SET_CHANNEL _IOW(BASE, 3, struct cc2520_set_channel_data)
#define CC2520_IO_RADIO_SET_ADDRESS _IOW(BASE, 4, struct cc2520_set_address_data)