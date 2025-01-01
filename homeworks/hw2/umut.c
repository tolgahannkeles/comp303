#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_THREADS 100
#define MAX_TABLES 100

sem_t resource, mutex;
int read_count = 0;

typedef struct
{
    int operationTime;
    int threadID;
    int tableID;
    char operationType[6];
    int duration;
    char value[256];
} activity;

pthread_mutex_t mutexLog;

void writeToTerminal(const char *message)
{
    printf("%s\n", message);
}

void log_activity(const char *message)
{
    pthread_mutex_lock(&mutexLog);
    FILE *logFile = fopen("logfile.txt", "a");
    if (logFile)
    {
        fprintf(logFile, "%s\n", message);
        fclose(logFile);
    }
    pthread_mutex_unlock(&mutexLog);
}

void *threadActivity(void *arg)
{
    activity *act = (activity *)arg; // cast arg to activity struct

    sleep(act->operationTime); // wait for operation time

    char log[512]; // log message

    if (strcmp(act->operationType, "read") == 0)
    {
        sem_wait(&mutex); // lock mutex
        read_count++;     // increment read count
        if (read_count == 1)
            sem_wait(&resource); // if first reader, lock resource
        sem_post(&mutex);        // unlock mutex

        snprintf(log, sizeof(log), "Reader %d started reading tbl%d.txt", act->threadID, act->tableID);
        writeToTerminal(log);
        log_activity(log);
        


        sleep(act->duration); // read duration

        snprintf(log, sizeof(log), "Reader %d finished reading tbl%d.txt", act->threadID, act->tableID);
        writeToTerminal(log);
        log_activity(log); // log message
        

        sem_wait(&mutex);

        read_count--;

        if (read_count == 0)
            sem_post(&resource);

        sem_post(&mutex);
    }
    else if (strcmp(act->operationType, "write") == 0)
    {
        snprintf(log, sizeof(log), "Writer %d is waiting to write to tbl%d.txt", act->threadID, act->tableID);
        log_activity(log); // log message
        writeToTerminal(log);

        sem_wait(&resource); // lock resource

        snprintf(log, sizeof(log), "Writer %d started writing to tbl%d.txt", act->threadID, act->tableID);
        log_activity(log); // log message
        writeToTerminal(log);

        sleep(act->duration);

        char filename[32];
        snprintf(filename, sizeof(filename), "tbl%d.txt", act->tableID);
        FILE *file = fopen(filename, "a");
        if (file)
        {
            fprintf(file, "%s\n", act->value);
            fclose(file);
        }

        // log the end of the writing
        snprintf(log, sizeof(log), "Writer %d finished writing to tbl%d.txt", act->threadID, act->tableID);
        writeToTerminal(log);
        log_activity(log);
       

        sem_post(&resource);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <input file> <number of threads> <number of tables>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]); // number of threads
    int num_tables = atoi(argv[2]);  // number of tables
    char *inputFile = argv[3];       // input file

    sem_init(&resource, 0, 1);           // initialize resource semaphore
    sem_init(&mutex, 0, 1);              // initialize mutex semaphore
    pthread_mutex_init(&mutexLog, NULL); // initialize log mutex

    // Create files for each table
    for (int i = 1; i < num_tables; i++)
    {
        char filename[32];
        snprintf(filename, sizeof(filename), "tbl%d.txt", i);
        FILE *file = fopen(filename, "w");
        if (file)
            fclose(file);
    }

    FILE *file = fopen(inputFile, "r"); // open input file
    if (!file)
    {
        perror("error opening input file");
        return 1;
    }

    activity activities[MAX_THREADS];
    int activity_count = 0;

    while (fscanf(file, "%d %d %d %s %d %[^\n]",
                 &activities[activity_count].operationTime,
                 &activities[activity_count].threadID,
                 &activities[activity_count].tableID,
                 &activities[activity_count].operationType,
                 &activities[activity_count].duration,
                 activities[activity_count].value)== 6){
                activity_count++;
    }
    fclose(file);

    pthread_t threads[MAX_THREADS]; // array of threads
    
    for (int i = 0; i < activity_count; i++)
    {
        pthread_create(&threads[i], NULL, threadActivity, &activities[i]);
    }

    for (int i = 0; i < activity_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&resource);
    sem_destroy(&mutex);
    pthread_mutex_destroy(&mutexLog);

    printf("All threads have finished\n Simulation completed please check logfile.txt file");
    
    return 0;    

}