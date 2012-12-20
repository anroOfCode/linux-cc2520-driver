#ifndef CC2520_H
#define CC2520_H

//////////////////////////////
// Configuration for driver
/////////////////////////////

// Physical mapping of GPIO pins on the CC2520
// to GPIO pins on the linux microcontroller.

#define CC2520_GPIO_0 -1
#define CC2520_GPIO_1 25
#define CC2520_GPIO_2 24
#define CC2520_GPIO_3 22
#define CC2520_GPIO_4 23
#define CC2520_GPIO_5 -1
#define CC2520_RESET 17

#define CC2520_DEBUG_0 21
#define CC2520_DEBUG_1 18

// Logical mapping of CC2520 GPIO pins to
// functions, we're going to keep these static
// for now, and map to the defaults, which
// mirror the CC2420 implementation.

// A 1Mhz Clock with 50% duty cycle.
#define CC2520_CLOCK CC2520_GPIO_0

// High when at least one byte is
// in the RX FIFO buffer
#define CC2520_FIFO CC2520_GPIO_1

// High when the number of bytes exceeds
// a programmed theshold or a full packet
// has been received.
#define CC2520_FIFOP CC2520_GPIO_2

// Clear channel assessment
#define CC2520_CCA CC2520_GPIO_3

// Start frame delimiter
#define CC2520_SFD CC2520_GPIO_4

// For Raspberry pi we're using the following
// SPI bus and CS pin.
#define SPI_BUS 0
#define SPI_BUS_CS0 0
#define SPI_BUS_SPEED 500000

//////////////////////////////
// Structs and definitions
/////////////////////////////

// XOSC Period in nanoseconds.
#define CC2520_XOSC_PERIOD 31

struct cc2520_gpioState {
	unsigned int fifopIrq;
	unsigned int sfdIrq;
};

struct cc2520_state {
	// Hardware
	struct cc2520_gpioState gpios;

	// Character Device
	unsigned int major;

};

// Radio
void cc2520_radio_init(void);

// Platform
int cc2520_plat_setupGpioPins(void);
void cc2520_plat_freeGpioPins(void);

int cc2520_plat_setupSpi(void);
void cc2520_plat_freeSpi(void);

extern struct cc2520_state state;

#endif