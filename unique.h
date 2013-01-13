#ifndef UNIQUE_H
#define UNIQUE_H

#include "cc2520.h"

extern struct cc2520_interface *unique_top;
extern struct cc2520_interface *unique_bottom;

int cc2520_unique_init(void);
void cc2520_unique_free(void);

#endif