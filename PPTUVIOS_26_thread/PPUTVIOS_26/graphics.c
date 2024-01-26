#include "graphics.h"
#include "global.h" // This should include the global variables and definitions
#include <math.h>
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

#include <stddef.h>

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



IDirectFBSurface *primary = NULL;
IDirectFB *dfbInterface = NULL;
IDirectFBWindow *window;
int screenWidth = 0;
int screenHeight = 0;
DFBSurfaceDescription surfaceDesc;
IDirectFBFont *fontInterface = NULL;
DFBFontDescription fontDesc;

void DFBInit(int32_t* argc, char*** argv){
    /* initialize DirectFB */
    
	DFBCHECK(DirectFBInit(argc, argv));
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
                               /*alpha*/ 0x00));
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
	

	
	
    /* specify the height of the font by raising the appropriate flag and setting the height value */
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 70;
	
    /* create the font and set the created font for primary surface text drawing */
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));

    /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Draw black rectangle (crniPravokutnik)
void crniPravokutnik() {
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
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    fontDesc.height = 70;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Radio kanal",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                 /*y coordinate of the lower left corner of the resulting text*/ screenHeight/2-300,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
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
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Radio kanal",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ screenWidth/2,
                                 /*y coordinate of the lower left corner of the resulting text*/ screenHeight/2-300,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Clear the screen
void clearScreen() {
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 0,
                                    /*upper left y coordinate*/ 0,
                                    /*rectangle width*/ screenWidth,
                                    /*rectangle height*/ screenHeight));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Draw channel information
void drawChannel(int channel, int hasTTX, int isRadio) {
    char str[10];
    sprintf(str, "%d", channel);
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
    primary->FillTriangle(primary, 200, 800, 300, 900, 100, 900);
    primary->FillTriangle(primary, 200, 1000, 100, 900, 300, 900);
    if(hasTTX){
        primary->FillTriangle(primary, 310, 910, 360, 960, 260, 960);
        primary->FillTriangle(primary, 310, 1010, 260, 960, 360, 960);
    }
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    fontDesc.height = 70;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Ch",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 900,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ str,
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 970,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    if(hasTTX){
        fontDesc.height = 35;
        DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	    DFBCHECK(primary->SetFont(primary, fontInterface));
        DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "TTX",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 310,
                                 /*y coordinate of the lower left corner of the resulting text*/ 980,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    }
    
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
    primary->FillTriangle(primary, 200, 800, 300, 900, 100, 900);
    primary->FillTriangle(primary, 200, 1000, 100, 900, 300, 900);

    if(hasTTX){
        primary->FillTriangle(primary, 310, 910, 360, 960, 260, 960);
        primary->FillTriangle(primary, 310, 1010, 260, 960, 360, 960);
    }
    fontDesc.height = 70;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "Ch",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 900,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ str,
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 200,
                                 /*y coordinate of the lower left corner of the resulting text*/ 970,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    if(hasTTX){
        fontDesc.height = 35;
        DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	    DFBCHECK(primary->SetFont(primary, fontInterface));
        DFBCHECK(primary->DrawString(primary,
                                 /*text to be drawn*/ "TTX",
                                 /*number of bytes in the string, -1 for NULL terminated strings*/ -1,
                                 /*x coordinate of the lower left corner of the resulting text*/ 310,
                                 /*y coordinate of the lower left corner of the resulting text*/ 980,
                                 /*in case of multiple lines, allign text to left*/ DSTF_CENTER));
    }
    
	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Draw volume information
void drawVolume(int vol, int isRadio) {
     if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
    
                               
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)-100, (screenWidth/2)+100, (screenHeight/2), (screenWidth/2)-100, (screenHeight/2));
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)+100, (screenWidth/2)-100, (screenHeight/2), (screenWidth/2)+100, (screenHeight/2));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));

    float firstScale=vol/25.0;
    float secondScale=(vol-25)/25.0;
    float thirdScale=(vol-50)/25.0;
    float fourthScale=(vol-75)/25.0;

    // truncate the negative values to 0
    secondScale = secondScale < 0 ? 0 : secondScale;
    thirdScale = thirdScale < 0 ? 0 : thirdScale;
    fourthScale = fourthScale < 0 ? 0 : fourthScale;

    // truncate the values to 1
    firstScale = firstScale > 1 ? 1 : firstScale;
    secondScale = secondScale > 1 ? 1 : secondScale;
    thirdScale = thirdScale > 1 ? 1 : thirdScale;
    fourthScale = fourthScale > 1 ? 1 : fourthScale;



    primary->FillTriangle(primary, (screenWidth/2)-1, (screenHeight/2)-90, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90*firstScale-1, (screenHeight/2)-90+90*firstScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90, (screenHeight/2), (screenWidth/2)+90-90*secondScale, (screenHeight/2)+90*secondScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-1, (screenHeight/2)+90, (screenWidth/2)-90*thirdScale-1, (screenHeight/2)+90-90*thirdScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-90, (screenHeight/2), (screenWidth/2)-90+90*fourthScale, (screenHeight/2)-90*fourthScale);
   

	DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    //draw again on other buffer
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x70,
                               /*green*/ 0x00,
                               /*blue*/ 0x70,
                               /*alpha*/ 0xbb));
    }
                               
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)-100, (screenWidth/2)+100, (screenHeight/2), (screenWidth/2)-100, (screenHeight/2));
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2)+100, (screenWidth/2)-100, (screenHeight/2), (screenWidth/2)+100, (screenHeight/2));
    DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0xff,
                               /*green*/ 0xff,
                               /*blue*/ 0xff,
                               /*alpha*/ 0xff));
    primary->FillTriangle(primary, (screenWidth/2)-1, (screenHeight/2)-90, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90*firstScale-1, (screenHeight/2)-90+90*firstScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)+90, (screenHeight/2), (screenWidth/2)+90-90*secondScale, (screenHeight/2)+90*secondScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-1, (screenHeight/2)+90, (screenWidth/2)-1-90*thirdScale, (screenHeight/2)+90-90*thirdScale);
    primary->FillTriangle(primary, (screenWidth/2), (screenHeight/2), (screenWidth/2)-90, (screenHeight/2), (screenWidth/2)-90+90*fourthScale, (screenHeight/2)-90*fourthScale);
    
    
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Clear channel information from the screen
void clearChannel() {
    int isRadio=(!tablePMT[channel].videoPID);
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ 100,
                                    /*upper left y coordinate*/ 800,
                                    /*rectangle width*/ 310,
                                    /*rectangle height*/ 210));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Clear volume information from the screen
