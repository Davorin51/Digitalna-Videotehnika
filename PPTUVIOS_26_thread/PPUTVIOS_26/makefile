PROJECTROOT=/home/$(USER)/pputvios1

SDK_ROOTFS=$(PROJECTROOT)/toolchain/marvell/marvell-sdk-1046/rootfs
SDK_GALOIS=$(SDK_ROOTFS)/home/galois/include

CC=$(PROJECTROOT)/toolchain/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi-gcc

# Include directories
INCS += -I./include/
INCS += -I./inih
INCS += -I$(PROJECTROOT)/toolchain/tdp_api
INCS += -I$(SDK_ROOTFS)/usr/include/
INCS += -I$(SDK_ROOTFS)/usr/include/directfb/
INCS += -I$(SDK_GALOIS)/Common/include/
INCS += -I$(SDK_GALOIS)/OSAL/include/
INCS += -I$(SDK_GALOIS)/OSAL/include/CPU1/
INCS += -I$(SDK_GALOIS)/PE/Common/include/

# Library paths
LIBS_PATH += -L$(PROJECTROOT)/toolchain/tdp_api
LIBS_PATH += -L$(SDK_ROOTFS)/home/galois/lib/
LIBS_PATH += -L$(SDK_ROOTFS)/home/galois/lib/directfb-1.4-6-libs

# Libraries to link
LIBS = $(LIBS_PATH) -ltdp -lOSAL -lshm -lPEAgent -ldirectfb -ldirect -lfusion -lrt

# Compiler flags
CFLAGS += -D__LINUX__ -O0 -Wno-psabi --sysroot=$(SDK_ROOTFS)

# Source files
SRCS += ./main.c
SRCS += ./channel.c
SRCS += ./timer.c
SRCS += ./graphics.c
SRCS += ./utils.c
# SRCS += ./inih/ini.c
# SRCS += ./remote.c
# SRCS += ./config.c

# Output file
OUT = $(PROJECTROOT)/ploca/tv_app_26_thread

all:
	$(CC) $(CFLAGS) $(INCS) $(SRCS) -o $(OUT) $(LIBS)
	cp config.xml $(PROJECTROOT)/ploca/

clean:
	rm -f $(OUT)

