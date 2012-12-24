#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "ioctl.h"
#include <unistd.h>

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

	printf("Setting tx power\n");
	struct cc2520_set_txpower_data txpower_data;
	txpower_data.txpower = CC2520_TXPOWER_0DBM;
	ioctl(file_desc, CC2520_IO_RADIO_SET_TXPOWER);

	printf("Turning on the radio...\n");
	ioctl(file_desc, CC2520_IO_RADIO_INIT, NULL);
	ioctl(file_desc, CC2520_IO_RADIO_ON, NULL);

	int i = 0;

	for (i = 0; i < 100; i++) {
		printf("Sending a test message...\n");

		// 8 Byte Header, 6 Byte Payload.
		char test_msg[] = {0x41, 0x88, (char) i, 0x22, 0x00, 0xFF, 0xFF, 0x01, 0x00, 0x3F, 0x06, 0x00, 0x01, 0x72, (char) i};
		result = write(file_desc, test_msg, 15);

		printf("result %d\n", result);
		usleep(250 * 1000);
	}

	close(file_desc);
}