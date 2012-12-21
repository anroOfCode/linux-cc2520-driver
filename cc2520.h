#ifndef CC2520_H
#define CC2520_H

#include <linux/types.h>

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
#define SPI_BUFF_SIZE 256

//////////////////////////////
// Structs and definitions
/////////////////////////////

// XOSC Period in nanoseconds.
#define CC2520_XOSC_PERIOD 31

struct cc2520_gpio_state {
	unsigned int fifop_irq;
	unsigned int sfd_irq;
};

struct cc2520_state {
	// Hardware
	struct cc2520_gpio_state gpios;

	// Character Device
	unsigned int major;

	struct spi_device *spi_device;
	u8 *tx_buf;
	u8 *rx_buf;
};

// Radio
void cc2520_radio_init(void);

// Platform
int cc2520_plat_gpio_init(void);
void cc2520_plat_gpio_free(void);

int cc2520_plat_spi_init(void);
void cc2520_plat_spi_free(void);

// Interface

int cc2520_interface_init(void);
void cc2520_interface_free(void);

void cc2520_radio_writeRegister(u8 reg, u8 value);
extern struct cc2520_state state;

extern const char cc2520_name[];

//////////////////////////////
// Radio Opcodes and Register Defs
/////////////////////////////

typedef union cc2520_status {
	u16 value;
	struct {
	  unsigned  rx_active    :1;
	  unsigned  tx_active    :1;
	  unsigned  dpu_l_active :1;
	  unsigned  dpu_h_active :1;

	  unsigned  exception_b  :1;
	  unsigned  exception_a  :1;
	  unsigned  rssi_valid   :1;
	  unsigned  xosc_stable  :1;
	};
} cc2520_status_t;

enum cc2520_register_enums
{
    // FREG Registers
    CC2520_FRMFILT0     = 0x00,
    CC2520_FRMFILT1     = 0x01,
    CC2520_SRCMATCH     = 0x02,
    CC2520_SRCSHORTEN0  = 0x04,
    CC2520_SRCSHORTEN1  = 0x05,
    CC2520_SRCSHORTEN2  = 0x06,
    CC2520_SRCEXTEN0    = 0x08,
    CC2520_SRCEXTEN1    = 0x09,
    CC2520_SRCEXTEN2    = 0x0A,
    CC2520_FRMCTRL0     = 0x0C,
    CC2520_FRMCTRL1     = 0x0D,
    CC2520_RXENABLE0    = 0x0E,
    CC2520_RXENABLE1    = 0x0F,
    CC2520_EXCFLAG0     = 0x10,
    CC2520_EXCFLAG1     = 0x11,
    CC2520_EXCFLAG2     = 0x12,
    CC2520_EXCMASKA0    = 0x14,
    CC2520_EXCMASKA1    = 0x15,
    CC2520_EXCMASKA2    = 0x16,
    CC2520_EXCMASKB0    = 0x18,
    CC2520_EXCMASKB1    = 0x19,
    CC2520_EXCMASKB2    = 0x1A,
    CC2520_EXCBINDX0    = 0x1C,
    CC2520_EXCBINDX1    = 0x1D,
    CC2520_EXCBINDY0    = 0x1E,
    CC2520_EXCBINDY1    = 0x1F,
    CC2520_GPIOCTRL0    = 0x20,
    CC2520_GPIOCTRL1    = 0x21,
    CC2520_GPIOCTRL2    = 0x22,
    CC2520_GPIOCTRL3    = 0x23,
    CC2520_GPIOCTRL4    = 0x24,
    CC2520_GPIOCTRL5    = 0x25,
    CC2520_GPIOPOLARITY = 0x26,
    CC2520_GPIOCTRL     = 0x28,
    CC2520_DPUCON       = 0x2A,
    CC2520_DPUSTAT      = 0x2C,
    CC2520_FREQCTRL     = 0x2E,
    CC2520_FREQTUNE     = 0x2F,
    CC2520_TXPOWER      = 0x30,
    CC2520_TXCTRL       = 0x31,
    CC2520_FSMSTAT0     = 0x32,
    CC2520_FSMSTAT1     = 0x33,
    CC2520_FIFOPCTRL    = 0x34,
    CC2520_FSMCTRL      = 0x35,
    CC2520_CCACTRL0     = 0x36,
    CC2520_CCACTRL1     = 0x37,
    CC2520_RSSI         = 0x38,
    CC2520_RSSISTAT     = 0x39,
    CC2520_RXFIRST      = 0x3C,
    CC2520_RXFIFOCNT    = 0x3E,
    CC2520_TXFIFOCNT    = 0x3F,

