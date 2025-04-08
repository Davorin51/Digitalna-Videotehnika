/******************************************************************************
 * zapper_full.c
 *
 * Demonstracijski kod "Zapper" koji:
 *  1) Zaključava tuner na 818 MHz,
 *  2) Postavlja PAT filter (PID=0x0000), parsira PAT,
 *  3) Kod odabranog kanala postavlja PMT filter, parsira PMT,
 *  4) Kreira i pokreće Audio/Video stream,
 *  5) U odvojenom threadu sluša CH+/CH- na daljinskom i mijenja kanale,
 *  6) Prikazuje OSD info o kanalu (DirectFB).
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

/* DirectFB */
#include <directfb.h>

/* TDP API (prilagodite include putanju) */
#include "tdp_api.h"

/* ----------------------------------------------------------------------------
 * Definicije tipki daljinskog (ako nisu već definirane)
 * Provjerite realne kodove tipki na vašem uređaju!
 * ---------------------------------------------------------------------------*/
#ifndef KEY_CHANNELUP
#define KEY_CHANNELUP   0x193
#endif
#ifndef KEY_CHANNELDOWN
#define KEY_CHANNELDOWN 0x194
#endif

/* Maksimalan broj programa koje ćemo spremiti iz PAT-a */
#define MAX_PROGRAMS  20

/* Struktura za spremanje (program_number, pmtPid) iz PAT-a */
typedef struct {
    uint16_t programNumber;
    uint16_t pmtPid;
} ProgramInfo_t;

/* Globalni popis programa */
static ProgramInfo_t gProgramList[MAX_PROGRAMS];
static int           gNumPrograms          = 0;  /* Koliko je programa pronađeno u PAT-u */
static int           gCurrentProgramIndex  = 0;  /* Koji je program trenutačno aktivan */

/* Za Tuner lock uvjetnu varijablu */
static pthread_cond_t  gTunerLockCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t gTunerLockMutex = PTHREAD_MUTEX_INITIALIZER;

/* TDP: Player i Demux handle-ovi */
static uint32_t gPlayerHandle     = 0;
static uint32_t gSourceHandle     = 0;
static uint32_t gPatFilterHandle  = 0;
static uint32_t gPmtFilterHandle  = 0;

/* Audio i Video stream handle-ovi */
static uint32_t gAudioStreamHandle = 0;
static uint32_t gVideoStreamHandle = 0;

/* File descriptor za daljinski (/dev/input/event0) */
static int gInputFd = -1;

/* DirectFB globalni pokazivači */
static IDirectFB         *gDfbInterface   = NULL;
static IDirectFBSurface  *gPrimarySurface = NULL;
static IDirectFBFont     *gFontInterface  = NULL;
static int                gScreenWidth    = 0;
static int                gScreenHeight   = 0;

/* Frekvencija i parametri DVB-T */
#define DESIRED_FREQUENCY 818000000
#define BANDWIDTH         8

/* ----------------------------------------------------------------------------
 * Deklaracije funkcija (tuner callback, demux callback, parsePAT, parsePMT)
 * ---------------------------------------------------------------------------*/
static int32_t  tunerStatusCallback(t_LockStatus status);
static int32_t  mySecFilterCallback(uint8_t *buffer);
static void     parsePAT(uint8_t *buffer);
static void     parsePMT(uint8_t *buffer);

/* Thread za daljinski */
static void*    remoteControlThread(void *arg);

/* Funkcija za prelazak na drugi kanal (postavljanje PMT filtera) */
static void     switchToChannel(int channelIndex);

/* Inicijalizacija/gašenje DirectFB */
static int      initDirectFB(void);
static void     deinitDirectFB(void);

/* Funkcija za kratki OSD prikaz info (kanal, PIDs) na 3-5 sekundi */
static void     showChannelInfoOSD(int channelIndex,
                                   uint16_t pmtPid,
                                   uint16_t audioPid,
                                   uint16_t videoPid);

