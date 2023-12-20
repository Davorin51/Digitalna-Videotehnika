#include "remote.h"


static int32_t inputFileDesc;
static int32_t exit_flag = 0;
static int32_t channel_counter = 0;
static struct input_event* event_buf;
static pthread_t remote;

int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead)
{
    int32_t ret = 0;
    

    ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0)
    {
        printf("Error code %d", ret);
        return ERROR;
    }
    
    *eventsRead = ret / (int)sizeof(struct input_event);
    
    return NO_ERROR;
}


void* remote_control_handler(void* arg)
{
    uint32_t event_cnt;
    uint32_t i;

    const char* dev = "/dev/input/event0";
    char deviceName[20];
    //struct input_event* eventBuf;
    //uint32_t eventCnt;
    //uint32_t i;
    
    inputFileDesc = open(dev, O_RDWR);
    if(inputFileDesc == -1)
    {
        printf("Error while opening device (%s) !", strerror(errno));
	    //return ERROR;
    }
    
    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
	printf("RC device opened succesfully [%s]\n", deviceName);
    
    event_buf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!event_buf)
    {
        printf("Error allocating memory !");
        //return ERROR;
    }
    
    while(!exit_flag)
    {
    getKeys(NUM_EVENTS, (uint8_t*)event_buf, &event_cnt);
    for (i = 0; i<event_cnt; i++){
            printf("Event time: %d sec, %d usec\n",(int)event_buf[i].time.tv_sec,(int)event_buf[i].time.tv_usec);
            printf("Event type: %hu\n",event_buf[i].type);
            printf("Event code: %hu\n",event_buf[i].code);
            printf("Event value: %d\n",event_buf[i].value);
            printf("\n");
        if(event_buf[i].type == EV_KEY) {
            if(event_buf[i].value == 0 || event_buf[i].value == 1){
                switch(event_buf[i].code){
                    case 102: 
                        exit_flag = 1;
                        break;
                    case 61:
                        if(channel_counter < 0){
                            channel_counter = 0;
                        }
                        else {
                            channel_counter--;
                        }
                        printf("Button down pressed, Channel: %d\n", channel_counter);
                        
                        break;
                    case 62:
                        
                        channel_counter++;
                        printf("Button up pressed, Channel: %d\n", channel_counter);
                        
                        break;
                }
                
            } 
        }
    }
}

free(event_buf);
close(inputFileDesc);
pthread_exit(NULL);

}
