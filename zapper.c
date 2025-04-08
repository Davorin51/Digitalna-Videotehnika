/******************************************************************************
 * zapper_demo.c
 *
 * Demonstracijski kod koji:
 * 1) Locka tuner na 818 MHz.
 * 2) Postavlja filter na PAT (PID=0x0000), parsira je (mySecFilterCallback).
 * 3) Kad se PAT parsira, dohvaća prvi program -> postavlja filter za taj PMT
 * 4) Parsira PMT, pokreće audio/video
 * 5) U drugom threadu prati daljinski (CH+/CH-), i pri promjeni kanala postavlja
 *    novi PMT filter, pa se iz PMT uzmu novi audio/video PID-ovi i puste.
 * 6) Ispisuje info o kanalu (index, pmt pid, audio i video pid).
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <linux/input.h>
#include <sys/time.h>
#include <time.h>

/* TDP API */
#include "tdp_api.h"

/* Koristimo Vaše funkcije za parse PMT i PAT */
static void parsePMT(uint8_t *buffer);
static int32_t mySecFilterCallback(uint8_t *buffer);

/* Pretpostavka da su definisani KEY_CHANNELUP i KEY_CHANNELDOWN */
#ifndef KEY_CHANNELUP
#define KEY_CHANNELUP   0x193
#endif
#ifndef KEY_CHANNELDOWN
#define KEY_CHANNELDOWN 0x194
#endif

#define MAX_PROGRAMS  20

typedef struct {
    uint16_t programNumber;
    uint16_t pmtPid;
} ProgramInfo_t;

/* Globalne varijable za popis programa iz PAT-a */
static ProgramInfo_t gProgramList[MAX_PROGRAMS];
static int gNumPrograms = 0;
static int gCurrentProgramIndex = 0;

/* Za Tuner lock uslovnu varijablu */
static pthread_cond_t  gTunerLockCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t gTunerLockMutex = PTHREAD_MUTEX_INITIALIZER;

/* Player i Demux */
static uint32_t gPlayerHandle  = 0;
static uint32_t gSourceHandle  = 0;
static uint32_t gPatFilterHandle = 0; 
static uint32_t gPmtFilterHandle = 0;

/* Audio i Video handle */
static uint32_t gAudioStreamHandle = 0;
static uint32_t gVideoStreamHandle = 0;

/* Daljinski FD */
static int gInputFd = -1;

/* Funkcije unaprijed: */
static int32_t tunerStatusCallback(t_LockStatus status);

/* Thread za daljinski */
static void* remoteControlThread(void* arg);

/* Pomocna za postavljanje PMT filtera za trenutno izabrani kanal */
static void switchToChannel(int channelIndex);

/* Frekvencija i parametri */
#define DESIRED_FREQUENCY 818000000
#define BANDWIDTH 8

