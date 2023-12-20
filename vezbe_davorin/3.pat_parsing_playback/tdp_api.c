/********************************************************
*
* FILE NAME: $URL$  tdp_api.c
*            $Date$
*            $Rev$
*
* DESCRIPTION
*
* - brief description of the file -
*
*********************************************************/
/********************************************************/
/*                 Includes                             */
/********************************************************/
#include "tdp_api.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>

#ifdef SATELITE
#include "cimax.h"
#include "mt_fe_def.h"
#endif
#include "gpio_common.h"

#include <stdio.h>
#include <OSAL_api.h>
#include <pe_api.h>

#include "i2c.h"

#include "drvi2c_feusb.h"
#include "sony_dvb_cxd2820.h"
#include "sony_dvb_demod_monitor.h"
#include "sony_dvb_demod_monitorT.h"
#include "sony_dvb_demod_monitorT2.h"
#include "sony_dvb_demod_monitorC.h"

#include "nutune_FT3114.h"

/********************************************************/
/*                 Defines                              */
/********************************************************/
#define DEFAULT_SYMBRATE 22000
#define BANDLOW  10700
#define BANDHIGH 12750
#define GPIO_POLARIZATION 1
#define GPIO_LINEDROP 2
#define GPIO_BAND 4
#define MY_READING_BUFF_SIZE (188*32)
#define MY_INPUT_TS_BUFFER_SIZE (2*1024*1024)
#define INPUT_TS_BUFFER_SIZE_MAIN		(4*1024*1024)
#define MY_SECTION_MAX_SIZE (4096+3)
#define MY_SECTION_BUFFER_SIZE (MY_SECTION_MAX_SIZE*8)
#define SECTION_SIZE_INFO                 3
#define CRC32_MAX_COEFFICIENTS 256
#define CRC32_POLYNOMIAL           0x04C11DB7
#define MAX_FILTER_NUMBER 7 
#define DVB_T2_ON
//#define NuTune_Tuner

/********************************************************/
/*                 Macros                               */
/********************************************************/
/********************************************************/
/*                 Typedefs                             */
/********************************************************/
/********************************************************/
/*                 Global Variables                     */
/********************************************************/
/********************************************************/
/*                 Local Module Variables               */
/********************************************************/
/********************************************************/
/*                 Local File Variables                 */
/********************************************************/
HANDLE hPE = 0;
UINT32 hSource = 0;
UINT32 hStreamA = 0;
UINT32 hStreamV = 0;
UINT32 hFilter = 0;
UINT32 hBuffer;

sony_dvb_tuner_t tuner;
sony_dvb_i2c_t tunerI2C ;

sony_dvb_demod_t demod;
sony_dvb_cxd2820_t cxd2820;
sony_dvb_i2c_t demodI2C ;

uint32_t gAttenuateFreq = 0;
uint32_t LowLNBFreq = 9750;
uint32_t HighLNBFreq = 10600;
uint32_t InitDone = 0;
int32_t tdt = 0;

pthread_mutex_t section_mutex;
pthread_t tune_check_thread;

Tuner_Status_Callback TunerStatusCallback = NULL;
Demux_Section_Filter_Callback DemuxSectionFilterCallback = NULL;

/********************************************************/
/*                 Local Functions Declarations         */
/********************************************************/
uint32_t AttenuateFreq(uint32_t freg);
uint32_t set_pinmux(int owner, int group, int value);
void p_writeChecksum(uint8_t * const checksum, uint8_t const * block, size_t const length);
int setPrimaryFilter(void);

#ifdef SATELITE
void Tuner_NotificationCallback(MT_FE_MSG msg, void *p_tp_info);
int32_t myPrivateTunerStatusCallback(t_LockStatus status);
#endif
int32_t myPrivateDemuxSectionCallback(uint8_t *buffer);
int32_t my_sec_filter_callback(uint8_t *buffer);
void* myTuneStatusCheckThread(void *);
int DMDi_Change_Config(int edge);

HRESULT m_sectionReceivedCallback(UINT32 EventCode, void *EventInfo, void *Context);

#ifdef SATELITE
void _mt_sleep(U32 ms) {
    usleep(ms);
}

/********************************************************/
/*                 Global Functions Declarations         */
/********************************************************/
extern int32 CIMAX_ResetPatch();
#endif

