#ifndef THREAD__H
#define THREAD__H
#include <stdio.h>
#include <directfb.h>
#include <stdint.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>


#include <directfb.h>



void *myThreadRemote();
void setupScreen();
void drawNumber();



#endif