#include "remote.h"


#define NON_STOP    1



int32_t main()
{    

    pthread_t remote_thread;
    if(pthread_create(&remote_thread, NULL, remote_control_handler, NULL)){
        perror("Error creating thread!");
        return ERROR;
    }

    pthread_join(remote_thread, NULL);

}
