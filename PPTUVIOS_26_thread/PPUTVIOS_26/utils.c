#include "utils.h"
#include "global.h"

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



void parse(char *filename, tunerData *tuner, initService *init){
    char line[255];
    char *token;
    FILE *fptr;
    fptr=fopen(filename,"r");
    if(fptr){

        fgets(line, sizeof(line), fptr);
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        tuner->frequency=atoi(strtok(NULL, "<"));
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        tuner->bandwidth=atoi(strtok(NULL, "<"));  
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        token=strtok(NULL, "<");
        sprintf(tuner->module, "%s", token);
        fgets(line, sizeof(line), fptr);
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        init->audioPID=atoi(strtok(NULL, "<"));
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        init->videoPID=atoi(strtok(NULL, "<"));
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        token=strtok(NULL, "<");
        sprintf(init->audioType, "%s", token);
        fgets(line, sizeof(line), fptr);
        strtok(line, ">");
        token=strtok(NULL, "<");
        sprintf(init->videoType, "%s", token);
        
    }
    fclose(fptr);
}

int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead)
{
    int32_t ret = 0;
    
    /* read input events and put them in buffer */
    ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0)
    {
        printf("Error code %d", ret);
        return ERROR;
    }
    /* calculate number of read events */
    *eventsRead = ret / (int)sizeof(struct input_event);
    
    return NO_ERROR;
}

