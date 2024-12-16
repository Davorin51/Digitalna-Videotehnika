#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include "tdp_api.h"
#include <math.h>
#include <time.h>
#include <sys/time.h>

/* Definicije konstant */
#define CONFIG_FILE "config.xml"
#define NUM_EVENTS  5

/* Error codes */
#define NO_ERROR 		0
#define ERROR			1

/* Helper macro for error checking */
#define ASSERT_TDP_RESULT(x,y)  if(NO_ERROR == x) \
                                                printf("%s uspješno\n", y); \
                                            else{ \
                                                printf("%s neuspješno\n", y); \
                                                return -1; \
                                            }

/* Strukture za PAT i PMT tablice */
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

typedef struct{
    uint16_t sectionLength;
    uint16_t transportStream;
    uint8_t versionNumber;
    uint16_t programNumber[8];
    uint16_t PID[8];
} PAT;

/* Globalne varijable */
PMT tablePMT[8];
PAT tablePAT;

static int32_t inputFileDesc;
int parseFlag = 1;
int channelCount;
int channel = 1;
pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;

/* Strukture za inicijalizaciju tunera i streamova */
typedef struct {
    int bandwidth;
    int frequency;
    char module[50];
} tunerData;

typedef struct {
    int audioPID;
    int videoPID;
    char audioType[50];
    char videoType[50];
} initService;

/* Funkcije */
void parse(char *filename, tunerData *tuner, initService *init);
int32_t myPrivateTunerStatusCallback(t_LockStatus status);
int32_t mySecFilterCallback(uint8_t *buffer);
void changeChannel(int channel);
int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead);
void *remoteThreadTask();
void parsePAT(uint8_t *buffer);
void parsePMT(uint8_t *buffer);

/* Handle za igrača i tunera */
int32_t result;
uint32_t playerHandle = 0;
uint32_t sourceHandle = 0;
uint32_t filterHandle = 0;
int videoStreamHandle = 0, audioStreamHandle = 0;