/* ----------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(void)
{
    struct timespec lockStatusWaitTime;
    struct timeval now;
    int32_t result;

    /* 0) Otvaranje daljinskog /dev/input/event0 */
    gInputFd = open("/dev/input/event0", O_RDWR);
    if (gInputFd < 0) {
        printf("Greska pri otvaranju /dev/input/event0: %s\n", strerror(errno));
        return -1;
    }

    gettimeofday(&now,NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec+10;
       
    /* 1) Tuner init */
    if (Tuner_Init() != 0) {
        printf("Tuner_Init fail!\n");
        close(gInputFd);
        return -1;
    }
    
    /* 2) Registriraj tuner status callback */
    if (Tuner_Register_Status_Callback(tunerStatusCallback) != 0) {
        printf("Tuner_Register_Status_Callback fail!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }
    
    /* 3) Lock na 818 MHz */
    if (Tuner_Lock_To_Frequency(DESIRED_FREQUENCY, BANDWIDTH, DVB_T) != 0) {
        printf("Tuner_Lock_To_Frequency fail!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }
    
    /* Cekamo max 10 sek da se tuner locka */
    pthread_mutex_lock(&gTunerLockMutex);
    int rc = pthread_cond_timedwait(&gTunerLockCond, &gTunerLockMutex, &lockStatusWaitTime);
    pthread_mutex_unlock(&gTunerLockMutex);

    if (rc == ETIMEDOUT) {
        printf("Lock timeout!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }
    
    /* 4) Player init */
    result = Player_Init(&gPlayerHandle);
    if (result != NO_ERROR) {
        printf("Player_Init fail!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }

    /* Open source */
    result = Player_Source_Open(gPlayerHandle, &gSourceHandle);
    if (result != NO_ERROR) {
        printf("Player_Source_Open fail!\n");
        Player_Deinit(gPlayerHandle);
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }
    
    /* 5) Registriramo sekcijski filter callback i postavimo filter na PAT */
    result = Demux_Register_Section_Filter_Callback(mySecFilterCallback);
    if (result != NO_ERROR) {
        printf("Demux_Register_Section_Filter_Callback fail!\n");
        // cleanup...
        return -1;
    }

    /* PAT PID=0x0000, table_id=0x00 */
    result = Demux_Set_Filter(gPlayerHandle, 0x0000, 0x00, &gPatFilterHandle);
    if (result != NO_ERROR) {
        printf("Demux_Set_Filter (PAT) fail!\n");
        // cleanup...
        return -1;
    }

    /* 6) Pokrecemo thread za daljinski (CH+/CH-) */
    pthread_t rThread;
    pthread_create(&rThread, NULL, remoteControlThread, NULL);

    /* 7) Pricekamo da stigne PAT i da se parsira (u mySecFilterCallback). 
          Ovdje moze “cekaj Enter” ili sl. Za demo: */
    printf("Cekamo da se PAT parsira... Pritisnite ENTER kada je stigla.\n");
    getchar();

    if (gNumPrograms == 0) {
        printf("PAT nije parsirana ili nema programa!\n");
    } else {
        /* Prelazimo na prvi kanal */
        switchToChannel(0);
    }

    /* 8) Program radi dok ne pritisnete ENTER opet */
    printf("Pritisni ENTER za kraj...\n");
    getchar();

    /* 9) Gasimo thread */
    pthread_cancel(rThread);
    pthread_join(rThread, NULL);

    /* 10) De-inicijalizacija: 
       - Free PMT filter
       - Free PAT filter
       - Stop/Remove audio/video
       - Player_Source_Close, Player_Deinit
       - Tuner_Deinit
       - Close remote FD
    */
    if (gPmtFilterHandle != 0) {
        Demux_Free_Filter(gPlayerHandle, gPmtFilterHandle);
        gPmtFilterHandle = 0;
    }
    if (gPatFilterHandle != 0) {
        Demux_Free_Filter(gPlayerHandle, gPatFilterHandle);
        gPatFilterHandle = 0;
    }

    if (gAudioStreamHandle != 0) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gAudioStreamHandle);
        gAudioStreamHandle = 0;
    }
    if (gVideoStreamHandle != 0) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gVideoStreamHandle);
        gVideoStreamHandle = 0;
    }

    Player_Source_Close(gPlayerHandle, gSourceHandle);
    Player_Deinit(gPlayerHandle);
    Tuner_Deinit();
    close(gInputFd);

    printf("Zavrseno.\n");
    return 0;
}

/* ----------------------------------------------------------------------------
 * Callback za tuner lock
 * ---------------------------------------------------------------------------*/
static int32_t tunerStatusCallback(t_LockStatus status)
{
    if (status == STATUS_LOCKED) {
        pthread_mutex_lock(&gTunerLockMutex);
        pthread_cond_signal(&gTunerLockCond);
        pthread_mutex_unlock(&gTunerLockMutex);
        printf("Tuner LOCKED!\n");
    } else {
        printf("Tuner NOT LOCKED!\n");
    }
    return 0;
}

/* ----------------------------------------------------------------------------
 * mySecFilterCallback - dobivamo sekcije (PAT ili PMT) ovisno o PID i table_id
 * ---------------------------------------------------------------------------*/
static int32_t mySecFilterCallback(uint8_t *buffer)
{
    if (!buffer) return -1;

    uint8_t table_id = buffer[0];
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];

    printf("\n\n[mySecFilterCallback] Section arrived, table_id=0x%X, length=%d\n",
           table_id, section_length);

    if (table_id == 0x00) {
        /* PAT */
        uint32_t i;
        uint16_t program_number, pid;
        
        /* Ocitamo broj programa iz (section_length-9)/4 - to je tipicno u PAT */
        /* offset: 8 + i*4 dobijamo program_number i PID */
        gNumPrograms = 0;
        for (i = 0; i < (section_length - 9)/4; i++) {
            program_number = (buffer[8 + i*4] << 8) | buffer[9 + i*4];
            pid = ((buffer[10 + i*4] & 0x1F) << 8) | buffer[11 + i*4];

            if (program_number == 0) {
                // network PID
                printf("Network PID: %u\n", pid);
            } else {
                printf("Program number=%u, PMT PID=%u\n", program_number, pid);
                if (gNumPrograms < MAX_PROGRAMS) {
                    gProgramList[gNumPrograms].programNumber = program_number;
                    gProgramList[gNumPrograms].pmtPid = pid;
                    gNumPrograms++;
                }
            }
        }
    }
    else if (table_id == 0x02) {
        /* PMT */
        /* Pozivamo parsePMT iz Vašeg koda */
        parsePMT(buffer);
    }
    else {
        printf("Druga tablica (table_id=0x%X), ne parsiramo.\n", table_id);
    }

    return 0;
}

/* ----------------------------------------------------------------------------
 * parsePMT - Vaša funkcija
 * Ovde dopunjena da dohvati A/V PID i pusti stream
 * ---------------------------------------------------------------------------*/
void parsePMT(uint8_t *buffer)
{
    /* Identicna je definicija iz Vaseg snippet-a, 
       ali cemo dopuniti da prepoznamo i ispisemo audioPid/videoPid i 
       pokrenemo stream. */

    uint8_t  *current_buffer_position = NULL;
    uint32_t parsed_length = 0;
    uint8_t  elementaryInfoCount = 0;

    uint16_t sectionLength = (((*(buffer+1)) << 8) + *(buffer+2)) & 0x0FFF;
    uint16_t programInfoLength = ((*(buffer+10) << 8) + *(buffer+11)) & 0x0FFF;

    printf("PMT: sectionLength=%d, programInfoLength=%d\n", sectionLength, programInfoLength);

    parsed_length = 12 + programInfoLength + 4 - 3; 
    /* -3 ??? - to je Vaš stari offset. U praksi treba točno pratiti standard,
       ali ostavimo kako Vam je radilo. */

    current_buffer_position = (uint8_t *)(buffer + 12 + programInfoLength);

    uint16_t foundAudioPid = 0;
    uint16_t foundVideoPid = 0;

    while (parsed_length < sectionLength && elementaryInfoCount < 20) {

        uint16_t streamType = current_buffer_position[0];
        uint16_t elementaryPid = ((current_buffer_position[1] & 0x1F) << 8) 
                                  | current_buffer_position[2];
        uint16_t esInfoLength = ((current_buffer_position[3] & 0x0F) << 8) 
                                 | current_buffer_position[4];

        printf("streamType=0x%X, pid=%u, esInfoLength=%d\n",
               streamType, elementaryPid, esInfoLength);

        /* Za demo: prepoznajmo tip: 0x02 (MPEG2 video), 0x03/0x04 (MPEG audio), 0x1B (H.264) itd. */
        if ( (streamType == 0x02) && (foundVideoPid == 0) ) {
            foundVideoPid = elementaryPid;
        } else if ((streamType == 0x03 || streamType == 0x04) && (foundAudioPid == 0)) {
            foundAudioPid = elementaryPid;
        }
        /* Pomak */
        current_buffer_position += 5 + esInfoLength;
        parsed_length += 5 + esInfoLength;
        elementaryInfoCount++;
    }

    printf("U PMT pronadjeno: AudioPid=%u, VideoPid=%u\n", foundAudioPid, foundVideoPid);

    /* Zaustavimo stare streamove pa napravimo nove */
    if (gAudioStreamHandle) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gAudioStreamHandle);
        gAudioStreamHandle = 0;
    }
    if (gVideoStreamHandle) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gVideoStreamHandle);
        gVideoStreamHandle = 0;
    }

    /* Ako nadjemo video pid, kreiramo stream (tip -> VIDEO_TYPE_MPEG2 itd. 
       Ovisi kakav je streamType, za punu točnost treba prepoznati i H.264 i sl.)
    */
    if (foundVideoPid) {
        Player_Stream_Create(gPlayerHandle, 
                             gSourceHandle, 
                             foundVideoPid, 
                             VIDEO_TYPE_MPEG2,  /* ili VIDEO_TYPE_H264 ako je 0x1B */
                             &gVideoStreamHandle);
        if (gVideoStreamHandle)
            printf("Video stream start (pid=%u)\n", foundVideoPid);
    }

    /* Audio pid */
    if (foundAudioPid) {
        Player_Stream_Create(gPlayerHandle,
                             gSourceHandle,
                             foundAudioPid,
                             AUDIO_TYPE_MPEG_AUDIO,  /* ili AUDIO_TYPE_AAC, AC3, sl. */
                             &gAudioStreamHandle);
        if (gAudioStreamHandle)
            printf("Audio stream start (pid=%u)\n", foundAudioPid);
    }

    /* Ispišemo info o kanalu */
    printf("***** Promjena kanala: idx=%d, PMT pid=%u, A_pid=%u, V_pid=%u *****\n",
           gCurrentProgramIndex, 
           gProgramList[gCurrentProgramIndex].pmtPid,
           foundAudioPid,
           foundVideoPid);

    /* Ako želite “nakon 5s skloniti informacije”, možete jednostavno pauzirati 5s 
       ili to riješiti s nekom OSD bibliotekom. Ovdje samo console ispis. */
}

