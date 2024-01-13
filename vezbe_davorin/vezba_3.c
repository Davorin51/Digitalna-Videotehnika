#include "tdp_api.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

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

#define DESIRED_FREQUENCY 818000000	/* Tune frequency in Hz */
#define BANDWIDTH 8    				/* Bandwidth in Mhz */
#define VIDEO_PID 101				/* Channel video pid */
#define AUDIO_PID 103				/* Channel audio pid */

typedef struct Reminder {
    char time[6]; // format vremena HH:MM
    unsigned int channel_index;
    struct Reminder *next;
} Reminder;

typedef struct Config {
    unsigned int frequency;
    unsigned int bandwidth;
    char module[10];
    unsigned int apid;
    unsigned int vpid;
    char atype[10];
    char vtype[10];
    Reminder *reminders;
} Config;

Config loadConfig(const char *filename) {
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    Config config = {0}; // Inicijalizacija na nule

    // Učitavanje XML dokumenta
    doc = xmlReadFile(filename, NULL, 0);
    if (doc == NULL) {
        printf("Error: could not parse file %s\n", filename);
        exit(-1);
    }

    // Dobijanje korenskog elementa
    root_element = xmlDocGetRootElement(doc);

    // Obrada korenskog elementa i ekstrakcija podataka
    xmlNode *currentNode = root_element->children;
    Reminder *lastReminder = NULL;

    while (currentNode) {
        if (currentNode->type == XML_ELEMENT_NODE) {
            if (strcmp((char *)currentNode->name, "frequency") == 0) {
                config.frequency = atoi((char *)xmlNodeGetContent(currentNode));
            } else if (strcmp((char *)currentNode->name, "bandwidth") == 0) {
                config.bandwidth = atoi((char *)xmlNodeGetContent(currentNode));
            } else if (strcmp((char *)currentNode->name, "module") == 0) {
                strcpy(config.module, (char *)xmlNodeGetContent(currentNode));
            } else if (strcmp((char *)currentNode->name, "init_service") == 0) {
                xmlNode *serviceNode = currentNode->children;
                while (serviceNode) {
                    if (serviceNode->type == XML_ELEMENT_NODE) {
                        if (strcmp((char *)serviceNode->name, "apid") == 0) {
                            config.apid = atoi((char *)xmlNodeGetContent(serviceNode));
                        } else if (strcmp((char *)serviceNode->name, "vpid") == 0) {
                            config.vpid = atoi((char *)xmlNodeGetContent(serviceNode));
                        } else if (strcmp((char *)serviceNode->name, "atype") == 0) {
                            strcpy(config.atype, (char *)xmlNodeGetContent(serviceNode));
                        } else if (strcmp((char *)serviceNode->name, "vtype") == 0) {
                            strcpy(config.vtype, (char *)xmlNodeGetContent(serviceNode));
                        }
                    }
                    serviceNode = serviceNode->next;
                }
            } else if (strcmp((char *)currentNode->name, "reminder") == 0) {
                Reminder *newReminder = (Reminder *)malloc(sizeof(Reminder));
                if (!newReminder) {
                    fprintf(stderr, "Greska pri alociranju memorije za podsjetnik\n");
                    exit(-1);
                }

                xmlNode *reminderNode = currentNode->children;
                while (reminderNode) {
                    if (reminderNode->type == XML_ELEMENT_NODE) {
                        if (strcmp((char *)reminderNode->name, "time") == 0) {
                            strcpy(newReminder->time, (char *)xmlNodeGetContent(reminderNode));
                        } else if (strcmp((char *)reminderNode->name, "channel_index") == 0) {
                            newReminder->channel_index = atoi((char *)xmlNodeGetContent(reminderNode));
                        }
                    }
                    reminderNode = reminderNode->next;
                }

                newReminder->next = NULL;
                if (lastReminder) {
                    lastReminder->next = newReminder;
                } else {
                    config.reminders = newReminder;
                }
                lastReminder = newReminder;
            }
        }
        currentNode = currentNode->next;
    }

    // Oslobađanje resursa
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return config;
}

static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

int32_t mySecFilterCallback(uint8_t *buffer);
static int32_t tunerStatusCallback(t_LockStatus status);