    // SREG registers
    CC2520_CHIPID       = 0x40,
    CC2520_VERSION      = 0x42,
    CC2520_EXTCLOCK     = 0x44,
    CC2520_MDMCTRL0     = 0x46,
    CC2520_MDMCTRL1     = 0x47,
    CC2520_FREQEST      = 0x48,
    CC2520_RXCTRL       = 0x4A,
    CC2520_FSCTRL       = 0x4C,
    CC2520_FSCAL0       = 0x4E,
    CC2520_FSCAL1       = 0x4F,
    CC2520_FSCAL2       = 0x50,
    CC2520_FSCAL3       = 0x51,
    CC2520_AGCCTRL0     = 0x52,
    CC2520_AGCCTRL1     = 0x53,
    CC2520_AGCCTRL2     = 0x54,
    CC2520_AGCCTRL3     = 0x55,
    CC2520_ADCTEST0     = 0x56,
    CC2520_ADCTEST1     = 0x57,
    CC2520_ADCTEST2     = 0x58,
    CC2520_MDMTEST0     = 0x5A,
    CC2520_MDMTEST1     = 0x5B,
    CC2520_DACTEST0     = 0x5C,
    CC2520_DACTEST1     = 0x5D,
    CC2520_ATEST        = 0x5E,
    CC2520_DACTEST2     = 0x5F,
    CC2520_PTEST0       = 0x60,
    CC2520_PTEST1       = 0x61,
    CC2520_RESERVED     = 0x62,
    CC2520_DPUBIST      = 0x7A,
    CC2520_ACTBIST      = 0x7C,
    CC2520_RAMBIST      = 0x7E,
};

enum cc2520_spi_command_enums
{
    CC2520_CMD_SNOP           = 0x00, //
    CC2520_CMD_IBUFLD         = 0x02, //
    CC2520_CMD_SIBUFEX        = 0x03, //
    CC2520_CMD_SSAMPLECCA     = 0x04, //
    CC2520_CMD_SRES           = 0x0f, //
    CC2520_CMD_MEMORY_MASK    = 0x0f, //
    CC2520_CMD_MEMORY_READ    = 0x10, // MEMRD
    CC2520_CMD_MEMORY_WRITE   = 0x20, // MEMWR
    CC2520_CMD_RXBUF          = 0x30, //
    CC2520_CMD_RXBUFCP        = 0x38, //
    CC2520_CMD_RXBUFMOV       = 0x32, //
    CC2520_CMD_TXBUF          = 0x3A, //
    CC2520_CMD_TXBUFCP        = 0x3E, //
    CC2520_CMD_RANDOM         = 0x3C, //
    CC2520_CMD_SXOSCON        = 0x40, //
    CC2520_CMD_STXCAL         = 0x41, //
    CC2520_CMD_SRXON          = 0x42, //
    CC2520_CMD_STXON          = 0x43, //
    CC2520_CMD_STXONCCA       = 0x44, //
    CC2520_CMD_SRFOFF         = 0x45, //
    CC2520_CMD_SXOSCOFF        = 0x46, //
    CC2520_CMD_SFLUSHRX       = 0x47, //
    CC2520_CMD_SFLUSHTX       = 0x48, //
    CC2520_CMD_SACK           = 0x49, //
    CC2520_CMD_SACKPEND       = 0x4A, //
    CC2520_CMD_SNACK          = 0x4B, //
    CC2520_CMD_SRXMASKBITSET  = 0x4C, //
    CC2520_CMD_SRXMASKBITCLR  = 0x4D, //
    CC2520_CMD_RXMASKAND      = 0x4E, //
    CC2520_CMD_RXMASKOR       = 0x4F, //
    CC2520_CMD_MEMCP          = 0x50, //
    CC2520_CMD_MEMCPR         = 0x52, //
    CC2520_CMD_MEMXCP         = 0x54, //
    CC2520_CMD_MEMXWR         = 0x56, //
    CC2520_CMD_BCLR           = 0x58, //
    CC2520_CMD_BSET           = 0x59, //
    CC2520_CMD_CTR_UCTR       = 0x60, //
    CC2520_CMD_CBCMAC         = 0x64, //
    CC2520_CMD_UCBCMAC        = 0x66, //
    CC2520_CMD_CCM            = 0x68, //
    CC2520_CMD_UCCM           = 0x6A, //
    CC2520_CMD_ECB            = 0x70, //
    CC2520_CMD_ECBO           = 0x72, //
    CC2520_CMD_ECBX           = 0x74, //
    CC2520_CMD_INC            = 0x78, //
    CC2520_CMD_ABORT          = 0x7F, //
    CC2520_CMD_REGISTER_READ  = 0x80, //
    CC2520_CMD_REGISTER_WRITE = 0xC0, //
};

#endif