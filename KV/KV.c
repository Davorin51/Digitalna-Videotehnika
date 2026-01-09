/*
  STUDENT START (HARD) — 20 TODO (2h)
  Cilj: Popuniti TODO(01..20) da aplikacija:
    - ucita config.ini robustno
    - zakljuca tuner (timeout i error handling)
    - parsira PAT/PMT stabilno (bez hardcode stream_count)
    - mapira 7 kanala (video+audio ili radio)
    - radi remote input bez busy-waita (select/poll)
    - crta overlay bez race conditiona (mutex)
    - timer brise overlay nakon 5 sek
    - EXIT uredno gasi sve bez crasha
*/

#include "tdp_api.h"
#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <directfb.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>

// TODO(01): Dodaj potrebne include-e za: strerror/strchr/strstr/memset/sprintf/strlen + gettimeofday + select
//  - <string.h>
//  - <sys/time.h>
//  - <sys/select.h>  (ili <sys/time.h> + <sys/types.h> ovisno o toolchainu)

#define NO_ERROR 0
#define ERROR    1

#define DEGREES_TO_RADIANS(deg) ((deg)*M_PI / 180.0)

/* TODO(02): Zamijeni DFBCHECK robustnim do{...}while(0) makroom */
#define DFBCHECK(x...)                               \
{                                                    \
    DFBResult err = x;                               \
    if (err != DFB_OK)                               \
    {                                                \
        fprintf(stderr, "%s <%d>:\n\t", __FILE__, __LINE__); \
        DirectFBErrorFatal(#x, err);                 \
    }                                                \
}

/* ---------------- Global state ---------------- */
static pthread_t remote_thread;
static int32_t inputFileDesc = -1;

static volatile int runMain = 1;
static volatile int runRemote = 1;

/* UI + state */
static uint16_t channel_btn = 2;   // intern: 2..8 (kao u originalu)
static uint32_t volume = 50;
static int muted = 0;

static char channel_msg[16] = "Channel 1";
static char volume_msg[16]  = "50%";

/* Forward declarations */
static void Screen_Init(int argc, char **argv);
static void Screen_Deinit(void);
static void Screen_Clear(void);
static void Info_Draw(uint32_t chbtn);
static void Radio_Draw(void);
static void Volume_Draw(void);
static void Channel_Draw(uint32_t chbtn);

static void timer_init(void);
static void timer_deinit(void);
static void timer_reset(void);

static void *remoteThreadTask(void *arg);

/* ---------------- Config ---------------- */
struct Conf {
  int Frequency;
  int Bandwidth;
  int Module;
  int aPID;
  int vPID;
  int aType;
  int vType;
};
static struct Conf Config;

/* TODO(03): Napiši helper funkcije za config parsing:
   - trim whitespace (lijevo/desno)
   - skip komentare (# ili ;)
   - ignoriraj prazne linije
   - parsiraj key=value
   Napravi: static char* trim(char* s); i koristi u Read_Config()
*/
static void Read_Config(void)
{
    FILE *fp = fopen("config.ini", "r");
    // TODO(04): Ako fp==NULL -> perror + postavi neke default vrijednosti (ne crash)
    // npr. default freq/bw/module/pid/type

    char *line = NULL;
    size_t cap = 0;

    while (fp && getline(&line, &cap, fp) != -1) {
        // TODO(05): Trim + skip komentari/prazno + provjeri '=' (ne smije biti NULL)
        // TODO(06): Parsiraj key i value i popuni Config polja.
        // Napomena: kljucevi su: frequency, bandwidth, module, aPID, vPID, aType, vType
    }

    if (line) free(line);
    if (fp) fclose(fp);

    // TODO(07): Validacija: ako neki bitni parametar nije setan, postavi default i ispiši upozorenje.
}

/* ---------------- Tuner + Player init ---------------- */
static pthread_cond_t  statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex     = PTHREAD_MUTEX_INITIALIZER;

static uint32_t playerHandle = 0;
static uint32_t sourceHandle = 0;
static uint32_t streamHandleAudio = 0;
static uint32_t streamHandleVideo = 0;

static int32_t Tuner_Status(t_LockStatus status)
{
    if(status == STATUS_LOCKED) {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("[TUNER] LOCKED\n");
    } else {
        printf("[TUNER] NOT LOCKED\n");
    }
    return 0;
}

/* TODO(08): Start_Default_Channel učini “production-grade”:
   - inicijaliziraj timespec tv_nsec=0
   - napravi timedwait s realnim timeoutom (10s)
   - ako timeout: uredno unlock + deinit + return -1
   - provjeri povratne vrijednosti Player_* poziva (ako API vraća error)
*/
static int32_t Start_Default_Channel(void)
{
    struct timespec lockWait;
    struct timeval  now;

    gettimeofday(&now, NULL);
    lockWait.tv_sec  = now.tv_sec + 10;
    lockWait.tv_nsec = 0;

    Tuner_Init();
    Tuner_Register_Status_Callback(Tuner_Status);
    Tuner_Lock_To_Frequency(Config.Frequency, Config.Bandwidth, Config.Module);

    pthread_mutex_lock(&statusMutex);
    if (ETIMEDOUT == pthread_cond_timedwait(&statusCondition, &statusMutex, &lockWait)) {
        pthread_mutex_unlock(&statusMutex);
        printf("[ERROR] Tuner lock timeout!\n");
        Tuner_Deinit();
        return -1;
    }
    pthread_mutex_unlock(&statusMutex);

    Player_Init(&playerHandle);
    Player_Source_Open(playerHandle, &sourceHandle);

    Player_Stream_Create(playerHandle, sourceHandle, Config.vPID, Config.vType, &streamHandleVideo);
    Player_Stream_Create(playerHandle, sourceHandle, Config.aPID, Config.aType, &streamHandleAudio);

    Player_Volume_Set(playerHandle, volume * 10000000);

    return 0;
}

static void Deinit_Player_Tuner(void)
{
    if (playerHandle && sourceHandle) {
        Player_Source_Close(playerHandle, sourceHandle);
    }
    if (playerHandle) {
        Player_Deinit(playerHandle);
    }
    Tuner_Deinit();
}

/* ---------------- DVB parsing (PAT/PMT) ---------------- */
struct Pat{
    uint16_t section_length;
    uint16_t transport_stream_id;
    uint16_t version_number;
    uint8_t  program_count;
    uint16_t program_number[8];
    uint16_t pid[8];
};

struct Pmt{
    uint16_t program_info_length;
    uint16_t program_number;
    uint16_t section_length;
    uint8_t  stream_count; // broj ES unosa
    uint8_t  stream_type[20];
    uint16_t elementary_pid[20];
    uint16_t ES_info_length[20];
};

struct Chnl{
    uint16_t Video_pid;
    uint16_t Audio_pid;
    uint16_t Channel_number;
    uint8_t  is_radio; // 1 ako nema video
};

static struct Pat  pat_table;
static struct Pmt  pmt_table[8];
static struct Chnl channel[7];

static volatile int pat_ready = 0;
static volatile int pmt_ready[8] = {0};

static uint32_t filterHandle = 0;
static uint16_t Pmt_index = 0;

/* TODO(09): Parse_PAT napravi sigurnim:
   - provjeri minimalne duljine
   - izracunaj program_count i capaj na max 8
   - popuni pid/program_number
*/
static int32_t Parse_PAT(uint8_t *buffer)
{
    int i;
    pat_table.section_length = (uint16_t)((((buffer[1] << 8) | buffer[2]) & 0x0FFF));
    pat_table.transport_stream_id = (uint16_t)((buffer[3] << 8) | buffer[4]);
    pat_table.version_number = (uint16_t)((buffer[5] >> 1) & 0x1F);

    pat_table.program_count = (uint8_t)((pat_table.section_length - 8) / 4);
    if (pat_table.program_count > 8) pat_table.program_count = 8;

    for (i = 0; i < pat_table.program_count; i++) {
        pat_table.program_number[i] = (uint16_t)((buffer[8+i*4] << 8) | buffer[9+i*4]);
        pat_table.pid[i] = (uint16_t)((((buffer[10+i*4] & 0x1F) << 8) | buffer[11+i*4]));
    }

    pat_ready = 1;
    return 0;
}

/* TODO(10): Parse_PMT napravi “bez hardcode-a”:
   - section_length izracunaj iz headera
   - program_info_length izracunaj
   - potom iteriraj ES petlju dok ne dodjes do kraja sekcije
   - stream_count = broj stvarno procitanih ES elemenata
   - capaj na max 20
*/
static int32_t Parse_PMT(uint8_t *buffer, uint16_t idx)
{
    // header (PMT)
    pmt_table[idx].program_number = (uint16_t)((buffer[3] << 8) | buffer[4]);
    pmt_table[idx].section_length = (uint16_t)((((buffer[1] << 8) | buffer[2]) & 0x0FFF));
    pmt_table[idx].program_info_length = (uint16_t)((((buffer[10] << 8) | buffer[11]) & 0x0FFF));

    uint32_t pos = 12 + pmt_table[idx].program_info_length;
    uint32_t end = 3 + pmt_table[idx].section_length; // “end” unutar sekcije (bez CRC detalja)
    uint8_t count = 0;

    // TODO(10): Ispravi end računanje i iteriraj ispravno do kraja ES petlje
    // Hint: PMT section includes CRC uključeno u section_length. ES loop završava prije CRC (zadnja 4 bajta).

    while (pos + 5 < end && count < 20) {
        uint8_t st = buffer[pos + 0];
        uint16_t pid = (uint16_t)((((buffer[pos+1] & 0x1F) << 8) | buffer[pos+2]));
        uint16_t es_info_len = (uint16_t)((((buffer[pos+3] << 8) | buffer[pos+4]) & 0x0FFF));

        pmt_table[idx].stream_type[count] = st;
        pmt_table[idx].elementary_pid[count] = pid;
        pmt_table[idx].ES_info_length[count] = es_info_len;

        count++;
        pos += 5 + es_info_len;
    }

    pmt_table[idx].stream_count = count;
    pmt_ready[idx] = 1;
    return 0;
}

/* callback from Demux */
static int32_t Section_Filter_Callback(uint8_t *buffer)
{
    // buffer[0] je table_id: 0x00 PAT, 0x02 PMT
    uint8_t table_id = buffer[0];

    if (table_id == 0x00) {
        Parse_PAT(buffer);
    } else if (table_id == 0x02) {
        // PMT ide u trenutni Pmt_index (global)
        if (Pmt_index < 8) Parse_PMT(buffer, Pmt_index);
    }
    return 0;
}

/* TODO(11): Parse_Tables treba biti deterministički:
   - memset pat_table + pmt_table + flags
   - postavi PAT filter, pa cekaj pat_ready (timeout npr 1s ili 2s)
   - za svaki program u PAT (1..program_count-1):
       * postavi Pmt_index = i-1
       * postavi PMT filter na pat_table.pid[i]
       * cekaj pmt_ready[Pmt_index] s timeout
   - oslobodi filtere u pravom redoslijedu
   - bez sleep(1) “na slijepo”
*/
static int Parse_Tables(void)
{
    memset(&pat_table, 0, sizeof(pat_table));
    memset(&pmt_table, 0, sizeof(pmt_table));
    pat_ready = 0;
    for (int i=0;i<8;i++) pmt_ready[i] = 0;

    Demux_Register_Section_Filter_Callback(Section_Filter_Callback);

    // PAT
    Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);

    // TODO(11): Cekaj pat_ready (poll + nanosleep ili timed wait) s timeout
    // Hint: nanosleep 10ms u petlji, max ~200 iteracija za 2s.

    // PMT
    for (int i = 1; i < pat_table.program_count; i++) {
        Demux_Free_Filter(playerHandle, filterHandle);

        Pmt_index = (uint16_t)(i - 1);
        uint16_t pmt_pid = pat_table.pid[i];

        Demux_Set_Filter(playerHandle, pmt_pid, 0x02, &filterHandle);

        // TODO(11): Cekaj pmt_ready[Pmt_index] s timeout
    }

    Demux_Free_Filter(playerHandle, filterHandle);
    return 0;
}

