/**
 * @file tdp_api.h
 *
 * @brief Tuner Demultiplexer and Player API for Marvell BG2 SoC and Nutune tuner
 *
 * @author Krsto Lazic 
 *         RT-RK 
 *         krsto.lazic@rt-rk.com)
 */

#ifndef TDPAPI_H_
#define TDPAPI_H_

#include <stdint.h>

/**
 * @brief Error codes
 */
typedef enum t_Error
{
    ERROR = -1,
    NO_ERROR
}t_Error;

/**
 * @brief Tuner lock status
 */
typedef enum t_LockStatus
{
    STATUS_ERROR = 0,
    STATUS_LOCKED = 1
}t_LockStatus;

/**
 * @brief Tuner status callback
 */
typedef int32_t(*Tuner_Status_Callback)(t_LockStatus status);

/**
 * @brief Demux section filter callback
 */
typedef int32_t(*Demux_Section_Filter_Callback)(uint8_t *buffer);

/**
 * @brief Polarization
 */
typedef enum t_Polarization
{
    POLAR_VERTICAL   = 0,
    POLAR_HORIZONTAL = 1
}t_Polarization;

/**
 * @brief Band
 */
typedef enum t_Band
{
    SPECTRUM_NORMAL = 0,
    SPECTRUM_INVERTED =1
}t_Band;

/**
 * @brief Module
 */
typedef enum t_Module
{
    DVB_T = 0,
    DVB_T2 =1
}t_Module;

/**
 * @brief Stream type
 */
typedef enum t_StreamType
{
    //audio stream types
    AUDIO_TYPE_DOLBY_AC3 			= 1, 
    AUDIO_TYPE_DOLBY_PLUS			= 2,
    AUDIO_TYPE_DOLBY_TRUE_HD	    = 3,
    AUDIO_TYPE_LPCM_SD			    = 4,
    AUDIO_TYPE_LPCM_BD			    = 5,
    AUDIO_TYPE_LPCM_HD			    = 6,
    AUDIO_TYPE_MLP				    = 7,
    AUDIO_TYPE_DTS				    = 8,
    AUDIO_TYPE_DTS_HD				= 9,
    AUDIO_TYPE_MPEG_AUDIO			= 10,
    AUDIO_TYPE_MP3				    = 11,
    AUDIO_TYPE_HE_AAC				= 12,
    AUDIO_TYPE_WMA				    = 13,
    AUDIO_TYPE_WMA_PRO			    = 14,
    AUDIO_TYPE_WMA_LOSSLESS			= 15,
    AUDIO_TYPE_RAW_PCM			    = 16,
    AUDIO_TYPE_SDDS				    = 17,
    AUDIO_TYPE_DD_DCV				= 18,
    AUDIO_TYPE_DRA				    = 19,
    AUDIO_TYPE_DRA_EXT			    = 20,
    AUDIO_TYPE_DTS_LBR			    = 21,
    AUDIO_TYPE_DTS_HRES			    = 22,
    AUDIO_TYPE_LPCM_SESF			= 23,
    AUDIO_TYPE_DV_SD				= 24,
    AUDIO_TYPE_VORBIS				= 25,
    AUDIO_TYPE_FLAC                 = 26,
    AUDIO_TYPE_RAW_AAC			    = 27,
    AUDIO_TYPE_RA8          	    = 28,                     
    AUDIO_TYPE_RAAC         	    = 29,              
    AUDIO_TYPE_ADPCM				= 30,
    AUDIO_TYPE_SPDIF_INPUT  	    = 31,
    AUDIO_TYPE_G711A				= 32,
    AUDIO_TYPE_G711U				= 33,
    AUDIO_RAW_SIGNED_PCM			= 34,
    AUDIO_RAW_UNSIGNED_PCM		    = 35,
    AUDIO_AMR_WB		            = 36,
    AUDIO_AMR_NB         		    = 37,
    AUDIO_TYPE_UNSUPPORTED          = 38,
    
    //video stream types
    VIDEO_TYPE_H264                 = 39,
    VIDEO_TYPE_VC1                  = 40,
    VIDEO_TYPE_MPEG4                = 41,
    VIDEO_TYPE_MPEG2                = 42,
    VIDEO_TYPE_MPEG1                = 43,
    VIDEO_TYPE_JPEG                 = 44,
    VIDEO_TYPE_DIV3                 = 45,
    VIDEO_TYPE_DIV4                 = 46,
    VIDEO_TYPE_DX50                 = 47,
    VIDEO_TYPE_MVC                  = 48,
    VIDEO_TYPE_WMV3                 = 49,
    VIDEO_TYPE_DIVX                 = 50,
    VIDEO_TYPE_DIV5                 = 51,
    VIDEO_TYPE_RV30                 = 52,
    VIDEO_TYPE_RV40                 = 53,
    VIDEO_TYPE_VP6                  = 54,
    VIDEO_TYPE_SORENSON             = 55,
    VIDEO_TYPE_H263                 = 56,
    VIDEO_TYPE_JPEG_SINGLE		    = 57,
    VIDEO_TYPE_VP8                  = 58,
    VIDEO_TYPE_VP6F				    = 59
}tStreamType;

