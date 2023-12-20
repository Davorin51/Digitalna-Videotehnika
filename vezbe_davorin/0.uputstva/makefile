PROJECTROOT=/home/$(USER)/pputvios1

TOOLCHAIN_CROSS_COMPILE=$(PROJECTROOT)/toolchain/marvell/armv5-marvell-linux-gnueabi-softfp/bin/arm-marvell-linux-gnueabi

CC=$(TOOLCHAIN_CROSS_COMPILE)-gcc

SRC = vezba_0.c
OUT = $(PROJECTROOT)/ploca/vezba_0

all: tuning_sample

tuning_sample:
	$(CC) -o $(OUT) $(SRC)
    
clean:
	rm -f $(OUT)


