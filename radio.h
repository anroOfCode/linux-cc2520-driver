#ifndef RADIO_H
#define RADIO_H

#include <linux/types.h>
#include <linux/semaphore.h>  /* Semaphore */
#include <linux/workqueue.h>
#include <linux/spinlock.h>

enum cc2520_radio_state_enum {
    CC2520_RADIO_STATE_IDLE,
    CC2520_RADIO_STATE_RX,
    CC2520_RADIO_STATE_TX
};

// Radio Initializers
int cc2520_radio_init(void);
void cc2520_radio_free(void);

// Radio Commands
void cc2520_radio_start(void);
void cc2520_radio_on(void);
void cc2520_radio_off(void);
void cc2520_radio_set_channel(int channel);
void cc2520_radio_set_address(u16 short_addr, u64 extended_addr, u16 pan_id);
void cc2520_radio_set_txpower(u8 power);

// Radio Interrupt Callbacks
void cc2520_radio_sfd_occurred(u64 nano_timestamp);
void cc2520_radio_fifop_occurred(void);

int cc2520_radio_tx(u8 *buf, u8 len);

extern struct cc2520_interface *radio_top;

#endif