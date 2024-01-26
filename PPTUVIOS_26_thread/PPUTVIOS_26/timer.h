#ifndef TIMER_H
#define TIMER_H


#include <signal.h> // Include signal.h for union sigval

void timerInit();
void scheduleReminder(int, int);
void displayReminderDialog(union sigval);

#endif // TIMER_H