/* TODO(12): Spremanje_Kanala napravi “ispravno”:
   - za svaki PMT (0..6) nađi:
       * video PID: stream_type == 0x02 (MPEG2 video) ili (ako želiš) 0x1B (H.264)
       * audio PID: stream_type == 0x03 (MPEG1 audio) ili 0x04 (MPEG2 audio)
   - ako nema video, označi kanal kao radio (is_radio=1)
   - inicijaliziraj vrijednosti na 0 prije popunjavanja
*/
static void Spremanje_Kanala(void)
{
    for (int ch = 0; ch < 7; ch++) {
        channel[ch].Channel_number = (uint16_t)(ch + 1);
        channel[ch].Video_pid = 0;
        channel[ch].Audio_pid = 0;
        channel[ch].is_radio = 0;

        // TODO(12): scan pmt_table[ch].stream_count i odaberi PID-eve
    }
}

/* TODO(13): Promjena_Kanala popravi mapping i robustnost:
   - trenutno channel_btn je 2..8, ali Channel_number je 1..7
   - izracunaj target_channel_number = channel_btn-1
   - pronađi Vpid/Apid
   - ako su 0 -> ne diraj streamove
   - ako je radio (no video) -> ukloni video stream i ostavi audio
*/
static void Promjena_Kanala(uint32_t chbtn)
{
    uint16_t target = (uint16_t)(chbtn - 1);
    uint16_t Vpid = 0, Apid = 0;
    uint8_t is_radio = 0;

    for (int i = 0; i < 7; i++) {
        if (channel[i].Channel_number == target) {
            Vpid = channel[i].Video_pid;
            Apid = channel[i].Audio_pid;
            is_radio = channel[i].is_radio;
            break;
        }
    }

    // TODO(13): robust update streamova (radio/video)
    (void)Vpid; (void)Apid; (void)is_radio;
}