int main()
{
	struct timespec lockStatusWaitTime;
	struct timeval now;
    int32_t result;
    
    uint32_t playerHandle = 0;
    uint32_t sourceHandle = 0;
    uint32_t filterHandle = 0;
    uint32_t audioStreamHandle = 0;
    uint32_t videoStreamHandle = 0;


    gettimeofday(&now,NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec+10;
       
    /*Initialize tuner device*/
    if(Tuner_Init())
    {
        printf("\n%s : ERROR Tuner_Init() fail\n", __FUNCTION__);
        return -1;
    }
    
    /* Register tuner status callback */
    if(Tuner_Register_Status_Callback(tunerStatusCallback))
    {
		printf("\n%s : ERROR Tuner_Register_Status_Callback() fail\n", __FUNCTION__);
	}
    
    /*Lock to frequency*/
    if(!Tuner_Lock_To_Frequency(DESIRED_FREQUENCY, BANDWIDTH, DVB_T))
    {
        printf("\n%s: INFO Tuner_Lock_To_Frequency(): %d Hz - success!\n",__FUNCTION__,DESIRED_FREQUENCY);
    }
    else
    {
        printf("\n%s: ERROR Tuner_Lock_To_Frequency(): %d Hz - fail!\n",__FUNCTION__,DESIRED_FREQUENCY);
        Tuner_Deinit();
        return -1;
    }
    
    // TODO: Check if this timeout works
    /* Wait for tuner to lock*/
    pthread_mutex_lock(&statusMutex);
    if(ETIMEDOUT == pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime))
    {
        printf("\n%s:ERROR Lock timeout exceeded!\n",__FUNCTION__);
        Tuner_Deinit();
        return -1;
    }
    pthread_mutex_unlock(&statusMutex);
   
    /**TO DO:**/
    /*Initialize player, set PAT pid to demultiplexer and register section filter callback*/
    /* Initialize player (demux is a part of player) */
    result = Player_Init(&playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Init");
    
    /* Open source (open data flow between tuner and demux) */
    result = Player_Source_Open(playerHandle, &sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Open");
    
    /* Set filter to demux */
    result = Demux_Set_Filter(playerHandle, 0xC8, 0x02, &filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Set_Filter");
    
    /* Register section filter callback */
    result = Demux_Register_Section_Filter_Callback(mySecFilterCallback);
    ASSERT_TDP_RESULT(result, "Demux_Register_Section_Filter_Callback");
    
     /**TO DO:**/
    /*Play audio and video*/
    result = Player_Stream_Create(playerHandle, sourceHandle, 101, VIDEO_TYPE_MPEG2, &videoStreamHandle);
	result = Player_Stream_Create(playerHandle, sourceHandle, 103, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
	
    /* Wait for a while */

    /* Wait for a while to receive several PAT sections */
    fflush(stdin);
    getchar();
    
    /* Deinitialization */
    
    /* Free demux filter */
    
   
    /**TO DO:**/
    /*Deinitialization*/
    Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

    
    result = Player_Stream_Create(playerHandle, sourceHandle, 201, VIDEO_TYPE_MPEG2, &videoStreamHandle);
	result = Player_Stream_Create(playerHandle, sourceHandle, 203, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);

       /* Wait for a while to receive several PAT sections */
    fflush(stdin);
    getchar();
    /**TO DO:**/
    /*Deinitialization*/
    Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

 


    
    /* Close previously opened source */
    result = Player_Source_Close(playerHandle, sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Close");
    

    Demux_Free_Filter(playerHandle, filterHandle);
    Player_Source_Close(playerHandle, sourceHandle);
    Player_Deinit(playerHandle);

    /*Deinitialize tuner device*/
    Tuner_Deinit();
  
    return 0;
}

int32_t tunerStatusCallback(t_LockStatus status)
{
    if(status == STATUS_LOCKED)
    {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("\n%s -----TUNER LOCKED-----\n",__FUNCTION__);
    }
    else
    {
        printf("\n%s -----TUNER NOT LOCKED-----\n",__FUNCTION__);
    }
    return 0;
}

void parsePMT(uint8_t *buffer){
    uint8_t *current_buffer_position = NULL;
    uint32_t parsed_length = 0;
    uint8_t elementaryInfoCount = 0;


    uint16_t sectionLength = ((*(buffer+1) << 8) + *(buffer+2)) & 0x0FFF;
    uint16_t programInfoLength = ((*(buffer+10) << 8) + *(buffer+11)) & 0x0FFF;
    

    printf("Section legnth and program info lenght: %d\t%d\t\n", sectionLength, programInfoLength);
    parsed_length = 12 + programInfoLength + 4 - 3;
    current_buffer_position = (uint8_t *)(buffer + 12 + programInfoLength);

    while(parsed_length < sectionLength & elementaryInfoCount < 20){

        uint16_t streamType = current_buffer_position[0];
        uint16_t elementaryPid = ((current_buffer_position[1] & 0x1F) << 8) | current_buffer_position[2];
        uint16_t esInfoLength = ((current_buffer_position[3] & 0x0F) << 8) | current_buffer_position[4];

        current_buffer_position += 5 + esInfoLength;
        parsed_length += 5 + esInfoLength;
        elementaryInfoCount++;
        printf("%d\t%d\t%d\n", streamType, elementaryPid, esInfoLength);
    }
}

/**TO DO:**/
/*Parse PAT Table*/
int32_t mySecFilterCallback(uint8_t *buffer)
{
    printf("\n\nSection arrived!!!\n\n");



    if(!buffer){
        return -1;
    }
    uint32_t i;
    uint8_t table_id = buffer[0];
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];
    printf("Section length: %d\n", section_length);


    if (table_id == 0x00){
        uint16_t program_number, pid;
    for(i = 0; i< (section_length-9) / 4; i++){
        program_number = (buffer[(8 + i * 4)]<<8) | buffer[9 + i*4];
        pid = ((buffer[(10 + i*4)] & 0x1F) << 8) | buffer[11 + i*4];

        if(program_number == 0){
            printf("Network PID: %u\n", pid);
        }
        else {
            printf("Program number: %u, PID: %u\n", program_number, pid);
        }
    }
    }
    else if (table_id == 0x02){
        parsePMT(buffer);
    }
    else {
        return 0;
    }
   
    return 0;
}


