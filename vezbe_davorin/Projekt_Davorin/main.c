#include "tdp_api.h"
//#include <libxml/parser.h>
//#include <libxml/tree.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include "directfb.h"

#include <linux/input.h>
#include <fcntl.h>

#include <unistd.h>

#include <errno.h>



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

#define NUM_EVENTS  5

#define NON_STOP    1

/* error codes */
#define NO_ERROR 		0
#define ERROR			1

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
static int32_t inputFileDesc;
void myThreadRemote();



enum REMOTE_BUTTON {
    REMOTE_MUTE = 60,
    REMOTE_CHANNEL_MINUS,
    REMOTE_CHANNEL_PLUS,
    REMOTE_VOLUME_PLUS,
    REMOTE_VOLUME_MINUS,
    REMOTE_CHANNEL_1=2,
    REMOTE_CHANNEL_2,
    REMOTE_CHANNEL_3,
    REMOTE_CHANNEL_4,
    REMOTE_CHANNEL_5,
    REMOTE_CHANNEL_6,
    REMOTE_CHANNEL_7,
    REMOTE_CHANNEL_8,
    REMOTE_CHANNEL_9,
    REMOTE_EXIT=102,
    REMOTE_INFO=358
};


int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventRead);

    IDirectFBSurface *primary = NULL;
    IDirectFB *dfbInterface = NULL; //sadrzi razne podatke o tom interfejsu
    int screenWidth = 0;
    int screenHeight = 0;
    DFBSurfaceDescription surfaceDesc; 

    IDirectFBFont *fontInterface = NULL;
    DFBFontDescription fontDesc;


typedef struct Config {
    int frequency;
    int bandwidth;
    char module[10];
    int apid;
    int vpid;
    char atype[10];
    char vtype[10];
    // Reminder struktura i slični elementi mogu biti dodati po potrebi
} Config;

typedef struct Service {
    uint16_t program_number;
    uint16_t video_pid;
    uint16_t audio_pid;
    struct Service *next;
} Service;

Service *services_head = NULL; // Početak liste servisa



int GetXmlTagValue(char *pResBuf, char *pTag, char *pTagValue)
{
    int len=0, pos = 0;
    char openTag[100] = {0}; //Opening Tag
    char closeTag[100] = {0};//Closing Tag
    int posOpenTag=0, posClosingTag=0;
    //check enter buffer
    len = strlen(pResBuf);
    if (len<=0)
    {
        return -1;
    }
    //Create Opening Tag
    memset(openTag, 0, sizeof(openTag));
    strcpy(openTag, "<");
    strcat(openTag, pTag);
    strcat(openTag, ">");
    //Create Closing tag
    memset(closeTag, 0, sizeof(closeTag));
    strcpy(closeTag, "</");
    strcat(closeTag, pTag);
    strcat(closeTag, ">");
    //Get len of open and close tag
    const int lenOpenTag = strlen(openTag);
    const int lenCloseTag = strlen(closeTag);
    //Get Opening tag position
    for (pos=0; pos<len; pos++)
    {
        if ( !memcmp(openTag,(pResBuf+pos),lenOpenTag))
        {
            posOpenTag = pos;
            break;
        }
    }
    //Get closing tag position
    for (pos=0; pos<len; pos++)
    {
        if ( !memcmp(closeTag,(pResBuf+pos),lenCloseTag) )
        {
            posClosingTag = pos;
            break;
        }
    }
    //get the tag value
    if ( (posOpenTag !=0) && (posClosingTag !=0) )
    {
        const int lenTagVal = posClosingTag-posOpenTag-lenOpenTag;
        const char * const pStartPosTagVal = pResBuf+posOpenTag+lenOpenTag;
        if (lenTagVal)
        {
            //Get tag value
            memcpy(pTagValue,pStartPosTagVal, lenTagVal);
            if (strlen(pTagValue))
            {
                return 1;
            }
        }
    }
    return -1;
}