/********************************************************/
/*                 Functions Definitions                */
/********************************************************/

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Init()
{
    I2C_Init();
#ifdef SATELITE
    MT_FE_RET ret = MtFeErr_Ok;
    
    I2C_Init();
    
    set_pinmux(0, 3, 3);
	usleep(500);
	set_pinmux(0, 6, 1);
	usleep(500);
	set_pinmux(0, 7, 1);
	usleep(500);
	set_pinmux(0, 8, 1);
	usleep(500);
	set_pinmux(0, 9, 1);
	usleep(500);
	set_pinmux(0, 18, 1);
	usleep(500);
	set_pinmux(0, 28, 0);
	usleep(500);
    
    ret = mt_fe_dmd_ds3k_init();
    
    if (ret != MtFeErr_Ok) 
    {
        printf("\n\nTuner initialization failed\n\n");
        return -1;
    }
    printf("\n\nTuner initialization OK\n\n");
    
    CIMAX_Init(NULL, NULL);
    InitDone = 1;
    return 0;
#else

    sony_dvb_result_t result = SONY_DVB_OK ;

    /* Setup I2C interfaces. */
    tunerI2C.gwAddress = SONY_DVB_CXD2820_DVBT_ADDRESS ;
    tunerI2C.gwSub = 0x09;  /* Connected via demod I2C gateway function. */
    tunerI2C.Read = drvi2c_feusb_ReadGw ;
    tunerI2C.Write = drvi2c_feusb_WriteGw ;
    tunerI2C.ReadRegister = dvb_i2c_CommonReadRegister;
    tunerI2C.WriteRegister = dvb_i2c_CommonWriteRegister;
    tunerI2C.WriteOneRegister = dvb_i2c_CommonWriteOneRegister;
    tunerI2C.user = NULL ;


    /* Setup demod I2C interfaces. */
    demodI2C.gwAddress = 0x00 ;
    demodI2C.gwSub = 0x00;  /* N/A */
    demodI2C.Read = drvi2c_feusb_Read ;
    demodI2C.Write = drvi2c_feusb_Write ;
    demodI2C.ReadRegister = dvb_i2c_CommonReadRegister;
    demodI2C.WriteRegister = dvb_i2c_CommonWriteRegister;
    demodI2C.WriteOneRegister = dvb_i2c_CommonWriteOneRegister;
    demodI2C.user = NULL ;

    demod.is_ascot = 0;

	printf("\n***************1******************\n");
	result = nutune_ft3114_Create (SONY_DVB_TUNER_NUTUNE_FT3114_ADDRESS,
                                      4,
                                      &tunerI2C,
                                      0,
                                      0,
                                      &tuner);


 	result =  dvb_cxd2820_Create (SONY_DVB_CXD2820_DVBT_ADDRESS,
                                  SONY_DVB_CXD2820_DVBC_ADDRESS,
                                  SONY_DVB_CXD2820_DVBT2_ADDRESS,
                                 &demodI2C, 
                                 &demod, 
                                 &tuner, 
                                 &cxd2820);
	printf("\n***************2******************\n");
	if(SONY_DVB_OK != result)
	{
		perror("Open Frontend ERROR: ");
    	return -1;
	}


	printf("\n***************4******************\n");
	result = dvb_cxd2820_Initialize(&cxd2820);
	if (result != SONY_DVB_OK) {
		perror("Open Frontend ERROR: ");
    	return -1;
   }
	cxd2820.pDemod->system = SONY_DVB_SYSTEM_UNKNOWN;


#ifdef NuTune_Tuner

	result = dvb_demod_SetConfig (&demod, DEMOD_CONFIG_PARALLEL_SEL, 0);/* 1 is default */

#endif


	demod.iffreq_config.config_DVBT6 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT (4.0);
	demod.iffreq_config.config_DVBT7 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT (4.5);
	demod.iffreq_config.config_DVBT8 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT (5.0);
	demod.iffreq_config.config_DVBC = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBC (5.0);

	/* T2 ITB setup. */
	demod.iffreq_config.config_DVBT2_5 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT2 (4.0);
	demod.iffreq_config.config_DVBT2_6 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT2 (4.0);
	demod.iffreq_config.config_DVBT2_7 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT2 (4.5);
	demod.iffreq_config.config_DVBT2_8 = DVB_DEMOD_MAKE_IFFREQ_CONFIG_DVBT2 (5.0); 

//For DVBT2
#ifdef DVB_T2_ON
	result = dvb_demod_SetConfig (&demod, DEMOD_CONFIG_DVBT2_PP2_LONG_ECHO, 1);
	if (result != SONY_DVB_OK) 
	{        	
		printf("[%s]dvb_demod_SetConfig DEMOD_CONFIG_DVBT2_PP2_LONG_ECHO failed!unit:\n",__FUNCTION__);        	
	}
#ifdef NuTune_Tuner
	result = dvb_demod_SetConfig (&demod,DEMOD_CONFIG_LATCH_ON_POSEDGE, 0);
	if (result != SONY_DVB_OK) 
	{        	
		printf("[%s]dvb_demod_SetConfig 2 failed!unit:\n",__FUNCTION__);        	
	}
#endif

#else

#ifdef NuTune_Tuner
result = dvb_demod_SetConfig (&demod,DEMOD_CONFIG_LATCH_ON_POSEDGE, 1);
	if (result != SONY_DVB_OK) 
	{        	
		printf(kTBOX_NIV_CRITICAL,"[%s]dvb_demod_SetConfig 2 failed!unit:\n",__FUNCTION__);        	
	}
#endif

#endif
   InitDone = 1;
   return 0;

#endif
}

