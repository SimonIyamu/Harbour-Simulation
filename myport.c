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
#include <wait.h>
#include "dataTypes.h"

int main(int argc, char *argv[]){
    char *config_file=NULL, id_string[INT_LENGTH]; 
    ShmStruct *sp;
    int val, i, id, capacity[3], capacity_sum=0, n;
    PublicLedger pl;
    FILE *fp;
    pid_t port_master_id, monitor_id;
    srand(time(NULL));

    /* Handling command line arguments */
    if(argc==5){
        for(i=1 ; i<argc ; i++){
            if(!strcmp(argv[i],"-l"))
                config_file = argv[i+1];
            if(!strcmp(argv[i],"-n"))
                n = atoi(argv[i+1]);
        }
    }
    else{
        printf("Wrong ammount of arguments.\n");
        return(1);
    }
    printf("%d\n",n);

    /* Reading config file */
    if((fp = fopen(config_file,"r")) == NULL){
        printf("Error in opening %s file\n",config_file);
        exit(1);
    }
    for(i=0 ; i<3 ; i++) {
        fscanf(fp,"%d", &capacity[i]);
        capacity_sum += capacity[i];
    }
    fclose(fp);

    /* Make shared memory segment. */
    id = shmget(IPC_PRIVATE, sizeof(ShmStruct) + capacity_sum * sizeof(Record) , SEGMENTPERM);
    if (id==-1){
        printf("Error creating shared memory.\n");
        exit(2);
    }
    printf("Shared memory allocated. Id : %d\n",id);
    sprintf(id_string,"%d",id);

    /* Attach to the segment. */
    sp = (ShmStruct *) shmat(id, (void*) 0, 0);
    if (sp == (void *) -1){
        printf("Error attaching to shared segment.\n");
        exit(2);
    }

    /* Initialize semaphores */
    if( sem_init((sem_t *) &(sp->applicationReady) ,1,0) ||
        sem_init((sem_t *) &(sp->responseReady) ,1,0) ||
        sem_init((sem_t *) &(sp->traffic) ,1,1) ||
        sem_init((sem_t *) &(sp->someoneLeft) ,1,0) ||
        sem_init((sem_t *) &(sp->pl_mutex) ,1,1) ||
        sem_init((sem_t *) &(sp->app_mutex) ,1,1) ){
        printf("Error initializing semaphores.\n");
        exit(2);
    }

    /* Initialize counters */
    sp->waiting_vessels = 0;
    for(i=0 ; i<3 ; i++){
        sp->parked_vessels[i] = 0;
        sp->stats.total_vessels[i] = 0;
        sp->stats.total_profit[i] = 0;
        sp->stats.total_waiting_time[i] = 0;
    }

    /* Initialize Public Ledger */
    Record *ptr =(Record *) ((ShmStruct *) sp + 1 );
    for(i = 0 ; i < capacity_sum ; i++) ptr[i].isFree=true;
    
    /* Write capacities to the shared memory */
    for(i=0 ; i<3 ; i++) sp->capacity[i] = capacity[i];

    sp->start_time = time(NULL);

    /* New port-master */
    switch(port_master_id = fork()){
        case -1:
            perror("Failed to fork\n");
            exit(1);
        case 0: //child
            execl("./port-master","port-master","-c","charges.txt","-s",id_string,NULL);
            printf("Error in execl\n");
            exit(1);
        default: //parent
            break;
    }

    /* New monitor */
    switch(monitor_id = fork()){
        case -1:
            perror("Failed to fork\n");
            exit(1);
        case 0: //child
            execl("./monitor","monitor","-d","1","-t","5","-s",id_string,NULL);
            printf("Error in execl\n");
            exit(1);
        default: //parent
            break;
    }

    char vessel_name[20],park_period_string[INT_LENGTH],type[2],post_type[2];
    int park_period;
    for(i=0 ; i < n ; i++){
        switch (rand()%3){
            case 0:
                strcpy(type,"S");
                switch (rand()%3){
                            case 0:
                                strcpy(post_type,"S");
                                break;
                            case 1:
                                strcpy(post_type,"M");
                                break;
                            case 2:
                                strcpy(post_type,"L");
                                break;
                            default:
                                break;
                        }
                break;
            case 1:
                strcpy(type,"M");
                if(rand()%2)
                    strcpy(post_type,"M");
                else
                    strcpy(post_type,"L");
                break;
            case 2:
                strcpy(type,"L");
                strcpy(post_type,"L");
                break;
            default:
                break;
        }

        park_period = rand()%5+1;
        sprintf(vessel_name,"ves%d",i+1);
        sprintf(park_period_string,"%d",park_period);
        /* New vessel */
        switch(fork()){
            case -1:
                perror("Failed to fork\n");
                exit(1);
            case 0: //child
                execl("./vessel","vessel","-n",vessel_name,"-t",type,"-u",post_type,"-p",park_period_string,"-m","1","-s",id_string,NULL);
                printf("Error in execl\n");
                exit(1);
            default: //parent
                break;
        }

        sleep(rand()%3);
    }

    getchar();

    kill(port_master_id, SIGUSR2);
    kill(monitor_id, SIGUSR2);
    waitpid(port_master_id,NULL, WUNTRACED | WCONTINUED);
    waitpid(monitor_id,NULL, WUNTRACED | WCONTINUED);

    /* Destroy semaphores */
    sem_destroy(&(sp->applicationReady));
    sem_destroy(&(sp->responseReady));
    sem_destroy(&(sp->app_mutex));
    sem_destroy(&(sp->pl_mutex));
    sem_destroy(&(sp->traffic));
    sem_destroy(&(sp->someoneLeft));

    /* Remove shared segment */
    int err = shmctl(id, IPC_RMID, 0);
    if(err == -1)
        printf("Error removing shared segment");
    else
        printf("Shared segment was removed.\n");

    return 0;
}
