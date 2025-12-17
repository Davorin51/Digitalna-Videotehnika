#include "tdp_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <linux/input.h>
#include <fcntl.h>
#include <sys/ioctl.h>

/* -------------------- User settings -------------------- */
#define DESIRED_FREQUENCY_HZ   754000000u   /* promijeni po potrebi */
#define BANDWIDTH_MHZ          8u
#define MODULE_TYPE            DVB_T

#define INPUT_DEV              "/dev/input/event0"

/* Key codes (kao u tvojoj Vježbi 1 primjeru) */
#define KEY_EXIT_CODE          102
#define KEY_CH_DOWN_CODE       61
#define KEY_CH_UP_CODE         62

/* Limits */
#define MAX_CHANNELS           64

/* ------------------------------------------------------- */

static inline void textColor(int32_t attr, int32_t fg, int32_t bg)
{
    char command[32];
    sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
    printf("%s", command);
}

#define ASSERT_TDP_RESULT(x,y)  do { \
    if (NO_ERROR == (x)) { \
        printf("%s success\n", (y)); \
    } else { \
        textColor(1,1,0); \
        printf("%s fail\n", (y)); \
        textColor(0,7,0); \
        return -1; \
    } \
} while(0)

/* -------------------- Data structs -------------------- */

typedef struct {
    uint16_t program_number;  /* npr 1,2,3... (0 je network) */
    uint16_t pmt_pid;         /* PID PMT tablice tog programa */
} channel_t;

typedef struct {
    uint16_t pcr_pid;

    uint16_t video_pid;
    tStreamType video_type;

    uint16_t audio_pid;
    tStreamType audio_type;
} pmt_info_t;

/* -------------------- Globals -------------------- */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_tuner_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  g_pat_cv   = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  g_pmt_cv   = PTHREAD_COND_INITIALIZER;

static int g_tuner_locked = 0;
static int g_pat_ready    = 0;
static int g_pmt_ready    = 0;

static channel_t g_channels[MAX_CHANNELS];
static int g_channel_count = 0;
static int g_current_idx   = 0;

static pmt_info_t g_pmt;

/* -------------------- Helpers -------------------- */

static void timespec_add_seconds(struct timespec *ts, int sec)
{
    ts->tv_sec += sec;
}

/* MPEG stream_type -> tdp_api tStreamType (minimal mapping) */
static tStreamType map_mpeg_stream_type_to_tdp(uint8_t st)
{
    switch (st) {
        /* Video */
        case 0x02: return VIDEO_TYPE_MPEG2;  /* MPEG-2 video */
        case 0x1B: return VIDEO_TYPE_H264;   /* H.264/AVC */

        /* Audio */
        case 0x03: /* MPEG-1 audio */
        case 0x04: /* MPEG-2 audio */
            return AUDIO_TYPE_MPEG_AUDIO;

        case 0x0F: return AUDIO_TYPE_HE_AAC; /* AAC */
        case 0x81: return AUDIO_TYPE_DOLBY_AC3; /* često privatni AC3 */
        default:   return AUDIO_TYPE_MPEG_AUDIO; /* fallback */
    }
}

/* -------------------- PSI parsing -------------------- */

/*
  Buffer format koji tdp_api.c šalje u callback:
  - buffer[0] = table_id
  - buffer[1..2] sadrži section_length (12-bit) + ostali bitovi
  - ukupna dužina u bajtovima je: 3 + section_length
*/