/* ----------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    struct timespec lockStatusWaitTime;
    struct timeval  now;
    int32_t         result;

    /* 1) Otvaranje daljinskog /dev/input/event0 */
    gInputFd = open("/dev/input/event0", O_RDWR);
    if (gInputFd < 0) {
        fprintf(stderr, "Greska pri otvaranju /dev/input/event0: %s\n", strerror(errno));
        return -1;
    }

    /* 2) Inicijalizacija tunera */
    if (Tuner_Init() != 0) {
        fprintf(stderr, "Tuner_Init fail!\n");
        close(gInputFd);
        return -1;
    }

    /* 3) Registriramo callback za lock status */
    if (Tuner_Register_Status_Callback(tunerStatusCallback) != 0) {
        fprintf(stderr, "Tuner_Register_Status_Callback fail!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }

    /* 4) Lock na željenu frekvenciju (818 MHz, DVB-T, bw=8) */
    if (Tuner_Lock_To_Frequency(DESIRED_FREQUENCY, BANDWIDTH, DVB_T) != 0) {
        fprintf(stderr, "Tuner_Lock_To_Frequency fail!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }

    /* 5) Čekamo do 10 sekundi da tuner javi STATUS_LOCKED */
    gettimeofday(&now, NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec + 10;

    pthread_mutex_lock(&gTunerLockMutex);
    int rc = pthread_cond_timedwait(&gTunerLockCond, &gTunerLockMutex, &lockStatusWaitTime);
    pthread_mutex_unlock(&gTunerLockMutex);

    if (rc == ETIMEDOUT) {
        fprintf(stderr, "Timeout čekanja na lock!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }

    /* 6) Inicijalizacija Player-a */
    result = Player_Init(&gPlayerHandle);
    if (result != NO_ERROR) {
        fprintf(stderr, "Player_Init fail!\n");
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }

    /* 7) Otvori izvor (source) */
    result = Player_Source_Open(gPlayerHandle, &gSourceHandle);
    if (result != NO_ERROR) {
        fprintf(stderr, "Player_Source_Open fail!\n");
        Player_Deinit(gPlayerHandle);
        Tuner_Deinit();
        close(gInputFd);
        return -1;
    }

    /* 8) Postavimo callback za sekcijske filtere i postavimo filter za PAT (PID=0x0000, table_id=0x00) */
    result = Demux_Register_Section_Filter_Callback(mySecFilterCallback);
    if (result != NO_ERROR) {
        fprintf(stderr, "Demux_Register_Section_Filter_Callback fail!\n");
        // cleanup...
        return -1;
    }

    result = Demux_Set_Filter(gPlayerHandle, 0x0000, 0x00, &gPatFilterHandle);
    if (result != NO_ERROR) {
        fprintf(stderr, "Demux_Set_Filter(PAT) fail!\n");
        // cleanup...
        return -1;
    }

    /* 9) Pokrenemo thread za daljinski (CH+/CH-) */
    pthread_t rThread;
    pthread_create(&rThread, NULL, remoteControlThread, NULL);

    /* 10) Inicijaliziramo DirectFB (za OSD), ako želimo prikaz na ekranu */
    if (initDirectFB() != 0) {
        fprintf(stderr, "initDirectFB fail - nastavljamo možda bez OSD-a.\n");
    }

    /* 11) Sačekamo da se PAT parsira (u callbacku). Za demo – “pritisnite ENTER”. */
    printf("Cekamo da se PAT parsira... Pritisnite ENTER kada je stigla.\n");
    getchar();

    if (gNumPrograms == 0) {
        printf("Nema programa u PAT-u ili PAT nije parsirano.\n");
    } else {
        /* Prelazimo na prvi kanal (index=0) */
        switchToChannel(0);
    }

    /* 12) Program “radi” dok ne pritisnete ENTER drugi put */
    printf("Pritisni ENTER za kraj...\n");
    getchar();

    /* 13) Gasimo thread za daljinski */
    pthread_cancel(rThread);
    pthread_join(rThread, NULL);

    /* 14) Deinit redom: 
          - PMT filter
          - PAT filter
          - Audio/Video stream remove
          - Player_Source_Close, Player_Deinit
          - Tuner_Deinit
          - DirectFB Deinit
          - close remote FD
    */
    if (gPmtFilterHandle) {
        Demux_Free_Filter(gPlayerHandle, gPmtFilterHandle);
        gPmtFilterHandle = 0;
    }
    if (gPatFilterHandle) {
        Demux_Free_Filter(gPlayerHandle, gPatFilterHandle);
        gPatFilterHandle = 0;
    }

    if (gAudioStreamHandle) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gAudioStreamHandle);
        gAudioStreamHandle = 0;
    }
    if (gVideoStreamHandle) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gVideoStreamHandle);
        gVideoStreamHandle = 0;
    }

    Player_Source_Close(gPlayerHandle, gSourceHandle);
    Player_Deinit(gPlayerHandle);
    Tuner_Deinit();
    deinitDirectFB();
    close(gInputFd);

    printf("Zavrseno.\n");
    return 0;
}

/* ----------------------------------------------------------------------------
 * tunerStatusCallback - kad Tuner javi STATUS_LOCKED/NOT_LOCKED
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
 * mySecFilterCallback - dolaze sekcije (PAT, PMT) na pid i table_id za koji je filter
 * ---------------------------------------------------------------------------*/
static int32_t mySecFilterCallback(uint8_t *buffer)
{
    if (!buffer) return -1;

    uint8_t  table_id       = buffer[0];
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];

    printf("\n[mySecFilterCallback] Section arrived, table_id=0x%X, length=%d\n",
           table_id, section_length);

    if (table_id == 0x00) {
        /* PAT */
        parsePAT(buffer);
    }
    else if (table_id == 0x02) {
        /* PMT */
        parsePMT(buffer);
    }
    else {
        printf("Nismo implementirali parse za table_id=0x%X\n", table_id);
    }

    return 0;
}

/* ----------------------------------------------------------------------------
 * parsePAT - minimalno dohvaca (programNumber, pmtPid)
 * ---------------------------------------------------------------------------*/
static void parsePAT(uint8_t *buffer)
{
    /* Kod dvb standarda, offset 8 i 9 su program_number, offset 10 i 11 su PMT pid (13 bit) */
    /* section_length = (buffer[1]&0x0F)<<8 | buffer[2] */
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];
    uint32_t i;
    uint16_t program_number, pid;

    gNumPrograms = 0;

    for (i = 0; i < (section_length - 9)/4; i++) {
        program_number = (buffer[8 + i*4] << 8) | buffer[9 + i*4];
        pid            = ((buffer[10 + i*4] & 0x1F) << 8) | buffer[11 + i*4];

        if (program_number == 0) {
            /* network PID */
            printf("Network PID = %u\n", pid);
        } else {
            printf("Program number=%u, PMT pid=%u\n", program_number, pid);
            if (gNumPrograms < MAX_PROGRAMS) {
                gProgramList[gNumPrograms].programNumber = program_number;
                gProgramList[gNumPrograms].pmtPid        = pid;
                gNumPrograms++;
            }
        }
    }
    printf("parsePAT: pronadjeno %d programa.\n", gNumPrograms);
}

