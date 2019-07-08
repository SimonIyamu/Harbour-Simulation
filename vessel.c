#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "dataTypes.h"

int main(int argc, char* argv[]){
    char type, post_type, given_parking_type;
    char *name;
    int park_period, man_time, shmid, val, type_index;
    int parking_cost;
    ShmStruct *sp;
    time_t arrival_time;

    /* Handling command line arguments */
    if(argc==13){
        int i;
        for(i=1 ; i<argc ; i++){
            if(!strcmp(argv[i],"-n"))
                name = argv[i+1];
            if(!strcmp(argv[i],"-t"))
                type = *argv[i+1];
            if(!strcmp(argv[i],"-u"))
                post_type = *argv[i+1];
            if(!strcmp(argv[i],"-p"))
                park_period = atoi(argv[i+1]);
            if(!strcmp(argv[i],"-m"))
                man_time = atoi(argv[i+1]);
            if(!strcmp(argv[i],"-s"))
                shmid = atoi(argv[i+1]);
        }
    }
    else{
        printf("Wrong ammount of arguments\n");
        return 1;
    }
    if(type!='S' && type!='M' && type!='L'){
        printf("Error: Wrong type of vessel (should be S,M or L).");    
        return 1;
    }
    if(post_type!='S' && post_type!='M' && post_type!='L'){
        printf("Error: Wrong post type of vessel (should be S,M or L).");    
        return 1;
    }
    if((type=='M' && post_type == 'S') || (type == 'L' && post_type!='L')){
        printf("Error: Wrong post type of vessel (should be of bigger or equal type)\n");
        return 1;
    }
    
    /* Attach to the shared segment. */
    sp = (ShmStruct *) shmat(shmid,(void *) 0 , 0);
    if (sp == (void *) -1){
        printf("Error attaching to shared segment.\n");
        exit(2);
    }

    /* #################### */

    arrival_time = time(NULL) - sp->start_time;
    bool spotFound = false;
    while(!spotFound){
        /* CS starts here */
        sem_wait((sem_t *) &(sp->app_mutex));

        /* Fill in the application for entering */
        sp->application.isForEntering = true;
        sp->application.arrival_time = arrival_time;
        strcpy(sp->application.vessel.name,name);
        sp->application.vessel.type = type;
        sp->application.vessel.post_type = post_type;
        sp->application.vessel.park_period = park_period;
        sp->application.vessel.man_time = man_time;

        /* Notify the post master that the application is writen to the shared */
        /* memory and wait for his response */
        sem_post((sem_t *) &(sp->applicationReady));
        sem_wait((sem_t *) &(sp->responseReady));

        /* Read success/fail */
        spotFound = sp->spotFound;

        /* CS ends here */
        sem_post((sem_t *) &(sp->app_mutex));

        if(!spotFound){
            __sync_fetch_and_add(&(sp->waiting_vessels),1); // atomic ++
            sem_wait((sem_t *) &(sp->someoneLeft));
        }
    }

    /* Stay in the port */
    sleep(park_period);

    /* CS starts here */
    sem_wait((sem_t *) &(sp->app_mutex));

    /* Fill in the application for leaving */
    sp->application.isForEntering = false;
    strcpy(sp->application.vessel.name,name);
    sp->application.vessel.type = type;
    sp->application.vessel.post_type = post_type;
    sp->application.vessel.park_period = park_period;
    sp->application.vessel.man_time = man_time;

    /* Notify the post master that the application is writen to the shared */
    /* memory and wait for his response */
    sem_post((sem_t *) &(sp->applicationReady));
    sem_wait((sem_t *) &(sp->responseReady));

    /* Read parking cost from shared memory */
    parking_cost = sp->parking_cost;

    /* CS ends here */
    sem_post((sem_t *) &(sp->app_mutex));

    /* #################### */

    /* Detach from the shared segment. */
    int err = shmdt((void *) sp);
    if( err == -1)
        printf("Error detaching from the shared segment \n");

    return 0;
}
