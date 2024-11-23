#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/mman.h>
#include <semaphore.h>

#define MAX_FILES 10
#define MAX_LINES 1000
#define MAX_LINE_LENGTH 1024

typedef struct {
    char lines[MAX_FILES][MAX_LINES][MAX_LINE_LENGTH];
    int line_counts[MAX_FILES];
    sem_t semaphores[MAX_FILES];
} shared_data_t;

void remove_punctuation(char *str) {
    // Pointers to traverse the string
    char *src = str, *dst = str;

    // Loop through each character in the string
    while (*src) {
        // If the character is a punctuation, skip it
        if (ispunct((unsigned char)*src)) {
            src++;
        } 
        // If the source and destination pointers are the same, just move both forward
        else if (src == dst) {
            src++;
            dst++;
        } 
        // Otherwise, copy the character from source to destination and move both pointers forward
        else {
            *dst++ = *src++;
        }
    }
    // Null-terminate the destination string
    *dst = '\0';
}

void process_input_file(char *input_file, char *positive_word, char *negative_word, int file_index, shared_data_t *shared_data) {
    FILE *file = fopen(input_file, "r");
    if (file == NULL) {
        printf("Error: File not found\n");
        exit(1);
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int line_number = 0;
    int sentiment_score = 0;
    while ((read = getline(&line, &len, file)) != -1) {
        line_number++;
        char *original_line = strdup(line);
        if (original_line == NULL) {
            printf("Error: strdup failed\n");
            fclose(file);
            free(line);
            exit(1);
        }
        remove_punctuation(line); // Remove punctuation from the line
        char *tokenized = strtok(line, " ");
        while (tokenized != NULL) {
            if (strcmp(tokenized, positive_word) == 0) {
                sentiment_score += 5;
            } else if (strcmp(tokenized, negative_word) == 0) {
                sentiment_score -= 3;
            }
            tokenized = strtok(NULL, " ");
        }
        if (sentiment_score != 0) {
            if (sem_wait(&shared_data->semaphores[file_index]) != 0) {
                printf("Error: sem_wait failed\n");
                fclose(file);
                free(line);
                free(original_line);
                exit(1);
            }
            snprintf(shared_data->lines[file_index][shared_data->line_counts[file_index]], MAX_LINE_LENGTH, 
                     "%s, %d: %sSentiment Score: %d\n", input_file, line_number, original_line, sentiment_score);
            shared_data->line_counts[file_index]++;
            if (sem_post(&shared_data->semaphores[file_index]) != 0) {
                printf("Error: sem_post failed\n");
                fclose(file);
                free(line);
                free(original_line);
                exit(1);
            }
        }
        free(original_line);
        sentiment_score = 0; // Reset sentiment score for the next line
    }
    free(line);
    fclose(file);
}

void collect_results(int num_files, char *final_output_file, shared_data_t *shared_data) {
    FILE *final_output = fopen(final_output_file, "w");
    if (final_output == NULL) {
        printf("Error: Could not open final output file\n");
        exit(1);
    }

    for (int i = 0; i < num_files; i++) {
        if (sem_wait(&shared_data->semaphores[i]) != 0) {
            printf("Error: sem_wait failed\n");
            fclose(final_output);
            exit(1);
        }
        for (int j = 0; j < shared_data->line_counts[i]; j++) {
            fputs(shared_data->lines[i][j], final_output);
        }
        if (sem_post(&shared_data->semaphores[i]) != 0) {
            printf("Error: sem_post failed\n");
            fclose(final_output);
            exit(1);
        }
    }

    fclose(final_output);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Usage: %s <positive_word> <negative_word> <num_files> <input_file1> [<input_file2> ...] <output_file>\n", argv[0]);
        return 1;
    }

    clock_t start_time = clock();

    char *positive_word = argv[1];
    char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    char *final_output_file = argv[argc - 1];

    shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_data == MAP_FAILED) {
        printf("Error: mmap failed\n");
        exit(1);
    }

    for (int i = 0; i < num_files; i++) {
        if (sem_init(&shared_data->semaphores[i], 1, 1) != 0) {
            printf("Error: sem_init failed\n");
            munmap(shared_data, sizeof(shared_data_t));
            exit(1);
        }
        shared_data->line_counts[i] = 0;
    }

    for (int i = 0; i < num_files; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("Error: Fork failed\n");
            munmap(shared_data, sizeof(shared_data_t));
            exit(1);
        } else if (pid == 0) {
            process_input_file(argv[4 + i], positive_word, negative_word, i, shared_data);
            exit(0);
        }
    }

    for (int i = 0; i < num_files; i++) {
        wait(NULL); // Wait for all child processes to finish
    }

    collect_results(num_files, final_output_file, shared_data);

    for (int i = 0; i < num_files; i++) {
        if (sem_destroy(&shared_data->semaphores[i]) != 0) {
            printf("Error: sem_destroy failed\n");
            munmap(shared_data, sizeof(shared_data_t));
            exit(1);
        }
    }

    if (munmap(shared_data, sizeof(shared_data_t)) != 0) {
        printf("Error: munmap failed\n");
        exit(1);
    }

    clock_t end_time = clock();
    double execution_time = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    printf("Execution time: %.5f seconds\n", execution_time);

    return 0;
}