/* ---------------- DirectFB UI ---------------- */
static IDirectFBSurface *primary = NULL;
static IDirectFB *dfb = NULL;
static int screenWidth = 0;
static int screenHeight = 0;

/* TODO(14): Uvedi mutex za sve DirectFB pozive (timer + remote thread race)
   - static pthread_mutex_t fbMutex = PTHREAD_MUTEX_INITIALIZER;
   - lock/unlock u: Screen_Clear, Info_Draw, Radio_Draw, Volume_Draw, Screen_Deinit
*/
static void Screen_Init(int argc, char **argv)
{
    DFBCHECK(DirectFBInit(&argc, &argv));
    DFBCHECK(DirectFBCreate(&dfb));
    DFBCHECK(dfb->SetCooperativeLevel(dfb, DFSCL_FULLSCREEN));

    DFBSurfaceDescription desc;
    desc.flags = DSDESC_CAPS;
    desc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;

    DFBCHECK(dfb->CreateSurface(dfb, &desc, &primary));
    DFBCHECK(primary->GetSize(primary, &screenWidth, &screenHeight));

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

    IDirectFBFont *font = NULL;
    DFBFontDescription fdesc;
    fdesc.flags = DFDESC_HEIGHT;
    fdesc.height = 60;
    DFBCHECK(dfb->CreateFont(dfb, "/home/galois/fonts/DejaVuSans.ttf", &fdesc, &font));
    DFBCHECK(primary->SetFont(primary, font));
}

