#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>

#define MAX_LINE 1024

// Helper function to remove punctuation from a word
void sanitize_word(char *word) {
    char *src = word, *dst = word;
    while (*src) {
        if (!ispunct((unsigned char)*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

// Function to calculate the total size of input files
size_t calculate_total_file_size(int file_count, char *file_names[]) {
    size_t total_size = 0;
    struct stat st;

    for (int i = 0; i < file_count; i++) {
        if (stat(file_names[i], &st) == 0) {
            total_size += st.st_size;
        } else {
            perror("Failed to get file size");
            exit(1);
        }
    }
    return total_size;
}

// Function to search in a file and write results to shared memory
void search_in_file(const char *filename, const char *positive_word, const char *negative_word, char *shared_mem, sem_t *sem) {
    FILE *input = fopen(filename, "r");
    if (!input) {
        perror("File opening failed");
        exit(1);
    }

    char line[MAX_LINE];
    int line_number = 0;

    while (fgets(line, sizeof(line), input)) {
        line_number++;
        int score = 0;
        char *line_copy = strdup(line); // Make a copy of the line for output
        char *token = strtok(line, " \t\n");

        int contains_positive = 0, contains_negative = 0;
        while (token) {
            sanitize_word(token); // Remove punctuation
            if (strcmp(token, positive_word) == 0) {
                score += 5;
                contains_positive = 1;
            }
            if (strcmp(token, negative_word) == 0) {
                score -= 3;
                contains_negative = 1;
            }
            token = strtok(NULL, " \t\n");
        }

        if (contains_positive || contains_negative) {
            sem_wait(sem);
            sprintf(shared_mem + strlen(shared_mem), "%s, %d: %s", filename, line_number, line_copy);
            sem_post(sem);
        }

        free(line_copy); // Free the duplicated line
    }

    fclose(input);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s positive_word negative_word n input1.txt ... inputN.txt output.txt\n", argv[0]);
        exit(1);
    }

    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int n = atoi(argv[3]);

    if (n < 1 || argc != n + 5) {
        fprintf(stderr, "Invalid number of input files.\n");
        exit(1);
    }

    const char *output_file = argv[n + 4];

    // Calculate the required shared memory size
    size_t shared_mem_size = calculate_total_file_size(n, &argv[4]);

    // Create shared memory dynamically
    char *shared_mem = mmap(NULL, shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_mem == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    memset(shared_mem, 0, shared_mem_size);

    // Create unnamed semaphore
    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem == MAP_FAILED) {
        perror("mmap for semaphore failed");
        exit(1);
    }
    sem_init(sem, 1, 1);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    pid_t pids[n];
    for (int i = 0; i < n; i++) {
        const char *input_file = argv[4 + i];

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            search_in_file(input_file, positive_word, negative_word, shared_mem, sem);
            exit(0);
        } else if (pid > 0) {
            // Parent process
            pids[i] = pid;
        } else {
            perror("fork failed");
            exit(1);
        }
    }

    // Parent process waits for all children to finish
    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }

    // Write final output to file
    FILE *output = fopen(output_file, "w");
    if (!output) {
        perror("Output file opening failed");
        exit(1);
    }
    fprintf(output, "%s", shared_mem);
    fclose(output);

    // Cleanup
    sem_destroy(sem);
    munmap(sem, sizeof(sem_t));
    munmap(shared_mem, shared_mem_size);

    gettimeofday(&end, NULL);
    long elapsed = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("Execution time: %ld microseconds\n", elapsed);

    return 0;
}