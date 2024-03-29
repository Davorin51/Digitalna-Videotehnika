#include "thread.h"

#define NUM_EVENTS  5

#define NON_STOP    1

/* error codes */
#define NO_ERROR 		0
#define ERROR			1

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
static int32_t inputFileDesc;

int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventRead);

    IDirectFBSurface *primary = NULL;
    IDirectFB *dfbInterface = NULL; //sadrzi razne podatke o tom interfejsu
    int screenWidth = 0;
    int screenHeight = 0;
    DFBSurfaceDescription surfaceDesc; 

    IDirectFBFont *fontInterface = NULL;
    DFBFontDescription fontDesc;

void stringPrintChannel(int channel){


    DFBCHECK(DirectFBInit(0,0));
    /* fetch the DirectFB interface */
	DFBCHECK(DirectFBCreate(&dfbInterface));
    /* tell the DirectFB to take the full screen for this application */
	DFBCHECK(dfbInterface->SetCooperativeLevel(dfbInterface, DFSCL_FULLSCREEN));
	
    
    /* create primary surface with double buffering enabled */
    
	surfaceDesc.flags = DSDESC_CAPS;
	surfaceDesc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	DFBCHECK (dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &primary));
    
    
    /* fetch the screen size */
    DFBCHECK (primary->GetSize(primary, &screenWidth, &screenHeight));


    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));    
    
    /* clear the screen before drawing anything (draw black full screen rectangle)*/
    
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
	
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth - 300,
                                    /*upper left y coordinate*/ channel,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));


    
    
    
    
	/* line drawing */
    /*
	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawLine(primary,
                                100,
                                screenHeight - 170,
                                screenWidth,
                              screenHeight - 170));
                              */
	
    
	/* draw text */

	IDirectFBFont *fontInterface = NULL;
	DFBFontDescription fontDesc;
	
    /* specify the height of the font by raising the appropriate flag and setting the height value */
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 50;
	
    printf("Kanal je %d\t", channel);
    /* create the font and set the created font for primary surface text drawing */
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    
    if(channel == 5){
	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    /* draw the text */
	DFBCHECK(primary->DrawString(primary,
                                /*text to be drawn*/ "5",
                             	/*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                /*y coordinate of the lower left corner of the resulting text*/ screenHeight - 200,
                                /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    }
    else if(channel == 4){
	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    /* draw the text */
	DFBCHECK(primary->DrawString(primary,
                                /*text to be drawn*/ "4",
                             	/*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                /*y coordinate of the lower left corner of the resulting text*/ screenHeight - 200,
                                /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
	
    }
    else{
       return;
    }

   
    /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    
    /* wait 5 seconds before terminating*/
	sleep(4);

    
    /*clean up*/
   	primary->Release(primary);
	dfbInterface->Release(dfbInterface);


    char str[50];
    sprintf(str,"%d",channel); 

    int tempChannel=channel, counter=0;

    while(tempChannel!=0){
        tempChannel/=10;
        counter++;
    }
    
}

enum REMOTE_BUTTON {
    REMOTE_MUTE = 60,
    REMOTE_CHANNEL_MINUS,
    REMOTE_CHANNEL_PLUS,
    REMOTE_VOLUME_PLUS,
    REMOTE_VOLUME_MINUS,
    REMOTE_CHANNEL_1=2,
    REMOTE_CHANNEL_2,
    REMOTE_CHANNEL_3,
    REMOTE_CHANNEL_4,
    REMOTE_CHANNEL_5,
    REMOTE_CHANNEL_6,
    REMOTE_CHANNEL_7,
    REMOTE_CHANNEL_8,
    REMOTE_CHANNEL_9,
    REMOTE_EXIT=102,
    REMOTE_INFO=358
};

void *myThreadRemote(){
    const char* dev = "/dev/input/event0";
    char deviceName[20];
    struct input_event* eventBuf;
    uint32_t eventCnt;
    uint32_t i;
    int flag = 0;

    inputFileDesc = open(dev, O_RDWR);
    if(inputFileDesc == -1)
    {
        printf("Error while opening device (%s) !", strerror(errno));
	    return NULL;
    }
    
    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
	printf("RC device opened succesfully [%s]\n", deviceName);
    
    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!eventBuf)
    {
        printf("Error allocating memory !");
        return NULL;
    }
    //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
    int volumeLevel = 0;
    int program = 0;

    while(flag == 0)
    {
        /* read input eventS */
        if(getKeys(NUM_EVENTS, (uint8_t*)eventBuf, &eventCnt))
        {
			printf("Error while reading input events !");
			return NULL;
		}

        for(i = 0; i < eventCnt; i++)
        {
            printf("Event time: %d sec, %d usec\n",(int)eventBuf[i].time.tv_sec,(int)eventBuf[i].time.tv_usec);
            printf("Event type: %hu\n",eventBuf[i].type);
            printf("Event code: %hu\n",eventBuf[i].code);
            printf("Event value: %d\n",eventBuf[i].value);
            printf("\n");

            if (eventBuf[i].type == 1 && (eventBuf[i].value == 1 || eventBuf[i].value ==2)) {
            switch(eventBuf[i].code) {
                case 61: {
                    if(program > 0) program--;
                    else program = 0;
                   // printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    

                    break;
                }
                case 62: {
                    program++;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 63: {
                    volumeLevel++;
                    printf("Volume level %d\n", volumeLevel);
                    break;
                }
                case 64: {
                    if(volumeLevel > 0) volumeLevel--;
                    else volumeLevel = 0;
                    printf("Volume level %d\n", volumeLevel);
                    break;
                }
                case 2: {
                    program = 1;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 3: {
                    program = 2;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 4: {
                    program = 3;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 5: {
                    program = 4;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 6: {
                    program = 5;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 7: {
                    program = 6;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 8: {
                    program = 7;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 9: {
                    program = 8;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 10: {
                    program = 9;
                    //printf("Upaljen program broj %d\n", program);
                    stringPrintChannel(program);
                    break;
                }
                case 102: {
                    printf("exit \n");
                    flag = 1;
                }
            }
            } 
        }
    }  
    
    free(eventBuf);
    
    return NULL;
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

void setupScreen(){
    
}