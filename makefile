PROJECTROOT=/home/student/pputvios1/toolchain

SDK_ROOT=$(PROJECTROOT)/marvell/marvell-sdk-1046
SDK_ROOTFS=$(SDK_ROOT)/rootfs
SDK_GALOIS=$(SDK_ROOTFS)/home/galois/include
SDK_INCLUDE=$(SDK_ROOTFS)/usr/include/ 

TOOLCHAIN_CROSS_COMPILE=$(PROJECTROOT)/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi

CROSS_COMPILE=$(TOOLCHAIN_CROSS_COMPILE)

CC_PREFIX=$(CROSS_COMPILE)-
CC=$(CC_PREFIX)gcc
CXX=$(CC_PREFIX)g++
LD=$(CC_PREFIX)ld
ROOTFS_PATH=$(SDK_ROOTFS)
GALOIS_INCLUDE=$(SDK_GALOIS)

CFLAGS= -I. \
        -I./include/ \
        -I$(ROOTFS_PATH)/usr/include/ \
        -I$(ROOTFS_PATH)/usr/include/directfb/
        
INCS =	-I$(PROJECTROOT)/tdp_api
INCS += -I$(GALOIS_INCLUDE)/Common/include/     \
		-I$(GALOIS_INCLUDE)/OSAL/include/		\
		-I$(GALOIS_INCLUDE)/OSAL/include/CPU1/	\
		-I$(GALOIS_INCLUDE)/PE/Common/include/
        
CFLAGS += -D__LINUX__ -O0 -Wno-psabi --sysroot=$(ROOTFS_PATH)

CXXFLAGS = $(CFLAGS)

LIBS_PATH = -L$(PROJECTROOT)/tdp_api
LIBS_PATH += -L$(ROOTFS_PATH)/home/galois/lib/
LIBS_PATH += -L$(ROOTFS_PATH)/home/galois/lib/directfb-1.4-6-libs

LIBS := $(LIBS_PATH) -ldirectfb -ldirect -lfusion -lrt
LIBS += $(LIBS_PATH) -ltdp
LIBS += $(LIBS_PATH) -lOSAL	-lshm -lPEAgent

SRC= tv_aplikacija.c

all: dfb_example

dfb_example:
	$(CC) -o tv_aplikacija $(INCS) $(SRC) $(CFLAGS) $(LIBS)
	cp tv_aplikacija /home/student/pputvios1/ploca/
	cp config.ini /home/student/pputvios1/ploca/
clean:
	rm -f tv_aplikacija
