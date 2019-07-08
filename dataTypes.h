#define SEGMENTPERM 0666
#define INT_LENGTH 10 /* because INT_MAX has 10 digits */

typedef enum { false, true } bool;

typedef struct vesselTag{
    char name[20];
    char type;
    char post_type;
    int park_period;
    int man_time;
}Vessel;

typedef struct applicationTag{
    bool isForEntering; /* False if the application is for leaving */
    time_t arrival_time;
    Vessel vessel;
}Application;

typedef struct publicLedgerRecordTag{
    bool isFree;
    Vessel vessel;
    time_t arrival_time;
    time_t waiting_time;
    int parking_cost;
    char given_parking_type; 
}Record;

typedef struct publicLedgerTag{
    Record *smallSpots; 
    Record *mediumSpots; 
    Record *largeSpots; 
}PublicLedger;

typedef struct statsTag{
    int total_vessels[3];
    int total_waiting_time[3];
    int total_profit[3];
}Stats;

typedef struct sharedSegmentStruct{
    sem_t applicationReady;
    sem_t responseReady;
    sem_t app_mutex;
    sem_t pl_mutex;
    sem_t traffic;
    sem_t someoneLeft;
    Application application;
    time_t start_time;
    bool spotFound;
    int waiting_vessels;
    int parking_cost;
    int capacity[3];
    int parked_vessels[3];
    Stats stats;
}ShmStruct;
