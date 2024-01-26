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
#include "graphics.h" // Include your graphics header
#include "channel.h"  // Include your channel header
#include "timer.h"    // Include your timer header
#include "global.h"
#include "utils.h"
#include "types.h"

#define CONFIG_FILE "config.xml"
#define NUM_EVENTS  5

#define NON_STOP    1

/* error codes */
#define NO_ERROR 		0
#define ERROR			1


/* helper macro for error checking */
#define DFBCHECK(x...)                                      \
{                                                           \
DFBResult err = x;                                          \
                                                            \
if (err != DFB_OK)                                          \
  {                                                         \
    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );  \
    DirectFBErrorFatal( #x, err );                          \
  }                                                         \
}


//timer
timer_t timerId;
//specificiranje i stvaranje timer-a
struct sigevent signalEvent;


struct itimerspec timerSpec;
struct itimerspec timerSpecOld;
int32_t timerFlags = 0;

//timer
timer_t timerId2;
//specificiranje i stvaranje timer-a
struct sigevent signalEvent2;


struct itimerspec timerSpec2;
struct itimerspec timerSpecOld2;
timer_t timerId3;
struct sigevent signalEvent3;
struct itimerspec timerSpec3;
timer_t timerId4;
struct sigevent signalEvent4;
struct itimerspec timerSpec4;


int32_t timerFlags2 = 0;

int32_t inputFileDesc;
int i;
int parseFlag = 1;
int channelCount;
int channel = 1;
uint16_t PID[8];

//void changeChannel(int channel);

//int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventRead);
static void *remoteThreadTask();
static pthread_t remote;
int remoteFlag = 1;
int reminderActive = 1;
int highlight = 0;
static inline void textColor(int32_t attr, int32_t fg, int32_t bg)
{
   char command[13];

   /* Command is the control command to the terminal */
   sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
   printf("%s", command);
}

#define ASSERT_TDP_RESULT(x,y)  if(NO_ERROR == x) \
                                    printf("%s success\n", y); \
                                else{ \
                                    textColor(1,1,0); \
                                    printf("%s fail\n", y); \
                                    textColor(0,7,0); \
                                    return -1; \
                                }

int32_t myPrivateTunerStatusCallback(t_LockStatus status);
int32_t mySecFilterCallback(uint8_t *buffer);
pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

int32_t result;
int videoStreamHandle, audioStreamHandle;
uint32_t playerHandle = 0;
uint32_t sourceHandle = 0;
uint32_t filterHandle = 0;

struct timespec lockStatusWaitTime;
struct timeval now;

