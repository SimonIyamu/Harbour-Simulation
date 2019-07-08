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
#include <time.h>
#include <unistd.h>
#include "dataTypes.h"

int getIndex(char);
char availableSpot(ShmStruct *, PublicLedger);
void insertRecord(ShmStruct *, PublicLedger, char, FILE *, int *);
void removeRecord(ShmStruct *,PublicLedger);
int parkingCost(ShmStruct *,PublicLedger);
void sighandler(int);
static FILE *logfp,*plh_fp;
static ShmStruct *sp;

int main(int argc, char *argv[]){
    char *charges_file=NULL, parking_type;
    int shmid, val, i, charge[3];
    FILE *fp;
    PublicLedger pl;

    /* Handling command line arguments */
    if(argc==5){
        int i;
        for(i=1 ; i<argc ; i++){
            if(!strcmp(argv[i],"-c"))
                charges_file = argv[i+1];
            if(!strcmp(argv[i],"-s"))
                shmid = atoi(argv[i+1]);
        }
    }
    else{
        printf("Wrong ammount of arguments\n");
        return(1);
    }

    signal(SIGUSR2, sighandler); /* This signal terminates the process */

    /* Reading config file */
    if((fp = fopen(charges_file,"r")) == NULL){
        printf("Error in opening %s file\n",charges_file);
        exit(1);
    }
    for(i=0 ; i<3 ; i++) fscanf(fp,"%d", &charge[i]);
    fclose(fp);

    /* Open log file  for writing*/
    if((logfp = fopen("logfile.txt","w")) == NULL){
        printf("Error in opening logfile.txt file\n");
        exit(1);
    }

    /* Open public ledger history file for writing*/
    if((plh_fp = fopen("plhistory.txt","w")) == NULL){
        printf("Error in opening plhistory.txt file\n");
        exit(1);
    }

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
   
    while(1){
        sem_wait((sem_t *) &(sp->applicationReady));
        
        if(sp->application.isForEntering){
            fprintf(logfp,"The vessel %s application to enter the port was received.\n",sp->application.vessel.name);

            /* Reading from Public Ledger CS*/
            sem_wait((sem_t *) &(sp->pl_mutex));
            parking_type = availableSpot(sp,pl);
            sem_post((sem_t *) &(sp->pl_mutex));

            if(parking_type != 'X'){
                fprintf(logfp,"The vessel %s is assigned to a %c parking spot.\n",sp->application.vessel.name,parking_type);
                /* Manoeuvre */
                sem_wait((sem_t *) &(sp->traffic));
                fprintf(logfp,"The vessel %s is manouvering.\n",sp->application.vessel.name);
                sleep(sp->application.vessel.man_time);
                sem_post((sem_t *) &(sp->traffic));

                /* Modifying Public Ledger CS*/
                sem_wait((sem_t *) &(sp->pl_mutex));
                insertRecord(sp,pl,parking_type,plh_fp,charge);
                sem_post((sem_t *) &(sp->pl_mutex));

                /* Atomic increment total_vessels of parking_type */
                __sync_fetch_and_add(&(sp->stats.total_vessels[getIndex(parking_type)]),1);

                fprintf(logfp,"The vessel %s is now parked in a %c spot.\n",sp->application.vessel.name,parking_type);
                sp->spotFound = true;
            }else{
                fprintf(logfp,"There is no available parking spot for the vessel %s at the moment.\n",sp->application.vessel.name);
                sp->spotFound = false;
            }

        }
        else{
            /* Application for leaving */
            sp->parking_cost = parkingCost(sp,pl);
            fprintf(logfp,"The vessel %s requested to leave. Parking cost: %d$.\n",sp->application.vessel.name, sp->parking_cost);

            /* Manoeuvre */
            sem_wait((sem_t *) &(sp->traffic));
            fprintf(logfp,"The vessel %s is manouvering.\n",sp->application.vessel.name);
            sleep(sp->application.vessel.man_time);
            sem_post((sem_t *) &(sp->traffic));

            /* Modifying Public Ledger CS*/
            sem_wait((sem_t *) &(sp->pl_mutex));
            removeRecord(sp,pl);
            sem_post((sem_t *) &(sp->pl_mutex));

            /* Wake all sleeping vessels up */
            for(i=0 ; i<sp->waiting_vessels ; i++) sem_post((sem_t *) &(sp->someoneLeft));
            sp->waiting_vessels = 0;

            fprintf(logfp,"The vessel %s has departed from the port.\n",sp->application.vessel.name);
        }

        /* Wake vessel up */
        sem_post((sem_t *) &(sp->responseReady));
    }
    
    /* #################### */

    fclose(logfp);
    fclose(plh_fp);

    /* Detach from the shared segment. */
    int err = shmdt((void *) sp);
    if( err == -1)
        printf("Error detaching from the shared segment \n");
    else
        printf("Port-master detached from the shared segment \n");

    return 0;
}

