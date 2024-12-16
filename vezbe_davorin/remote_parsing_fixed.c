#include "tdp_api.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

/* Konstante za tuner i bandwidth */
#define FREQUENCY 818000000
#define BANDWIDTH 8

/* Putanja do uređaja daljinskog upravljača i tipke za promjenu kanala */
#define REMOTE_DEVICE "/dev/input/event0"
#define KEY_CHANNEL_UP 62     // primjer koda za CH+
#define KEY_CHANNEL_DOWN 61   // primjer koda za CH-

/* Maksimalan broj programa */
#define MAX_PROGRAMS 8
#define NUM_EVENTS 5

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

typedef struct {
    uint16_t sectionLength;
    uint16_t transportStream;
    uint8_t versionNumber;
    uint16_t programNumber[MAX_PROGRAMS];
    uint16_t PID[MAX_PROGRAMS];
} PAT;

PAT tablePAT;
PMT tablePMT[MAX_PROGRAMS];

int channelCount = 0;
int channel = 1; // Trenutni kanal (1-based indexing)
int parseFlag = 1;
int currentIndexForPMT = 0;

/* Globalni handle-i */
uint32_t playerHandle = 0;
uint32_t sourceHandle = 0;
int videoStreamHandle = 0;
int audioStreamHandle = 0;

/* Za sinkronizaciju tunera */
pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

/* Za daljinski upravljač */
static int inputFileDesc;
static pthread_t remoteThread;
static int remoteFlag = 1;

/* Deklaracije funkcija */
int32_t mySecFilterCallback(uint8_t *buffer);
int32_t myTunerStatusCallback(t_LockStatus status);
void parsePAT(uint8_t *buffer);
void parsePMT(uint8_t *buffer);
void changeChannel(int channel);

/* Funkcija za parsiranje PAT tablice */
void parsePAT(uint8_t *buffer) {
    tablePAT.sectionLength = (uint16_t)(((*(buffer+1) << 8) + *(buffer + 2)) & 0x0FFF);
    tablePAT.transportStream = (uint16_t)(((*(buffer+3) << 8) + *(buffer +4)));
    tablePAT.versionNumber = (uint8_t)((*(buffer+5) >> 1) & 0x1F);

    channelCount = (tablePAT.sectionLength * 8 - 64) / 32;
    for (int i = 0; i < channelCount && i < MAX_PROGRAMS; i++){
        tablePAT.programNumber[i] = (uint16_t)((*(buffer + (i * 4) + 8) << 8) + (*(buffer + (i * 4) + 9)));
        tablePAT.PID[i] = (uint16_t)((( *(buffer + (i * 4) + 10) << 8) + (*(buffer + (i * 4) + 11))) & 0x1FFF);
        printf("Program %d -> PMT PID: %d\n", tablePAT.programNumber[i], tablePAT.PID[i]);
    }
    printf("Found %d programs.\n", channelCount);

    // Postavljanje demux filtera za svaki PMT PID
    for(int j = 0; j < channelCount && j < MAX_PROGRAMS; j++) {
        uint32_t filterHandle_PMT;
        if(Demux_Set_Filter(playerHandle, tablePAT.PID[j], 0x02, &filterHandle_PMT) != NO_ERROR) { // 0x02 je table_id za PMT
            printf("Demux_Set_Filter for PMT PID %d failed\n", tablePAT.PID[j]);
        }
    }

    // Inicijalno postavljanje na kanal 2 ako postoji
    if(channelCount >= 2){
        channel = 2;
        changeChannel(channel);
    } else if(channelCount > 0){
        channel = 1;
        changeChannel(channel);
    }
}

/* Funkcija za parsiranje PMT tablice */
void parsePMT(uint8_t *buffer) {
    if(currentIndexForPMT >= MAX_PROGRAMS){
        printf("Reached maximum number of PMT entries.\n");
        return;
    }

    parseFlag = 0;
    tablePMT[currentIndexForPMT].sectionLength = (uint16_t)(((*(buffer + 1) << 8) + *(buffer + 2)) & 0x0FFF);
    tablePMT[currentIndexForPMT].programNumber = (uint16_t)((*(buffer + 3) << 8) + *(buffer + 4));
    tablePMT[currentIndexForPMT].programInfoLength = (uint16_t)(((*(buffer + 10) << 8) + *(buffer + 11)) & 0x0FFF);
    tablePMT[currentIndexForPMT].streamCount = 0;
    tablePMT[currentIndexForPMT].hasTTX = 0;

    uint8_t *m_buffer = (uint8_t*)buffer + 12 + tablePMT[currentIndexForPMT].programInfoLength;

    while((m_buffer - buffer + 5) < tablePMT[currentIndexForPMT].sectionLength){
        uint8_t streamType = *m_buffer;
        uint16_t elementaryPID = (uint16_t)(((*(m_buffer + 1) << 8) + *(m_buffer + 2)) & 0x1FFF);
        uint16_t esInfoLength = (uint16_t)(((*(m_buffer + 3) << 8) + *(m_buffer + 4)) & 0x0FFF);
        uint8_t descriptor = (uint8_t)*(m_buffer + 5);

        if(streamType == 3 || streamType == 4){ // MPEG-1 i MPEG-2 audio
            tablePMT[currentIndexForPMT].audioPID = elementaryPID;
        } else if(streamType == 2){ // MPEG-2 video
            tablePMT[currentIndexForPMT].videoPID = elementaryPID;
        }

        if(streamType == 6 && descriptor == 86)
            tablePMT[currentIndexForPMT].hasTTX = 1;

        tablePMT[currentIndexForPMT].streamCount++;
        m_buffer += 5 + esInfoLength;
    }

    printf("PMT parsed: ProgramNumber=%d, VideoPID=%d, AudioPID=%d, hasTTX=%d\n",
           tablePMT[currentIndexForPMT].programNumber,
           tablePMT[currentIndexForPMT].videoPID,
           tablePMT[currentIndexForPMT].audioPID,
           tablePMT[currentIndexForPMT].hasTTX);

    currentIndexForPMT++;
}