/* ----------------------------------------------------------------------------
 * remoteControlThread - čitanje CH+/CH- s daljinskog
 * ---------------------------------------------------------------------------*/
static void* remoteControlThread(void* arg)
{
    struct input_event events[8];

    while (1) {
        int n = read(gInputFd, events, sizeof(events));
        if (n > 0) {
            int count = n / sizeof(struct input_event);
            for (int i=0; i<count; i++) {
                if ((events[i].type == EV_KEY) && (events[i].value == 1)) {
                    if (events[i].code == KEY_CHANNELUP) {
                        printf("[Remote] CH+\n");
                        if (gNumPrograms > 0) {
                            gCurrentProgramIndex++;
                            if (gCurrentProgramIndex >= gNumPrograms) 
                                gCurrentProgramIndex = 0;
                            switchToChannel(gCurrentProgramIndex);
                        }
                    }
                    else if (events[i].code == KEY_CHANNELDOWN) {
                        printf("[Remote] CH-\n");
                        if (gNumPrograms > 0) {
                            gCurrentProgramIndex--;
                            if (gCurrentProgramIndex < 0)
                                gCurrentProgramIndex = gNumPrograms - 1;
                            switchToChannel(gCurrentProgramIndex);
                        }
                    }
                }
            }
        }
        usleep(50000); /* 50 ms */
    }

    return NULL;
}

/* ----------------------------------------------------------------------------
 * switchToChannel - postavlja filter na PMT kanala s indexom 'channelIndex'
 * ---------------------------------------------------------------------------*/
static void switchToChannel(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= gNumPrograms) return;

    /* Oslobodi stari PMT filter ako postoji */
    if (gPmtFilterHandle != 0) {
        Demux_Free_Filter(gPlayerHandle, gPmtFilterHandle);
        gPmtFilterHandle = 0;
    }

    /* Postavi PMT filter na pmtPid za dati kanal */
    uint16_t pmtPid = gProgramList[channelIndex].pmtPid;
    printf("switchToChannel: idx=%d, pmtPid=%u\n", channelIndex, pmtPid);

    /* table_id za PMT je 0x02 */
    int32_t res = Demux_Set_Filter(gPlayerHandle, pmtPid, 0x02, &gPmtFilterHandle);
    if (res != NO_ERROR) {
        printf("Demux_Set_Filter PMT fail (pid=%u)!\n", pmtPid);
    }
}
