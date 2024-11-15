#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>

#define SHARED_DATA_SIZE 60000  // Bellek boyutunu artırdık

typedef struct {
    char filename[256];
    int line_number;
    char line[512];
    int sentiment_score;
} result_t;

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

void search_in_file(const char *filename, const char *positive_word, const char *negative_word, sem_t *sem, result_t *results, int *result_count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        return;
    }

    char line[512];
    int line_number = 0;
    int total_sentiment = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        int positive_count = count_word_occurrences(line, positive_word);
        int negative_count = count_word_occurrences(line, negative_word);
        int sentiment_score = positive_count * 5 - negative_count * 3;

        if (sentiment_score != 0) {
            sem_wait(sem);
            if (*result_count < SHARED_DATA_SIZE) {
                result_t *result = &results[*result_count];
                strncpy(result->filename, filename, sizeof(result->filename));
                result->line_number = line_number;
                strncpy(result->line, line, sizeof(result->line));
                result->sentiment_score = sentiment_score;
                (*result_count)++;
            }
            sem_post(sem);
            total_sentiment += sentiment_score;
        }
    }
    printf("Total sentiment score for %s: %d\n", filename, total_sentiment);
    fclose(file);
}

int compare_results(const void *a, const void *b) {
    result_t *result_a = (result_t *)a;
    result_t *result_b = (result_t *)b;
    int filename_cmp = strcmp(result_a->filename, result_b->filename);
    if (filename_cmp != 0) {
        return filename_cmp;
    }
    return result_a->line_number - result_b->line_number;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s positive_word negative_word n input1.txt ... inputN.txt output.txt\n", argv[0]);
        exit(1);
    }
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal2.c |\n");
    printf("----------------\n");

    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int n = atoi(argv[3]);
    const char *output_filename = argv[argc - 1];

    // Create shared memory with anonymous mapping
    result_t *shared_memory = mmap(NULL, SHARED_DATA_SIZE * sizeof(result_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Initialize shared memory
    memset(shared_memory, 0, SHARED_DATA_SIZE * sizeof(result_t));

    // Create an unnamed semaphore
    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem == MAP_FAILED) {
        perror("mmap");
        munmap(shared_memory, SHARED_DATA_SIZE * sizeof(result_t));
        exit(1);
    }

    if (sem_init(sem, 1, 1) == -1) {
        perror("sem_init");
        munmap(shared_memory, SHARED_DATA_SIZE * sizeof(result_t));
        munmap(sem, sizeof(sem_t));
        exit(1);
    }

    int *result_count = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (result_count == MAP_FAILED) {
        perror("mmap");
        sem_destroy(sem);
        munmap(shared_memory, SHARED_DATA_SIZE * sizeof(result_t));
        munmap(sem, sizeof(sem_t));
        exit(1);
    }
    *result_count = 0;

    pid_t pids[n];

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork");
            sem_destroy(sem);
            munmap(shared_memory, SHARED_DATA_SIZE * sizeof(result_t));
            munmap(sem, sizeof(sem_t));
            munmap(result_count, sizeof(int));
            exit(1);
        }

        if (pids[i] == 0) {
            // Child process
            search_in_file(argv[4 + i], positive_word, negative_word, sem, shared_memory, result_count);
            exit(0);
        }
    }

    // Parent process waits for all children to finish
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }

    // Sort the results
    qsort(shared_memory, *result_count, sizeof(result_t), compare_results);

    // Write results to the output file
    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("fopen");
        sem_destroy(sem);
        munmap(shared_memory, SHARED_DATA_SIZE * sizeof(result_t));
        munmap(sem, sizeof(sem_t));
        munmap(result_count, sizeof(int));
        exit(1);
    }

    for (int i = 0; i < *result_count; i++) {
        fprintf(output_file, "%s, %d: %s", shared_memory[i].filename, shared_memory[i].line_number, shared_memory[i].line);
    }

    fclose(output_file);

    // Destroy the semaphore
    if (sem_destroy(sem) == -1) {
        perror("sem_destroy");
    }

    // Unmap the shared memory
    if (munmap(shared_memory, SHARED_DATA_SIZE * sizeof(result_t)) == -1) {
        perror("munmap");
    }

    if (munmap(sem, sizeof(sem_t)) == -1) {
        perror("munmap");
    }

    if (munmap(result_count, sizeof(int)) == -1) {
        perror("munmap");
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");

    return 0;
}