void clearVolume() {
   int isRadio=(!tablePMT[channel].videoPID);
    if(isRadio){
        DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0xff));
    }
    else{
       DFBCHECK(primary->SetColor(/*surface to draw on*/ primary,
                               /*red*/ 0x00,
                               /*green*/ 0x00,
                               /*blue*/ 0x00,
                               /*alpha*/ 0x00)); 
    }
	DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
    DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,
                                    /*upper left x coordinate*/ screenWidth/2-100,
                                    /*upper left y coordinate*/ screenHeight/2-100,
                                    /*rectangle width*/ 200,
                                    /*rectangle height*/ 200));
    DFBCHECK(primary->Flip(primary,
                           /*region to be updated, NULL for the whole surface*/NULL,
                           /*flip flags*/0));
}

// Draw time on the screen
void drawTime() {
    struct timeval now;
    gettimeofday(&now, NULL);
    struct tm *t = localtime(&now.tv_sec);  // Convert time_t to struct tm

    char timeStr[10];
    strftime(timeStr, sizeof(timeStr)-1, "%H:%M", t);

    // Positioning the rhombus in the top right corner
    int posX = screenWidth - 210; // 210 pixels from the right edge
    int posY = 100;  // 100 pixels from the top

    // Set color for the rhombus
    DFBCHECK(primary->SetColor(primary, 0x70, 0x00, 0x70, 0xff)); // Purple color

    // Draw the rhombus
    primary->FillTriangle(primary, posX, posY, posX + 100, posY + 100, posX - 100, posY + 100);
    primary->FillTriangle(primary, posX, posY + 200, posX - 100, posY + 100, posX + 100, posY + 100);

    // Set color for the text
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff)); // White color

    // Set font size for the time text
    fontDesc.height = 40; // Adjust font size if needed
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
    DFBCHECK(primary->SetFont(primary, fontInterface));

    // Draw the time string inside the rhombus
    DFBCHECK(primary->DrawString(primary, timeStr, -1, posX, posY + 100, DSTF_CENTER));

    // Flip the primary surface to update the display
    DFBCHECK(primary->Flip(primary, NULL, 0));

    signalEvent3.sigev_notify = SIGEV_THREAD;
    signalEvent3.sigev_notify_function = clearTimeDisplay;
    signalEvent3.sigev_value.sival_ptr = NULL;
    signalEvent3.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME, &signalEvent3, &timerId3);

    timerSpec3.it_value.tv_sec = 3; // 3 seconds
    timerSpec3.it_value.tv_nsec = 0;
    timer_settime(timerId3, 0, &timerSpec3, NULL);
}