int32_t main(int32_t argc, char** argv)
{
    
    tunerData data;
    initService init;
    parse(CONFIG_FILE, &data, &init);
    printf("freq: %d\n", data.frequency);
    printf("band: %d\n", data.bandwidth);
    printf("%s\n", data.module);

    printf("apid: %d\n", init.audioPID);
    printf("vpid: %d\n", init.videoPID);
    printf("at: %s\n", init.audioType);
    printf("vt: %s\n", init.videoType);

    
    
    gettimeofday(&now,NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec+10;

    
    
    /* Initialize tuner */
    result = Tuner_Init();
    ASSERT_TDP_RESULT(result, "Tuner_Init");
    
    /* Register tuner status callback */
    result = Tuner_Register_Status_Callback(myPrivateTunerStatusCallback);
    ASSERT_TDP_RESULT(result, "Tuner_Register_Status_Callback");
    
    /* Lock to frequency */
    result = Tuner_Lock_To_Frequency(data.frequency, data.bandwidth, DVB_T);
    ASSERT_TDP_RESULT(result, "Tuner_Lock_To_Frequency");
    
    pthread_mutex_lock(&statusMutex);
    if(ETIMEDOUT == pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime))
    {
        printf("\n\nLock timeout exceeded!\n\n");
        return -1;
    }
    pthread_mutex_unlock(&statusMutex);
    
    /* Initialize player (demux is a part of player) */
    result = Player_Init(&playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Init");
    
    /* Open source (open data flow between tuner and demux) */
    result = Player_Source_Open(playerHandle, &sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Open");
    
    /* Set filter to demux */
    result = Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Set_Filter");
    
    /* Register section filter callback */
    result = Demux_Register_Section_Filter_Callback(mySecFilterCallback);
    ASSERT_TDP_RESULT(result, "Demux_Register_Section_Filter_Callback");

    Player_Stream_Create(playerHandle, sourceHandle, init.videoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle);
    Player_Stream_Create(playerHandle, sourceHandle, init.audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    Player_Volume_Set(playerHandle, 20*10000000);
    sleep(1);
    
    result = Demux_Free_Filter(playerHandle, filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Free_Filter");

    for(i=1;i<8;i++){
        parseFlag = 1;
        result = Demux_Set_Filter(playerHandle, tablePAT.PID[i], 0x02, &filterHandle);
        ASSERT_TDP_RESULT(result, "Demux_Set_Filter");
        while(parseFlag);
        result = Demux_Free_Filter(playerHandle, filterHandle);
        ASSERT_TDP_RESULT(result, "Demux_Free_Filter");
    }
    scheduleReminder(23, 43);
    pthread_create(&remote, NULL, &remoteThreadTask, NULL);
    DFBInit(&argc, &argv);
    timerInit();
	while(remoteFlag);
    pthread_join(remote, NULL);
    
   
    /* Deinitialization */
    
    primary->Release(primary);
	dfbInterface->Release(dfbInterface);
    timer_delete(timerId);
    timer_delete(timerId2);
    timer_delete(timerId3);
    timer_delete(timerId4);
    
    /* Close previously opened source */
    result = Player_Source_Close(playerHandle, sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Close");
    
    /* Deinit player */
    result = Player_Deinit(playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Deinit");
    
    /* Deinit tuner */
    result = Tuner_Deinit();
    ASSERT_TDP_RESULT(result, "Tuner_Deinit");
    
    return 0;
}


void *remoteThreadTask()
{
    
    const char* dev = "/dev/input/event0";
    char deviceName[20];
    struct input_event* eventBuf;
    uint32_t eventCnt;
    uint32_t i;
    int vol = 20;
    int mute = 0;
    int isRadio;
    int localChannel=1;
 //   int highlight = 1;
 //   int reminderActive = 1;
    
    inputFileDesc = open(dev, O_RDWR);
    if(inputFileDesc == -1)
    {
        printf("Error while opening device (%s) !", strerror(errno));
	    //return ERROR;
    }
    
    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
	printf("RC device opened succesfully [%s]\n", deviceName);
    
    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!eventBuf)
    {
        printf("Error allocating memory !");
        //return ERROR;
    }
    while(remoteFlag)
    {
        /* read input eventS */
        
        if(getKeys(NUM_EVENTS, (uint8_t*)eventBuf, &eventCnt))
        {
			printf("Error while reading input events !");
			//return ERROR;
		}
        
        for(i = 0; i < eventCnt; i++)
        {
            if(eventBuf[i].type == 1 && (eventBuf[i].value == 1 || eventBuf[i].value == 2)){
                
                switch (eventBuf[i].code){
                    case 358: {
                        //info
                        timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);  
                        drawChannel(channel, 1, !(tablePMT[channel].videoPID));
                        drawTime();
                        sleep(3);
                        clearScreen();
                        break;
                    }
                    case 60: {
                        mute = !mute;
                        if(mute){
                            Player_Volume_Set(playerHandle, 0);
                            timer_settime(timerId2,timerFlags2,&timerSpec2,&timerSpecOld2);  
                            drawVolume(0, !(tablePMT[channel].videoPID));
                        }
                        else{
                            Player_Volume_Set(playerHandle, vol*10000000);
                            timer_settime(timerId2,timerFlags2,&timerSpec2,&timerSpecOld2);  
                            drawVolume(vol, !(tablePMT[channel].videoPID));
                        }
                        break;
                    }
                    case 63: {
                        if(vol < 100){
                            vol++;
                        }
                        if(eventBuf[i].value == 2 && vol<100) vol++;
                        Player_Volume_Set(playerHandle, vol*10000000);
                        drawVolume(vol, !(tablePMT[channel].videoPID));
                        timer_settime(timerId2,timerFlags2,&timerSpec2,&timerSpecOld2);
                        printf("vol: %d\n", vol);
                        break;
                    }
                    case 64: {
                        if(vol != 0){
                            vol--;
                        }
                        if(eventBuf[i].value == 2 && vol>0) vol--;

                        Player_Volume_Set(playerHandle, vol*10000000);
                        drawVolume(vol, !(tablePMT[channel].videoPID));
                        timer_settime(timerId2,timerFlags2,&timerSpec2,&timerSpecOld2);
                        printf("vol: %d\n", vol);
                        break;
                    }
                    case 62: {
                        if(channel >= channelCount-1){
                            channel = 1;
                        }else{
                            channel++;
                        }
                        isRadio=!(tablePMT[channel].videoPID);
                        if(isRadio){
                            crniPravokutnik();
                        }
                        else{
                            clearScreen();
                        }

                        changeChannel(channel);
                        drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);  
                        printf("%d\n",channel);
                        
                        break;
                    }
                    case 61: {
                        if(channel == 1){
                            channel = channelCount-1;
                        }
                        else{
                            channel--;
                        }
                        isRadio=!(tablePMT[channel].videoPID);
                        if(isRadio){
                            crniPravokutnik();
                        }
                        else{
                            clearScreen();
                        }
                        changeChannel(channel);
                        drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);  
                        printf("%d\n",channel);
                        break;
                    }
                    case 2: {
                        localChannel = 1;
                        
                        if(localChannel<=channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);    
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 3: {
                        localChannel = 2;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 4: {
                        localChannel = 3;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 5: {
                        localChannel = 4;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 6: {
                        localChannel = 5;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 7: {
                        localChannel = 6;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 8: {
                        localChannel = 7;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 9: {
                        localChannel = 8;
                        
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    case 10: {
                        localChannel = 9;
                       
                        if(localChannel<channelCount){
                            channel=localChannel;
                            isRadio=!(tablePMT[channel].videoPID);
                            if(isRadio){
                                crniPravokutnik();
                            }
                            else{
                                clearScreen();
                            }
                            timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
                            printf("%d\n",channel);
                            changeChannel(channel);
                            drawChannel(channel, tablePMT[channel].hasTTX, isRadio);
                        }
                        break;
                    }
                    
                    case 105: {
                        if(reminderActive) {
                            highlight = 1;
                            drawReminderDialog("Reminder Activated!", "Switch to Channel 4?", "YES", "NO", highlight);
                     }       
                     break;
                    }
                    case 106 : {
                        if (reminderActive) {
                            highlight = 2; // Highlight "NO"
                            drawReminderDialog("Reminder Activated!", "Switch to Channel 4?", "YES", "NO", highlight);
                        }
                    break;
                    }
                    case 108: {
                        localChannel = 4;
                        channel = localChannel;
                    if (reminderActive && highlight == 1) {
                        changeChannel(channel);
                        reminderActive = 0;  // Reset the reminder active flag
                        clearScreen();  // Clear the dialog from the screen
                       
                    }
                    else {
                        sleep(15);
                        clearScreen();
                    }
                     break;
                    }
                        

                    case 102:{
                        Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
                        Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);
                        remoteFlag = 0;
                        break;
                    }
                }
            }
        }
    }
    
    free(eventBuf);
    
    return NO_ERROR;
 }
 
int32_t mySecFilterCallback(uint8_t *buffer){

    uint8_t tableId = *buffer; 
    if(tableId==0x00){
        parsePAT(buffer);
    }
    else if(tableId==0x02){
        parsePMT(buffer);
    }
    return 0;
}



int32_t myPrivateTunerStatusCallback(t_LockStatus status)
{
    if(status == STATUS_LOCKED)
    {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("\n\n\tCALLBACK LOCKED\n\n");
    }
    else
    {
        printf("\n\n\tCALLBACK NOT LOCKED\n\n");
    }
    return 0;
}