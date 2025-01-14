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

void Screen_Deinit();
void Channel_Draw();
void timer_reset();
void timer_init();
void timer_deinit();

uint16_t channel_btn = 2;                                           //definiranje kanala da je na 2 pocetna a volumen na 50
uint32_t volume = 50;

#define DEGREES_TO_RADIANS(deg) ((deg)*M_PI / 180.0)                //za crtanje kruga za info dijalog

#define NO_ERROR    0                                               //definiranje errora 0-1
#define ERROR       1
                                                                    //definiranje Check-a i u slucaju errora ispis file-a i linij
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

static pthread_t remote;
static int32_t inputFileDesc;
static int32_t runMain = 1;
int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventRead);
void *remoteThreadTask();
 
//**************** START.C POCETAK**************************
                                                                    //-> Struktura za konfiguraciju -> podaci iz configa
struct Conf {
  int Frequency;
  int Bandwidth;
  int Module;
  int aPID;
  int vPID;
  int aType;
  int vType;
};
struct Conf Config;

static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t playerHandle = 0;                                            //Pocetne vrijednosti 0 za sve
uint32_t sourceHandle = 0;
uint32_t filterHandle = 0;
uint32_t streamHandleAudio = 0;
uint32_t streamHandleVideo = 0;

void Read_Config()                                                    // Citanje konfiguracije iz config.ini
{
    FILE *File_Pointer;
    char *Line;
    char *Data;
    int *Length;
    File_Pointer = fopen("config.ini", "r");
    while (getline(&Line, &Length, File_Pointer) != -1) 
    {        
        Data = strchr( Line, '=' ) + 1;
        if(strstr(Line, "frequency") != 0)  {  Config.Frequency = atoi(Data);  }          //atoi string value pretvara u integer
        if(strstr(Line, "bandwidth") != 0)  {  Config.Bandwidth = atoi(Data);  }
        if(strstr(Line, "module") != 0)     {  Config.Module = atoi(Data);  }
        if(strstr(Line, "aPID") != 0)       {  Config.aPID= atoi(Data);  }
        if(strstr(Line, "vPID") != 0)       {  Config.vPID = atoi(Data);  }
        if(strstr(Line, "aType") != 0)      {  Config.aType = atoi(Data);  }
        if(strstr(Line, "vType") != 0)      {  Config.vType = atoi(Data);  }
    }
    fclose(File_Pointer);
}

int32_t Tuner_Status(t_LockStatus status)
{
    if(status == STATUS_LOCKED)
    {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("\n%s -----TUNER ZAKLJUCAN-----\n",__FUNCTION__);
    }else{
        printf("\n%s -----TUNER NIJE ZAKLJUCAN-----\n",__FUNCTION__);
    }
    return 0;
}

int32_t Start_Default_Channel(){

    struct timespec lockStatusWaitTime;
	struct timeval now;
    
    gettimeofday(&now,NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec+10;
    
    Tuner_Init();
    Tuner_Register_Status_Callback(Tuner_Status);
    Tuner_Lock_To_Frequency(Config.Frequency, Config.Bandwidth, Config.Module);

    pthread_mutex_lock(&statusMutex);
    if(ETIMEDOUT == pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime))
    {
        printf("\n%s:ERROR Lock timeout exceeded!\n",__FUNCTION__);
        Tuner_Deinit();
        return -1;
    }
    pthread_mutex_unlock(&statusMutex);
   
    Player_Init(&playerHandle);
    Player_Source_Open(playerHandle, &sourceHandle);
    Player_Stream_Create(playerHandle,sourceHandle, Config.vPID, Config.vType, &streamHandleVideo);
    Player_Stream_Create(playerHandle,sourceHandle, Config.aPID, Config.aType, &streamHandleAudio);
    Player_Volume_Set(playerHandle, volume*10000000);
}

void Deinit_Player_Tuner(){
    Player_Source_Close(playerHandle, sourceHandle);
    Player_Deinit(playerHandle);
    Tuner_Deinit();
}
//****************** START.C ZAVRSETAK ******************************

