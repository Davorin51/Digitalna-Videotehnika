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
int channel = 1; // Trenutni kanal
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
    // TODO: Provjeriti je li buffer NULL prije parsiranja


    tablePAT.sectionLength = (uint16_t)(((*(buffer+1)<<8)+*(buffer+2)) & 0x0FFF);
    tablePAT.transportStream = (uint16_t)(((*(buffer+3)<<8)+*(buffer+4)));
    tablePAT.versionNumber = (uint8_t)((*(buffer+5)>>1)& 0x1F);

    // TODO: Dodati provjeru za maksimalni broj programa


    channelCount = (tablePAT.sectionLength * 8 - 64) / 32;
    for (int i = 0; i < channelCount; i++) {
        tablePAT.programNumber[i] = (uint16_t)((*(buffer + (i * 4) + 8) << 8) + *(buffer + (i * 4) + 9));
        tablePAT.PID[i] = (uint16_t)((*(buffer + (i * 4) + 10) << 8) + *(buffer + (i * 4) + 11)) & 0x1FFF;

        // TODO: Ispisati pronađeni program i njegov PMT PID

    }

    // TODO: Ispisati ukupan broj pronađenih programa

}

/* Funkcija za parsiranje PMT tablice */
void parsePMT(uint8_t *buffer) {
    parseFlag = 0;
    tablePMT[currentIndexForPMT].sectionLength = (uint16_t)(((*(buffer+1)<<8)+*(buffer+2)) & 0x0FFF);
    tablePMT[currentIndexForPMT].programNumber = (uint16_t)((*(buffer+3)<<8)+*(buffer+4));

    // TODO: Dodati provjeru za duljinu sekcije


    tablePMT[currentIndexForPMT].programInfoLength = (uint16_t)(((*(buffer+10)<<8)+*(buffer+11))&0x0FFF);
    uint8_t *m_buffer = (uint8_t*)buffer + 12 + tablePMT[currentIndexForPMT].programInfoLength;

    while ((m_buffer - buffer + 5) < tablePMT[currentIndexForPMT].sectionLength) {
        uint8_t streamType = *m_buffer;
        uint16_t elementaryPID = (uint16_t)(((*(m_buffer+1)<<8) + *(m_buffer+2)) & 0x1FFF);

        // TODO: Ispisati streamType i elementaryPID za svaki stream
        

        m_buffer += 5;
    }

    // TODO: Ispisati parsirane informacije o PMT

}

/* Funkcija za promjenu kanala */
void changeChannel(int channel) {
    int videoPID = tablePMT[channel].videoPID;
    int audioPID = tablePMT[channel].audioPID;

    // TODO: Ukloniti postojeće streamove ako postoje


    // TODO: Kreirati nove streamove s novim PID-ovima

}

/* Nit za obradu daljinskog upravljača */
void *remoteThreadTask(void *arg) {
    struct input_event *eventBuf;
    uint32_t eventCnt;
    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));

    while (remoteFlag) {
        // TODO: Pročitati događaje s daljinskog upravljača
       
    }

    free(eventBuf);
    return NULL;
}


/* Main funkcija */
int main() {
    inputFileDesc = open(REMOTE_DEVICE, O_RDWR);
    if (inputFileDesc == -1) {
        printf("Error while opening remote device!\n");
        return -1;
    }

    pthread_create(&remoteThread, NULL, remoteThreadTask, NULL);

    printf("Press Enter to exit...\n");
    getchar();

    remoteFlag = 0;
    pthread_join(remoteThread, NULL);
    close(inputFileDesc);

    return 0;
}
