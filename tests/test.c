#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl.h"

int main(char ** argv, int argc)
{
	printf("Testing cc2520 driver...\n");
	int file_desc;
	file_desc = open("/dev/radio", 0);	

	printf("Setting channel\n");
	struct cc2520_set_channel_data chan_data;
	chan_data.channel = 26;
	ioctl(file_desc, CC2520_IO_RADIO_SET_CHANNEL, &chan_data);

	printf("Setting address\n");
	struct cc2520_set_address_data addr_data;
	addr_data.short_addr = 0x0001;
	addr_data.extended_addr = 0x0000000000000001;
	addr_data.pan_id = 0x22;
	ioctl(file_desc, CC2520_IO_RADIO_SET_ADDRESS, &addr_data);

	ioctl(file_desc, CC2520_IO_RADIO_INIT, NULL);
	ioctl(file_desc, CC2520_IO_RADIO_ON, NULL);
	close(file_desc);
}