// ***************** POCETAK PARSIRANJA ******************************
struct Pat{                                                      //PAT struktura sa parametrima
    uint16_t section_length;
    uint16_t transport_stream_id;
    uint16_t version_number;
    uint8_t program_count;
    uint16_t program_number[8];
    uint16_t pid[8];
};

struct Pmt{                                                     //PMT struktura s parametrima
    uint16_t program_info_length;
    uint16_t program_number;
    uint16_t section_lenght;
    uint8_t stream_count;
    uint8_t stream_type[20];
    uint16_t elementary_pid[20];
    uint16_t ES_info_length[20];
    uint16_t descriptor[20];
};

struct Chnl{                                                    //Kanal struktura sa paramterima
    uint16_t Video_pid;
    uint16_t Audio_pid;
    uint16_t Channel_number;
};

uint16_t Pmt_index = 0;
struct Pat pat_table;
struct Pmt pmt_table[8];
struct Chnl channel[7];                                         //Imamo 7 kanala 

int32_t Parse_PAT(uint8_t *buffer){                             //Parsiranje PAT-a
    int i;
    pat_table.section_length = (uint16_t) (((*(buffer+1)<<8)+*(buffer+2)) & 0x0FFF);
    pat_table.transport_stream_id = (uint16_t) ((*(buffer+3)<<8) + *(buffer+4));
    pat_table.version_number = (uint16_t) ((*(buffer+5)>>1) & 0x001F);
    pat_table.program_count = (uint8_t) ((pat_table.section_length-8)/4);
    
    for (i=0; i<pat_table.program_count; i++)
    {
        pat_table.program_number[i] = (uint16_t) ((*(buffer+8+(i*4))<<8)+*(buffer+9+(i*4)));
        pat_table.pid[i] = (uint16_t) (((*(buffer+10+(i*4)) & 0x001F)<<8) + *(buffer+11+(i*4)));
    }
    return 0;
}

int32_t Parse_PMT(uint8_t *buffer){                           //Parsiranje PMT-a
    int i;
    pmt_table[Pmt_index].program_number = (uint16_t)((*(buffer+3)<<8) + *(buffer+4));
    pmt_table[Pmt_index].program_info_length = (uint16_t)(((*(buffer+10)<<8) + *(buffer+11)) & 0x0FFF);
    pmt_table[Pmt_index].section_lenght = (uint16_t)(((*(buffer+1)<<8) + *(buffer+2)) & 0x0FFF);
    pmt_table[Pmt_index].stream_count = 10;

    uint8_t *m_buffer = (uint8_t*)(buffer) + 12 + pmt_table[Pmt_index].program_info_length;
    for(i = 0; i<pmt_table[Pmt_index].stream_count-1; ++i){
        pmt_table[Pmt_index].stream_type[i] = m_buffer[0];
        pmt_table[Pmt_index].elementary_pid[i] = (uint16_t)((*(m_buffer + 1)<<8) + *(m_buffer + 2)) & 0x1FFF;
        pmt_table[Pmt_index].ES_info_length[i] = (uint16_t)((*(m_buffer + 3)<<8) + *(m_buffer + 4)) & 0x0FFF;
        pmt_table[Pmt_index].descriptor[i] = (uint8_t)*(m_buffer+5);
        printf("%d\n", pmt_table[Pmt_index].descriptor[i]);
        m_buffer += 5 + pmt_table[Pmt_index].ES_info_length[i];
    }
    return 0;
}

int32_t Filter(uint8_t *buffer)
{
    uint32_t temp;
    if((uint32_t)(*buffer) == 0){
        temp = Parse_PAT(buffer);
    }else{
        temp = Parse_PMT(buffer);
    }
    return 0;
}

void Spremanje_Kanala(){                                    //Spremanje kanala ->audio video pidovi -> elementarni pidovi
    Pmt_index = 0;
    int i;
    for(Pmt_index=0; Pmt_index<7; Pmt_index++)
    {
        channel[Pmt_index].Channel_number = Pmt_index+1;
        for(i = 0 ; i < 10; i++)
        {
            if(pmt_table[Pmt_index].stream_type[i] == 2)
            {  channel[Pmt_index].Video_pid = pmt_table[Pmt_index].elementary_pid[i]; }

            if(pmt_table[Pmt_index].stream_type[i] == 3)
            {  channel[Pmt_index].Audio_pid = pmt_table[Pmt_index].elementary_pid[i]; }
        }
    }
}

