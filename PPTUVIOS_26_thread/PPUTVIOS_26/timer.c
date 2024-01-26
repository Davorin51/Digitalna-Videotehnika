#include "timer.h"
#include "global.h" // Include the globals header for access to global variables
#include "graphics.h"
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
// Initialize timers
void timerInit(){
    //reći OS-u da notifikaciju šalje pozivanjem specificirane funkcije iz posebne niti
    signalEvent.sigev_notify = SIGEV_THREAD; 
    //funkcija koju će OS prozvati kada interval istekne
    signalEvent.sigev_notify_function = clearChannel; 
    //argumenti funkcije
    signalEvent.sigev_value.sival_ptr = NULL;
    //atributi niti - if NULL default attributes are applied
    signalEvent.sigev_notify_attributes = NULL; 
    timer_create(/*sistemski sat za mjerenje vremena*/ CLOCK_REALTIME,                
                /*podešavanja timer-a*/ &signalEvent,                      
            /*mjesto gdje će se smjestiti ID novog timer-a*/ &timerId);

    //brisanje strukture prije setiranja vrijednosti
    memset(&timerSpec,0,sizeof(timerSpec));

    //specificiranje vremenskih podešavanja timer-a
    timerSpec.it_value.tv_sec = 3;
    timerSpec.it_value.tv_nsec = 0;

    //reći OS-u da notifikaciju šalje pozivanjem specificirane funkcije iz posebne niti
    signalEvent2.sigev_notify = SIGEV_THREAD; 
    //funkcija koju će OS prozvati kada interval istekne
    signalEvent2.sigev_notify_function = clearVolume; 
    //argumenti funkcije
    signalEvent2.sigev_value.sival_ptr = NULL;
    //atributi niti - if NULL default attributes are applied
    signalEvent2.sigev_notify_attributes = NULL; 
    timer_create(/*sistemski sat za mjerenje vremena*/ CLOCK_REALTIME,                
                /*podešavanja timer-a*/ &signalEvent2,                      
            /*mjesto gdje će se smjestiti ID novog timer-a*/ &timerId2);

    signalEvent4.sigev_notify = SIGEV_THREAD; 
    signalEvent4.sigev_notify_function = displayReminderDialog; 
    signalEvent4.sigev_value.sival_ptr = NULL; 
    signalEvent4.sigev_notify_attributes = NULL; 
    timer_create(CLOCK_REALTIME, &signalEvent4, &timerId4);

    //brisanje strukture prije setiranja vrijednosti
    memset(&timerSpec2,0,sizeof(timerSpec2));

    //specificiranje vremenskih podešavanja timer-a
    timerSpec2.it_value.tv_sec = 3; //3 seconds timeout
    timerSpec2.it_value.tv_nsec = 0;
}

// Schedule a reminder
void scheduleReminder(int remindAtHour, int remindAtMinute){
  struct timeval now;
    struct timespec remindTimeSpec;
    reminderActive = 1;

    gettimeofday(&now, NULL);
    struct tm *currentTime = localtime(&now.tv_sec);

    // Calculate the number of seconds until the reminder time
    int secondsUntilReminder = (remindAtHour - currentTime->tm_hour) * 3600 
                               + (remindAtMinute - currentTime->tm_min) * 60 
                               - currentTime->tm_sec;

    // If the reminder time is in the past, set it for the next day
    if (secondsUntilReminder < 0) {
        secondsUntilReminder += 24 * 3600; // Add 24 hours
    }

    remindTimeSpec.tv_sec = now.tv_sec + secondsUntilReminder;
    remindTimeSpec.tv_nsec = 0;

    // Set the timer for the reminder
    signalEvent4.sigev_notify = SIGEV_THREAD;
    signalEvent4.sigev_notify_function = displayReminderDialog;
    signalEvent4.sigev_value.sival_ptr = NULL;
    signalEvent4.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &signalEvent4, &timerId4);

    // Zero out the timerSpec4 structure before setting it
    memset(&timerSpec4, 0, sizeof(timerSpec4));
    timerSpec4.it_value.tv_sec = secondsUntilReminder;
    timerSpec4.it_value.tv_nsec = 0;
    
    // Set the timer to be relative (TIMER_ABSTIME is for absolute time)
    timer_settime(timerId4, 0, &timerSpec4, NULL);
}

// Display the reminder dialog
void displayReminderDialog(union sigval sv){
    highlight = 1; // Highlight the "YES" option by default
    drawReminderDialog("Reminder Activated!", "Switch to Channel 4?", "YES", "NO", highlight);
}
