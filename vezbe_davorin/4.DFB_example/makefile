PROJECTROOT=/home/$(USER)/pputvios1

SDK_ROOTFS=$(PROJECTROOT)/toolchain/marvell/marvell-sdk-1046/rootfs
SDK_GALOIS=$(SDK_ROOTFS)/home/galois/include
SDK_INCLUDE=$(SDK_ROOTFS)/usr/include/ 

TOOLCHAIN_CROSS_COMPILE=$(PROJECTROOT)/toolchain/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi

CC=$(TOOLCHAIN_CROSS_COMPILE)-gcc

CFLAGS= -I. \
        -I./include/ \
        -I$(SDK_ROOTFS)/usr/include/ \
        -I$(SDK_ROOTFS)/usr/include/directfb/
        
CFLAGS += -D__LINUX__ -O0 -Wno-psabi 

CXXFLAGS = $(CFLAGS)

LIBS_PATH += -L$(SDK_ROOTFS)/home/galois/lib/directfb-1.4-6-libs

LIBS := $(LIBS_PATH) -ldirectfb -ldirect -lfusion -lrt

SRC= vezba_4T.c

all: dfb_example

dfb_example:
	$(CC) -o vezba_4 $(SRC) $(CFLAGS) $(LIBS)
    #cp vezba_4 /home/student/pputvios1/ploca/
clean:
	rm -f vezba_4