void parseConfig(char* xmlData, Config* config) {
    char pTagValue[100] = {0};

    // Obrada 'frequency'
    GetXmlTagValue(xmlData, "frequency", pTagValue);
    config->frequency = strtol(pTagValue, NULL, 10);
    memset(pTagValue, 0, sizeof(pTagValue));

    // Obrada 'bandwidth'
    GetXmlTagValue(xmlData, "bandwidth", pTagValue);
    config->bandwidth = strtol(pTagValue, NULL, 10);
    memset(pTagValue, 0, sizeof(pTagValue));

    // Obrada 'module'
    GetXmlTagValue(xmlData, "module", pTagValue);
    strncpy(config->module, pTagValue, sizeof(config->module) - 1);
    config->module[sizeof(config->module) - 1] = '\0';
    memset(pTagValue, 0, sizeof(pTagValue));

    // Obrada 'apid'
    GetXmlTagValue(xmlData, "apid", pTagValue);
    config->apid = strtol(pTagValue, NULL, 10);
    memset(pTagValue, 0, sizeof(pTagValue));

    // Obrada 'vpid'
    GetXmlTagValue(xmlData, "vpid", pTagValue);
    config->vpid = strtol(pTagValue, NULL, 10);
    memset(pTagValue, 0, sizeof(pTagValue));

    // Obrada 'atype'
    GetXmlTagValue(xmlData, "atype", pTagValue);
    strncpy(config->atype, pTagValue, sizeof(config->atype) - 1);
    config->atype[sizeof(config->atype) - 1] = '\0';
    memset(pTagValue, 0, sizeof(pTagValue));

    // Obrada 'vtype'
    GetXmlTagValue(xmlData, "vtype", pTagValue);
    strncpy(config->vtype, pTagValue, sizeof(config->vtype) - 1);
    config->vtype[sizeof(config->vtype) - 1] = '\0';
    memset(pTagValue, 0, sizeof(pTagValue));

  
}




static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;
uint16_t program_number_1;

int32_t mySecFilterCallback(uint8_t *buffer);
static int32_t tunerStatusCallback(t_LockStatus status);

Service* addOrUpdateService(uint16_t program_number, uint16_t video_pid, uint16_t audio_pid) {
    // Pretražujemo listu da vidimo da li već imamo ovaj servis
    Service *current = services_head;
    while (current != NULL) {
        if (current->program_number == program_number_1) {
            // Ažuriramo postojeći servis
            current->video_pid = video_pid;
            current->audio_pid = audio_pid;
            return current;
        }
        current = current->next;
    }

    // Ako servis ne postoji, dodajemo novi
    Service *newService = (Service *)malloc(sizeof(Service));
	if (newService) {
		newService->program_number = program_number;
		newService->video_pid = video_pid;
		newService->audio_pid = audio_pid;
		newService->next = services_head;
		services_head = newService;
	}
return newService;
}

void printServiceList() {
    Service *current = services_head;
    printf("\nLista Servisa:\n");
    printf("Program Number\tVideo PID\tAudio PID\n");
    printf("-------------------------------------\n");

    while (current != NULL) {
        printf("%d\t\t%d\t\t%d\n", 
               current->program_number, 
               current->video_pid, 
               current->audio_pid);
        current = current->next;
    }
}

void stringPrintChannel(int channel, int volume){

	char str[50];
    sprintf(str,"%d",channel); 
	char volumeStr[50];
    sprintf(volumeStr, "Jacina zvuka: %d", volume);

    int tempChannel=channel, counter=0;
	
	DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xf0));
	int i;
	int x = 5, y = screenHeight - 250, width = 200, height = 50;
    for (i = 0; i < 5; i++) {
        DFBCHECK(primary->FillRectangle(primary, x, y + (i*50), width - (i*40), height));
        if (i != 0) {
            DFBCHECK(primary->FillRectangle(primary, x, y - (i*50), width - (i*40), height));
        }
    }
	
    /* draw the text */

	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));

	// Postavljanje teksta
    int textX = 150; // X koordinata za centriranje teksta
    int textY = screenHeight - 150; // Y koordinata za centriranje teksta
    DFBCHECK(primary->DrawString(primary, str, -1, textX, textY, DSTF_CENTER));

	// Prikazivanje jačine zvuka
    int volumeTextX = screenWidth - 200; // Prilagodite ovo prema potrebama vašeg ekrana
    int volumeTextY = screenHeight / 2;  // Centriranje u odnosu na visinu ekrana

    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff)); // Boja teksta
    DFBCHECK(primary->DrawString(primary, volumeStr, -1, volumeTextX, volumeTextY, DSTF_RIGHT));

  /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));


   
	
    /* wait 5 seconds before terminating*/
	sleep(1);

   
  
	
    

    /* draw the text */
	
    while(tempChannel!=0){
        tempChannel/=10;
        counter++;
    }
    
    
}