void Parse_Tables(){
    Demux_Register_Section_Filter_Callback(Filter);
    Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);
    sleep(1);
    int i;
    for(i=1;i<pat_table.program_count;i++)
    {
        Demux_Free_Filter(playerHandle, filterHandle);
        Demux_Set_Filter(playerHandle, pat_table.pid[i], 0x02, &filterHandle);
        sleep(1);
        Pmt_index++;
    }
    Demux_Free_Filter(playerHandle, filterHandle);

    Spremanje_Kanala();                                                       //Pozivanje funkcije spremanja kanala 
}
//*************** ZAVRSENO PARSIRANJE ************************

//*************** PROMJENA KANALA POCETAK ********************

char channel_msg[10] = "Channel 1\0";
char volume_msg[6] = "50%%\0";
char ttx_msg[4];

void Promjena_Kanala(uint32_t channel_btn){                              //promjena kanala na osnovu primanja channel btn
    uint16_t Vpid;
    uint16_t Apid;
    int i = 0;
    for(i=0; i<7; i++)
    {
        if(channel[i].Channel_number == (channel_btn-1))
        {
            Vpid = channel[i].Video_pid;
            Apid = channel[i].Audio_pid;
        }
    }
    Player_Stream_Remove(playerHandle, sourceHandle, streamHandleVideo);
    Player_Stream_Remove(playerHandle, sourceHandle, streamHandleAudio);

    Player_Stream_Create(playerHandle,sourceHandle,Vpid,VIDEO_TYPE_MPEG2,&streamHandleVideo);
    Player_Stream_Create(playerHandle,sourceHandle,Apid,AUDIO_TYPE_MPEG_AUDIO,&streamHandleAudio);
}

//**************** PROMJENA KANALA ZAVRSENA ****************************



//*************** POCETAK CRTANJE :/ *******************************
static IDirectFBSurface *primary = NULL;
IDirectFB *dfbInterface = NULL;
static int screenWidth = 0;
static int screenHeight = 0;
DFBSurfaceDescription surfaceDesc;

void Screen_Init(int32_t argc, char** argv)
{
	DFBCHECK(DirectFBInit(&argc, &argv));
	DFBCHECK(DirectFBCreate(&dfbInterface));
	DFBCHECK(dfbInterface->SetCooperativeLevel(dfbInterface, DFSCL_FULLSCREEN));
	
	surfaceDesc.flags = DSDESC_CAPS;
	surfaceDesc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	DFBCHECK (dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &primary));
    DFBCHECK (primary->GetSize(primary, &screenWidth, &screenHeight));
    
    /* Clearanje ekrana prije ikakvog crtanja (black screen pravokutnik full)*/
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
	DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
    DFBCHECK(primary->SetColor(primary, 0xff, 0x00, 0x00, 0xff));

    IDirectFBFont *fontInterface = NULL;
	DFBFontDescription fontDesc;

	/* Definiranje visine fonta povecavanjem odgovarajuce zastavice i postavljanje vrijedednosti visine*/
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 60;                       //vrijednost visine (vjerojatno jos veca)

	/* Odredivanje fonta i setanje na primarni*/
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
}

void Screen_Deinit()                                      // Deinicijalizacija
{
    primary->Release(primary);
	dfbInterface->Release(dfbInterface);
}

void Radio_Draw()                                          // Crtanje za radio kanal  (5,6,7)
{
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
	DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawString(primary, "RADIO\0", -1, (1.5*screenWidth/5)+400, (1.5*screenHeight/5)+200, DSTF_CENTER));

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, (2*screenWidth/3), 30, (screenWidth/3)-40, 130));
    
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawString(primary, channel_msg, -1, (2*screenWidth/3)+45, 120, DSTF_LEFT));

    primary -> Flip(primary, NULL, 0);
    timer_reset();
}