/* Callback za sekcijski filter */
int32_t mySecFilterCallback(uint8_t *buffer) {
    if(!buffer) return -1;

    uint8_t tableId = *buffer; 
    if(tableId == 0x00){
        parsePAT(buffer);
    }
    else if(tableId == 0x02){
        parsePMT(buffer);
    }
    return 0;
}

/* Callback za tuner lock */
int32_t myTunerStatusCallback(t_LockStatus status) {
    if(status == STATUS_LOCKED) {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("Tuner Locked!\n");
    } else {
        printf("Tuner Not Locked!\n");
    }
    return 0;
}

/* Funkcija za promjenu kanala (postavljanje novih A/V PID-ova) */
void changeChannel(int channel){
    if(channel < 1 || channel > channelCount){
        printf("Invalid channel number: %d\n", channel);
        return;
    }

    int pmtIndex = channel - 1; // 1-based channel to 0-based index
    if(pmtIndex >= MAX_PROGRAMS){
        printf("Channel index out of range: %d\n", pmtIndex);
        return;
    }

    int videoPID = tablePMT[pmtIndex].videoPID;
    int audioPID = tablePMT[pmtIndex].audioPID;

    // Uklanjanje postojećih streamova
    if(videoStreamHandle){
        Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
        videoStreamHandle = 0;
    }
    if(audioStreamHandle) {
        Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);
        audioStreamHandle = 0;
    }

    // Kreiranje novih streamova
    if(videoPID){
        if(Player_Stream_Create(playerHandle, sourceHandle, videoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle) != NO_ERROR){
            printf("Failed to create video stream for PID %d\n", videoPID);
        }
        if(Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle) != NO_ERROR){
            printf("Failed to create audio stream for PID %d\n", audioPID);
        }
    } else {
        if(Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle) != NO_ERROR){
            printf("Failed to create audio stream for PID %d\n", audioPID);
        }
    }

    printf("Changed to channel %d (VideoPID=%d, AudioPID=%d)\n", channel, videoPID, audioPID);
}

/* Funkcija za čitanje događaja s daljinskog */
int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead) {
    int32_t ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0){
        return -1;
    }
    *eventsRead = ret / (int)sizeof(struct input_event);
    return 0;
}

/* Nit koja sluša daljinski upravljač i mijenja kanale */
void *remoteThreadTask(void *arg) {
    struct input_event* eventBuf;
    uint32_t eventCnt;
    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!eventBuf) {
        printf("Error allocating eventBuf!\n");
        return NULL;
    }

    while(remoteFlag) {
        if(getKeys(NUM_EVENTS, (uint8_t*)eventBuf, &eventCnt)){
            continue; 
        }

        for(uint32_t i = 0; i < eventCnt; i++) {
            if(eventBuf[i].type == EV_KEY && (eventBuf[i].value == 1 || eventBuf[i].value == 2)) {
                switch (eventBuf[i].code) {
                    case KEY_CHANNEL_UP: // CH+
                        if(channel >= channelCount){
                            channel = 1;
                        } else {
                            channel++;
                        }
                        changeChannel(channel);
                        break;
                    case KEY_CHANNEL_DOWN: // CH-
                        if(channel <= 1){
                            channel = channelCount;
                        } else {
                            channel--;
                        }
                        changeChannel(channel);
                        break;
                    // Dodavanje obrade dodatnih tipki za direktan odabir kanala (tipke 1-9)
                    case 2: // Tipka 1
                        if(1 <= channelCount){
                            channel = 1;
                            changeChannel(channel);
                        }
                        break;
                    case 3: // Tipka 2
                        if(2 <= channelCount){
                            channel = 2;
                            changeChannel(channel);
                        }
                        break;
                    case 4: // Tipka 3
                        if(3 <= channelCount){
                            channel = 3;
                            changeChannel(channel);
                        }
                        break;
                    case 5: // Tipka 4
                        if(4 <= channelCount){
                            channel = 4;
                            changeChannel(channel);
                        }
                        break;
                    case 6: // Tipka 5
                        if(5 <= channelCount){
                            channel = 5;
                            changeChannel(channel);
                        }
                        break;
                    case 7: // Tipka 6
                        if(6 <= channelCount){
                            channel = 6;
                            changeChannel(channel);
                        }
                        break;
                    case 8: // Tipka 7
                        if(7 <= channelCount){
                            channel = 7;
                            changeChannel(channel);
                        }
                        break;
                    case 9: // Tipka 8
                        if(8 <= channelCount){
                            channel = 8;
                            changeChannel(channel);
                        }
                        break;
                    case 10: // Tipka 9
                        if(9 <= channelCount){
                            channel = 9;
                            changeChannel(channel);
                        }
                        break;
                    case 102: // Tipka za izlaz
                        Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
                        Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);
                        remoteFlag = 0;
                        break;
                    default:
                        // Neobrađene tipke
                        break;
                }
            }
        }
    }

    free(eventBuf);
    return NULL;
}