int main()
{

	FILE *file = fopen("my_config.xml", "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    // Pronalaženje veličine datoteke
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Alokacija memorije za čitavu datoteku
    char *xmlData = malloc(fileSize + 1);
    if (!xmlData) {
        perror("Error allocating memory");
        fclose(file);
        return -1;
    }

    // Čitanje cele datoteke
    fread(xmlData, 1, fileSize, file);
    xmlData[fileSize] = '\0'; // Dodavanje null terminatora

    fclose(file);

    // Parsiranje XML-a
    Config config;
    parseConfig(xmlData, &config);

    // Oslobađanje memorije
    free(xmlData);
/*	
    if (argc < 2) {
       // fprintf(stderr, "Nedostaje putanja do konfiguracione datoteke\n");
        return -1;
    }
*/
	struct timespec lockStatusWaitTime;
	struct timeval now;
    int32_t result;
    
    uint32_t playerHandle = 0;
    uint32_t sourceHandle = 0;
    uint32_t filterHandle = 0;
    uint32_t audioStreamHandle = 0;
    uint32_t videoStreamHandle = 0;

	DFBCHECK(DirectFBInit(0,0));
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
    
    
	/* line drawing */
    /*
	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawLine(primary,
                                100,
                                screenHeight - 170,
                                screenWidth,
                              screenHeight - 170));
                              */
	
    
	/* draw text */

	IDirectFBFont *fontInterface = NULL;
	DFBFontDescription fontDesc;
	
    /* specify the height of the font by raising the appropriate flag and setting the height value */
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 50;
	
    //printf("Kanal je %d\t", channel);
    /* create the font and set the created font for primary surface text drawing */
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    

    //Config config = loadConfig(argv[1]);

    int frequency = config.frequency;
    int bandwidth = config.bandwidth;
    //unsigned int apid = config.apid;
    //unsigned int vpid = config.vpid;

	printf("\n Ovo je broj freqeuncy: %d", (int)frequency);

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
    if(!Tuner_Lock_To_Frequency(frequency, bandwidth, DVB_T))
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
    result = Player_Stream_Create(playerHandle, sourceHandle, config.vpid, VIDEO_TYPE_MPEG2, &videoStreamHandle);
	result = Player_Stream_Create(playerHandle, sourceHandle, config.apid, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
	
    /* Wait for a while */

    /* Wait for a while to receive several PAT sections */
    fflush(stdin);
    getchar();
    
    /* Deinitialization */

    myThreadRemote();
    /* Free demux filter */
	
      /*clean up*/
   	primary->Release(primary);
	dfbInterface->Release(dfbInterface);
   
    /**TO DO:**/
    /*Deinitialization*/
    Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

    
    Player_Stream_Create(playerHandle, sourceHandle, 201, VIDEO_TYPE_MPEG2, &videoStreamHandle);
	Player_Stream_Create(playerHandle, sourceHandle, 203, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);

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
	uint16_t video_pid = 0;
    uint16_t audio_pid = 0;

    uint16_t sectionLength = ((*(buffer+1) << 8) + *(buffer+2)) & 0x0FFF;
    uint16_t programInfoLength = ((*(buffer+10) << 8) + *(buffer+11)) & 0x0FFF;
    

    //printf("Section legnth and program info lenght: %d\t%d\t\n", sectionLength, programInfoLength);
    parsed_length = 12 + programInfoLength + 4 - 3;
    current_buffer_position = (uint8_t *)(buffer + 12 + programInfoLength);

    while(parsed_length < sectionLength & elementaryInfoCount < 20){

        uint16_t streamType = current_buffer_position[0];
        uint16_t elementaryPid = ((current_buffer_position[1] & 0x1F) << 8) | current_buffer_position[2];
        uint16_t esInfoLength = ((current_buffer_position[3] & 0x0F) << 8) | current_buffer_position[4];

		if(streamType == 0x02) { 
            video_pid = elementaryPid;
        } else if (streamType == 0x03) { // MPEG2 ili AAC audio strim
            audio_pid = elementaryPid;
        }

        current_buffer_position += 5 + esInfoLength;
        parsed_length += 5 + esInfoLength;
        elementaryInfoCount++;
		addOrUpdateService(program_number_1, video_pid, audio_pid);
        //printf("%d\t%d\t%d\n", streamType, elementaryPid, esInfoLength);

    }
}

/**TO DO:**/
/*Parse PAT Table*/
int32_t mySecFilterCallback(uint8_t *buffer)
{
    //printf("\n\nSection arrived!!!\n\n");



    if(!buffer){
        return -1;
    }
    uint32_t i;
    uint8_t table_id = buffer[0];
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];
    //printf("Section length: %d\n", section_length);


    if (table_id == 0x00){
        uint16_t program_number, pid;
    for(i = 0; i< (section_length-9) / 4; i++){
        program_number = (buffer[(8 + i * 4)]<<8) | buffer[9 + i*4];
		program_number_1 = (buffer[(8 + i * 4)]<<8) | buffer[9 + i*4];
        pid = ((buffer[(10 + i*4)] & 0x1F) << 8) | buffer[11 + i*4];

        if(program_number == 0){
          //  printf("Network PID: %u\n", pid);
        }
        else {
          //  printf("Program number: %u, PID: %u\n", program_number, pid);
        }
    }
    }
    else if (table_id == 0x02){
        parsePMT(buffer);
		// Nakon obrade PAT i PMT sekcija
	printServiceList();

    }
    else {
        return 0;
    }
	// Nakon obrade PAT i PMT sekcija
	//printServiceList();

   
    return 0;
}


void myThreadRemote(){
    const char* dev = "/dev/input/event0";
    char deviceName[20];
    struct input_event* eventBuf;
    uint32_t eventCnt;
    uint32_t i;
    int flag = 0;

    inputFileDesc = open(dev, O_RDWR);
    if(inputFileDesc == -1)
    {
        printf("Error while opening device (%s) !", strerror(errno));
	    return;
    }
    
    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
	printf("RC device opened succesfully [%s]\n", deviceName);
    
    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!eventBuf)
    {
        printf("Error allocating memory !");
        return;
    }
    //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
    int volumeLevel = 0;
    int program = 0;

    while(flag == 0)
    {
        /* read input eventS */
        if(getKeys(NUM_EVENTS, (uint8_t*)eventBuf, &eventCnt))
        {
			printf("Error while reading input events !");
			return;
		}

        for(i = 0; i < eventCnt; i++)
        {
            printf("Event time: %d sec, %d usec\n",(int)eventBuf[i].time.tv_sec,(int)eventBuf[i].time.tv_usec);
            printf("Event type: %hu\n",eventBuf[i].type);
            printf("Event code: %hu\n",eventBuf[i].code);
            printf("Event value: %d\n",eventBuf[i].value);
            printf("\n");

            if (eventBuf[i].type == 1 && (eventBuf[i].value == 1 || eventBuf[i].value ==2)) {
            switch(eventBuf[i].code) {
                case 61: {
                    if(program > 0) program--;
                    else program = 0;
                   // printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    

                    break;
                }
                case 62: {
                    program++;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 63: {
                    volumeLevel++;
                    printf("Volume level %d\n", volumeLevel);
					stringPrintChannel(0, volumeLevel);
                    break;
                }
                case 64: {
                    if(volumeLevel > 0) volumeLevel--;
                    else volumeLevel = 0;
                    printf("Volume level %d\n", volumeLevel);
					stringPrintChannel(0, volumeLevel);
                    break;
                }
                case 2: {
                    program = 1;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 3: {
                    program = 2;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 4: {
                    program = 3;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 5: {
                    program = 4;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 6: {
                    program = 5;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 7: {
                    program = 6;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 8: {
                    program = 7;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 9: {
                    program = 8;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program, 0);
                    break;
                }
                case 10: {
                    program = 9;
                    //printf("Upaljen program broj %d\n", program);
                   stringPrintChannel(program, 0);
                    break;
                }
                case 102: {
                    printf("exit \n");
                    flag = 1;
                }
            }
            } 
        }
    }  
    
    free(eventBuf);
    
    return;
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

void setupScreen(){
    
}
