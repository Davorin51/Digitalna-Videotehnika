PROJECTROOT=/home/$(USER)/pputvios1

SDK_ROOTFS=$(PROJECTROOT)/toolchain/marvell/marvell-sdk-1046/rootfs
SDK_INCLUDE=$(SDK_ROOTFS)/usr/include/ 

TOOLCHAIN_CROSS_COMPILE=$(PROJECTROOT)/toolchain/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi

CC=$(TOOLCHAIN_CROSS_COMPILE)-gcc

INCS =	-I$(PROJECTROOT)/toolchain/tdp_api 		\
		-I./include/ 							\
		-I$(SDK_INCLUDE)

LIBS_PATH =	-L$(PROJECTROOT)/toolchain/tdp_api	\
			-L$(SDK_ROOTFS)/home/galois/lib/

LIBS = $(LIBS_PATH) -ltdp -lOSAL -lshm -lPEAgent

CFLAGS += -D__LINUX__ -O0 -Wno-psabi --sysroot=$(SDK_ROOTFS)

SRC = vezba_2.c
OUT = vezba_2

all: tuning_sample

tuning_sample:
	$(CC) -o $(OUT) $(INCS) $(SRC) $(CFLAGS) $(LIBS)
	cp vezba_2 /home/student/pputvios1/ploca/
	cp config.ini /home/student/pputvios1/ploca/
clean:
	rm -f vezba_2
