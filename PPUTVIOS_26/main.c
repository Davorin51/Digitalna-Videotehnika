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

static IDirectFBSurface *primary = NULL;
IDirectFB *dfbInterface = NULL;
IDirectFBWindow *window;
static int screenWidth = 0;
static int screenHeight = 0;
DFBSurfaceDescription surfaceDesc;
IDirectFBFont *fontInterface = NULL;
DFBFontDescription fontDesc;

void DFBInit(int32_t*, char***);
void timerInit();


void crniPravougaonik();
void clearScreen();
void drawChannel(int, int, int);
void drawVolume(int, int);
void clearChannel();
void clearVolume();
void drawTime();
void clearTimeDisplay();
void drawReminderDialog(const char*, const char*, const char*, int);
void scheduleReminder(int, int);
void displayReminderDialog(union sigval);


typedef struct{
    uint8_t streamType;
    uint16_t elementaryPID;
    uint16_t esInfoLength;
    uint8_t descriptor;
} Stream;

typedef struct{
    uint16_t sectionLength;
    uint16_t programNumber;
    uint16_t programInfoLength;
    uint8_t hasTTX;
    uint16_t audioPID;
    uint16_t videoPID;
    uint8_t streamCount;
    Stream streams[15];
} PMT;
PMT tablePMT[8];

typedef struct{
    uint16_t sectionLength;
    uint16_t transportStream;
    uint8_t versionNumber;
    uint16_t programNumber[8];
    uint16_t PID[8];
} PAT;
PAT tablePAT;


static int32_t inputFileDesc;
int i;
int parseFlag = 1;
int channelCount;
int channel = 1;
uint16_t PID[8];

void changeChannel(int channel);

int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventRead);
static void *remoteThreadTask();
static pthread_t remote;
static int remoteFlag = 1;
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

typedef struct {
  int bandwidth;
  int frequency;
  char module[50];
}tunerData;

typedef struct {
    int audioPID;
    int videoPID;
    char audioType[50];
    char videoType[50];
}initService;



