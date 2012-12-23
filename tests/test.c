#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl.h"

int main(char ** argv, int argc)
{
	int result = 0;
	printf("Testing cc2520 driver...\n");
	int file_desc;
	file_desc = open("/dev/radio", O_RDWR);	

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

	printf("Turning on the radio...\n");
	ioctl(file_desc, CC2520_IO_RADIO_INIT, NULL);
	//ioctl(file_desc, CC2520_IO_RADIO_ON, NULL);

	printf("Sending a test message...\n");
	char test_msg[] = {0xAA, 0xBB, 0xCC, 0xDD};;
	result = write(file_desc, test_msg, 4);

	printf("result %d\n", result);
	close(file_desc);
}