void Info_Draw(uint32_t channel_btn)                              //Crtanje info dijaloga
{
    if(channel_btn>5)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
	    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	    DFBCHECK(primary->DrawString(primary, "RADIO\0", -1, (1.5*screenWidth/5)+400, (1.5*screenHeight/5)+200, DSTF_CENTER));
    }else{
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
	    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
    }


    //Pokusaj crtanja pravokutnika s zaobljenim rubovvima - uspjesno//
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      //jedan pravokutnik
    DFBCHECK(primary->FillRectangle(primary, (2*screenWidth/3)-20, 30, (screenWidth/3)-40, 130));
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      //drugi pravokutnik
    DFBCHECK(primary->FillRectangle(primary, (2*screenWidth/3)-40, 50, (screenWidth/3), 90));

    //*********KRUG GORE LIJEVO NA INFO DIJALOGU**********//
    int radius = 40;
    int x = (2*screenWidth/3)-20;
    int y =50;
    int startDeg = 0 ;
    int x1, y1, x2, y2, i = 0;
    x1= x + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg));
    y1= y + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg));
    for(i = startDeg +1; i <= 360; i++)
    {
        x2= x + radius / 2.3 * cos(DEGREES_TO_RADIANS(i));
        y2= y + radius / 2.3 * sin(DEGREES_TO_RADIANS(i));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, x, y, x1, y1, x2, y2));
        x1=x2;
        y1=y2;
    }
    //*****************KRUG GORE DESNO NA INFO DIJALOGU*********//
    int xx = (screenWidth)-60;
    int yy =50;
    int x11, y11, x22, y22, ii = 0;
    int startDeg2 = 0 ;
    x11= xx + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg2));
    y11= yy + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg2));
    for(ii = startDeg2 +1; ii <= 360; ii++)
    {
        x22= xx + radius / 2.3 * cos(DEGREES_TO_RADIANS(ii));
        y22= yy + radius / 2.3 * sin(DEGREES_TO_RADIANS(ii));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, xx, yy, x11, y11, x22, y22));
        x11=x22;
        y11=y22;
    }

    ///**********************KRUG DOLE LIJEVOO**********************//
    int xxx = (2*screenWidth/3)-20;
    int yyy =140;
    int x111, y111, x222, y222, iii = 0;
    int startDeg3 = 0 ;
    x111= xxx + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg3));
    y111= yyy + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg3));
    for(iii = startDeg3 +1; iii <= 360; iii++)
    {
        x222= xxx + radius / 2.3 * cos(DEGREES_TO_RADIANS(iii));
        y222= yyy + radius / 2.3 * sin(DEGREES_TO_RADIANS(iii));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, xxx, yyy, x111, y111, x222, y222));
        x111=x222;
        y111=y222;
    }

    //***********************KRUG DOLJE DESNO************************//
    int xxxx = (screenWidth)-60;
    int yyyy =140;
    int x1111, y1111, x2222, y2222, iiii = 0;
    int startDeg4 = 0 ;
    x1111= xxxx + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg4));
    y1111= yyyy + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg4));
    for(iiii = startDeg3 +1; iiii <= 360; iiii++)
    {
        x2222= xxxx + radius / 2.3 * cos(DEGREES_TO_RADIANS(iiii));
        y2222= yyyy + radius / 2.3 * sin(DEGREES_TO_RADIANS(iiii));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, xxxx, yyyy, x1111, y1111, x2222, y2222));
        x1111=x2222;
        y1111=y2222;
    }
    
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawString(primary, channel_msg, -1, (2*screenWidth/3)+45, 120, DSTF_LEFT));
	
    
    DFBCHECK(primary->Flip(primary, NULL, 0));
    timer_reset();
}

void Channel_Draw(uint32_t channel_btn)                      // Ovisno o btn koji je kanal tako crta (video-radio)    
{
    if(channel_btn<6)
    {
        Info_Draw(channel_btn);
    }else{
        Radio_Draw();
    }
}