void parse(char *filename, tunerData *tuner, initService *init){
    char line[255];
    char *token;
    FILE *fptr;
    fptr=fopen(filename,"r");
    if(fptr){

        fgets(line, sizeof(line), fptr);
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        tuner->frequency=atoi(strtok(NULL, "<"));
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        tuner->bandwidth=atoi(strtok(NULL, "<"));  
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        token=strtok(NULL, "<");
        sprintf(tuner->module, "%s", token);
        fgets(line, sizeof(line), fptr);
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        init->audioPID=atoi(strtok(NULL, "<"));
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        init->videoPID=atoi(strtok(NULL, "<"));
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        token=strtok(NULL, "<");
        sprintf(init->audioType, "%s", token);
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        token=strtok(NULL, "<");
        sprintf(init->videoType, "%s", token);
        
    }
    fclose(fptr);
}

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
    scheduleReminder(23, 34);
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
    int highlight = 1;
    int reminderActive = 0;
    
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
                            crniPravougaonik();
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
                            crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                                crniPravougaonik();
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
                         if (highlight == 1) {
                               
                                changeChannel(4);
                            }
                            // Clear the reminder dialog from the screen
                            clearScreen();
                            reminderActive = 0;
                            break;
                    }
                    case 106 : {
                        highlight = 1;
                        drawReminderDialog("Reminder Activated! Switch to Channel 4?", "YES", "NO", highlight);
                        break;
                    }
                    case 107: {
                        highlight = 2;
                        drawReminderDialog("Reminder Activated! Switch to Channel 4?", "YES", "NO", highlight);
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

parsePAT(uint8_t *buffer){
    tablePAT.sectionLength=(uint16_t)(((*(buffer+1)<<8)+*(buffer + 2)) & 0x0FFF);
    tablePAT.transportStream=(uint16_t)(((*(buffer+3)<<8)+*(buffer + 4)));
    tablePAT.versionNumber=(uint8_t)((*(buffer+5)>>1)& 0x1F);
    
    channelCount=(tablePAT.sectionLength*8-64)/32;
    int i=0;
    
    for(;i<channelCount;i++){
        tablePAT.programNumber[i]=(uint16_t)(*(buffer+(i*4)+8)<<8)+(*(buffer+(i*4)+9));
        tablePAT.PID[i]=(uint16_t)((*(buffer+(i*4)+10)<<8)+(*(buffer+(i*4)+11)) & 0x1FFF);
        printf("%d\tpid: %d\n", tablePAT.programNumber[i], tablePAT.PID[i]);
    }
    printf("\n\nSection arrived!!!\nsection length: %d\nts ID: %d\nversion number: %d\nchannel number: %d\n", tablePAT.sectionLength, tablePAT.transportStream, tablePAT.versionNumber, channelCount);
}

parsePMT(uint8_t *buffer){
    parseFlag=0;
        tablePMT[i].sectionLength=(uint16_t)(((*(buffer+1)<<8)+*(buffer + 2)) & 0x0FFF);
        tablePMT[i].programNumber=(uint16_t)((*(buffer+3)<<8)+*(buffer + 4));
        tablePMT[i].programInfoLength=(uint16_t)(((*(buffer+10)<<8)+*(buffer + 11))& 0x0FFF);
        tablePMT[i].streamCount=0;

        tablePMT[i].hasTTX=0;
        int j;

        printf("\n\nSection arrived!!! PMT: %d \t%d\t%d\n", tablePMT[i].sectionLength, tablePMT[i].programNumber, tablePMT[i].programInfoLength);

        uint8_t *m_buffer = (uint8_t*)buffer + 12 + tablePMT[i].programInfoLength;

        for ( j = 0; ((uint16_t)(m_buffer-buffer)+5<tablePMT[i].sectionLength); j++)
        {

            tablePMT[i].streams[j].streamType=*(m_buffer);
            tablePMT[i].streams[j].elementaryPID=(uint16_t)((*(m_buffer+1)<<8) + *(m_buffer+2)) & 0x1FFF;
            tablePMT[i].streams[j].esInfoLength=(uint16_t)((*(m_buffer+3)<<8) + *(m_buffer+4)) & 0x0FFF;
            tablePMT[i].streams[j].descriptor=(uint8_t)*(m_buffer+5);

            // find audio stream
            if(tablePMT[i].streams[j].streamType==3){
                tablePMT[i].audioPID=tablePMT[i].streams[j].elementaryPID;
            }
            else if(tablePMT[i].streams[j].streamType==2){
                tablePMT[i].videoPID=tablePMT[i].streams[j].elementaryPID;
            }

            if(tablePMT[i].streams[j].streamType==6 && tablePMT[i].streams[j].descriptor==86)
                tablePMT[i].hasTTX=1;

            printf("streamtype: %d epid: %d len %d desc: %d\n", tablePMT[i].streams[j].streamType, tablePMT[i].streams[j].elementaryPID, tablePMT[i].streams[j].esInfoLength, tablePMT[i].streams[j].descriptor);
            m_buffer+= 5 + tablePMT[i].streams[j].esInfoLength;
            tablePMT[i].streamCount++;
        }
        printf("%d\thasTTX: %d", tablePMT[i].streamCount, tablePMT[i].hasTTX);
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

void changeChannel(int channel){
    int videoPID, audioPID;

    audioPID = tablePMT[channel].audioPID;
    videoPID = tablePMT[channel].videoPID;

    if(videoStreamHandle){
            Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
            videoStreamHandle=0;
    }
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

    if(videoPID){
        Player_Stream_Create(playerHandle, sourceHandle, videoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle);
        Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    }
    else{
        videoStreamHandle=0;
        Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    }

    
}

int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead)
{
    int32_t ret = 0;
    
    /* read input events and put them in buffer */
    ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0)
    {
        printf("Error code %d", ret);
        return ERROR;
    }
    /* calculate number of read events */
    *eventsRead = ret / (int)sizeof(struct input_event);
    
    return NO_ERROR;
}

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


void DFBInit(int32_t* argc, char*** argv){
    /* initialize DirectFB */
    
	DFBCHECK(DirectFBInit(argc, argv));
    /* fetch the DirectFB interface */
	DFBCHECK(DirectFBCreate(&dfbInterface));
    /* tell the DirectFB to take the full screen for this application */
	DFBCHECK(dfbInterface->SetCooperativeLevel(dfbInterface, DFSCL_FULLSCREEN));
	
    
    /* create primary surface with double buffering enabled */
    
	surfaceDesc.flags = DSDESC_CAPS;
	surfaceDesc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	DFBCHECK (dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &primary));
    
    
    /* fetch the screen size */
    DFBCHECK (primary->GetSize(primary, &screenWidth, &screenHeight));
    
    
    /* clear the screen before drawing anything (draw black full screen rectangle)*/
    
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00));
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
	

	
	
    /* specify the height of the font by raising the appropriate flag and setting the height value */
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 70;
	
    /* create the font and set the created font for primary surface text drawing */
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));

    /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

