#ifndef UTILS_H
#define UTILS_H



#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <directfb.h>
#include "tdp_api.h"
#include "types.h" // Include the header file where tunerData and initService are defined


void parse(char*, tunerData*, initService*);
int32_t getKeys(int32_t, uint8_t*, int32_t*);

#endif // UTILS_H
