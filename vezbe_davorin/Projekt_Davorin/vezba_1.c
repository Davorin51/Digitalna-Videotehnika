#include "thread.h"



int32_t main()
{    


    pthread_t remote;


   
    pthread_create(&remote, NULL, &myThreadRemote, NULL);
    pthread_join(remote, NULL);



    return 0;
}
