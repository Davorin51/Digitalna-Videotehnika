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
uint16_t currentPMTPID = 0;

// Globalni handleri
uint32_t playerHandle = 0;
uint32_t sourceHandle = 0;
uint32_t filterHandle = 0;

// Synchronizacija
static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

// Funkcije
void parsePAT(uint8_t *buffer);
void parsePMT(uint8_t *buffer);
int32_t mySecFilterCallback(uint8_t *buffer);
int32_t tunerStatusCallback(t_LockStatus status);
void change_channel(int direction);

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

void change_channel(int direction) {
    static int current_channel_index = 0;

    if (direction == 1) {
        current_channel_index = (current_channel_index + 1) % program_count;
    } else if (direction == -1) {
        current_channel_index = (current_channel_index == 0) ? program_count - 1 : current_channel_index - 1;
    }

    currentPMTPID = program_list[current_channel_index].pmt_pid;
    printf("Switching to Program Number: %u, PMT PID: %u\n",
           program_list[current_channel_index].program_number, currentPMTPID);

    // Postavi novi filter za PMT
    Demux_Free_Filter(playerHandle, filterHandle);
    Demux_Set_Filter(playerHandle, currentPMTPID, 0x02, &filterHandle);
}

void *remote_control_handler(void *arg) {
    int input_fd = open(REMOTE_DEVICE, O_RDONLY);
    if (input_fd == -1) {
        perror("Error opening remote control device");
        pthread_exit(NULL);
    }

    struct input_event event;
    while (1) {
        if (read(input_fd, &event, sizeof(struct input_event)) == sizeof(struct input_event)) {
            if (event.type == EV_KEY && event.value == 1) {
                if (event.code == KEY_CHANNEL_UP) {
                    printf("CH+ pressed: Switching to next channel\n");
                    change_channel(1);
                } else if (event.code == KEY_CHANNEL_DOWN) {
                    printf("CH- pressed: Switching to previous channel\n");
                    change_channel(-1);
                }
            }
        }
    }

    close(input_fd);
    pthread_exit(NULL);
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

    // Kreiraj nit za daljinski upravljač
    pthread_t remote_thread;
    if (pthread_create(&remote_thread, NULL, remote_control_handler, NULL) != 0) {
        perror("Failed to create remote control thread");
        return -1;
    }

    printf("Press Enter to exit...\n");
    getchar();

    // Čišćenje resursa
    pthread_cancel(remote_thread);
    pthread_join(remote_thread, NULL);
    Demux_Free_Filter(playerHandle, filterHandle);
    Player_Source_Close(playerHandle, sourceHandle);
    Player_Deinit(playerHandle);
    Tuner_Deinit();

    return 0;
}
