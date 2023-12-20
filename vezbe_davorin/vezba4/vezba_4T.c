#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>


#include <directfb.h>

#define NUM_EVENTS  5

#define NON_STOP    1

/* error codes */
#define NO_ERROR 		0
#define ERROR			1

void *myThreadRemote();
static int32_t inputFileDesc;

//192.168.28.118 stb ip
//192.168.28.13 pc ip


/* helper macro for error checking */
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



int32_t main(int32_t argc, char** argv)
{

    pthread_t remote;
    static IDirectFBSurface *primary = NULL; //glavni, u njemu se radi sve
    IDirectFB *dfbInterface = NULL; //sadrzi razne podatke o tom interfejsu
    static int screenWidth = 0;
    static int screenHeight = 0;
	DFBSurfaceDescription surfaceDesc; 
    
    
    /* initialize DirectFB */
    
	DFBCHECK(DirectFBInit(&argc, &argv));
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
    
    //while()
    /* clear the screen before drawing anything (draw black full screen rectangle)*/
    
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff)); //0xff je full neprozirno


	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
	
    
    primary->Release(primary);
	dfbInterface->Release(dfbInterface);

    
    /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    
    
    /* wait 5 seconds before terminating*/
	sleep(30);

    
	primary->Release(primary);
	dfbInterface->Release(dfbInterface);
 

    return 0;
}
