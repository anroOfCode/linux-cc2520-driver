#ARCH=arm
#CROSS_COMPILE=arm-linux-gnueabi-gcc
DRIVER = spike

TARGET = cc2520
OBJS = radio.o interface.o module.o platform.o sack.o lpl.o packet.o csma.o unique.o

obj-m += $(TARGET).o
cc2520-objs = radio.o interface.o module.o platform.o sack.o lpl.o packet.o csma.o unique.o

# Set this is your linux kernel checkout.
KDIR := /home/androbin/rpi/linux
PWD := $(shell pwd)

default:
		  $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
		  $(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

.PHONY: clean default
