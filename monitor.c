#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "dataTypes.h"

void sighandler(int);
static ShmStruct *sp;

int main(int argc, char* argv[]){
    int i,time, stat_freq, shmid;
    PublicLedger pl;

    /* Handling command line arguments */
    if(argc==7){
        int i;
        for(i=1 ; i<argc ; i++){
            if(!strcmp(argv[i],"-d")){
                time = atoi(argv[i+1]);
            }
            if(!strcmp(argv[i],"-t"))
                stat_freq = atoi(argv[i+1]);
            if(!strcmp(argv[i],"-s"))
                shmid = atoi(argv[i+1]);
       }
    }
    else{
        printf("Wrong ammount of arguments\n");
        return(1);
    }

    signal(SIGUSR2,sighandler);

    /* Attach to the shared segment. */
    sp = (ShmStruct *) shmat(shmid,(void *) 0 , 0);
    if (sp == (void *) -1){
        printf("Error attaching to shared segment.\n");
        exit(2);
    }

    /* Public Ledger arrays */
    pl.smallSpots = (Record *) ((ShmStruct *) sp + 1 );
    pl.mediumSpots = (Record *) pl.smallSpots + sp->capacity[0];
    pl.largeSpots = (Record *) pl.mediumSpots + sp->capacity[1];

    /* #################### */

    int iteration = 0;
    while(1){
        iteration++;
    
        /* Clear screen */
        printf("\033[2J\033[1;1H");

        /* Access to Public Ledger CS */
        sem_wait((sem_t *) &(sp->pl_mutex));

        /* Print Monitor Table */
        printf("\t _______________________________________________________________________\n");
        printf("\t|                                Monitor                                |\n");

        printf("\t|_______________________________________________________________________|\n");
        printf("\t|Name\t|Type\t|PostTyp|Arrival|Waiting|ParkPrd|ManTime|Parking|Cost\t|\n");
        printf("\t|_______|_______|_______|_______|_______|_______|_______|_______|_______|\n");
        for(i=0 ; i<sp->capacity[0] + sp->capacity[1] + sp->capacity[2] ; i++){
            if(!(pl.smallSpots[i].isFree)){
                printf("\t|%s\t",pl.smallSpots[i].vessel.name);
                printf("|%c\t",pl.smallSpots[i].vessel.type);
                printf("|%c\t",pl.smallSpots[i].vessel.post_type);
                printf("|%ld\t",pl.smallSpots[i].arrival_time);
                printf("|%ld\t",pl.smallSpots[i].waiting_time);
                printf("|%d\t",pl.smallSpots[i].vessel.park_period);
                printf("|%d\t",pl.smallSpots[i].vessel.man_time);
                printf("|%c\t",pl.smallSpots[i].given_parking_type);
                printf("|%d$\t|\n",pl.smallSpots[i].parking_cost);
            }   
        }
        printf("\t|_______|_______|_______|_______|_______|_______|_______|_______|_______|\n");
        printf("Current Vessels:\nSmall : %d/%d    Medium : %d/%d    Large : %d/%d\n\n",sp->parked_vessels[0],sp->capacity[0],sp->parked_vessels[1],sp->capacity[1],sp->parked_vessels[2],sp->capacity[2]);


        sem_post((sem_t *) &(sp->pl_mutex));

        if(iteration == stat_freq){
            iteration = 0;

            float avg_wt[3] = {0,0,0}; /* Average waiting time */
            float avg_profit[3] = {0,0,0}; /* Average profit */
            int total_profit = 0;
            for(i=0 ; i<3 ; i++) 
                if(sp->stats.total_vessels[i] != 0 ){
                    avg_wt[i] = sp->stats.total_waiting_time[i] / (float) sp->stats.total_vessels[i];
                    avg_profit[i] = sp->stats.total_profit[i] / (float) sp->stats.total_vessels[i];
                    total_profit += sp->stats.total_profit[i];
                }
            
            printf("Total Vessels:\nSmall : %d    Medium : %d    Large : %d\n\n",
                sp->stats.total_vessels[0],sp->stats.total_vessels[1],sp->stats.total_vessels[2]);
            printf("Average Waiting Time:\n");
            printf("Small : %.3f    Medium : %.3f    Large : %.3f\n\n",avg_wt[0],avg_wt[1],avg_wt[2]);
            printf("Average Profit:\n");
            printf("Small : %.3f$    Medium : %.3f$    Large : %.3f$\n\n",avg_profit[0],avg_profit[1],avg_profit[2]);
            printf("Total Profit: %d$\n",total_profit);
        }
            

        sleep(time);
    }

    /* #################### */

    /* Detach from the shared segment. */
    int err = shmdt((void *) sp);
    if( err == -1)
        printf("Error detaching from the shared segment \n");
    else
        printf("Monitor detached from the shared segment \n");

    return 0;
}

void sighandler(int signum){
    /* Detach from the shared segment. */
    int err = shmdt((void *) sp);
    if( err == -1)
        printf("Error detaching from the shared segment \n");
    else
        printf("Monitor detached from the shared segment \n");

    exit(0);
}