static void Screen_Deinit(void)
{
    // TODO(14): lock mutex
    if (primary) { primary->Release(primary); primary = NULL; }
    if (dfb)     { dfb->Release(dfb); dfb = NULL; }
    // TODO(14): unlock mutex
}

static void Radio_Draw(void)
{
    if (!primary) return;
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    DFBCHECK(primary->DrawString(primary, "RADIO", -1, screenWidth/2, screenHeight/2, DSTF_CENTER));

    DFBCHECK(primary->DrawString(primary, channel_msg, -1, (2*screenWidth/3)+30, 120, DSTF_LEFT));
    primary->Flip(primary, NULL, 0);
    timer_reset();
}

static void Info_Draw(uint32_t chbtn)
{
    if (!primary) return;

    // jednostavno overlay polje gore desno
    if (chbtn > 5) {
        Radio_Draw();
        return;
    }

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, (2*screenWidth/3)-30, 30, (screenWidth/3), 140));

    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    DFBCHECK(primary->DrawString(primary, channel_msg, -1, (2*screenWidth/3)+30, 120, DSTF_LEFT));

    primary->Flip(primary, NULL, 0);
    timer_reset();
}

static void Volume_Draw(void)
{
    if (!primary) return;

    // overlay dolje
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, screenWidth/4, screenHeight-200, screenWidth/2, 140));

    // barovi
    int base = screenWidth/4 + 20;
    int step = 40;
    int barw = 35;
    for(int b=0; b<16; b++) {
        float thr = (float)(b+1) * 6.25f;
        if(volume < thr) DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        else             DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, base + b*step, screenHeight-180, barw, 90));
    }

    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    DFBCHECK(primary->DrawString(primary, volume_msg, -1, (3*screenWidth/4)-160, screenHeight-120, DSTF_CENTER));

    primary->Flip(primary, NULL, 0);
    timer_reset();
}