int getIndex(char type){
    switch(type){
        case 'S':
            return 0;
            break;
        case 'M':
            return 1;
            break;
        case 'L':
            return 2;
            break;
        default:
            return -1;
            break;
    }
}


char availableSpot(ShmStruct *sp,PublicLedger pl){
    int type_index = getIndex(sp->application.vessel.type), post_type_index = getIndex(sp->application.vessel.post_type);
    
    if(sp->parked_vessels[type_index] < sp->capacity[type_index])
        return sp->application.vessel.type;
    if(sp->parked_vessels[post_type_index] < sp->capacity[post_type_index])
        return sp->application.vessel.post_type;
    return 'X';
}

void insertRecord(ShmStruct *sp,PublicLedger pl,char parking_type, FILE *plh_fp, int *charge){
/* Copies vessel to the first available spot of the array */
    char parking_type_index=-1;
    Record *typeSpots;
    switch(parking_type){
        case 'S':
            typeSpots=pl.smallSpots;
            parking_type_index=0;
            break;
        case 'M':
            typeSpots=pl.mediumSpots;
            parking_type_index=1;
            break;
        case 'L':
            typeSpots=pl.largeSpots;
            parking_type_index=2;
            break;
        default:
            break;
    }
   
    int i=0; 
    while(i < sp->capacity[parking_type_index]){
        if(typeSpots[i].isFree){

            /* Update public ledger */
            strcpy(typeSpots[i].vessel.name,sp->application.vessel.name);
            typeSpots[i].vessel.type = sp->application.vessel.type;
            typeSpots[i].vessel.post_type = sp->application.vessel.post_type;
            typeSpots[i].vessel.park_period = sp->application.vessel.park_period;
            typeSpots[i].vessel.man_time = sp->application.vessel.man_time;
            typeSpots[i].parking_cost = 2 * sp->application.vessel.park_period * charge[getIndex(parking_type)];
            typeSpots[i].given_parking_type = parking_type;
            typeSpots[i].arrival_time = sp->application.arrival_time;
            /* Manoeuvre time is excluded from waiting_time: */
            typeSpots[i].waiting_time = (time(NULL) - sp->start_time) - sp->application.arrival_time - sp->application.vessel.man_time; 
            sp->parked_vessels[getIndex(parking_type)]++;

            __sync_fetch_and_add(&(sp->stats.total_waiting_time[parking_type_index]),typeSpots[i].waiting_time); //atomic add
            __sync_fetch_and_add(&(sp->stats.total_profit[parking_type_index]),typeSpots[i].parking_cost);

            typeSpots[i].isFree = false;
            break;
        }
        i++;
    }
}

void removeRecord(ShmStruct *sp,PublicLedger pl){
    Record *allSpots = pl.smallSpots;
    int i=0;
    while(i < sp->capacity[0] + sp->capacity[1] + sp->capacity[2]){
        if(!allSpots[i].isFree && !strcmp(allSpots[i].vessel.name,sp->application.vessel.name)){

            /* Update public ledger history */
            fprintf(plh_fp,"=============\n");
            fprintf(plh_fp,"Name: %s\n",allSpots[i].vessel.name);
            fprintf(plh_fp,"Type: %c\n",allSpots[i].vessel.type);
            fprintf(plh_fp,"Post type: %c\n",allSpots[i].vessel.post_type);
            fprintf(plh_fp,"Arrival time: %ld\n",allSpots[i].arrival_time);
            fprintf(plh_fp,"Waiting time: %ld\n",allSpots[i].waiting_time);
            fprintf(plh_fp,"Departure time: %ld\n",time(NULL) - sp->start_time);
            fprintf(plh_fp,"Park period: %d\n",allSpots[i].vessel.park_period);
            fprintf(plh_fp,"Man Time: %d\n",allSpots[i].vessel.man_time);
            fprintf(plh_fp,"Parking Spot: %c\n",allSpots[i].given_parking_type);

            (sp->parked_vessels[getIndex(allSpots[i].given_parking_type)])--;
            allSpots[i].isFree=true;
            break;
        }
        i++;
    }
}

int parkingCost(ShmStruct *sp,PublicLedger pl){
/* Looks for the application vessel in PL and returns the parking cost */
    Record *allSpots = pl.smallSpots;
    int i=0;
    while(i < sp->capacity[0] + sp->capacity[1] + sp->capacity[2]){
        if(!allSpots[i].isFree && !strcmp(allSpots[i].vessel.name,sp->application.vessel.name)){
            return allSpots[i].parking_cost;
        }
        i++;
    }
}
 
void sighandler(int signum){
    fclose(logfp); 
    fclose(plh_fp);

    /* Detach from the shared segment. */
    int err = shmdt((void *) sp);
    if( err == -1)
        printf("Error detaching from the shared segment \n");
    else
        printf("Port-master detached from the shared segment \n");

    exit(0);
}

