#include "channel.h"
#include "global.h" // Include the globals header for access to global variables

#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <signal.h>
#include <directfb.h>
#include "tdp_api.h"

int i;
// Implementation of changeChannel function
void changeChannel(int channelNumber){
    int videoPID, audioPID;

    audioPID = tablePMT[channelNumber].audioPID;
    videoPID = tablePMT[channelNumber].videoPID;

    if(videoStreamHandle){
        Player_Stream_Remove(playerHandle, sourceHandle, videoStreamHandle);
        videoStreamHandle = 0;
    }
    Player_Stream_Remove(playerHandle, sourceHandle, audioStreamHandle);

    if(videoPID){
        Player_Stream_Create(playerHandle, sourceHandle, videoPID, VIDEO_TYPE_MPEG2, &videoStreamHandle);
        Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    } else {
        videoStreamHandle = 0;
        Player_Stream_Create(playerHandle, sourceHandle, audioPID, AUDIO_TYPE_MPEG_AUDIO, &audioStreamHandle);
    }
}

void parsePAT(uint8_t *buffer){
    tablePAT.sectionLength=(uint16_t)(((*(buffer+1)<<8)+*(buffer + 2)) & 0x0FFF);
    tablePAT.transportStream=(uint16_t)(((*(buffer+3)<<8)+*(buffer + 4)));
    tablePAT.versionNumber=(uint8_t)((*(buffer+5)>>1)& 0x1F);
    
    channelCount=(tablePAT.sectionLength*8-64)/32;
    int i=0;
    
    for(;i<channelCount;i++){
        tablePAT.programNumber[i]=(uint16_t)(*(buffer+(i*4)+8)<<8)+(*(buffer+(i*4)+9));
        tablePAT.PID[i]=(uint16_t)((*(buffer+(i*4)+10)<<8)+(*(buffer+(i*4)+11)) & 0x1FFF);
        printf("%d\tpid: %d\n", tablePAT.programNumber[i], tablePAT.PID[i]);
    }
    printf("\n\nSection arrived!!!\nsection length: %d\nts ID: %d\nversion number: %d\nchannel number: %d\n", tablePAT.sectionLength, tablePAT.transportStream, tablePAT.versionNumber, channelCount);
}

void parsePMT(uint8_t *buffer){
    parseFlag=0;
        tablePMT[i].sectionLength=(uint16_t)(((*(buffer+1)<<8)+*(buffer + 2)) & 0x0FFF);
        tablePMT[i].programNumber=(uint16_t)((*(buffer+3)<<8)+*(buffer + 4));
        tablePMT[i].programInfoLength=(uint16_t)(((*(buffer+10)<<8)+*(buffer + 11))& 0x0FFF);
        tablePMT[i].streamCount=0;

        tablePMT[i].hasTTX=0;
        int j;

        printf("\n\nSection arrived!!! PMT: %d \t%d\t%d\n", tablePMT[i].sectionLength, tablePMT[i].programNumber, tablePMT[i].programInfoLength);

        uint8_t *m_buffer = (uint8_t*)buffer + 12 + tablePMT[i].programInfoLength;

        for ( j = 0; ((uint16_t)(m_buffer-buffer)+5<tablePMT[i].sectionLength); j++)
        {

            tablePMT[i].streams[j].streamType=*(m_buffer);
            tablePMT[i].streams[j].elementaryPID=(uint16_t)((*(m_buffer+1)<<8) + *(m_buffer+2)) & 0x1FFF;
            tablePMT[i].streams[j].esInfoLength=(uint16_t)((*(m_buffer+3)<<8) + *(m_buffer+4)) & 0x0FFF;
            tablePMT[i].streams[j].descriptor=(uint8_t)*(m_buffer+5);

            // find audio stream
            if(tablePMT[i].streams[j].streamType==3){
                tablePMT[i].audioPID=tablePMT[i].streams[j].elementaryPID;
            }
            else if(tablePMT[i].streams[j].streamType==2){
                tablePMT[i].videoPID=tablePMT[i].streams[j].elementaryPID;
            }

            if(tablePMT[i].streams[j].streamType==6 && tablePMT[i].streams[j].descriptor==86)
                tablePMT[i].hasTTX=1;

            printf("streamtype: %d epid: %d len %d desc: %d\n", tablePMT[i].streams[j].streamType, tablePMT[i].streams[j].elementaryPID, tablePMT[i].streams[j].esInfoLength, tablePMT[i].streams[j].descriptor);
            m_buffer+= 5 + tablePMT[i].streams[j].esInfoLength;
            tablePMT[i].streamCount++;
        }
        printf("%d\thasTTX: %d", tablePMT[i].streamCount, tablePMT[i].hasTTX);
}

