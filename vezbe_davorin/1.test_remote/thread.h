#include <stdio.h>
#include <linux/input.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>


#include <directfb.h>

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

void *myThreadRemote();
void setupScreen();

IDirectFBSurface *primary = NULL; //glavni, u njemu se radi sve
IDirectFB *dfbInterface = NULL; //sadrzi razne podatke o tom interfejsu
int screenWidth = 0;
int screenHeight = 0;
DFBSurfaceDescription surfaceDesc; 

IDirectFBFont *fontInterface = NULL;
DFBFontDescription fontDesc;