/* Funkcija za parsiranje konfiguracijske datoteke */
void parse(char *filename, tunerData *tuner, initService *init){
    char line[255];
    char *token;
    FILE *fptr;
    fptr = fopen(filename,"r");
    if(fptr){
        // Ovisno o formatu config.xml, prilagodite čitanje
        // Ovo je primjer parsiranja XML-a jednostavnim linijskim čitanjem
        while(fgets(line, sizeof(line), fptr)){
            if(strstr(line, "<frequency>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                tuner->frequency = atoi(token);
            }
            else if(strstr(line, "<bandwidth>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                tuner->bandwidth = atoi(token);
            }
            else if(strstr(line, "<module>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                strncpy(tuner->module, token, sizeof(tuner->module));
                tuner->module[sizeof(tuner->module)-1] = '\0'; // Osiguravanje NULL terminacije
            }
            else if(strstr(line, "<audioPID>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                init->audioPID = atoi(token);
            }
            else if(strstr(line, "<videoPID>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                init->videoPID = atoi(token);
            }
            else if(strstr(line, "<audioType>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                strncpy(init->audioType, token, sizeof(init->audioType));
                init->audioType[sizeof(init->audioType)-1] = '\0';
            }
            else if(strstr(line, "<videoType>")){
                token = strtok(line, ">");
                token = strtok(NULL, "<");
                strncpy(init->videoType, token, sizeof(init->videoType));
                init->videoType[sizeof(init->videoType)-1] = '\0';
            }
        }
    }
    fclose(fptr);
}

/* Callback za status tunera */
int32_t myPrivateTunerStatusCallback(t_LockStatus status)
{
    if(status == STATUS_LOCKED)
    {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("\n\n\tTuner je zaključan na frekvenciju.\n\n");
    }
    else
    {
        printf("\n\n\tTuner nije zaključan.\n\n");
    }
    return 0;
}

/* Callback za sekcijske filtere */
int32_t mySecFilterCallback(uint8_t *buffer){
    uint8_t tableId = *buffer; 
    if(tableId == 0x00){
        parsePAT(buffer);
    }
    else if(tableId == 0x02){
        parsePMT(buffer);
    }
    return 0;
}

/* Funkcija za parsiranje PAT tablice */
void parsePAT(uint8_t *buffer){
    if(buffer == NULL){
        printf("Buffer je NULL u parsePAT.\n");
        return;
    }

    tablePAT.sectionLength = (uint16_t)(((*(buffer+1) << 8) + *(buffer + 2)) & 0x0FFF);
    tablePAT.transportStream = (uint16_t)(((*(buffer+3) << 8) + *(buffer + 4)));
    tablePAT.versionNumber = (uint8_t)((*(buffer+5) >> 1) & 0x1F);
    
    // Izračunavanje broja kanala na osnovu sectionLength
    // Ovo je pojednostavljeno i može zahtijevati prilagodbu
    channelCount = (tablePAT.sectionLength - 9) / 4; // Svaki kanal zauzima 4 bajta
    if(channelCount > 8) channelCount = 8; // Ograničenje na 8 kanala
    
    int i = 0;
    
    for(; i < channelCount; i++){
        tablePAT.programNumber[i] = (uint16_t)(*(buffer + (i * 4) + 8) << 8) + (*(buffer + (i * 4) + 9));
        tablePAT.PID[i] = (uint16_t)((*(buffer + (i * 4) + 10) << 8) + *(buffer + (i * 4) + 11)) & 0x1FFF;
        printf("Kanal %d\tPID: %d\n", tablePAT.programNumber[i], tablePAT.PID[i]);
    }
    printf("\n\nPAT Parsiran: Section Length: %d, TS ID: %d, Verzija: %d, Broj Kanala: %d\n", 
            tablePAT.sectionLength, tablePAT.transportStream, tablePAT.versionNumber, channelCount);
}

/* Funkcija za parsiranje PMT tablice */
void parsePMT(uint8_t *buffer){
    if(buffer == NULL){
        printf("Buffer je NULL u parsePMT.\n");
        return;
    }

    parseFlag = 0;

    // Extract program_number from PMT
    uint16_t program_number = (buffer[3] << 8) | buffer[4];

    // Find the channel index that matches the program_number
    int currentChannel = -1;
    for(int i = 0; i < channelCount; i++){
        if(tablePAT.programNumber[i] == program_number){
            currentChannel = i;
            break;
        }
    }

    if(currentChannel == -1){
        printf("PMT program_number %d ne odgovara nijednom PAT program_number.\n", program_number);
        return;
    }

    tablePMT[currentChannel].sectionLength = (uint16_t)(((*(buffer+1) << 8) + *(buffer + 2)) & 0x0FFF);
    tablePMT[currentChannel].programNumber = program_number;
    tablePMT[currentChannel].programInfoLength = (uint16_t)(((*(buffer+10) << 8) + *(buffer + 11)) & 0x0FFF);
    tablePMT[currentChannel].streamCount = 0;
    tablePMT[currentChannel].hasTTX = 0;
    int j;
    
    printf("\n\nPMT Parsiran za Kanal %d: Section Length: %d, Program Number: %d, Program Info Length: %d\n", 
            currentChannel+1, tablePMT[currentChannel].sectionLength, tablePMT[currentChannel].programNumber, tablePMT[currentChannel].programInfoLength);
    
    uint8_t *m_buffer = buffer + 12 + tablePMT[currentChannel].programInfoLength;
    
    for (j = 0; ((m_buffer - buffer) + 5 < tablePMT[currentChannel].sectionLength); j++)
    {
        if(j >= 15){
            printf("Previše streamova za Kanal %d. Odbacujem dodatne.\n", currentChannel+1);
            break;
        }

        tablePMT[currentChannel].streams[j].streamType = *(m_buffer);
        tablePMT[currentChannel].streams[j].elementaryPID = ((*(m_buffer+1) << 8) + *(m_buffer+2)) & 0x1FFF;
        tablePMT[currentChannel].streams[j].esInfoLength = ((*(m_buffer+3) << 8) + *(m_buffer+4)) & 0x0FFF;
        tablePMT[currentChannel].streams[j].descriptor = *(m_buffer+5);
        
        // Identifikacija audio i video streamova
        if(tablePMT[currentChannel].streams[j].streamType == 3){
            tablePMT[currentChannel].audioPID = tablePMT[currentChannel].streams[j].elementaryPID;
        }
        else if(tablePMT[currentChannel].streams[j].streamType == 2){
            tablePMT[currentChannel].videoPID = tablePMT[currentChannel].streams[j].elementaryPID;
        }
        
        // Provjera za TTX
        if(tablePMT[currentChannel].streams[j].streamType == 6 && 
           tablePMT[currentChannel].streams[j].descriptor == 86)
            tablePMT[currentChannel].hasTTX = 1;
        
        printf("Stream Tip: %d, EPID: %d, Duljina: %d, Descriptor: %d\n", 
                tablePMT[currentChannel].streams[j].streamType, 
                tablePMT[currentChannel].streams[j].elementaryPID, 
                tablePMT[currentChannel].streams[j].esInfoLength, 
                tablePMT[currentChannel].streams[j].descriptor);
        m_buffer += 5 + tablePMT[currentChannel].streams[j].esInfoLength;
        tablePMT[currentChannel].streamCount++;
    }
    printf("Ukupan broj Streamova: %d, Ima TTX: %d\n", tablePMT[currentChannel].streamCount, tablePMT[currentChannel].hasTTX);
}

/* Funkcija za promjenu kanala */
void changeChannel(int channel){
    if(channel < 1 || channel > channelCount){
        printf("Nevažeći broj kanala: %d\n", channel);
        return;
    }

    int videoPID, audioPID;

    audioPID = tablePMT[channel-1].audioPID;
    videoPID = tablePMT[channel-1].videoPID;

    if(videoStreamHandle){
        Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
        videoStreamHandle = 0;
    }
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

    if(videoPID){
        result = Player_Stream_Create(playerHandle, sourceHandle, videoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle);
        if(result != NO_ERROR){
            printf("Neuspjelo kreiranje video streama za Kanal %d.\n", channel);
        }
        result = Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
        if(result != NO_ERROR){
            printf("Neuspjelo kreiranje audio streama za Kanal %d.\n", channel);
        }
    }
    else{
        videoStreamHandle = 0;
        result = Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
        if(result != NO_ERROR){
            printf("Neuspjelo kreiranje audio streama za Kanal %d.\n", channel);
        }
    }
    printf("Prebaceno na Kanal %d\n", channel);
}

/* Funkcija za čitanje tipki sa daljinskog upravljača */
int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead)
{
    int32_t ret = 0;
    
    /* Čitanje input događaja i pohranjivanje u buffer */
    ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0)
    {
        printf("Greška kod čitanja input događaja: %d\n", ret);
        return ERROR;
    }
    /* Izračunavanje broja pročitanih događaja */
    *eventsRead = ret / (int)sizeof(struct input_event);
    
    return NO_ERROR;
}

/* Niti za upravljanje daljinskim upravljačem */
void *remoteThreadTask()
{
    const char* dev = "/dev/input/event0"; // Provjerite točan uređaj
    char deviceName[256];
    struct input_event* eventBuf;
    uint32_t eventCnt;
    uint32_t i;
    
    inputFileDesc = open(dev, O_RDONLY);
    if(inputFileDesc == -1)
    {
        printf("Greška prilikom otvaranja uređaja (%s)!\n", strerror(errno));
        pthread_exit(NULL);
    }
    
    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
    printf("Daljinski upravljač otvoren uspješno [%s]\n", deviceName);
    
    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!eventBuf)
    {
        printf("Greška prilikom alociranja memorije!\n");
        close(inputFileDesc);
        pthread_exit(NULL);
    }
    
    while(1)
    {
        /* Čitanje input događaja */
        if(getKeys(NUM_EVENTS, (uint8_t*)eventBuf, &eventCnt))
        {
            printf("Greška prilikom čitanja input događaja!\n");
            break;
        }
        
        for(i = 0; i < eventCnt; i++)
        {
            if(eventBuf[i].type == EV_KEY && 
               (eventBuf[i].value == 1 || eventBuf[i].value == 2)){
                
                switch (eventBuf[i].code){
                    case KEY_INFO: { // Zamijenite s odgovarajućim kodom tipke
                        printf("INFO tipka pritisnuta\n");
                        // Implementirajte željenu funkcionalnost
                        break;
                    }
                    case KEY_MUTE: { // Zamijenite s odgovarajućim kodom tipke
                        printf("MUTE tipka pritisnuta\n");
                        // Implementirajte željenu funkcionalnost (npr., mute zvuka)
                        break;
                    }
                    case KEY_VOLUMEUP: { // Zamijenite s odgovarajućim kodom tipke
                        printf("Volume Up tipka pritisnuta\n");
                        // Implementirajte povećanje zvuka
                        // Na primjer:
                        // increaseVolume();
                        break;
                    }
                    case KEY_VOLUMEDOWN: { // Zamijenite s odgovarajućim kodom tipke
                        printf("Volume Down tipka pritisnuta\n");
                        // Implementirajte smanjenje zvuka
                        // Na primjer:
                        // decreaseVolume();
                        break;
                    }
                    case KEY_CHANNELUP: {
                        if(channel >= channelCount){
                            channel = 1;
                        } else{
                            channel++;
                        }
                        changeChannel(channel);
                        break;
                    }
                    case KEY_CHANNELDOWN: {
                        if(channel == 1){
                            channel = channelCount;
                        }
                        else{
                            channel--;
                        }
                        changeChannel(channel);
                        break;
                    }
                    case KEY_1: case KEY_2: case KEY_3:
                    case KEY_4: case KEY_5: case KEY_6:
                    case KEY_7: case KEY_8: case KEY_9:
                    case KEY_0: {
                        int pressedKey = eventBuf[i].code - KEY_1 + 1;
                        if(pressedKey == 0) pressedKey = 10; // Ako je 0 za kanal 10
                        if(pressedKey >=1 && pressedKey <= channelCount){
                            channel = pressedKey;
                            changeChannel(channel);
                        }
                        break;
                    }
                    case KEY_EXIT: { // Zamijenite s odgovarajućim kodom tipke za izlaz
                        printf("EXIT tipka pritisnuta\n");
                        // Zaustavite aplikaciju
                        goto cleanup;
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
    
cleanup:
    free(eventBuf);
    close(inputFileDesc);
    pthread_exit(NULL);
}

/* Glavna funkcija */
int32_t main(int32_t argc, char** argv)
{
    tunerData data;
    initService init;
    pthread_t remote;
    
    /* Parsiranje konfiguracijske datoteke */
    parse(CONFIG_FILE, &data, &init);
    printf("Frekvencija: %d\n", data.frequency);
    printf("Širina pojasa: %d\n", data.bandwidth);
    printf("Modul: %s\n", data.module);

    printf("Audio PID: %d\n", init.audioPID);
    printf("Video PID: %d\n", init.videoPID);
    printf("Audio Tip: %s\n", init.audioType);
    printf("Video Tip: %s\n", init.videoType);
    
    /* Inicijalizacija tunera */
    result = Tuner_Init();
    ASSERT_TDP_RESULT(result, "Tuner_Init");
    
    /* Registracija callback funkcije za status tunera */
    result = Tuner_Register_Status_Callback(myPrivateTunerStatusCallback);
    ASSERT_TDP_RESULT(result, "Tuner_Register_Status_Callback");
    
    /* Zaključavanje na frekvenciju */
    result = Tuner_Lock_To_Frequency(data.frequency, data.bandwidth, DVB_T);
    ASSERT_TDP_RESULT(result, "Tuner_Lock_To_Frequency");
    
    /* Čekanje na zaključavanje tunera */
    pthread_mutex_lock(&statusMutex);
    pthread_cond_wait(&statusCondition, &statusMutex);
    pthread_mutex_unlock(&statusMutex);
    
    /* Inicijalizacija playera */
    result = Player_Init(&playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Init");
    
    /* Otvaranje izvora */
    result = Player_Source_Open(playerHandle, &sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Open");
    
    /* Postavljanje sekcijskog filtera za PAT */
    result = Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Set_Filter (PAT)");
    
    /* Registracija callbacka za sekcijski filter */
    result = Demux_Register_Section_Filter_Callback(mySecFilterCallback);
    ASSERT_TDP_RESULT(result, "Demux_Register_Section_Filter_Callback");
    
    /* Kreiranje streamova za početni kanal */
    result = Player_Stream_Create(playerHandle, sourceHandle, init.videoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle);
    ASSERT_TDP_RESULT(result, "Player_Stream_Create (Video)");
    
    result = Player_Stream_Create(playerHandle, sourceHandle, init.audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    ASSERT_TDP_RESULT(result, "Player_Stream_Create (Audio)");
    
    /* Postavljanje početne glasnoće */
    result = Player_Volume_Set(playerHandle, 20 * 10000000);
    ASSERT_TDP_RESULT(result, "Player_Volume_Set");
    sleep(1);
    
    /* Oslobađanje filtera za PAT */
    result = Demux_Free_Filter(playerHandle, filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Free_Filter (PAT)");
    
    /* Postavljanje filtera za PMT za svaki kanal */
    for(int i = 1; i <= channelCount; i++){
        parseFlag = 1;
        result = Demux_Set_Filter(playerHandle, tablePAT.PID[i-1], 0x02, &filterHandle);
        ASSERT_TDP_RESULT(result, "Demux_Set_Filter (PMT)");
        // Čekanje dok se PMT ne parsira
        while(parseFlag){
            usleep(100000); // 100ms
        }
        result = Demux_Free_Filter(playerHandle, filterHandle);
        ASSERT_TDP_RESULT(result, "Demux_Free_Filter (PMT)");
    }
    
    /* Kreiranje niti za upravljanje daljinskim upravljačem */
    if(pthread_create(&remote, NULL, &remoteThreadTask, NULL)){
        printf("Greška prilikom kreiranja niti za daljinski upravljač!\n");
        // Nastaviti ili zaustaviti aplikaciju
    }
    
    /* Glavna nit može obavljati druge zadatke ili čekati da se nit za daljinski upravljač završi */
    pthread_join(remote, NULL);
    
    /* De-inicijalizacija */
    result = Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
    ASSERT_TDP_RESULT(result, "Player_Stream_Remove (Video)");
    
    result = Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);
    ASSERT_TDP_RESULT(result, "Player_Stream_Remove (Audio)");
    
    /* Zatvaranje izvora */
    result = Player_Source_Close(playerHandle, sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Close");
    
    /* De-inicijalizacija playera */
    result = Player_Deinit(playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Deinit");
    
    /* De-inicijalizacija tunera */
    result = Tuner_Deinit();
    ASSERT_TDP_RESULT(result, "Tuner_Deinit");
    
    return 0;
}