/* ----------------------------------------------------------------------------
 * parsePMT - pronadje audio i video pid (minimalni primjer)
 *            i kreira streamove
 * ---------------------------------------------------------------------------*/
static void parsePMT(uint8_t *buffer)
{
    uint16_t sectionLength    = ((buffer[1] & 0x0F) << 8) | buffer[2];
    uint16_t programInfoLen   = ((buffer[10] & 0x0F) << 8) | buffer[11];
    uint8_t *currentPos       = (uint8_t*)(buffer + 12 + programInfoLen);
    uint32_t parsedLength     = 12 + programInfoLen + 4 - 3; /* -3 je iz Vašeg primjera */
    uint8_t  elementaryCount  = 0;

    uint16_t foundAudioPid = 0;
    uint16_t foundVideoPid = 0;

    printf("parsePMT: sectionLength=%u, programInfoLength=%u\n", sectionLength, programInfoLen);

    while (parsedLength < sectionLength && elementaryCount < 20) {
        uint8_t  streamType   = currentPos[0];
        uint16_t elementaryPid= ((currentPos[1] & 0x1F) << 8) | currentPos[2];
        uint16_t esInfoLength = ((currentPos[3] & 0x0F) << 8) | currentPos[4];

        printf(" streamType=0x%X, pid=%u, esInfoLength=%u\n",
               streamType, elementaryPid, esInfoLength);

        /* Prepoznajemo neke osnovne streamove */
        if (streamType == 0x02 && foundVideoPid == 0) {
            /* MPEG2 Video */
            foundVideoPid = elementaryPid;
        } else if ((streamType == 0x03 || streamType == 0x04) && foundAudioPid == 0) {
            /* MPEG Audio */
            foundAudioPid = elementaryPid;
        } else if (streamType == 0x1B && foundVideoPid == 0) {
            /* H.264 / AVC video */
            foundVideoPid = elementaryPid;
        }
        /* Pomak na sljedeći ES descriptor */
        currentPos    += 5 + esInfoLength;
        parsedLength  += 5 + esInfoLength;
        elementaryCount++;
    }

    printf("PMT: pronadjen AudioPid=%u, VideoPid=%u\n", foundAudioPid, foundVideoPid);

    /* Zaustavimo stare streamove (ako postoje) */
    if (gAudioStreamHandle) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gAudioStreamHandle);
        gAudioStreamHandle = 0;
    }
    if (gVideoStreamHandle) {
        Player_Stream_Remove(gPlayerHandle, gSourceHandle, gVideoStreamHandle);
        gVideoStreamHandle = 0;
    }

    /* Kreiramo video stream (ako pronađen) */
    if (foundVideoPid) {
        /* Ovdje za 0x02 (MPEG2) -> VIDEO_TYPE_MPEG2, za 0x1B -> VIDEO_TYPE_H264 itd. 
           Jednostavno cemo uvijek stavljati VIDEO_TYPE_MPEG2, 
           ali realno treba prepoznati streamType. */
        Player_Stream_Create(gPlayerHandle, gSourceHandle, foundVideoPid,
                             VIDEO_TYPE_MPEG2, /* prilagodite prema stvarnom streamTypeu */
                             &gVideoStreamHandle);
    }

    /* Kreiramo audio stream (ako pronađen) */
    if (foundAudioPid) {
        Player_Stream_Create(gPlayerHandle, gSourceHandle, foundAudioPid,
                             AUDIO_TYPE_MPEG_AUDIO, /* prilagodite ako je npr. AAC/AC3 */
                             &gAudioStreamHandle);
    }

    /* Ispiši info (i na OSD) */
    printf("***** KANAL idx=%d, PMT pid=%u, A_pid=%u, V_pid=%u *****\n",
           gCurrentProgramIndex,
           gProgramList[gCurrentProgramIndex].pmtPid,
           foundAudioPid,
           foundVideoPid);

    /* Prikažemo info na ekranu npr. 3 sekunde */
    showChannelInfoOSD(gCurrentProgramIndex,
                       gProgramList[gCurrentProgramIndex].pmtPid,
                       foundAudioPid,
                       foundVideoPid);
}