#ifdef SATELITE
/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Lock_To_Frequency(uint32_t tuneFrequency, t_Polarization polarization, t_Band band, uint32_t symbolRate)
{
    MT_FE_RET ret = MtFeErr_Ok;
    MT_FE_LOCK_STATE lock_state = MtFeLockState_Locked;
    MT_FE_RET eMtErrorCode = MtFeErr_Ok;
    int err = 0;
    
    set_gpio(GPIO_LINEDROP, 1);
    set_gpio(GPIO_POLARIZATION, polarization == POLAR_HORIZONTAL ? 1 : 0);
    set_gpio(GPIO_BAND, band == SPECTRUM_INVERTED ? 1 : 0);
    mt_fe_dmd_ds3k_set_LNB(TRUE,
              band == SPECTRUM_NORMAL ? 0 : 1,
              polarization == POLAR_HORIZONTAL ? MtFeLNB_18V : MtFeLNB_13V, 
              0);
    
    printf("\n\nfrequency=%u, converted=%u\n\n",tuneFrequency,AttenuateFreq(tuneFrequency));
    
    ret = mt_fe_dmd_ds3k_connect(AttenuateFreq(tuneFrequency),
            symbolRate,
            MtFeType_DvbS_Unknown);
    if (ret != MtFeErr_Ok) 
    {
        printf("\n\nCould not connect\n\n");
        return -1;
    }
    
    pthread_create( &tune_check_thread, NULL, myTuneStatusCheckThread, NULL );

    return 0;
}
#else
/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Lock_To_Frequency(uint32_t tuneFrequency, uint32_t bandwidth, t_Module modul)
{
    sony_dvb_result_t result = SONY_DVB_OK ;
    int edgeConfigStatus = 0;
    sony_dvb_tune_params_t tuneParams ;


    if(modul == DVB_T)
    {
        edgeConfigStatus = DMDi_Change_Config(1);
        if(edgeConfigStatus < 0)
        {
            return -1;
        }
        result = dvb_cxd2820_BlindTune(&cxd2820, SONY_DVB_SYSTEM_DVBT, tuneFrequency/1000, bandwidth);
        if (result != SONY_DVB_OK) 
        {
            printf("Error: Unable to Tune DVB Auto %d  (status=%d)\r\n",tuneFrequency/1000, result);
            return -1;
        }
    }
    else
    {
        edgeConfigStatus = DMDi_Change_Config(0);
        if(edgeConfigStatus < 0)
        {
            return -1;
        }
        result = dvb_cxd2820_BlindTune(&cxd2820, SONY_DVB_SYSTEM_DVBT2, tuneFrequency/1000, bandwidth);
        if (result != SONY_DVB_OK) 
        {
            printf("Error: Unable to Tune DVB T2 Auto %d  (status=%d)\r\n",tuneFrequency/1000, result);
            return -1;
        }
     
    }
    
    pthread_create( &tune_check_thread, NULL, myTuneStatusCheckThread, NULL );

    if (result != SONY_DVB_OK) 
    {
        return -1;
    }
    return 0;
}
#endif

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Deinit()
{
#ifdef SATELITE
    MT_FE_RET ret = MtFeErr_Ok;
    printf("\n\nTuner Terminate\n\n");

    if (InitDone == 1)
    {
        printf("\n\nBegin deinit\n\n");
        ret = mt_fe_dmd_ds3k_soft_reset();
        if(ret!=MtFeErr_Ok)
        {
            printf("\n\nSoft Reset Problem\n\n");
        }
        ret= mt_fe_dmd_ds3k_hard_reset();
        if(ret!=MtFeErr_Ok)
        {
            printf("\n\nHard Reset Problem\n\n");
        }
        I2C_Close();
        CIMAX_Term();
        InitDone = 0;
        return 0;
    }
    printf("\n\nTuner not initialized, cannot deinit\n\n");
    return -1;
#else
    if (InitDone == 1)
    {
        return 0;
    }
    printf("\n\nTuner not initialized, cannot deinit\n\n");
    return -1;
#endif

}