void Volume_Draw()                                       // Crtanje Volume dijaloga 
{    
     if(channel_btn>5)
     {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
	    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	    DFBCHECK(primary->DrawString(primary, "RADIO\0", -1, (1.5*screenWidth/5)+400, (1.5*screenHeight/5)+200, DSTF_CENTER));
    }else{
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
	    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
    }


    //***************************************************************************************************//
    //*********************************TEST DIJALOG OKRUZEN*********************************************//

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4), 870, (screenWidth/2), 140));
    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
    DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)-20, 890, (screenWidth/2)+40, 100));

    //*********KRUG GORE LIJEVO NA VOLUME DIJALOGU**********//
    int radius = 40;
    int c = (1*screenWidth/4);
    int v =890;
    int startDeg5 = 0 ;
    int c1, v1, c2, v2, k = 0;
    c1= c + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg5));
    v1= v + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg5));
    for(k = startDeg5 +1; k<= 360; k++)
    {
        c2= c + radius / 2.3 * cos(DEGREES_TO_RADIANS(k));
        v2= v + radius / 2.3 * sin(DEGREES_TO_RADIANS(k));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, c, v, c1, v1, c2, v2));
        c1=c2;
        v1=v2;
    }
    //**************KRUG DOLE LIJEVO NA VOLUME**************************//

    int cc = (1*screenWidth/4);
    int vv =990;
    int startDeg6 = 0 ;
    int c11, v11, v22, c22, kk = 0;
    c11= cc + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg6));
    v11= vv + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg6));
    for(kk = startDeg6 +1; kk <= 360; kk++)
    {
        c22= cc + radius / 2.3 * cos(DEGREES_TO_RADIANS(kk));
        v22= vv + radius / 2.3 * sin(DEGREES_TO_RADIANS(kk));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, cc, vv, c11, v11, c22, v22));
        c11=c22;
        v11=v22;
    }

    //**************KRUG GORE DESNO NA VOLUME**************************//

    int ccc = (3*screenWidth/4);
    int vvv =890;
    int startDeg7 = 0 ;
    int c111, v111, c222, v222, kkk = 0;
    c111= ccc + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg7));
    v111= vvv + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg7));
    for(kkk = startDeg7 +1; kkk <= 360; kkk++)
    {
        c222= ccc + radius / 2.3 * cos(DEGREES_TO_RADIANS(kkk));
        v222= vvv + radius / 2.3 * sin(DEGREES_TO_RADIANS(kkk));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, ccc, vvv, c111, v111, c222, v222));
        c111=c222;
        v111=v222;
    }

     //**************KRUG DOLE DESNO NA VOLUME**************************//

    int cccc = (3*screenWidth/4);
    int vvvv =990;
    int startDeg8 = 0 ;
    int c1111, v1111, c2222, v2222, kkkk = 0;
    c1111= cccc + radius / 2 * cos(DEGREES_TO_RADIANS(startDeg8));
    v1111= vvvv + radius / 2 * sin(DEGREES_TO_RADIANS(startDeg8));
    for(kkkk = startDeg8 +1; kkkk <= 360; kkkk++)
    {
        c2222= cccc + radius / 2.3 * cos(DEGREES_TO_RADIANS(kkkk));
        v2222= vvvv + radius / 2.3 * sin(DEGREES_TO_RADIANS(kkkk));
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));                                      
        DFBCHECK(primary->FillTriangle(primary, cccc, vvvv, c1111, v1111, c2222, v2222));
        c1111=c2222;
        v1111=v2222;
    }

    //OVJDE DALJE

    //**************************************************************************************************//
    //*****************TEST DO OVDJE UPITNO *******************************************************//

    

    if(volume < 6.25)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+20,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+20,  890, 35, 100));
    }

    if(volume < 12.5)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+60,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+60,  890, 35, 100));
    }

    if(volume < 18.75)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+100,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+100,  890, 35, 100));
    }

    if(volume < 25)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+140,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+140,  890, 35, 100));
    }  
    
    if(volume < 31.25)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+180,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+180,  890, 35, 100));
    }

    if(volume < 37.5)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+220,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+220,  890, 35, 100));
    }

    if(volume < 43.75)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+260,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+260,  890, 35, 100));
    }
    
    if(volume < 50)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+300,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+300,  890, 35, 100));
    }

    if(volume < 56.25)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+340,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+340,  890, 35, 100));
    }

    if(volume < 62.5)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+380,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+380,  890, 35, 100));
    }

    if(volume < 68.75)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+420,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+420,  890, 35, 100));
    }

    if(volume < 75)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+460,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+460,  890, 35, 100));
    }

    if(volume < 81.25)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+500,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+500,  890, 35, 100));
    }

    if(volume < 87.5)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+540,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+540,  890, 35, 100));
    }

    if(volume < 93.75)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+580,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+580,  890, 35, 100));
    }

    if(volume < 100)
    {
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+620,  890, 35, 100)); 
    }else{
        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
        DFBCHECK(primary->FillRectangle(primary, (1*screenWidth/4)+620,  890, 35, 100));
    }

    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawString(primary, volume_msg, -1, (3*screenWidth/4)-160, 960, DSTF_CENTER));

    DFBCHECK(primary->Flip(primary, NULL, 0));
    timer_reset();
}