// Clear time display from the screen
void clearTimeDisplay() {
    // Assuming the time is displayed in the top right corner
    int posX = screenWidth - 410;
    int posY = 100;
    int width = 200; // Width of the area to clear
    int height = 200; // Height of the area to clear

    DFBCHECK(primary->SetColor(primary, 0x00, 0x00, 0x00, 0x00));
    DFBCHECK(primary->FillRectangle(primary, posX, posY, width, height));
    DFBCHECK(primary->Flip(primary, NULL, 0));
}

// Draw reminder dialog
void drawReminderDialog(const char* messageLine1, const char* messageLine2, const char* firstOption, const char* secondOption, int highlight) {
   
   // Define center of the screen
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;
    int size = 200;
    int stretchFactor = 2.5; // Factor to stretch the rhombus horizontally
    int horizontalSide = 250;
    int verticalSide = 180;

    // Define the size of the hexagon (distance from center to any vertex)
    //int size = 100; // Change this value as needed for your desired size
    int i;
    // Calculate the vertices of the hexagon
    DFBPoint vertices[6];
    for (i = 0; i < 6; ++i) {
         vertices[i].x = centerX + (size * cos(i * 2 * M_PI / 6)) * (i % 3 == 0 ? stretchFactor : 1);
        vertices[i].y = centerY + size * sin(i * 2 * M_PI / 6);
    }

    DFBCHECK(primary->SetColor(primary, 0x70, 0x00, 0x70, 0x80)); // Color for the hexagon
    for (i = 0; i < 6; ++i) {
        DFBCHECK(primary->FillTriangle(
            primary,
            centerX,
            centerY,
            vertices[i].x,
            vertices[i].y,
            vertices[(i + 1) % 6].x,
            vertices[(i + 1) % 6].y
        ));
    }

    // Define positions for YES and NO options inside the hexagon
    int optionYesX = centerX - size / 4;
    int optionNoX = centerX + size / 4;
    int optionsY = centerY + size / 4;  // Vertical position is the same for both options


    // Set font for text
    fontDesc.height = 32;
    DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
    DFBCHECK(primary->SetFont(primary, fontInterface));

  // Calculate the vertical position offsets based on font size
    int lineSpacing = fontDesc.height + 10; // 10 pixels spacing between lines
    int firstLineY = centerY - verticalSide / 4; // Adjust as needed
    int secondLineY = firstLineY + lineSpacing; // Position of second line

    // Draw the message text
    DFBCHECK(primary->SetColor(primary, 0xff, 0xff, 0xff, 0xff)); // White color for the text
    DFBCHECK(primary->DrawString(primary, messageLine1, -1, centerX, firstLineY, DSTF_CENTER));
    DFBCHECK(primary->DrawString(primary, messageLine2, -1, centerX, secondLineY, DSTF_CENTER));

     // Draw the first option (YES)
    DFBCHECK(primary->SetColor(primary, highlight == 1 ? 0xff : 0xff, highlight == 1 ? 0xff : 0xff, highlight == 1 ? 0x00 : 0xff, 0xff));
    DFBCHECK(primary->DrawString(primary, firstOption, -1, optionYesX, optionsY, DSTF_CENTER));

    // Draw the second option (NO)
    DFBCHECK(primary->SetColor(primary, highlight == 2 ? 0xff : 0xff, highlight == 2 ? 0xff : 0xff, highlight == 2 ? 0x00 : 0xff, 0xff));
    DFBCHECK(primary->DrawString(primary, secondOption, -1, optionNoX, optionsY, DSTF_CENTER));

 // Draw rectangles around options if highlighted
    if (highlight == 1) {
        // Rectangle around "YES"
        DFBCHECK(primary->DrawRectangle(primary, optionYesX - size / 8, optionsY - fontDesc.height / 2, size / 4, fontDesc.height));
    } else if (highlight == 2) {
        // Rectangle around "NO"
        DFBCHECK(primary->DrawRectangle(primary, optionNoX - size / 8, optionsY - fontDesc.height / 2, size / 4, fontDesc.height));
    }
    // Flip the primary surface to update the display
    DFBCHECK(primary->Flip(primary, NULL, 0));
}