int main() {
    struct timespec lockStatusWaitTime;
    struct timeval now;
    int32_t result;
    uint32_t filterHandle_PAT;

    // Inicijalizacija tunera
    if(Tuner_Init() != NO_ERROR) {
        printf("Tuner_Init fail\n");
        return -1;
    }

    // Registracija tuner callback-a
    Tuner_Register_Status_Callback(myTunerStatusCallback);

    // Zaključavanje na frekvenciju
    if(Tuner_Lock_To_Frequency(FREQUENCY, BANDWIDTH, DVB_T) != NO_ERROR) {
        printf("Tuner_Lock_To_Frequency fail\n");
        Tuner_Deinit();
        return -1;
    }

    // Čekanje na lock
    gettimeofday(&now, NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec + 10; // 10 sekundi timeout
    pthread_mutex_lock(&statusMutex);
    if(pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime) == ETIMEDOUT){
        printf("Lock timeout exceeded!\n");
        pthread_mutex_unlock(&statusMutex);
        Tuner_Deinit();
        return -1;
    }
    pthread_mutex_unlock(&statusMutex);

    // Player init
    if(Player_Init(&playerHandle) != NO_ERROR) {
        printf("Player_Init fail\n");
        Tuner_Deinit();
        return -1;
    }

    // Otvaranje izvora
    if(Player_Source_Open(playerHandle, &sourceHandle) != NO_ERROR) {
        printf("Player_Source_Open fail\n");
        Player_Deinit(playerHandle);
        Tuner_Deinit();
        return -1;
    }

    // Postavljanje PAT filtera (PID 0x0000, table_id 0x00)
    result = Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle_PAT);
    if(result != NO_ERROR) {
        printf("Demux_Set_Filter fail\n");
        Player_Source_Close(playerHandle, sourceHandle);
        Player_Deinit(playerHandle);
        Tuner_Deinit();
        return -1;
    }

    // Registriraj sekcijski filter callback
    if(Demux_Register_Section_Filter_Callback(mySecFilterCallback) != NO_ERROR) {
        printf("Demux_Register_Section_Filter_Callback fail\n");
        Demux_Free_Filter(playerHandle, filterHandle_PAT);
        Player_Source_Close(playerHandle, sourceHandle);
        Player_Deinit(playerHandle);
        Tuner_Deinit();
        return -1;
    }

    // Otvaranje uređaja daljinskog upravljača
    inputFileDesc = open(REMOTE_DEVICE, O_RDWR);
    if(inputFileDesc == -1) {
        printf("Error while opening remote device (%s)!\n", strerror(errno));
        // Ipak ćemo nastaviti, ali nećete moći mijenjati kanale preko daljinskog
    } else {
        // Kreiranje niti za daljinski
        if(pthread_create(&remoteThread, NULL, remoteThreadTask, NULL) != 0) {
            printf("Failed to create remote thread\n");
            // Alternativno rukovanje ako niti nije moguće kreirati
            // Možete dodati kod za korištenje komandne linije za promjenu kanala
        }
    }

    // Inicijalno postavljanje kanala je već obavljeno u parsePAT funkciji

    printf("Press Enter to exit...\n");
    getchar();

    // Gašenje niti za daljinski
    remoteFlag = 0;
    if(inputFileDesc >= 0) close(inputFileDesc);
    pthread_join(remoteThread, NULL);

    // Čišćenje resursa
    Demux_Free_Filter(playerHandle, filterHandle_PAT);
    Player_Source_Close(playerHandle, sourceHandle);
    Player_Deinit(playerHandle);
    Tuner_Deinit();

    return 0;
}
