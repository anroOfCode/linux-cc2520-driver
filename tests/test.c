#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "../ioctl.h"

int main(char ** argv, int argc)
{
	printf("Testing ioctl\n");
	int file_desc;
	file_desc = open("/dev/radio", 0);	
	ioctl(file_desc, CC2520_IO_RADIO_INIT, NULL);
	close(file_desc);
}