static void parse_pat(uint8_t *buffer)
{
    uint8_t table_id = buffer[0];
    if (table_id != 0x00) return; /* PAT */

    uint16_t section_length = (uint16_t)(((buffer[1] & 0x0F) << 8) | buffer[2]);

    /* Program loop bytes = section_length - 9  (5 header + 4 CRC) */
    if (section_length < 9) return;

    int entries = (section_length - 9) / 4;
    if (entries > MAX_CHANNELS) entries = MAX_CHANNELS;

    int count = 0;

    for (int i = 0; i < entries; i++) {
        uint16_t program_number = (uint16_t)((buffer[8 + i*4] << 8) | buffer[9 + i*4]);
        uint16_t pid = (uint16_t)(((buffer[10 + i*4] & 0x1F) << 8) | buffer[11 + i*4]);

        if (program_number == 0) {
            /* network PID - može se ispisat, ali nije kanal */
            continue;
        }

        g_channels[count].program_number = program_number;
        g_channels[count].pmt_pid = pid;
        count++;
    }

    pthread_mutex_lock(&g_lock);
    g_channel_count = count;
    g_pat_ready = (count > 0);
    pthread_cond_signal(&g_pat_cv);
    pthread_mutex_unlock(&g_lock);

    /* debug */
    printf("\nPAT parsed: channels=%d\n", count);
    for (int k = 0; k < count; k++) {
        printf("  idx=%d  program=%u  PMT_PID=0x%X\n",
               k, g_channels[k].program_number, g_channels[k].pmt_pid);
    }
    fflush(stdout);
}

static void parse_pmt(uint8_t *buffer)
{
    uint8_t table_id = buffer[0];
    if (table_id != 0x02) return; /* PMT */

    uint16_t section_length = (uint16_t)(((buffer[1] & 0x0F) << 8) | buffer[2]);

    /* total bytes in this section buffer = 3 + section_length */
    if (section_length < 13) return;

    uint16_t pcr_pid = (uint16_t)(((buffer[8]  & 0x1F) << 8) | buffer[9]);
    uint16_t program_info_length = (uint16_t)(((buffer[10] & 0x0F) << 8) | buffer[11]);

    uint32_t start = 12u + (uint32_t)program_info_length;     /* first ES entry */
    uint32_t end   = (uint32_t)(3u + section_length - 4u);    /* stop before CRC */

    if (start >= end) return;

    /* init as "not found" */
    pmt_info_t info;
    memset(&info, 0, sizeof(info));
    info.pcr_pid = pcr_pid;
    info.video_pid = 0x1FFF;
    info.audio_pid = 0x1FFF;
    info.video_type = VIDEO_TYPE_MPEG2;
    info.audio_type = AUDIO_TYPE_MPEG_AUDIO;

    uint32_t pos = start;
    int safety = 0;

    while ((pos + 5u) <= end && safety < 32) {
        uint8_t  stream_type = buffer[pos + 0];
        uint16_t elem_pid = (uint16_t)(((buffer[pos + 1] & 0x1F) << 8) | buffer[pos + 2]);
        uint16_t es_info_length = (uint16_t)(((buffer[pos + 3] & 0x0F) << 8) | buffer[pos + 4]);

        /* pick first video + first audio (simple) */
        if (info.video_pid == 0x1FFF) {
            if (stream_type == 0x02 || stream_type == 0x1B) {
                info.video_pid = elem_pid;
                info.video_type = map_mpeg_stream_type_to_tdp(stream_type);
            }
        }
        if (info.audio_pid == 0x1FFF) {
            if (stream_type == 0x03 || stream_type == 0x04 || stream_type == 0x0F || stream_type == 0x81) {
                info.audio_pid = elem_pid;
                info.audio_type = map_mpeg_stream_type_to_tdp(stream_type);
            }
        }

        pos += 5u + (uint32_t)es_info_length;
        safety++;
    }

    pthread_mutex_lock(&g_lock);
    g_pmt = info;
    g_pmt_ready = 1;
    pthread_cond_signal(&g_pmt_cv);
    pthread_mutex_unlock(&g_lock);

    /* debug */
    printf("\nPMT parsed: PCR=0x%X  V=0x%X(type=%d)  A=0x%X(type=%d)\n",
           info.pcr_pid, info.video_pid, (int)info.video_type, info.audio_pid, (int)info.audio_type);
    fflush(stdout);
}

/* -------------------- Callbacks -------------------- */

