#ifndef REMOTE_H
#define REMOTE_H

#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>

#define NUM_EVENTS 5
#define NO_ERROR 0
#define ERROR 1

void* remote_control_handler(void* arg);
int32_t getKeys(int32_t count, uint8_t* bud, int32_t* eventsRead);

//extern int exit_flag;
//extern int channel_counter;

#endif 

