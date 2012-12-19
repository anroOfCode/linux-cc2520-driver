#ARCH=arm
#CROSS_COMPILE=arm-linux-gnueabi-gcc

TARGET = cc2520
OBJS = radio.o interface.o module.o platform.o

obj-m += $(TARGET).o
cc2520-objs = radio.o interface.o module.o platform.o

# Set this is your linux kernel checkout.
KDIR := /home/androbin/linux-rpi-3.2.27
PWD := $(shell pwd)

default:
		  $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
		  $(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

.PHONY: clean default