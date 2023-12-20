#include <stdio.h>
#include <directfb.h>
#include <stdint.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>


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
    static IDirectFBSurface *primary = NULL;
    IDirectFB *dfbInterface = NULL;
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
    
    
    /* clear the screen before drawing anything (draw black full screen rectangle)*/
    
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

    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
	
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth - 300,
                                    /*upper left y coordinate*/ 100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));


        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
	
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 100,
                                    /*rectangle width*/ 100,
                                    /*rectangle height*/ 100));
    
    
    /* rectangle drawing */
    
    //DFBCHECK(primary->SetColor(primary, 0x03, 0x03, 0xff, 0xff));
    //DFBCHECK(primary->FillRectangle(primary, screenWidth/5, screenHeight/5, screenWidth/3, screenHeight/3));
    
    
	/* line drawing */
    
	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
	DFBCHECK(primary->DrawLine(primary,
                               /*x coordinate of the starting point*/ 100,
                               /*y coordinate of the starting point*/ screenHeight - 170,
                               /*x coordinate of the ending point*/ screenWidth,
                               /*y coordinate of the ending point*/ screenHeight - 170));
	
    
	/* draw text */

	IDirectFBFont *fontInterface = NULL;
	DFBFontDescription fontDesc;
	
    /* specify the height of the font by raising the appropriate flag and setting the height value */
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 50;
	
    /* create the font and set the created font for primary surface text drawing */
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    
	DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff));
    /* draw the text */
	DFBCHECK(primary->DrawString(primary,
                                /*text to be drawn*/ "CRVENO",
                             	/*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                /*y coordinate of the lower left corner of the resulting text*/ screenHeight - 200,
                                /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
	
    
	/* draw image from file */
    
	IDirectFBImageProvider *provider;
	IDirectFBSurface *logoSurface = NULL;
	int32_t logoHeight, logoWidth;
	
    /* create the image provider for the specified file */
	DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "dfblogo_alpha.png", &provider));
    /* get surface descriptor for the surface where the image will be rendered */
	DFBCHECK(provider->GetSurfaceDescription(provider, &surfaceDesc));
    /* create the surface for the image */
	DFBCHECK(dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &logoSurface));
    /* render the image to the surface */
	DFBCHECK(provider->RenderTo(provider, logoSurface, NULL));
	
    /* cleanup the provider after rendering the image to the surface */
	provider->Release(provider);
	
    /* fetch the logo size and add (blit) it to the screen */
	//DFBCHECK(logoSurface->GetSize(logoSurface, &logoWidth, &logoHeight));
	//DFBCHECK(primary->Blit(primary,
      //                     /*source surface*/ logoSurface,
        //                   /*source region, NULL to blit the whole surface*/ NULL,
          //                 /*destination x coordinate of the upper left corner of the image*/50,
            //               /*destination y coordinate of the upper left corner of the image*/screenHeight - logoHeight -50));
    
    
    /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    
    
    /* wait 5 seconds before terminating*/
	sleep(5);
	
    
    /*clean up*/
    
	primary->Release(primary);
	dfbInterface->Release(dfbInterface);
    
    //TODO 1: display the keycode of the button on the remote each time any of the buttons are pressed
    //        add background and a frame to the keycode diplay
    //        the displayed keycode should disappear after 3 seconds 
    //TODO 2: add fade-in and fade-out effects
    //TODO 3: add animation - the display should start at the left part of the screen and at the right
    //TODO 4: add support for displaying up to 3 keycodes at the same time, each in a separate row

    return 0;
}