void crniPravougaonik(){
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    fontDesc.height = 70;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Radio kanal",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                 /*y coordinate of the lower left corner of the resulting text*/ screenHeight/2-300,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Radio kanal",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                 /*y coordinate of the lower left corner of the resulting text*/ screenHeight/2-300,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

void clearScreen(){
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

void clearChannel(){
    int isRadio=(!tablePMT[channel].videoPID);
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

void clearVolume(){
    int isRadio=(!tablePMT[channel].videoPID);
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

void clearTimeDisplay() {
    // Assuming the time is displayed in the top right corner
    int posX = screenWidth - 410;
    int posY = 100;
    int width = 200; // Width of the area to clear
    int height = 200; // Height of the area to clear

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
    DFBCHECK(primary->FillRectangle(primary, posX, posY, width, height));
    DFBCHECK(primary->Flip(primary, NULL, 0));
}

void scheduleReminder(int remindAtHour, int remindAtMinute) {
    struct timeval now;
    struct timespec remindTimeSpec;

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




void drawTime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    struct tm *t = localtime(&now.tv_sec);  // Convert time_t to struct tm

    char timeStr[10];
    strftime(timeStr, sizeof(timeStr)-1, "%H:%M", t);

    // Positioning the rhombus in the top right corner
    int posX = screenWidth - 210; // 210 pixels from the right edge
    int posY = 100;  // 100 pixels from the top

    // Set color for the rhombus
    DFBCHECK(primary->SetColor(primary, 0x70, 0x00, 0x70, 0xff)); // Purple color

    // Draw the rhombus
    primary->FillTriangle(primary, posX, posY, posX + 100, posY + 100, posX - 100, posY + 100);
    primary->FillTriangle(primary, posX, posY + 200, posX - 100, posY + 100, posX + 100, posY + 100);

    // Set color for the text
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff)); // White color

    // Set font size for the time text
    fontDesc.height = 40; // Adjust font size if needed
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
    DFBCHECK(primary->SetFont(primary, fontInterface));

    // Draw the time string inside the rhombus
    DFBCHECK(primary->DrawString(primary, timeStr, -1, posX, posY + 100, DSTF_CENTER));

    // Flip the primary surface to update the display
    DFBCHECK(primary->Flip(primary, NULL, 0));

    signalEvent3.sigev_notify = SIGEV_THREAD;
    signalEvent3.sigev_notify_function = clearTimeDisplay;
    signalEvent3.sigev_value.sival_ptr = NULL;
    signalEvent3.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &signalEvent3, &timerId3);

    timerSpec3.it_value.tv_sec = 3; // 3 seconds
    timerSpec3.it_value.tv_nsec = 0;
    timer_settime(timerId3, 0, &timerSpec3, NULL);
}

void displayReminderDialog(union sigval sv) {
    // Display the reminder dialog
    drawReminderDialog("Reminder Activated! Switch to Channel 4?", "YES", "NO", 1);
}


void drawReminderDialog(const char* message, const char* firstOption, const char* secondOption, int highlight) {
    int dialogWidth = 400;
    int dialogHeight = 200;
    int dialogX = screenWidth / 2 - dialogWidth / 2;
    int dialogY = screenHeight / 2 - dialogHeight / 2;

    // Draw the dialog background
    DFBCHECK(primary->SetColor(primary, 0x70, 0x00, 0x70, 0xff)); // Adjust the color as needed
    DFBCHECK(primary->FillRectangle(primary, dialogX, dialogY, dialogWidth, dialogHeight));

    // Draw the message text
    fontDesc.height = 24; // Adjust the font size as needed
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
    DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    DFBCHECK(primary->DrawString(primary, message, -1, dialogX + 20, dialogY + 50, DSTF_LEFT));

    // Draw the options
    DFBCHECK(primary->DrawString(primary, firstOption, -1, dialogX + 50, dialogY + 150, DSTF_LEFT));
    DFBCHECK(primary->DrawString(primary, secondOption, -1, dialogX + 250, dialogY + 150, DSTF_LEFT));

    // Highlight the selected option
    if (highlight == 1) {
        // Highlight first option
        DFBCHECK(primary->DrawRectangle(primary, dialogX + 40, dialogY + 140, 100, 30));
    } else {
        // Highlight second option
        DFBCHECK(primary->DrawRectangle(primary, dialogX + 240, dialogY + 140, 100, 30));
    }

    // Flip the primary surface to update the display
    DFBCHECK(primary->Flip(primary, NULL, 0));
}



void drawChannel(int channel, int hasTTX, int isRadio){
    char str[10];
    sprintf(str, "%d", channel);
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
    primary->FillTriangle(primary, 200, 800, 300, 900, 100, 900);
    primary->FillTriangle(primary, 200, 1000, 100, 900, 300, 900);
    if(hasTTX){
        primary->FillTriangle(primary, 310, 910, 360, 960, 260, 960);
        primary->FillTriangle(primary, 310, 1010, 260, 960, 360, 960);
    }
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    fontDesc.height = 70;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Ch",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 900,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ str,
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 970,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    if(hasTTX){
        fontDesc.height = 35;
        DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	    DFBCHECK(primary->SetFont(primary, fontInterface));
        DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "TTX",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 310,
                                 /*y coordinate of the lower left corner of the resulting text*/ 980,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    }
    
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
    primary->FillTriangle(primary, 200, 800, 300, 900, 100, 900);
    primary->FillTriangle(primary, 200, 1000, 100, 900, 300, 900);

    if(hasTTX){
        primary->FillTriangle(primary, 310, 910, 360, 960, 260, 960);
        primary->FillTriangle(primary, 310, 1010, 260, 960, 360, 960);
    }
    fontDesc.height = 70;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Ch",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 900,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ str,
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 970,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    if(hasTTX){
        fontDesc.height = 35;
        DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	    DFBCHECK(primary->SetFont(primary, fontInterface));
        DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "TTX",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 310,
                                 /*y coordinate of the lower left corner of the resulting text*/ 980,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    }
    
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}






void drawVolume(int vol, int isRadio){
    
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
    
                               
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)-100, (screenWidth/2)+100, (screenHeight/2), (screenWidth/2)-100, (screenHeight/2));
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)+100, (screenWidth/2)-100, (screenHeight/2), (screenWidth/2)+100, (screenHeight/2));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));

    float firstScale=vol/25.0;
    float secondScale=(vol-25)/25.0;
    float thirdScale=(vol-50)/25.0;
    float fourthScale=(vol-75)/25.0;

    // truncate the negative values to 0
    secondScale = secondScale < 0 ? 0 : secondScale;
    thirdScale = thirdScale < 0 ? 0 : thirdScale;
    fourthScale = fourthScale < 0 ? 0 : fourthScale;

    // truncate the values to 1
    firstScale = firstScale > 1 ? 1 : firstScale;
    secondScale = secondScale > 1 ? 1 : secondScale;
    thirdScale = thirdScale > 1 ? 1 : thirdScale;
    fourthScale = fourthScale > 1 ? 1 : fourthScale;



    primary->FillTriangle(primary, (screenWidth/2)-1, (screenHeight/2)-90, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90*firstScale-1, (screenHeight/2)-90+90*firstScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90, (screenHeight/2), (screenWidth/2)+90-90*secondScale, (screenHeight/2)+90*secondScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-1, (screenHeight/2)+90, (screenWidth/2)-90*thirdScale-1, (screenHeight/2)+90-90*thirdScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-90, (screenHeight/2), (screenWidth/2)-90+90*fourthScale, (screenHeight/2)-90*fourthScale);
   

	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    //draw again on other buffer
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
                               
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)-100, (screenWidth/2)+100, (screenHeight/2), (screenWidth/2)-100, (screenHeight/2));
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)+100, (screenWidth/2)-100, (screenHeight/2), (screenWidth/2)+100, (screenHeight/2));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    primary->FillTriangle(primary, (screenWidth/2)-1, (screenHeight/2)-90, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90*firstScale-1, (screenHeight/2)-90+90*firstScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90, (screenHeight/2), (screenWidth/2)+90-90*secondScale, (screenHeight/2)+90*secondScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-1, (screenHeight/2)+90, (screenWidth/2)-1-90*thirdScale, (screenHeight/2)+90-90*thirdScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-90, (screenHeight/2), (screenWidth/2)-90+90*fourthScale, (screenHeight/2)-90*fourthScale);
    
    
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}