static int32_t tunerStatusCallback(t_LockStatus status)
{
    pthread_mutex_lock(&g_lock);
    if (status == STATUS_LOCKED) {
        g_tuner_locked = 1;
        pthread_cond_signal(&g_tuner_cv);
        printf("\n[TUNER] LOCKED\n");
    } else {
        printf("\n[TUNER] NOT LOCKED\n");
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int32_t sectionCallback(uint8_t *buffer)
{
    if (!buffer) return -1;

    uint8_t table_id = buffer[0];
    if (table_id == 0x00) {
        parse_pat(buffer);
    } else if (table_id == 0x02) {
        parse_pmt(buffer);
    } else {
        /* ignore other tables */
    }
    return 0;
}

/* -------------------- Zapper logic -------------------- */

static int tune_channel(
    int idx,
    uint32_t playerHandle,
    uint32_t sourceHandle,
    uint32_t *filterHandle,
    uint32_t *videoStreamHandle,
    uint32_t *audioStreamHandle
)
{
    int32_t result;

    /* stop old streams */
    if (*videoStreamHandle) {
        Player_Stream_Remove(playerHandle, sourceHandle, *videoStreamHandle);
        *videoStreamHandle = 0;
    }
    if (*audioStreamHandle) {
        Player_Stream_Remove(playerHandle, sourceHandle, *audioStreamHandle);
        *audioStreamHandle = 0;
    }

    /* request PMT */
    uint16_t pmt_pid = g_channels[idx].pmt_pid;

    pthread_mutex_lock(&g_lock);
    g_pmt_ready = 0;
    pthread_mutex_unlock(&g_lock);

    result = Demux_Set_Filter(playerHandle, pmt_pid, 0x02, filterHandle);
    if (result != NO_ERROR) {
        printf("Demux_Set_Filter(PMT) failed (PID=0x%X)\n", pmt_pid);
        return -1;
    }

    /* wait for PMT */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    timespec_add_seconds(&ts, 3);

    pthread_mutex_lock(&g_lock);
    while (!g_pmt_ready) {
        int rc = pthread_cond_timedwait(&g_pmt_cv, &g_lock, &ts);
        if (rc == ETIMEDOUT) break;
    }
    int ok = g_pmt_ready;
    pmt_info_t info = g_pmt;
    pthread_mutex_unlock(&g_lock);

    /* must free filter before setting another one later */
    Demux_Free_Filter(playerHandle, *filterHandle);

    if (!ok) {
        printf("PMT timeout (idx=%d, PMT_PID=0x%X)\n", idx, pmt_pid);
        return -1;
    }

    /* start streams */
    if (info.video_pid != 0x1FFF) {
        result = Player_Stream_Create(playerHandle, sourceHandle, info.video_pid, info.video_type, videoStreamHandle);
        if (result != NO_ERROR) printf("Player_Stream_Create(video) failed\n");
    } else {
        printf("No video PID found in PMT\n");
    }

    if (info.audio_pid != 0x1FFF) {
        result = Player_Stream_Create(playerHandle, sourceHandle, info.audio_pid, info.audio_type, audioStreamHandle);
        if (result != NO_ERROR) printf("Player_Stream_Create(audio) failed\n");
    } else {
        printf("No audio PID found in PMT\n");
    }

    /* “info overlay” u terminalu */
    printf("\n==== NOW PLAYING ====\n");
    printf("CH idx=%d  program=%u\n", idx, g_channels[idx].program_number);
    printf("PMT PID=0x%X  V PID=0x%X  A PID=0x%X\n", pmt_pid, info.video_pid, info.audio_pid);
    printf("=====================\n");
    fflush(stdout);

    return 0;
}

/* -------------------- Main -------------------- */

int main(void)
{
    int32_t result;

    uint32_t playerHandle = 0;
    uint32_t sourceHandle = 0;
    uint32_t filterHandle = 0;

    uint32_t videoStreamHandle = 0;
    uint32_t audioStreamHandle = 0;

    /* 1) Tuner init + lock */
    result = Tuner_Init();
    ASSERT_TDP_RESULT(result, "Tuner_Init");

    result = Tuner_Register_Status_Callback(tunerStatusCallback);
    ASSERT_TDP_RESULT(result, "Tuner_Register_Status_Callback");

    result = Tuner_Lock_To_Frequency(DESIRED_FREQUENCY_HZ, BANDWIDTH_MHZ, MODULE_TYPE);
    ASSERT_TDP_RESULT(result, "Tuner_Lock_To_Frequency");

    /* wait lock (10s) */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    timespec_add_seconds(&ts, 10);

    pthread_mutex_lock(&g_lock);
    while (!g_tuner_locked) {
        int rc = pthread_cond_timedwait(&g_tuner_cv, &g_lock, &ts);
        if (rc == ETIMEDOUT) break;
    }
    int locked = g_tuner_locked;
    pthread_mutex_unlock(&g_lock);

    if (!locked) {
        printf("Lock timeout exceeded!\n");
        Tuner_Deinit();
        return -1;
    }

    /* 2) Player init + source open */
    result = Player_Init(&playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Init");

    result = Player_Source_Open(playerHandle, &sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Open");

    /* 3) Demux register callback */
    result = Demux_Register_Section_Filter_Callback(sectionCallback);
    ASSERT_TDP_RESULT(result, "Demux_Register_Section_Filter_Callback");

    /* 4) PAT filter + wait PAT */
    pthread_mutex_lock(&g_lock);
    g_pat_ready = 0;
    pthread_mutex_unlock(&g_lock);

    result = Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Set_Filter(PAT)");

    clock_gettime(CLOCK_REALTIME, &ts);
    timespec_add_seconds(&ts, 3);

    pthread_mutex_lock(&g_lock);
    while (!g_pat_ready) {
        int rc = pthread_cond_timedwait(&g_pat_cv, &g_lock, &ts);
        if (rc == ETIMEDOUT) break;
    }
    int pat_ok = g_pat_ready;
    pthread_mutex_unlock(&g_lock);

    /* Free PAT filter (API drži samo jedan filter globalno) */
    Demux_Free_Filter(playerHandle, filterHandle);

    if (!pat_ok || g_channel_count <= 0) {
        printf("PAT not received / parsed (timeout)\n");
        Player_Source_Close(playerHandle, sourceHandle);
        Player_Deinit(playerHandle);
        Tuner_Deinit();
        return -1;
    }

    /* 5) Play first channel (first in PAT list) */
    g_current_idx = 0;
    tune_channel(g_current_idx, playerHandle, sourceHandle, &filterHandle, &videoStreamHandle, &audioStreamHandle);

    /* 6) Open remote input */
    int fd = open(INPUT_DEV, O_RDONLY);
    if (fd < 0) {
        printf("Cannot open %s (%s)\n", INPUT_DEV, strerror(errno));
        /* nastavi bez daljinskog, ali realno nema smisla */
    } else {
        printf("\nRemote ready. Use CH+/CH- to zap, EXIT to quit.\n");
    }

    /* 7) Main loop: wait for key events */
    while (fd >= 0) {
        struct input_event ev;
        int r = read(fd, &ev, sizeof(ev));
        if (r != (int)sizeof(ev)) {
            if (errno == EINTR) continue;
            printf("Read input error (%s)\n", strerror(errno));
            break;
        }

        if (ev.type != EV_KEY) continue;
        if (!(ev.value == 1 || ev.value == 0)) continue; /* press/release only */

        /* react on press only */
        if (ev.value != 1) continue;

        if (ev.code == KEY_EXIT_CODE) {
            printf("EXIT pressed -> quitting.\n");
            break;
        }

        if (ev.code == KEY_CH_UP_CODE) {
            g_current_idx++;
            if (g_current_idx >= g_channel_count) g_current_idx = 0;
            tune_channel(g_current_idx, playerHandle, sourceHandle, &filterHandle, &videoStreamHandle, &audioStreamHandle);
        } else if (ev.code == KEY_CH_DOWN_CODE) {
            g_current_idx--;
            if (g_current_idx < 0) g_current_idx = g_channel_count - 1;
            tune_channel(g_current_idx, playerHandle, sourceHandle, &filterHandle, &videoStreamHandle, &audioStreamHandle);
        }
    }

    if (fd >= 0) close(fd);

    /* 8) Cleanup */
    if (videoStreamHandle) Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
    if (audioStreamHandle) Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

    /* unregister callback (optional, ali uredno) */
    Demux_Unregister_Section_Filter_Callback(sectionCallback);

    Player_Source_Close(playerHandle, sourceHandle);
    Player_Deinit(playerHandle);

    Tuner_Unregister_Status_Callback(tunerStatusCallback);
    Tuner_Deinit();

    return 0;
}