uint32_t AttenuateFreq(uint32_t freg) 
{

    if (freg >= BANDLOW) {
        gAttenuateFreq = (freg > (BANDLOW + BANDHIGH) / 2) ? HighLNBFreq : LowLNBFreq;
        freg -= gAttenuateFreq;

    } else {
        gAttenuateFreq = 0;
    }

    return freg;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
HRESULT m_sectionReceivedCallback(UINT32 EventCode, void *EventInfo, void *Context)
{

    UINT32 hFilterCallback    = *((UINT32*)EventInfo);
    UINT8 *pBuffer; 
    UINT32 sectionSize,Fullness;
    UINT32   checksum;
    HRESULT hr;
    pthread_mutex_lock(&section_mutex);
  
    pBuffer =  malloc(MY_SECTION_BUFFER_SIZE);
    hr = MV_PE_StreamBufGetFullness(hPE, hBuffer, &Fullness);
    if(hr != S_OK)
    {
        if(pBuffer != NULL)
        {
            free(pBuffer);
            pBuffer = NULL;
        }
        pthread_mutex_unlock(&section_mutex);
        return E_FAIL;
    }
    else
    {
        if(Fullness <= 0)
        {
            printf("\n\nm_sectionReceivedCallback: Fullness is 0\n\n");
            if(pBuffer != NULL)
            {
                free(pBuffer);
                pBuffer = NULL;
            }
            pthread_mutex_unlock(&section_mutex);
            return E_FAIL;
        }
    }

    if(Fullness >= SECTION_SIZE_INFO)
    {
        hr = MV_PE_StreamBufRead(hPE,
                                                 hBuffer,
                                                 pBuffer,
                                                 SECTION_SIZE_INFO);
        if(hr != S_OK)
        {
            printf("\n\nm_sectionReceivedCallback: Error in MV_PE_StreamBufRead1\n\n");
            if(pBuffer != NULL)
            {
                free(pBuffer);
                pBuffer = NULL;
            }
            pthread_mutex_unlock(&section_mutex);
            return E_FAIL;
        }

       sectionSize = (uint16_t) (pBuffer[2] | ((pBuffer[1] & 0x0F) << 8));

       hr = MV_PE_StreamBufRead(hPE,
                                                    hBuffer,
                                                    &pBuffer[3],
                                                    sectionSize);
       if(hr != S_OK)
       {
           hr = MV_PE_StreamBufGetFullness(hPE, hBuffer, &Fullness);
           printf("\nFullnes %d\n",Fullness);
           hr = MV_PE_StreamBufRead(hPE,
                                                    hBuffer,
                                                    NULL,
                                                    Fullness);
           hr = MV_PE_StreamBufGetFullness(hPE, hBuffer, &Fullness);
           printf("\n\nFullnes after read %d\n\n",Fullness);
           printf("\n\nm_sectionReceivedCallback: Error in MV_PE_StreamBufRead2 %d buf 0 %x buf 1 %x buf 2 %x\n\n",sectionSize,pBuffer[0],pBuffer[1],pBuffer[2]);
            if(pBuffer != NULL)
            {
               free(pBuffer);
               pBuffer = NULL;
            }
            pthread_mutex_unlock(&section_mutex);
            return E_FAIL;
       }
       
       if(pBuffer[0]!=0x70)
       {
           p_writeChecksum((uint8_t*) &checksum,
                                        pBuffer,
                                        sectionSize + SECTION_SIZE_INFO);
        }
        else
        {
            checksum = 0;
        }
        if(checksum)
        {
            printf("\n\nCheckusm problem Buffer %x %x %x %x %x \n\n",pBuffer[0],pBuffer[1],pBuffer[2],pBuffer[3],pBuffer[4]); 
            if(pBuffer != NULL)
            {
               free(pBuffer);
               pBuffer = NULL;
            }
       }
       else
       {
            //printf("\nOK Buffer %x %x %x %x %x \n",pBuffer[0],pBuffer[1],pBuffer[2],pBuffer[3],pBuffer[4]); 

            if(NULL != DemuxSectionFilterCallback)
            {
                DemuxSectionFilterCallback(pBuffer);
            }

            if(pBuffer != NULL)
            {
                free(pBuffer);
                pBuffer = NULL;
            }
        }
    }
    pthread_mutex_unlock(&section_mutex);
    return S_OK;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
void p_writeChecksum(uint8_t * const checksum, uint8_t const * block, size_t const length)
{
    static crc32_table[ CRC32_MAX_COEFFICIENTS ] = {1};

    if (crc32_table[0])
    {
        int loopCntCoef, loopCntBit;

        for (loopCntCoef = 0; loopCntCoef < CRC32_MAX_COEFFICIENTS; loopCntCoef++)
        {
            uint32_t coef32 = (uint32_t) loopCntCoef << 24;

            for (loopCntBit = 0; loopCntBit < 8; loopCntBit++)
                if (coef32 & 0x80000000)
                    coef32 = ((coef32 << 1) ^ CRC32_POLYNOMIAL);
                else
                    coef32 <<= 1;

            crc32_table[loopCntCoef] = coef32;
        }
    }

    uint8_t const * const end = block + length;

    uint32_t crc32 = 0xFFFFFFFF;

    for (; block < end; block++)
        crc32 = (crc32 << 8) ^ crc32_table[ ((crc32 >> 24) ^ *block) & 0x000000FF ];

    checksum[0] = (uint8_t) (crc32 >> 24);
    checksum[1] = (uint8_t) (crc32 >> 16);
    checksum[2] = (uint8_t) (crc32 >> 8);
    checksum[3] = (uint8_t) (crc32);
}

#if 0
/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
int setPrimaryFilter(void)
{
    HRESULT hr;
	MV_PE_SECTION_FILTER_PARAM secParam;
    printf("\nsetPrimaryFilter\n");
    int i = 0;
    for(i = 0; i< 3; i++)
    {
       hr = MV_PE_StreamBufAllocate(hPE, MY_SECTION_BUFFER_SIZE, &hBuffer[i]);
        if(S_OK != hr)
        {
            printf("%s(%d): failed to allocate section buffer!\n", __FUNCTION__, __LINE__);
            return -1;
        }

        /* 'Universal' filter setup */

        memset(secParam.Match,   0x00, MV_PE_MAX_FILTER_LENGTH);
        memset(secParam.Mask,    0x00,MV_PE_MAX_FILTER_LENGTH);
        memset(secParam.NotMask, 0x00, MV_PE_MAX_FILTER_LENGTH);

        switch(i)
        {
            case 0:
            secParam.Pid = 0x11;
            secParam.Match[0]=0x42;
            secParam.Mask[0] = 0xff;
            break;
            case 1:
            secParam.Pid = 0x14;
            secParam.Match[0]=0x70;
            secParam.Mask[0] = 0xff;
            break;
            case 2:
            secParam.Pid = 0x00;
            secParam.Match[0]=0x00;
            secParam.Mask[0] = 0xff;
            break;
        }

        printf("\nIndex %d %d\n",i,secParam.Pid);
        hr = MV_PE_SourceAddSectionFilter(hPE, hSource, &secParam, (HANDLE)hBuffer[i], 0, &hFilter[i]);
        if(S_OK != hr)
        {
            printf("%s(%d): MV_PE_SourceAddSectionFilte!\n", __FUNCTION__, __LINE__);
            hr = MV_PE_StreamBufRelease(hPE, hBuffer[i]);
            return -1;
        }
    }
    return 0;
}
#endif

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Register_Status_Callback(Tuner_Status_Callback tunerStatusCallback){
    if(NULL != TunerStatusCallback)
    {
        printf("%s(%d): Tuner_Register_Status_Callback failed, callback already registered!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    TunerStatusCallback = tunerStatusCallback;
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Unregister_Status_Callback(Tuner_Status_Callback tunerStatusCallback){
    if(NULL == TunerStatusCallback)
    {
        printf("%s(%d): Tuner_Unregister_Status_Callback failed, callback already unregistered!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    if(TunerStatusCallback != tunerStatusCallback)
    {
        printf("%s(%d): Tuner_Unregister_Status_Callback failed, wrong callback function!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    TunerStatusCallback = NULL;
    return 0;
}

#ifdef SATELITE
/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
void Tuner_NotificationCallback(MT_FE_MSG msg, void *p_tp_info){
    if(msg == MtFeMsg_BSTpLocked)
    {
        CIMAX_ResetPatch();
        if(NULL != TunerStatusCallback)
        {
            TunerStatusCallback(STATUS_LOCKED);
        }
        printf("\n\nLOCKED to frequency!!!\n\n");
    }
    else
    {
        if(NULL != TunerStatusCallback)
        {
            TunerStatusCallback(STATUS_ERROR);
        }
        printf("\n\nNOT locked to frequency!!!\n\n");
    }
    
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
void* myTuneStatusCheckThread(void *arg)
{
    MT_FE_RET eMtErrorCode = MtFeErr_Ok;
    MT_FE_LOCK_STATE lock_state = MtFeLockState_Locked;
    uint32_t i = 0;
    for(i = 0; i < 60; i++)
    {
        eMtErrorCode = mt_fe_dmd_ds3k_get_lock_state(&lock_state);
    
        if (eMtErrorCode != MtFeErr_Ok) 
        {
            printf("\n\nGet lock state failed\n\n");
            Tuner_NotificationCallback(MtFeMsg_BSTpUnlock, NULL);
            return NULL;
        }

        if (lock_state == MtFeLockState_Locked) 
        {
            Tuner_NotificationCallback(MtFeMsg_BSTpLocked, NULL);
            return NULL;
        }
        else
        {
            usleep(50000); //50ms
        }
    }
    Tuner_NotificationCallback(MtFeMsg_BSTpUnlock, NULL);
    return NULL;
}
#else
/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
void* myTuneStatusCheckThread(void *arg)
{
    sony_dvb_result_t result = SONY_DVB_OK ;
    
    result = dvb_cxd2820_WaitTSLock(&cxd2820);

    if (result != SONY_DVB_OK) 
    {
        if(NULL != TunerStatusCallback)
        {
            TunerStatusCallback(STATUS_ERROR);
        }
    }else
    {
        if(NULL != TunerStatusCallback)
        {
            TunerStatusCallback(STATUS_LOCKED);
        }
    }
    
    return NULL;
}

#endif

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Init(uint32_t *playerHandle)
{
    HRESULT rc;
    
    if(NULL == playerHandle)
    {
        printf("\n%s failed, playerHandle is NULL\n", __FUNCTION__);
        return -1;
    }
    
    MV_OSAL_Init();
    
	MV_OSAL_Task_Sleep(1);
    
    /* Initialize player */
    {
        /* Init a PE handle before do any PE API calls */
        rc = MV_PE_Init(&hPE);
        if(rc != S_OK)
        {
            /* Deinit and exit */
            printf("Fail to init PE, exit!\n");
            return -1;
        }
        
        rc = MV_PE_ClearScreen(hPE, MV_PE_CHANNEL_PRIMARY_VIDEO);
        if(rc != S_OK)
        {
            /* Deinit and exit */
            printf("Fail to clear screen, exit!\n");
            return -1;
        }
	}
    *playerHandle = hPE;
    
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Deinit(uint32_t playerHandle)
{
    HRESULT rc;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot deinit...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer already deinitialized...\n\n");
        return -1;
    }
    
    if(hSource)
    {
        MV_PE_Stop(hPE, hSource);
	}
    	
    if(hStreamV)
    {
        MV_PE_StreamRemove(hPE, hStreamV);
        hStreamV = 0;
    }
	
    if(hStreamA)
    {
        MV_PE_StreamRemove(hPE, hStreamA);
        hStreamA = 0;
	}
    
    if(hSource)
    {
        MV_PE_SourceClose(hPE, hSource);
        hSource = 0;
	}
    
    if(hBuffer)
    {
        MV_PE_StreamBufRelease(hPE, hBuffer);
        hBuffer = 0;
    }
    
    if(hFilter)
    {
        MV_PE_SourceRemoveSectionFilter(hPE, hSource, hFilter);
        hFilter = 0;
    }
    
    if(hPE)
    {
        MV_PE_Remove(hPE);
        hPE = 0;
    }	
    
	/* Exit OS */
	MV_OSAL_Exit();
    
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Source_Open(uint32_t playerHandle, uint32_t *sourceHandle)
{
    HRESULT rc;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot open source...\n\n");
        return -1;
    }
    
    if(NULL == sourceHandle)
    {
        printf("\n%s failed, sourceHandle is NULL\n", __FUNCTION__);
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }

    /* Get Source handle before adding av streams */
    rc = MV_PE_SourceOpen(hPE, MV_PE_SOURCE_TYPE_TS, 0, 0, &hSource);
    if(rc != S_OK)
    {
        /* Deinit and exit */
        printf("\n\nFail to open source! %d\n\n", rc);
        return -1;
    }
    //rc = MV_PE_RegisterEventCallBack(hPE,MV_PE_EVENT_PSI_SECTION_COMPLETE,m_sectionReceivedCallback,NULL,NULL);
    
    rc = MV_PE_Play(hPE, hSource);
    if(rc != S_OK)
    {
        /* Deinit and exit */
        printf("Fail to start play function in Player_Source_Open!\n");
        return -1;
    }
    
    *sourceHandle = hSource;
    
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Source_Close(uint32_t playerHandle, uint32_t sourceHandle)
{
    HRESULT rc;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot close source...\n\n");
        return -1;
    }
    
    if(sourceHandle != hSource)
    {
        printf("\n\nWrong source handle, cannot close source...\n\n");
        return -1;
    }
    
    if(0 == hSource)
    {
        printf("\n\nSource already closed...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }
    
    rc = MV_PE_Stop(hPE, hSource);
    if(rc != S_OK)
    {
        /* Deinit and exit */
        printf("\n\nFail to stop player in Player_Source_Close!\n\n");
        return -1;
    }
    
    rc = MV_PE_SourceClose(hPE, hSource);
    if(rc != S_OK)
    {
        /* Deinit and exit */
        printf("\n\nFail to close source!\n\n");
        return -1;
    }
    
    hSource = 0;
    
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Stream_Create(uint32_t playerHandle, uint32_t sourceHandle, uint32_t PID, tStreamType streamType, uint32_t *streamHandle)
{
    HRESULT rc;
    MV_PE_STREAM_ATTRIB* pAttrib;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot create stream...\n\n");
        return -1;
    }
    
    if(sourceHandle != hSource)
    {
        printf("\n\nWrong source handle, cannot create stream...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }
    
    if(0 == hSource)
    {
        printf("\n\nSource not open error...\n\n");
        return -1;
    }
    
    if(NULL == streamHandle)
    {
        printf("\n%s failed, streamHandle is NULL\n", __FUNCTION__);
        return -1;
    }
    
    pAttrib = MV_PE_StreamAllocateAttribSet(((streamType >= 39) ? MV_PE_CONTENT_STREAM_TYPE_VIDEO : MV_PE_CONTENT_STREAM_TYPE_AUDIO), 0);
	pAttrib->IDType  =  MV_PE_STREAM_ID_TYPE_PID; /* PID type */
	pAttrib->ID      = PID; /* Video PID */
	pAttrib->Channel = ((streamType >= 39) ? MV_PE_CHANNEL_PRIMARY_VIDEO : MV_PE_CHANNEL_PRIMARY_AUDIO); /* Primary video channel */
	pAttrib->pVideoAttrib->Type = (streamType >= 39) ? streamType - 38 : streamType; /* Stream encode type */	
	
    /* Create stream handle */
	rc = MV_PE_StreamCreate(hPE, hSource, pAttrib, (streamType >= 39) ? &hStreamV : &hStreamA);
	if(rc != S_OK)
	{
        /* Deinit and exit */
		printf("\n\nFail to create stream!\n\n");
        MV_PE_StreamFreeAttribSet(pAttrib);
        return -1;
	}	
	MV_PE_StreamFreeAttribSet(pAttrib);	
    *streamHandle = (streamType >= 39) ? hStreamV : hStreamA;
    if(streamType >= 39)
    {
        rc = MV_PE_VideoSetVisible(hPE, MV_PE_CHANNEL_PRIMARY_VIDEO, 1);
        if(rc != S_OK)
        {
            /* Deinit and exit */
            printf("\n\nFail to set video visible!\n\n");
            return -1;
        }
    }
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Stream_Remove(uint32_t playerHandle, uint32_t sourceHandle, uint32_t streamHandle)
{
    HRESULT rc;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot remove stream...\n\n");
        return -1;
    }
    
    if(sourceHandle != hSource)
    {
        printf("\n\nWrong source handle, cannot remove stream...\n\n");
        return -1;
    }
    
    if(streamHandle != hStreamA && streamHandle != hStreamV)
    {
        printf("\n\nWrong stream handle, cannot remove stream...\n\n");
        return -1;
    }
    
    if(streamHandle == 0)
    {
        printf("\n\nStream already removed\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }
    
    if(0 == hSource)
    {
        printf("\n\nSource not open error...\n\n");
        return -1;
    }
    
    if(streamHandle == hStreamA)
    {
        rc = MV_PE_StreamRemove(hPE, hStreamA);
        if(rc != S_OK)
        {
            /* Deinit and exit */
            printf("\n\nFail to remove stream!\n\n");
            return -1;
        }
        hStreamA = 0;
    }
    else
    {
        rc = MV_PE_StreamRemove(hPE, hStreamV);
        if(rc != S_OK)
        {
            /* Deinit and exit */
            printf("\n\nFail to remove stream!\n\n");
            return -1;
        }
        hStreamV = 0;
    }
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Volume_Set(uint32_t playerHandle, uint32_t volume)
{
    HRESULT rc;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot change volume...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }
    
    rc = MV_PE_AOutSetVolume(hPE, MV_PE_OUTPUT_AUDIO_PATH_HDMI, volume);
    if(rc != S_OK)
    {
        /* Deinit and exit */
        printf("\n\nFail to set volume!\n\n");
        return -1;
    }
    
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Player_Volume_Get(uint32_t playerHandle, uint32_t *volume)
{
    HRESULT rc;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot get volume...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }
    
    if(NULL == volume)
    {
        printf("\n%s failed, volume is NULL\n", __FUNCTION__);
        return -1;
    }
    
    rc = MV_PE_AOutGetVolume(hPE, MV_PE_OUTPUT_AUDIO_PATH_HDMI, volume);
    if(rc != S_OK)
    {
        /* Deinit and exit */
        printf("\n\nFail to get volume!\n\n");
        return -1;
    }
    
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Demux_Register_Section_Filter_Callback(Demux_Section_Filter_Callback demuxSectionFilterCallback)
{
    HRESULT hr;
    
    if(NULL != DemuxSectionFilterCallback )
    {
        printf("\n\n%s(%d): failed to register callback! Callback already registered\n\n");
        return -1;
    }
    hr = MV_PE_RegisterEventCallBack(hPE,MV_PE_EVENT_PSI_SECTION_COMPLETE,m_sectionReceivedCallback,NULL,NULL);
    if(S_OK != hr)
    {
        printf("\n\n%s(%d): failed to register callback!\n\n", __FUNCTION__, __LINE__);
        return -1;
    }
    DemuxSectionFilterCallback = demuxSectionFilterCallback;
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Demux_Unregister_Section_Filter_Callback(Demux_Section_Filter_Callback demuxSectionFilterCallback)
{
    HRESULT hr;
    
    if(NULL == DemuxSectionFilterCallback )
    {
        printf("\n\n%s(%d): failed to unregister callback! Callback already unregistered\n\n");
        return -1;
    }
    if(demuxSectionFilterCallback != DemuxSectionFilterCallback)
    {
        printf("\n\n%s(%d): failed to unregister callback! Wrong callback function\n\n");
        return -1;
    }
    
    hr = MV_PE_UnRegisterEventCallBack(hPE, MV_PE_EVENT_PSI_SECTION_COMPLETE, m_sectionReceivedCallback);
    if(S_OK != hr)
    {
        printf("\n\n%s(%d): failed to unregister callback!\n\n", __FUNCTION__, __LINE__);
        return -1;
    }
    DemuxSectionFilterCallback = NULL;
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Demux_Set_Filter(uint32_t playerHandle, uint32_t PID, uint32_t tableID, uint32_t *filterHandle)
{
    HRESULT rc;
    MV_PE_SECTION_FILTER_PARAM secParam;
 
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot set filter...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized set filter...\n\n");
        return -1;
    }
    
    if(NULL == filterHandle)
    {
        printf("\n%s failed, filterHandle is NULL\n", __FUNCTION__);
        return -1;
    }
    
    /* Allocate PE resources */
    rc = MV_PE_StreamBufAllocate(hPE, MY_SECTION_BUFFER_SIZE, &hBuffer);
    if(S_OK != rc)
    {
        printf("\n\n%s(%d): failed to allocate section buffer!\n\n", __FUNCTION__, __LINE__);
        return -1;
    }

    secParam.Pid = PID;
    
    /* 'Universal' filter setup */
    memset(secParam.Match,   0x00, MV_PE_MAX_FILTER_LENGTH);
    memset(secParam.Mask,    0x00,MV_PE_MAX_FILTER_LENGTH);
    memset(secParam.NotMask, 0x00, MV_PE_MAX_FILTER_LENGTH);

    secParam.Match[0]=tableID;
    secParam.Mask[0]=0xff;

    printf("\nPID %x secParam.Match[0] %x secParam.Mask[0] %x\n",secParam.Pid,secParam.Match[0],secParam.Mask[0]);
    rc = MV_PE_SourceAddSectionFilter(hPE, hSource, &secParam, (HANDLE)hBuffer, 0, &hFilter);
    
    if(S_OK != rc)
    {
        printf("%s(%d): failed to add section filter!\n", __FUNCTION__, __LINE__);
        MV_PE_StreamBufRelease(hPE, hBuffer);
        return -1;
    }
    *filterHandle = hFilter;
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Demux_Free_Filter(uint32_t playerHandle, uint32_t filterHandle)
{
    HRESULT hr;
    
    if(playerHandle != hPE)
    {
        printf("\n\nWrong player handle, cannot free filter...\n\n");
        return -1;
    }
    
    if(filterHandle != hFilter)
    {
        printf("\n\nWrong filter handle, cannot free filter...\n\n");
        return -1;
    }
    
    if(0 == hPE)
    {
        printf("\n\nPlayer not initialized error...\n\n");
        return -1;
    }
    
    if(0 == hFilter)
    {
        printf("\n\nFilter not set error...\n\n");
        return -1;
    }
    
    hr = MV_PE_SourceRemoveSectionFilter(hPE, hSource, hFilter);
    if(S_OK != hr)
    {
        printf("\n\n%s(%d): failed to remove section filter!\n\n", __FUNCTION__, __LINE__);

    }

    hr = MV_PE_StreamBufRelease(hPE, hBuffer);
    if(S_OK != hr)
    {
        printf("\n\n%s(%d): failed to release section buffer!\n\n", __FUNCTION__, __LINE__);
    }
    hBuffer = 0;
    hFilter = 0;
    return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
int DMDi_Change_Config(int edge)
{
#if 0
    sony_dvb_result_t result = SONY_DVB_OK ;
	int tunerInit = -1;

    result = dvb_demod_SoftReset (&demod);
	tunerInit = Tuner_Init();
	if(tunerInit < 0 )
	{
        printf("\n\nInit tuner error in Change Config\n\n");
		return -1;
	}
	result = dvb_demod_SetConfig (&demod,DEMOD_CONFIG_LATCH_ON_POSEDGE, edge);
	if (result != SONY_DVB_OK) 
	{        	
		printf("[%s]dvb_demod_SetConfig DEMOD_CONFIG_DVBT2_PP2_LONG_ECHO failed!unit:\n",__FUNCTION__);        	
	}
 	return 0;
#endif
    sony_dvb_result_t result = SONY_DVB_OK ;
    static int32_t i32OldEdge = -1;

    if(i32OldEdge != edge)
    {
        printf("\n%s------------------------------------> %s\n",(i32OldEdge==0)?"DVB-T2" : "DVB-T",(edge==0)?"DVB-T2" : "DVB-T");
        result = dvb_cxd2820_Sleep(&cxd2820);
        if (result != SONY_DVB_OK) 
	    {        	
		    printf("[%sdvb_cxd2820_Sleep failed!:\n",__FUNCTION__);        	
	    }
	    result = dvb_demod_SetConfig (&demod,DEMOD_CONFIG_LATCH_ON_POSEDGE, edge);
	    if (result != SONY_DVB_OK) 
	    {        	
		    printf("[%s]dvb_demod_SetConfig DEMOD_CONFIG_DVBT2_PP2_LONG_ECHO failed!unit:\n",__FUNCTION__); 
            return -1;      	
	    }
        i32OldEdge = edge;
    }

 	return 0;
}

/***********************************************************************
* Function Name : 
*
* Description   :
*
* Side effects  : 
*
* Comment       : 
*
* Parameters    :
*
* Returns       : 
*
**********************************************************************/
t_Error Tuner_Get_Signal_Quality(uint8_t *signalQuality)
{
    sony_dvb_result_t result = SONY_DVB_OK ;
    uint8_t syncState = 0 ;
    uint8_t tsLock = 0 ;
    
    if(NULL == signalQuality)
    {
        printf("\n%s failed, signalQuality is NULL\n", __FUNCTION__);
        return -1;
    }
    
    result = dvb_demod_monitorT_SyncStat (cxd2820.pDemod, &syncState, &tsLock);
    if (result == SONY_DVB_OK)
    {
        if(tsLock == 1)
        {
            result = dvb_demod_monitor_Quality (cxd2820.pDemod,signalQuality);
            printf("\n######Signal quality DVBT %d ##########\n",*signalQuality);
        }
        else
        {
            result = dvb_demod_monitorT2_SyncStat (cxd2820.pDemod, &syncState, &tsLock);
            if(tsLock == 1)
            {
                result = dvb_demod_monitor_Quality (cxd2820.pDemod,signalQuality);
                printf("\n######Signal quality DVBT %d ##########\n",*signalQuality);
            }
            else
            {
                printf("\nDVBT NOT LOCKED\n");
                return -1;
            }
        }
     }
     return 0;
}
