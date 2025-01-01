#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// Semaphore initialization
sem_t *mutex, *write_lock;

// Read count initialization
int *read_count;

// Activity structure
typedef struct
{
    int operation_time;
    int thread_id;
    int table_id;
    char operation_type[6];
    int duration;
    char *written_value;
} Activity;

// Activity initialization
Activity *activities;
// Number of activities
int num_activities = 0;

// Thread initialization
pthread_t *threads;

// Table initialization
char **tables;

// Log file initialization
FILE *log_file;

// Funtion to create file with given filename
void create_file(const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        fprintf(stderr, "Error creating table: %s\n", filename);
        perror("Error");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

// Function to perform read operation
void perform_read(int thread_id, int table_id, int duration)
{
    // Wait for mutex
    sem_wait(&mutex[table_id]);
    
    // Increment read count
    read_count[table_id]++;

    // If first reader, wait for write lock
    if (read_count[table_id] == 1)
        sem_wait(&write_lock[table_id]);
    
    // Release mutex for other readers
    sem_post(&mutex[table_id]);

    // Reading operation
    fprintf(log_file, "Reader %d started reading %s\n", thread_id, tables[table_id]);
    fflush(log_file);

    // Wait for duration
    sleep(duration);

    /*Read Operation*/

    // Log the end of reading
    fprintf(log_file, "Reader %d finished reading %s\n", thread_id, tables[table_id]);
    fflush(log_file);

    // Wait for mutex
    sem_wait(&mutex[table_id]);
    // Decrement read count
    read_count[table_id]--;
    // If last reader, release write lock
    if (read_count[table_id] == 0)
        sem_post(&write_lock[table_id]);
    // Release mutex for other readers
    sem_post(&mutex[table_id]);
}


// Function to perform write operation
void perform_write(int thread_id, int table_id, int duration, const char *value)
{
    fprintf(log_file, "Writer %d waiting to write to %s\n", thread_id, tables[table_id]);

    // Wait for write lock
    sem_wait(&write_lock[table_id]);

    // Writing operation
    fprintf(log_file, "Writer %d started writing to %s\n", thread_id, tables[table_id]);
    fflush(log_file);

    // Wait for duration
    sleep(duration);

    // Write value to table file
    FILE *table_file = fopen(tables[table_id], "a");
    if (table_file)
    {
        fprintf(table_file, "%s\n", value);
        fclose(table_file);
    }

    // Log the end of writing
    fprintf(log_file, "Writer %d finished writing to %s\n", thread_id, tables[table_id]);
    fflush(log_file);

    // Release write lock
    sem_post(&write_lock[table_id]);
}

void *thread_function(void *arg)
{
    // Get thread id from arguments
    int thread_id = *(int *)arg;

    // Iterate through activities to find activities for this thread
    for (int i = 0; i < num_activities; i++)
    {
        // If activity is not for this thread, skip
        if (activities[i].thread_id != thread_id)
            continue;

        // Wait for operation time
        sleep(activities[i].operation_time);

        // Perform read or write operation based on operation type
        int table_id = activities[i].table_id - 1;
        if (strcmp(activities[i].operation_type, "read") == 0)
        {
            perform_read(thread_id, table_id, activities[i].duration);
        }
        else if (strcmp(activities[i].operation_type, "write") == 0)
        {
            perform_write(thread_id, table_id, activities[i].duration, activities[i].written_value);
        }
    }
    return NULL;
}

void parse_activity_file(const char *filename)
{
    // Open activity file
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening activity file");
        exit(EXIT_FAILURE);
    }

    // Parse each line of the activity file
    // Maximum line length is 1024
    char line[1024];

    // Read each line of the file
    while (fgets(line, sizeof(line), file))
    {
        // Reallocate memory for activities
        activities = realloc(activities, (num_activities + 1) * sizeof(Activity));
        // Get pointer to new activity
        Activity *activity = &activities[num_activities];
        // Allocate memory for written value
        activity->written_value = malloc(1024);

        // Parse activity line
        sscanf(line, "%d %d %d %s %d %[^\n]",
               &activity->operation_time,
               &activity->thread_id,
               &activity->table_id,
               activity->operation_type,
               &activity->duration,
               activity->written_value);

        // Increment number of activities
        num_activities++;
    }

    fclose(file);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <Number_of_threads> <Number_of_tables> <Activity_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Get number of threads, number of tables, and activity file from arguments
    int num_threads = atoi(argv[1]);
    int num_tables = atoi(argv[2]);
    const char *activity_file = argv[3];

    // Allocate memory for tables, semaphores, and read counts
    tables = malloc(num_tables * sizeof(char *));
    mutex = malloc(num_tables * sizeof(sem_t));
    write_lock = malloc(num_tables * sizeof(sem_t));
    read_count = calloc(num_tables, sizeof(int));

    // Create tables and initialize semaphores
    for (int i = 0; i < num_tables; i++)
    {
        // Allocate memory for table name
        tables[i] = malloc(16);

        sprintf(tables[i], "tbl%d.txt", i + 1);
        create_file(tables[i]);

        // Initialize mutex as 1, write lock as 1
        sem_init(&mutex[i], 0, 1);
        sem_init(&write_lock[i], 0, 1);
    }

    // Parse activity file
    parse_activity_file(activity_file);

    // Open log file
    log_file = fopen("logfile.txt", "w");
    if (!log_file)
    {
        perror("Error opening log file");
        return EXIT_FAILURE;
    }

    // Create threads
    threads = malloc(num_threads * sizeof(pthread_t));
    int *thread_ids = malloc(num_threads * sizeof(int));
    for (int i = 0; i < num_threads; i++)
    {
        // Create thread with thread id
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }

    // Join threads
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    fclose(log_file);

    // Free memory and destroy semaphores for each table
    for (int i = 0; i < num_tables; i++)
    {
        sem_destroy(&mutex[i]);
        sem_destroy(&write_lock[i]);
        free(tables[i]);
    }
    free(tables);
    free(mutex);
    free(write_lock);
    free(read_count);

    // Free memory for activities and threads
    for (int i = 0; i < num_activities; i++)
    {
        free(activities[i].written_value);
    }
    free(activities);
    free(threads);
    free(thread_ids);

    return EXIT_SUCCESS;
}
