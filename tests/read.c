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
	ioctl(file_desc, CC2520_IO_RADIO_SET_TXPOWER, &txpower_data);

	printf("Turning on the radio...\n");
	ioctl(file_desc, CC2520_IO_RADIO_INIT, NULL);
	ioctl(file_desc, CC2520_IO_RADIO_ON, NULL);

	int i = 0;

	char buf[256];
	char pbuf[1024];
	char *buf_ptr = NULL;

	for (i = 0; i < 100; i++) {
		printf("Receiving a test message...\n");
		result = read(file_desc, buf, 127);

		printf("result %d\n", result);
		if (result > 0) {
			buf_ptr = pbuf;
			for (i = 0; i < result; i++)
			{
				buf_ptr += sprintf(buf_ptr, " 0x%02X", buf[i]);
			}
			*(buf_ptr) = '\0';
			printf("read %s\n", pbuf);
		}
	}

	printf("Turning off the radio...\n");
	ioctl(file_desc, CC2520_IO_RADIO_OFF, NULL);

	close(file_desc);
}