static void Channel_Draw(uint32_t chbtn)
{
    if (chbtn < 6) Info_Draw(chbtn);
    else           Radio_Draw();
}

/* TODO(15): Screen_Clear napravi “pametno”:
   - ne briši cijeli ekran (flicker), nego samo overlay zone:
     * gore desno (info)
     * dolje (volume)
   - ako je radio: ostavi RADIO tekst
*/
static void Screen_Clear(void)
{
    if (!primary) return;

    // TODO(15): selective clear (overlay zone)
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

    primary->Flip(primary, NULL, 0);
}

/* ---------------- Timer (SIGEV_THREAD) ---------------- */
static timer_t timerId;
static int32_t timerFlags = 0;
static struct itimerspec timerSpec;
static struct itimerspec timerSpecOld;

/* TODO(16): Ispravan timer callback:
   - napravi static void Screen_Clear_cb(union sigval sv) { (void)sv; Screen_Clear(); }
   - u timer_init postavi sigev_notify_function = Screen_Clear_cb
   - timer treba biti “one-shot 5s”, a timer_reset ga restart-a
*/
static void timer_init(void)
{
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;

    // NAMJERNO krivo (student mora popraviti)
    sev.sigev_notify_function = (void (*)(union sigval))Screen_Clear;

    timer_create(CLOCK_REALTIME, &sev, &timerId);

    memset(&timerSpec, 0, sizeof(timerSpec));
    timerSpec.it_value.tv_sec  = 5;
    timerSpec.it_value.tv_nsec = 0;
}

static void timer_deinit(void)
{
    memset(&timerSpec, 0, sizeof(timerSpec));
    timer_settime(timerId, 0, &timerSpec, &timerSpecOld);
    timer_delete(timerId);
}

static void timer_reset(void)
{
    timer_settime(timerId, timerFlags, &timerSpec, &timerSpecOld);
}

/* ---------------- Remote input ---------------- */

/* TODO(17): getKeys/remote input napravi bez busy-waita:
   - otvori /dev/input/event0 kao O_RDONLY | O_NONBLOCK
   - koristi select() s timeoutom (npr 200ms) da thread moze uredno izaci
   - čitaj evente u buffer i obradi
*/
static void Apply_Volume_And_Draw(void)
{
    if (muted) {
        Player_Volume_Set(playerHandle, 0);
        snprintf(volume_msg, sizeof(volume_msg), "MUTED");
    } else {
        Player_Volume_Set(playerHandle, volume * 10000000);
        snprintf(volume_msg, sizeof(volume_msg), "%u%%", volume);
    }
    Volume_Draw();
}

