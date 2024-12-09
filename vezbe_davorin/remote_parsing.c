#include "tdp_api.h"
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#define DESIRED_FREQUENCY 818000000  /* Frekvencija tunera u Hz (818 MHz) */
#define BANDWIDTH 8                  /* Širina kanala u MHz */

#define REMOTE_DEVICE "/dev/input/event0" // Putanja do uređaja daljinskog upravljača
#define KEY_CHANNEL_UP 103                 // Kod za CH+ (zamijenite stvarnim kodom tipke)
#define KEY_CHANNEL_DOWN 108               // Kod za CH- (zamijenite stvarnim kodom tipke)

#define MAX_PROGRAMS 10

// Struktura za spremanje informacija o programima
typedef struct {
    uint16_t program_number;
    uint16_t pmt_pid;
} ProgramInfo;

ProgramInfo program_list[MAX_PROGRAMS];
uint8_t program_count = 0;

// Globalni PID-ovi za video i audio
uint16_t currentVideoPID = 0;
uint16_t currentAudioPID = 0;
uint16_t currentPMTPID = 0;

// Globalni handleri
uint32_t playerHandle = 0;
uint32_t sourceHandle = 0;
uint32_t filterHandle = 0;
uint32_t videoStreamHandle = 0;
uint32_t audioStreamHandle = 0;

// Synchronizacija
static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

// Funkcije
void parsePAT(uint8_t *buffer);
void parsePMT(uint8_t *buffer);
int32_t mySecFilterCallback(uint8_t *buffer);
int32_t tunerStatusCallback(t_LockStatus status);
void change_channel(int direction);
void playChannel();

void parsePAT(uint8_t *buffer) {
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];
    program_count = 0;

    for (uint32_t i = 0; i < (section_length - 9) / 4 && program_count < MAX_PROGRAMS; i++) {
        uint16_t program_number = (buffer[8 + i * 4] << 8) | buffer[9 + i * 4];
        uint16_t pmt_pid = ((buffer[10 + i * 4] & 0x1F) << 8) | buffer[11 + i * 4];

        program_list[program_count].program_number = program_number;
        program_list[program_count].pmt_pid = pmt_pid;

        printf("Program %u -> PMT PID: %u\n", program_number, pmt_pid);
        program_count++;
    }

    // Odaberi prvi kanal
    if (program_count > 0) {
        currentPMTPID = program_list[0].pmt_pid;
        printf("Initial Program Selected: %u, PMT PID: %u\n", program_list[0].program_number, currentPMTPID);
    } else {
        printf("No programs found!\n");
        exit(EXIT_FAILURE);
    }
}

void parsePMT(uint8_t *buffer) {
    uint16_t section_length = ((buffer[1] & 0x0F) << 8) | buffer[2];
    uint16_t program_info_length = ((buffer[10] & 0x0F) << 8) | buffer[11];

    uint8_t *current_pos = buffer + 12 + program_info_length;
    uint16_t parsed_length = 12 + program_info_length;

    currentVideoPID = 0;
    currentAudioPID = 0;

    while (parsed_length < (section_length + 3)) {
        uint8_t stream_type = current_pos[0];
        uint16_t elementary_pid = ((current_pos[1] & 0x1F) << 8) | current_pos[2];
        uint16_t es_info_length = ((current_pos[3] & 0x0F) << 8) | current_pos[4];

        if (stream_type == 0x02 && currentVideoPID == 0) {
            currentVideoPID = elementary_pid;
        } else if (stream_type == 0x03 && currentAudioPID == 0) {
            currentAudioPID = elementary_pid;
        }

        current_pos += 5 + es_info_length;
        parsed_length += 5 + es_info_length;
    }

    printf("Video PID: %u, Audio PID: %u\n", currentVideoPID, currentAudioPID);
    playChannel();
}

void playChannel() {
    if (currentVideoPID && currentAudioPID) {
        Player_Stream_Create(playerHandle, sourceHandle, currentVideoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle);
        Player_Stream_Create(playerHandle, sourceHandle, currentAudioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    }
}

int32_t mySecFilterCallback(uint8_t *buffer) {
    uint8_t table_id = buffer[0];
    if (table_id == 0x00) {
        parsePAT(buffer);
    } else if (table_id == 0x02) {
        parsePMT(buffer);
    }
    return 0;
}

int32_t tunerStatusCallback(t_LockStatus status) {
    if (status == STATUS_LOCKED) {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("Tuner Locked!\n");
    }
    return 0;
}

int main() {
    struct timespec lockStatusWaitTime;
    struct timeval now;

    Tuner_Init();
    Tuner_Register_Status_Callback(tunerStatusCallback);
    Tuner_Lock_To_Frequency(DESIRED_FREQUENCY, BANDWIDTH, DVB_T);

    gettimeofday(&now, NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec + 10;
    pthread_mutex_lock(&statusMutex);
    pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime);
    pthread_mutex_unlock(&statusMutex);

    Player_Init(&playerHandle);
    Player_Source_Open(playerHandle, &sourceHandle);
    Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);
    Demux_Register_Section_Filter_Callback(mySecFilterCallback);

    printf("Press Enter to exit...\n");
    getchar();

    // Čišćenje resursa
    Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);
    Demux_Free_Filter(playerHandle, filterHandle);
    Player_Source_Close(playerHandle, sourceHandle);
    Player_Deinit(playerHandle);
    Tuner_Deinit();

    return 0;
}