/* ----------------------------------------------------------------------------
 * remoteControlThread - beskonačna petlja za CH+/CH- tipke
 * ---------------------------------------------------------------------------*/
static void* remoteControlThread(void *arg)
{
    struct input_event events[8];

    while (1) {
        int n = read(gInputFd, events, sizeof(events));
        if (n > 0) {
            int count = n / (int)sizeof(struct input_event);

            for (int i = 0; i < count; i++) {
                if (events[i].type == EV_KEY && events[i].value == 1) {
                    /* value=1 -> pritisak tipke */
                    if (events[i].code == KEY_CHANNELUP) {
                        printf("[Remote] CH+\n");
                        if (gNumPrograms > 0) {
                            gCurrentProgramIndex++;
                            if (gCurrentProgramIndex >= gNumPrograms) {
                                gCurrentProgramIndex = 0;
                            }
                            switchToChannel(gCurrentProgramIndex);
                        }
                    }
                    else if (events[i].code == KEY_CHANNELDOWN) {
                        printf("[Remote] CH-\n");
                        if (gNumPrograms > 0) {
                            gCurrentProgramIndex--;
                            if (gCurrentProgramIndex < 0) {
                                gCurrentProgramIndex = gNumPrograms - 1;
                            }
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
 * switchToChannel - postavljanje PMT filtera za odabrani kanal
 * ---------------------------------------------------------------------------*/
static void switchToChannel(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= gNumPrograms) return;

    /* Oslobodi stari PMT filter */
    if (gPmtFilterHandle) {
        Demux_Free_Filter(gPlayerHandle, gPmtFilterHandle);
        gPmtFilterHandle = 0;
    }

    uint16_t pmtPid = gProgramList[channelIndex].pmtPid;
    printf("switchToChannel: idx=%d, PMT pid=%u\n", channelIndex, pmtPid);

    /* Postavimo filter za PMT (table_id=0x02) */
    if (Demux_Set_Filter(gPlayerHandle, pmtPid, 0x02, &gPmtFilterHandle) != NO_ERROR) {
        printf("Demux_Set_Filter PMT fail (pid=%u)!\n", pmtPid);
    }
}

/* ----------------------------------------------------------------------------
 * initDirectFB - inicijalizacija DirectFB i kreiranje primary surface
 * ---------------------------------------------------------------------------*/
static int initDirectFB(void)
{
    DFBResult            ret;
    DFBSurfaceDescription dsc;
    DFBFontDescription    fontDesc;
    int                  argc = 0;
    char               **argv = NULL;

    /* 1) DirectFBInit bez argumenata */
    ret = DirectFBInit(&argc, &argv);
    if (ret != DFB_OK) {
        fprintf(stderr, "DirectFBInit fail!\n");
        return -1;
    }

    /* 2) Kreiraj DF interface */
    ret = DirectFBCreate(&gDfbInterface);
    if (ret != DFB_OK) {
        fprintf(stderr, "DirectFBCreate fail!\n");
        return -1;
    }

    /* 3) Fullscreen cooperative level */
    ret = gDfbInterface->SetCooperativeLevel(gDfbInterface, DFSCL_FULLSCREEN);
    if (ret != DFB_OK) {
        fprintf(stderr, "SetCooperativeLevel fail!\n");
        return -1;
    }

    /* 4) Primary surface s flipping */
    memset(&dsc, 0, sizeof(dsc));
    dsc.flags = DSDESC_CAPS;
    dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

    ret = gDfbInterface->CreateSurface(gDfbInterface, &dsc, &gPrimarySurface);
    if (ret != DFB_OK) {
        fprintf(stderr, "CreateSurface fail!\n");
        return -1;
    }

    /* 5) Dimenzije ekrana */
    gPrimarySurface->GetSize(gPrimarySurface, &gScreenWidth, &gScreenHeight);
    printf("DirectFB screen: %dx%d\n", gScreenWidth, gScreenHeight);

    /* 6) Učitavanje fonta (po potrebi) */
    fontDesc.flags  = DFDESC_HEIGHT;
    fontDesc.height = 36;  /* Veličina fonta */

    ret = gDfbInterface->CreateFont(gDfbInterface, "/usr/share/fonts/DejaVuSans.ttf",
                                    &fontDesc, &gFontInterface);
    if (ret == DFB_OK) {
        gPrimarySurface->SetFont(gPrimarySurface, gFontInterface);
    } else {
        fprintf(stderr, "CreateFont fail! (Ako nemate font, preskocite)\n");
        gFontInterface = NULL;
    }

    /* Očisti ekran na crno */
    gPrimarySurface->SetColor(gPrimarySurface, 0x00, 0x00, 0x00, 0xFF);
    gPrimarySurface->FillRectangle(gPrimarySurface, 0, 0, gScreenWidth, gScreenHeight);
    gPrimarySurface->Flip(gPrimarySurface, NULL, 0);

    return 0;
}

/* ----------------------------------------------------------------------------
 * deinitDirectFB - oslobađa surface, font i DF interface
 * ---------------------------------------------------------------------------*/
static void deinitDirectFB(void)
{
    if (gFontInterface) {
        gFontInterface->Release(gFontInterface);
        gFontInterface = NULL;
    }
    if (gPrimarySurface) {
        gPrimarySurface->Release(gPrimarySurface);
        gPrimarySurface = NULL;
    }
    if (gDfbInterface) {
        gDfbInterface->Release(gDfbInterface);
        gDfbInterface = NULL;
    }
}

/* ----------------------------------------------------------------------------
 * showChannelInfoOSD - prikaz OSD teksta o kanalu na par sekundi
 * ---------------------------------------------------------------------------*/
static void showChannelInfoOSD(int channelIndex,
                               uint16_t pmtPid,
                               uint16_t audioPid,
                               uint16_t videoPid)
{
    if (!gPrimarySurface) {
        /* Ako nismo uspješno inicijalizirali DirectFB, samo ispis u konzolu. */
        return;
    }

    /* Crna poluprozirna podloga */
    int boxW = gScreenWidth / 3;
    int boxH = gScreenHeight / 5;
    int boxX = (gScreenWidth - boxW) / 2;
    int boxY = (gScreenHeight - boxH) / 3;

    gPrimarySurface->SetColor(gPrimarySurface, 0x00, 0x00, 0x00, 0xA0); /* RGBA, poluprozirno */
    gPrimarySurface->FillRectangle(gPrimarySurface, boxX, boxY, boxW, boxH);

    /* Bijela boja za tekst */
    gPrimarySurface->SetColor(gPrimarySurface, 0xFF, 0xFF, 0xFF, 0xFF);

    char lineBuf[128];
    snprintf(lineBuf, sizeof(lineBuf),
             "CH idx=%d\nPMT=%u\nA=%u\nV=%u",
             channelIndex, pmtPid, audioPid, videoPid);

    /* Ispišemo svaki red teksta malo niže */
    const int lineHeight = 40;
    int yPos = boxY + lineHeight;
    char *line = strtok(lineBuf, "\n");
    while (line) {
        gPrimarySurface->DrawString(gPrimarySurface, line, -1, boxX + 20, yPos, DSTF_LEFT);
        yPos += lineHeight;
        line = strtok(NULL, "\n");
    }
    gPrimarySurface->Flip(gPrimarySurface, NULL, DSFLIP_NONE);

    /* Ostavimo 3 sekunde, pa obrišemo taj pravokutnik (opet iscrtamo crno) */
    sleep(3);

    /* Obrišemo */
    gPrimarySurface->SetColor(gPrimarySurface, 0x00, 0x00, 0x00, 0xFF);
    gPrimarySurface->FillRectangle(gPrimarySurface, boxX, boxY, boxW, boxH);
    gPrimarySurface->Flip(gPrimarySurface, NULL, DSFLIP_NONE);
}