/* TODO(18): Key handling poboljšaj:
   - value==1 press: normal
   - value==2 hold/repeat: ubrzaj (npr volume +5 svaka 3 ponavljanja ili slično)
   - Debounce: ignoriraj brze duple pritiske unutar npr 50ms (za CH)
*/
static void Handle_Key(uint16_t code, int value)
{
    // prihvati press i hold
    if (!(value == 1 || value == 2)) return;

    switch(code)
    {
        case 60: // MUTE
            if (value == 1) {
                muted = !muted;
                Apply_Volume_And_Draw();
            }
            break;

        case 63: // VOL+
            if (!muted) {
                if (volume < 100) volume++;
                // TODO(18): ubrzanje na hold
                Apply_Volume_And_Draw();
            }
            break;

        case 64: // VOL-
            if (!muted) {
                if (volume > 0) volume--;
                // TODO(18): ubrzanje na hold
                Apply_Volume_And_Draw();
            }
            break;

        case 61: // CH-
            if (value == 1) {
                if (channel_btn < 3) channel_btn = 8; else channel_btn -= 1;
                // TODO(19): Na promjenu kanala osvježi streamove, poruke i overlay ispravno
                // Promjena_Kanala(channel_btn); snprintf(channel_msg,...); Channel_Draw(channel_btn);
            }
            break;

        case 62: // CH+
            if (value == 1) {
                if (channel_btn > 7) channel_btn = 2; else channel_btn += 1;
                // TODO(19): isto kao gore
            }
            break;

        case 2: case 3: case 4: case 5: case 6: case 7: case 8:
            if (value == 1) {
                channel_btn = (uint16_t)code; // 2..8
                // TODO(19): isto kao gore, + fix za kanal 7 (code 8) bez off-by-one
            }
            break;

        case 358: // INFO
            if (value == 1) Info_Draw(channel_btn);
            break;

        case 102: // EXIT
            if (value == 1) {
                runRemote = 0;
                runMain = 0;
            }
            break;
    }
}

static void *remoteThreadTask(void *arg)
{
    (void)arg;

    const char* dev = "/dev/input/event0";

    // TODO(17): O_RDONLY | O_NONBLOCK
    inputFileDesc = open(dev, O_RDONLY);
    if (inputFileDesc < 0) {
        printf("[ERROR] open(%s): %s\n", dev, strerror(errno));
        runRemote = 0;
        runMain = 0;
        return NULL;
    }

    struct input_event evbuf[16];

    while (runRemote)
    {
        // TODO(17): select() na inputFileDesc s timeoutom; ako timeout -> loop (provjeri runRemote)
        // Ako je ready -> read() koliko može i obradi sve evente

        int r = read(inputFileDesc, evbuf, sizeof(evbuf));
        if (r > 0) {
            int n = r / (int)sizeof(struct input_event);
            for (int i=0;i<n;i++) {
                if (evbuf[i].type == EV_KEY) {
                    Handle_Key((uint16_t)evbuf[i].code, evbuf[i].value);
                }
            }
        } else {
            // TODO(17): ako EAGAIN -> normalno (nema eventa), ako drugo -> error i prekid
        }
    }

    close(inputFileDesc);
    inputFileDesc = -1;
    return NULL;
}

/* TODO(20): Graceful shutdown redoslijed:
   - prije Deinit_Player_Tuner: makni streamove ako postoje
   - ugasi timer
   - Screen_Deinit tek kad vise nema timer callbacka koji crta (mutex + timer_deinit prije)
   - join remote thread
*/
int main(int argc, char **argv)
{
    Read_Config();

    if (Start_Default_Channel() != 0) {
        return -1;
    }

    if (Parse_Tables() != 0) {
        Deinit_Player_Tuner();
        return -1;
    }

    Spremanje_Kanala();

    Screen_Init(argc, argv);

    // TODO(16): timer_init mora biti tek nakon Screen_Init (da primary != NULL)
    timer_init();

    pthread_create(&remote_thread, NULL, remoteThreadTask, NULL);

    // bez busy-wait: main neka spava
    while (runMain) {
        usleep(10000);
    }

    // TODO(20): clean exit (order!)
    timer_deinit();

    // makni streamove
    Player_Stream_Remove(playerHandle, sourceHandle, streamHandleVideo);
    Player_Stream_Remove(playerHandle, sourceHandle, streamHandleAudio);

    pthread_join(remote_thread, NULL);

    Screen_Deinit();
    Deinit_Player_Tuner();
    return 0;
}