/****************************************************************************
* @brief    Tuner initialization function.
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Tuner_Init();

#ifdef SATELITE
/****************************************************************************
* @brief    Lock to a specific frequency
* 
* @param    [in] tuneFrequency - tune frequency in kHz
* @param    [in] polarization - polarization
* @param    [in] band - band
* @param    [in] symbolRate - symbolRate
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Tuner_Lock_To_Frequency(uint32_t tuneFrequency, 
                                t_Polarization polarization, 
                                t_Band band, 
                                uint32_t symbolRate);
#else
/****************************************************************************
* @brief    Lock to a specific frequency
* 
* @param    [in] tuneFrequency - tune frequency in kHz
* @param    [in] bandwidth - bandwidth in kHz
* @param    [in] modul - module
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Tuner_Lock_To_Frequency(uint32_t tuneFrequency, uint32_t bandwidth, t_Module modul);
#endif

/****************************************************************************
* @brief    Register tuner status callback
* 
* @param    [in] tunerStatusCallback - status callback function
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Tuner_Register_Status_Callback(Tuner_Status_Callback tunerStatusCallback);

/****************************************************************************
* @brief    Unregister tuner status callback
* 
* @param    [in] tunerStatusCallback - status callback function
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Tuner_Unregister_Status_Callback(Tuner_Status_Callback tunerStatusCallback);

/****************************************************************************
*
* @brief    Get current signal quality
* 
* @param    [out] signalQuality - signal quality
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/ 
t_Error Tuner_Get_Signal_Quality(uint8_t *signalQuality);

/****************************************************************************
* @brief    Tuner deinitialization function.
*
* @return   NO_ERROR, if there is no error
* @return   ERROR, in case of error
*
*****************************************************************************/ 
t_Error Tuner_Deinit();

/****************************************************************************
* @brief    Set filter to demux
* 
* @param    [in] palyerHandle - handle of initialized player instance
* @param    [in] PID - PID
* @param    [in] tableID - table id
* @param    [out] filterHandle - handle of created filter
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Demux_Set_Filter(uint32_t playerHandle, uint32_t PID, uint32_t tableID, uint32_t *filterHandle);

/****************************************************************************
* @brief    Free demux filter
* 
* @param    [in] palyerHandle - handle of initialized player instance
* @param    [in] filterHandle - handle of created filter
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Demux_Free_Filter(uint32_t playerHandle, uint32_t filterHandle);

/****************************************************************************
* @brief    Register section filter callback
* 
* @param    [in] demuxSectionFilterCallback - callback function
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Demux_Register_Section_Filter_Callback(Demux_Section_Filter_Callback demuxSectionFilterCallback);

/****************************************************************************
* @brief    Unregister section filter callback
* 
* @param    [in] demuxSectionFilterCallback - callback function
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Demux_Unregister_Section_Filter_Callback(Demux_Section_Filter_Callback demuxSectionFilterCallback);

/****************************************************************************
* @brief    Initialize player
* 
* @param    [out] playerHandle - handle of initialized player
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Init(uint32_t *playerHandle);

/****************************************************************************
* @brief    Deinitialize player
* 
* @param    [in] playerHandle - handle of previously initialized player
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Deinit(uint32_t playerHandle);

/****************************************************************************
* @brief    Open source
* 
* @param    [in] playerHandle - handle of previously initialized player
* @param    [out] sourceHandle - handle of opened source
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Source_Open(uint32_t playerHandle, uint32_t *sourceHandle);

/****************************************************************************
* @brief    Close source
* 
* @param    [in] playerHandle - handle of previously initialized player
* @param    [in] sourceHandle - handle of previously opened source
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Source_Close(uint32_t playerHandle, uint32_t sourceHandle);

/****************************************************************************
* @brief    Create stream
* 
* @param    [in] playerHandle - handle of previously initialized player
* @param    [in] sourceHandle - handle of prevoiusly opened source
* @param    [in] PID - audio or video PID
* @param    [in] stream type - stream type
* @param    [out] streamHandle - handle of created stream
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Stream_Create(uint32_t playerHandle, uint32_t sourceHandle, uint32_t PID, tStreamType streamType, uint32_t *streamHandle);

/****************************************************************************
* @brief    Remove stream
* 
* @param    [in] playerHandle - handle of previously initialized player
* @param    [in] sourceHandle - handle of prevoiusly opened source
* @param    [in] streamHandle - handle of previously created stream
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Stream_Remove(uint32_t playerHandle, uint32_t sourceHandle, uint32_t streamHandle);

/****************************************************************************
* @brief    Set volume
* 
* @param    [in] playerHandle - handle of previously initialized player
* @param    [in] volume - volume
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Volume_Set(uint32_t playerHandle, uint32_t volume);

/****************************************************************************
* @brief    Get volume
* 
* @param    [in] playerHandle - handle of previously initialized player
* @param    [out] volume - volume
*
* @return   NO_ERROR - no error
* @return   ERROR - error
*
*****************************************************************************/
t_Error Player_Volume_Get(uint32_t playerHandle, uint32_t *volume);

#endif //TDPAPI_H_