void Screen_Clear()
{
    if(channel_btn>5){
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0xff));
	    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));

        DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	    DFBCHECK(primary->DrawString(primary, "RADIO\0", -1, (1.5*screenWidth/5)+400, (1.5*screenHeight/5)+200, DSTF_CENTER));
    }else{
        DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
	    DFBCHECK(primary->FillRectangle(primary, 0, 0, screenWidth, screenHeight));
    }
    primary -> Flip(primary, NULL, 0);
}
//********************** ZAVRSETAK CRTANJA **********************


//********************* REMOTE POCETAK *************************

int toggle = 0;
int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead)
{
    int32_t ret = 0;
    /* Cita input evente i stavlja ih u buffer*/
    ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0)
    {
        printf("Error code %d", ret);
        return ERROR;
    }
    /* Racuna broj procitanih eventa */
    *eventsRead = ret / (int)sizeof(struct input_event);
    return NO_ERROR;
}

void *remoteThreadTask(){
    const char* dev = "/dev/input/event0";
    char deviceName[20];
    struct input_event* eventBuf;
    uint32_t eventCnt;
    uint32_t i;
    uint32_t runThread = 1;
    
    inputFileDesc = open(dev, O_RDWR);
    if(inputFileDesc == -1)
    {
        printf("Error while opening device (%s) !", strerror(errno));
    }
    
    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
	printf("RC device opened succesfully [%s]\n", deviceName);
    
    eventBuf = malloc(5 * sizeof(struct input_event));
    if(!eventBuf)
    {
        printf("Error allocating memory !");
    }

    while(runThread)
    {
        /* read input eventS */
        if(getKeys(5, (uint8_t*)eventBuf, &eventCnt))
        {
			printf("Error while reading input events !");
		}

        for(i = 0; i < eventCnt; i++)
        {
            if(eventBuf[i].type == 1 && (eventBuf[i].value == 1 || eventBuf[i].value == 2)){
                switch(eventBuf[i].code){
                    case 60:{                                                                    //*****MUTE*****
                        if(toggle == 0)
                        {
                            toggle = 1;
                            Player_Volume_Set(playerHandle, 0);
                            printf("Ton: 0\n");
                            sprintf(volume_msg, "MUTED\0", volume);
                            Volume_Draw();
                        }else if(toggle == 1){
                            toggle = 0;
                            Player_Volume_Set(playerHandle, volume*10000000);
                            printf("Ton: %d\n",volume);
                            sprintf(volume_msg, "%d%%\0", volume);
                            Volume_Draw();
                        }
                        break;
                    }
                    case 63:{                                                                  //*****VOLUME PLUS******
                        if(volume < 100)
                        {
                            volume=volume+1;
                        }
                        Player_Volume_Set(playerHandle, volume*10000000);
                        printf("Ton: %d\n",volume);
                        sprintf(volume_msg, "%d%%\0", volume);
                        Volume_Draw();
                        break;
                    }
                    case 64:{                                                                  //*****VOLUME MINUS******
                        if(volume > 0)
                        {
                            volume=volume-1;
                        }
                        Player_Volume_Set(playerHandle, volume*10000000);
                        printf("Ton: %d\n",volume);
                        sprintf(volume_msg, "%d%%\0", volume);
                        Volume_Draw();
                        break;
                    }
                    case 61:{                                                                   //*****CHANNEL MINUS*****
                        if(channel_btn < 3)
                        {
                            channel_btn = 8;
                        }else{
                            channel_btn -= 1;
                        }
                        Promjena_Kanala(channel_btn);                                            
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 62:{                                                                   //*****CHANNEL PLUS******
                        if(channel_btn > 7)
                        {
                            channel_btn = 2;
                        }else{
                            channel_btn += 1;
                        }
                        Promjena_Kanala(channel_btn);                                
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 2:{                                                                    //**** KANAL 1 *****
                        channel_btn = 2;
                        Promjena_Kanala(channel_btn);                                
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 3:{                                                                    //**** KANAL 2 *****
                        channel_btn = 3;
                        Promjena_Kanala(channel_btn);                                
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 4:{                                                                    //**** KANAL 3 *****
                        channel_btn = 4;
                        Promjena_Kanala(channel_btn);                                
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 5:{                                                                    //**** KANAL 4 *****
                        channel_btn = 5;    
                        Promjena_Kanala(channel_btn);                                                
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 6:{                                                                    //**** KANAL 5 *****
                        channel_btn = 6;
                        Promjena_Kanala(channel_btn);                                            
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 7:{                                                                    //**** KANAL 6 *****
                        channel_btn = 7;
                        Promjena_Kanala(channel_btn);                                               
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 8:{                                                                    //**** KANAL 7 *****
                        channel_btn = 8;
                        Promjena_Kanala(channel_btn-2);                                      
                        printf("Program: %d\n", channel_btn-1);
                        sprintf(channel_msg, "Channel %d\0", channel_btn-1);
                        Channel_Draw(channel_btn);
                        break;
                    }
                    case 102:{                                                                   //**** EXIT *****
                        printf("Exit!\n");
                        Player_Stream_Remove(playerHandle, sourceHandle, streamHandleVideo);
                        Player_Stream_Remove(playerHandle, sourceHandle, streamHandleAudio);
                        Screen_Deinit();                                                         
                        Deinit_Player_Tuner();
                        timer_deinit();
                        runMain = 0;
                        runThread = 0;
                        break;
                    }
                    case 358:{                                                                     //**** INFO *****
                        printf("Info!\n");
                        Info_Draw(channel_btn);
                        break;
                    }
                }
            }
        }
    }
    free(eventBuf);
}
//********************************* REMOTE ZAVRSEN ******************************


//********************************* TIMER POCETAK *******************************
timer_t timerId;
int32_t timerFlags = 0;
struct itimerspec timerSpec;
struct itimerspec timerSpecOld;

void timer_init(){
    struct sigevent signalEvent;
    signalEvent.sigev_notify = SIGEV_THREAD;
    signalEvent.sigev_notify_function = Screen_Clear;
    signalEvent.sigev_value.sival_ptr = NULL;
    signalEvent.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &signalEvent, &timerId);
    memset(&timerSpec,0,sizeof(timerSpec));
    timerSpec.it_value.tv_sec = 5; 
    timerSpec.it_value.tv_nsec = 0;
}

void timer_deinit(){
    memset(&timerSpec,0,sizeof(timerSpec));
    timer_settime(timerId,0,&timerSpec,&timerSpecOld);
    timer_delete(timerId);
}

void timer_reset(){
    timer_settime(timerId,timerFlags,&timerSpec,&timerSpecOld);
}
//****************************** TIMER KRAJ******************************************

int32_t main(int32_t argc, char** argv)
{   
    timer_init();
    Read_Config();
    Start_Default_Channel();
    Parse_Tables();
    Screen_Init(argc, argv);
    pthread_create(&remote, NULL, &remoteThreadTask, NULL);
    while(runMain);
    pthread_join(remote, NULL);
    return 0;
}