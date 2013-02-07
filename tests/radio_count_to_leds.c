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
	chan_data.channel = 24;
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
	ioctl(file_desc, CC2520_IO_RADIO_SET_TXPOWER, &txpower_data);

	printf("Turning on the radio...\n");
	ioctl(file_desc, CC2520_IO_RADIO_INIT, NULL);
	ioctl(file_desc, CC2520_IO_RADIO_ON, NULL);

	uint16_t i = 0;

	while (1) {
		printf("Sending RCTL packet.\n");
		// 8 Byte Header, 6 Byte Payload.
		char test_msg[] = {0x0D, 0x41, 0x88, (char) i, 0x22, 0xFF, 0xFF, 0x00, 0x01, 0x06, (char) (i >> 8), (char) (i & 0xFF)};
		result = write(file_desc, test_msg, 12);
		usleep(250 * 1000);
	}

	printf("Turning off the radio...\n");
	ioctl(file_desc, CC2520_IO_RADIO_OFF, NULL);

	close(file_desc);
}
