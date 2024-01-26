#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stddef.h>

void crniPravokutnik();
void clearScreen();
void drawChannel(int, int, int);
void drawVolume(int, int);
void clearChannel();
void clearVolume();
void drawTime();
void clearTimeDisplay();
//void DFBInit(int32_t*, char ***);
void drawReminderDialog(const char*, const char*, const char*, const char*, int);


#endif

