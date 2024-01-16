PROJECTROOT=/home/$(USER)/pputvios1

SDK_ROOTFS=$(PROJECTROOT)/toolchain/marvell/marvell-sdk-1046/rootfs
SDK_GALOIS=$(SDK_ROOTFS)/home/galois/include

CC=$(PROJECTROOT)/toolchain/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi-gcc

INCS += -I./include/
INCS += -I./inih
INCS += -I$(PROJECTROOT)/toolchain/tdp_api
INCS += -I$(SDK_ROOTFS)/usr/include/
INCS += -I$(SDK_ROOTFS)/usr/include/directfb/
INCS += -I$(SDK_GALOIS)/Common/include/
INCS += -I$(SDK_GALOIS)/OSAL/include/
INCS += -I$(SDK_GALOIS)/OSAL/include/CPU1/
INCS += -I$(SDK_GALOIS)/PE/Common/include/

LIBS_PATH += -L$(PROJECTROOT)/toolchain/tdp_api
LIBS_PATH += -L$(SDK_ROOTFS)/home/galois/lib/
LIBS_PATH += -L$(SDK_ROOTFS)/home/galois/lib/directfb-1.4-6-libs

LIBS = $(LIBS_PATH) -ltdp -lOSAL -lshm -lPEAgent -ldirectfb -ldirect -lfusion -lrt

CFLAGS += -D__LINUX__ -O0 -Wno-psabi --sysroot=$(SDK_ROOTFS)

SRCS += ./main.c
# SRCS += ./inih/ini.c
# SRCS += ./remote.c
# SRCS += ./config.c
# SRCS += ./graphics

OUT = $(PROJECTROOT)/ploca/tv_app_26

all:
	$(CC) -o $(OUT) $(INCS) $(SRCS) $(CFLAGS) $(LIBS)
	cp config.xml $(PROJECTROOT)/ploca/

