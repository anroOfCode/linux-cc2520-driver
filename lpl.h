#ifndef LPL_H
#define LPL_H

#include "cc2520.h"

extern struct cc2520_interface *lpl_top;
extern struct cc2520_interface *lpl_bottom;

int cc2520_lpl_init(void);
void cc2520_lpl_free(void);

void cc2520_lpl_set_enabled(bool enabled);
void cc2520_lpl_set_listen_length(int length);
void cc2520_lpl_set_wakeup_interval(int interval);

#endif