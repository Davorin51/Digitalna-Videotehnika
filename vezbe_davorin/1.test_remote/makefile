PROJECTROOT =/home/$(USER)/pputvios1

SDK_ROOTFS = $(PROJECTROOT)/toolchain/marvell/marvell-sdk-1046/rootfs
SDK_GALOIS = $(SDK_ROOTFS)/home/galois/include
SDK_INCLUDE = $(SDK_ROOTFS)/usr/include/ 

TOOLCHAIN_CROSS_COMPILE = $(PROJECTROOT)/toolchain/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi

CC = $(TOOLCHAIN_CROSS_COMPILE)-gcc

LIBS = -lpthread

SRC = vezba_1.c remote.c 
OUT = $(PROJECTROOT)/ploca/vezba_1

all: tuning_sample

tuning_sample: 
	$(CC) -o $(OUT) $(SRC) $(CFLAGS) $(LIBS)
    
clean:
	rm -f $(OUT)
