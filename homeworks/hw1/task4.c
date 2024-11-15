#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

#define MAX_FILES 100
#define SHARED_DATA_SIZE 60000  // Bellek boyutunu artırdık

typedef struct {
    char filename[256];
    int line_number;
    char line[512];
    int sentiment_score;
} result_t;

typedef struct {
    const char *filename;
    const char *positive_word;
    const char *negative_word;
    result_t *results;
    int *result_count;
    sem_t *sem;
} ThreadData;

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

void *search_in_file(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    FILE *file = fopen(data->filename, "r");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    char line[512];
    int line_number = 0;
    int total_sentiment = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        int positive_count = count_word_occurrences(line, data->positive_word);
        int negative_count = count_word_occurrences(line, data->negative_word);
        int sentiment_score = positive_count * 5 - negative_count * 3;

        if (sentiment_score != 0) {
            sem_wait(data->sem);
            if (*data->result_count < SHARED_DATA_SIZE) {
                result_t *result = &data->results[*data->result_count];
                strncpy(result->filename, data->filename, sizeof(result->filename));
                result->line_number = line_number;
                strncpy(result->line, line, sizeof(result->line));
                result->sentiment_score = sentiment_score;
                (*data->result_count)++;
            }
            sem_post(data->sem);
            total_sentiment += sentiment_score;
        }
    }
    printf("Total sentiment score for %s: %d\n", data->filename, total_sentiment);
    fclose(file);
    return NULL;
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
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> ... <fileN> <output_file>\n", argv[0]);
        exit(1);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal4.c |\n");
    printf("----------------\n");

    char *positive_word = argv[1];
    char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    pthread_t threads[MAX_FILES];
    ThreadData thread_data[MAX_FILES];

    result_t *shared_memory = malloc(SHARED_DATA_SIZE * sizeof(result_t));
    if (shared_memory == NULL) {
        perror("malloc");
        exit(1);
    }

    int result_count = 0;
    sem_t sem;
    sem_init(&sem, 0, 1);

    for (int i = 0; i < num_files; i++) {
        thread_data[i].filename = argv[4 + i];
        thread_data[i].positive_word = positive_word;
        thread_data[i].negative_word = negative_word;
        thread_data[i].results = shared_memory;
        thread_data[i].result_count = &result_count;
        thread_data[i].sem = &sem;
        pthread_create(&threads[i], NULL, search_in_file, &thread_data[i]);
    }

    for (int i = 0; i < num_files; i++) {
        pthread_join(threads[i], NULL);
    }

    // Sort the results
    qsort(shared_memory, result_count, sizeof(result_t), compare_results);

    // Write results to the output file
    FILE *output_file_ptr = fopen(output_file, "w");
    if (!output_file_ptr) {
        perror("fopen");
        free(shared_memory);
        sem_destroy(&sem);
        exit(1);
    }

    for (int i = 0; i < result_count; i++) {
        fprintf(output_file_ptr, "%s, %d: %s", shared_memory[i].filename, shared_memory[i].line_number, shared_memory[i].line);
    }

    fclose(output_file_ptr);

    free(shared_memory);
    sem_destroy(&sem);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");

    return 0;
}