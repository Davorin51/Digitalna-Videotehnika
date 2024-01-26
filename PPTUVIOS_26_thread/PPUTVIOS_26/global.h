#ifndef GLOBAL_H
#define GLOBAL_H

#include <directfb.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
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
#include <math.h>
#include "types.h"
// DirectFB interfaces
extern IDirectFBSurface *primary;
extern IDirectFB *dfbInterface;
extern IDirectFBFont *fontInterface;

extern int videoStreamHandle;
extern uint32_t playerHandle;
extern uint32_t sourceHandle;
extern int audioStreamHandle;
extern int parseFlag;

// Screen dimensions
extern int screenWidth;
extern int screenHeight;

// Font description
extern DFBFontDescription fontDesc;

// Timer IDs
extern timer_t timerId;
extern timer_t timerId2;
extern timer_t timerId3;
extern timer_t timerId4;

// Signal events
extern struct sigevent signalEvent;
extern struct sigevent signalEvent2;
extern struct sigevent signalEvent3;
extern struct sigevent signalEvent4;

// Timer specifications
extern struct itimerspec timerSpec;
extern struct itimerspec timerSpecOld;
extern struct itimerspec timerSpec2;
extern struct itimerspec timerSpecOld2;
extern struct itimerspec timerSpec3;
extern struct itimerspec timerSpec4;

// Flags and other global variables
extern int32_t timerFlags;
extern int32_t timerFlags2;
extern int32_t inputFileDesc;
extern int channelCount;
extern int channel;
extern int remoteFlag;
extern int reminderActive;
extern int highlight;

// PMT and PAT tables
extern PMT tablePMT[8];
extern PAT tablePAT;

// Thread for remote control
//extern pthread_t remote;

// Function prototypes (if shared across different source files)
void DFBInit(int32_t*, char***);
//void timerInit();
// ... other function prototypes

#endif // GLOBALS_H
