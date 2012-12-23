#ifndef SACK_H
#define SACK_H

#include "cc2520.h"

extern struct cc2520_interface *sack_top;
extern struct cc2520_interface *sack_bottom;

int cc2520_sack_init(void);
void cc2520_sack_free(void);

#endif