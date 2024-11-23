#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILE_COUNT 10
#define SHARED_MEM_SIZE (1024 * 1024 * 500)

// Struct to hold each line's information
typedef struct
{
    char filename[256];
    int line_number;
    char line[MAX_LINE_LENGTH];
} MmapData;

// Shared memory structure to hold results
typedef struct
{
    MmapData lines[SHARED_MEM_SIZE / sizeof(MmapData)];
    int line_count;
    sem_t semaphore;
} SharedMemory;

// Function to check if a character is a word boundary by checking if the character is an alphabet or not
int is_word_boundary(char c)
{
    return !isalnum(c);
}

// Function to count the number of occurrences of a word in a line
int count_word_in_line(const char *line, const char *word)
{
    int count = 0;
    const char *current_position = line;
    size_t word_length = strlen(word);

    while ((current_position = strstr(current_position, word)) != NULL)
    {
        // Check if the found word is a separate word
        int is_start_of_word = (current_position == line || is_word_boundary(*(current_position - 1)));
        int is_end_of_word = is_word_boundary(*(current_position + word_length));

        if (is_start_of_word && is_end_of_word)
        {
            count++;
        }

        // Move past the current match to continue searching
        current_position += word_length;
    }

    return count;
}

// Compare function for sorting the MmapData array
int compare_lines(const void *a, const void *b)
{
    MmapData *line1 = (MmapData *)a;
    MmapData *line2 = (MmapData *)b;

    // Compare by filename
    int filename_cmp = strcmp(line1->filename, line2->filename);
    if (filename_cmp != 0)
    {
        return filename_cmp;
    }

    // If filenames are equal, compare by line number
    return line1->line_number - line2->line_number;
}

void process_file(const char *filename, const char *pos_word, const char *neg_word, SharedMemory *shm)
{
    // Open the file
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;

    int total_sentiment = 0;

    // Read the file line by line
    while (fgets(line, sizeof(line), file))
    {
        line_number++;

        // Calculate sentiment score for the line
        int num_positive = count_word_in_line(line, pos_word);
        int num_negative = count_word_in_line(line, neg_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0)
        {
            // Write the line to shared memory
            sem_wait(&shm->semaphore);

            // Check if there is enough space in shared memory
            if (shm->line_count < (SHARED_MEM_SIZE / sizeof(MmapData)))
            {
                // Add the line to shared memory
                MmapData *new_line = &shm->lines[shm->line_count++];
                strncpy(new_line->filename, filename, sizeof(new_line->filename) - 1);
                new_line->line_number = line_number;
                strncpy(new_line->line, line, sizeof(new_line->line) - 1);
            }
            // If there is not enough space, print an error message and exit
            else
            {
                // Release the semaphore before exiting
                fprintf(stderr, "Shared memory buffer overflow. Too many lines matched.\n");
                sem_post(&shm->semaphore);
                fclose(file);
                exit(EXIT_FAILURE);
            }
            // Release the semaphore
            sem_post(&shm->semaphore);
            total_sentiment += sentiment_score;
        }
    }

    printf("Total sentiment score for %s: %d\n", filename, total_sentiment);

    fclose(file);
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <input_files...> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    // Measure the start time
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal2.c |\n");
    printf("----------------\n");

    // Get the input arguments
    const char *pos_word = argv[1];
    const char *neg_word = argv[2];
    int num_files = atoi(argv[3]);
    char *output_file = argv[4 + num_files];

    /**
     * Set up shared memory with flags: MAP_SHARED | MAP_ANONYMOUS and PROT_READ | PROT_WRITE where
     * MAP_SHARED creates a shared mapping and
     * MAP_ANONYMOUS creates an anonymous mapping that is not backed by a file
     * PROT_READ | PROT_WRITE specifies that the memory can be read from and written to
     **/
    SharedMemory *shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap failed");
        return EXIT_FAILURE;
    }

    // Zero-initialize shared memory and initialize unnamed semaphore
    memset(shm, 0, sizeof(SharedMemory));
    // Initialize the semaphore with a value of 1
    sem_init(&shm->semaphore, 1, 1);

    // Create child processes for each input file
    pid_t pids[MAX_FILE_COUNT];
    for (int i = 0; i < num_files; i++)
    {
        if ((pids[i] = fork()) == 0)
        {
            // Child process
            process_file(argv[4 + i], pos_word, neg_word, shm);
            exit(0);
        }
    }

    // Wait for all children to finish
    for (int i = 0; i < num_files; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    // Sort the results with compare_lines function via qsort
    qsort(shm->lines, shm->line_count, sizeof(MmapData), compare_lines);

    // Write sorted results to output file
    FILE *out_file = fopen(output_file, "w");
    if (!out_file)
    {
        perror("Error opening output file");
        return EXIT_FAILURE;
    }

    // Write the sorted lines to the output file
    for (int i = 0; i < shm->line_count; i++)
    {
        fprintf(out_file, "%s, %d: %s",
                shm->lines[i].filename,
                shm->lines[i].line_number,
                shm->lines[i].line);
    }

    fclose(out_file);

    // Clean up: destroy the semaphore, unmap the shared memory
    sem_destroy(&shm->semaphore);
    munmap(shm, sizeof(SharedMemory));

    // Measure the end time and calculate the execution time
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");

    return EXIT_SUCCESS;
}
