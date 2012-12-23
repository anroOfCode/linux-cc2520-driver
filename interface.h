#ifndef INTERFACE_H
#define iNTERFACE_H

// Interface
int cc2520_interface_init(void);
void cc2520_interface_free(void);

extern struct cc2520_interface *interface_bottom;

#endif