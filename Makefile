#ARCH=arm
#CROSS_COMPILE=arm-linux-gnueabi-gcc

TARGET = cc2520
OBJS = cc2520.o

obj-m += $(TARGET).o

# Set this is your linux kernel checkout.
KDIR := /home/androbin/linux-rpi-3.2.27
PWD := $(shell pwd)

default:
		  $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
		  $(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

.PHONY: clean default