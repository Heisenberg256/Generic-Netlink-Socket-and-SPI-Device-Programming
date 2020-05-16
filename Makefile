KDIR:=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux/usr/src/kernel
export PATH:=/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin:/opt/iot-devkit/1.7.2/sysroots/x86_64-pokysdk-linux/usr/bin/i586-poky-linux:$(PATH)
CC = i586-poky-linux-gcc
CROSS_COMPILE = i586-poky-linux-

PWD:= $(shell pwd)

ARCH = x86
SROOT=/opt/iot-devkit/1.7.2/sysroots/i586-poky-linux
EXTRA_CFLAGS += -Wall

LDLIBS = -L$(SROOT)/usr/lib
CCFLAGS = -I$(SROOT)/usr/include/libnl3

APP = main
EXAMPLE = main

obj-m:= spi_hcsr_netlink.o

.PHONY:all
all: main spi_hcsr_netlink.ko

spi_hcsr_netlink.ko:
	make ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

main:
	$(CC) -Wall -o $(EXAMPLE) main.c -lpthread $(CCFLAGS) -lnl-genl-3 -lnl-3

.PHONY:clean
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *.o $(EXAMPLE) $(APP) $(SPI)

deploy:
	tar czf programs.tar.gz $(APP) $(EXAMPLE) $(SPI) spi_hcsr_netlink.ko
	scp programs.tar.gz root@10.0.1.100:/home/root
	ssh root@10.0.1.100 'tar xzf programs.tar.gz'
