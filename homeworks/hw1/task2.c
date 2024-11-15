#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILES 10
#define SHARED_DATA_SIZE 4096

// Function to check if a word is a separate word (using word boundaries)
int is_word_boundary(char c) {
    return !isalnum(c);
}

// Function to count occurrences of the word in a line
int count_word_occurrences(const char *line, const char *word) {
    int count = 0;
    const char *current_position = line;
    size_t word_length = strlen(word);

    while ((current_position = strstr(current_position, word)) != NULL) {
        // Check if the found word is a separate word
        int is_start_of_word = (current_position == line || is_word_boundary(*(current_position - 1)));
        int is_end_of_word = is_word_boundary(*(current_position + word_length));

        if (is_start_of_word && is_end_of_word) {
            count++;
        }

        // Move past the current match to continue searching
        current_position += word_length;
    }

    return count;
}

void process_file(const char* filename, const char* positive_word, const char* negative_word, char *shared_memory, sem_t *sem) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;

    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_number++;
      
        int num_positive = count_word_occurrences(line, positive_word);
        int num_negative = count_word_occurrences(line, negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0) {
            sem_wait(sem);
            snprintf(shared_memory + strlen(shared_memory), SHARED_DATA_SIZE - strlen(shared_memory), "%s, %d, score: %d -> %s", filename, line_number, sentiment_score, line);
            sem_post(sem);
        }
    }

    fclose(file);
}

void process_files(const char* positive_word, const char* negative_word, int num_files, char *files[], char *shared_memory, sem_t *sem) {
    pid_t pids[MAX_FILES];
    for (int i = 0; i < num_files; i++) {
        if ((pids[i] = fork()) == 0) {
            // Child process
            process_file(files[i], positive_word, negative_word, shared_memory, sem);
            exit(0);
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_files; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

int main(int argc, char *argv[]) {
   struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> [<file2> ... <fileN>]\n", argv[0]);
        exit(1);
    }

    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    // Create shared memory
    char *shared_memory = mmap(NULL, SHARED_DATA_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    memset(shared_memory, 0, SHARED_DATA_SIZE);

    // Initialize unnamed semaphore
    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    sem_init(sem, 1, 1);

    // Process files
    process_files(positive_word, negative_word, num_files, &argv[4], shared_memory, sem);

    // Write results to output file
    FILE *outfile = fopen(output_file, "w");
    if (outfile == NULL) {
        perror("fopen");
        exit(1);
    }
    fprintf(outfile, "%s", shared_memory);
    fclose(outfile);

    // Clean up
    sem_destroy(sem);
    munmap(sem, sizeof(sem_t));
    munmap(shared_memory, SHARED_DATA_SIZE);
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    return 0;
}