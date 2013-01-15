#ifndef CSMA_H
#define CSMA_H

#include "cc2520.h"

extern struct cc2520_interface *csma_top;
extern struct cc2520_interface *csma_bottom;

int cc2520_csma_init(void);
void cc2520_csma_free(void);

void cc2520_csma_set_enabled(bool enabled);
void cc2520_csma_set_min_backoff(int timeout);
void cc2520_csma_set_init_backoff(int timeout);
void cc2520_csma_set_cong_backoff(int timeout);

#endif