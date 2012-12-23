

#include "cc2520.h"

void cc2520_sack_tx()
{
	cc2520_interface_write_cb(0);
}