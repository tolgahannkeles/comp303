#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILE_COUNT 10
#define SHARED_MEM_SIZE 1024 * 1024 // 1 MB shared memory

// Shared memory structure to hold results
typedef struct {
    char results[SHARED_MEM_SIZE];
    int result_size;
    sem_t semaphore;
} SharedMemory;

void search_in_file(const char* filename, const char* pos_word, const char* neg_word, SharedMemory* shm) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    int sentiment_score = 0;
    char temp_result[SHARED_MEM_SIZE] = "";

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (strstr(line, pos_word) || strstr(line, neg_word)) {
            // Calculate sentiment score
            char *pos_ptr = line;
            char *neg_ptr = line;
            
            while ((pos_ptr = strstr(pos_ptr, pos_word)) != NULL) {
                sentiment_score += 5;
                pos_ptr += strlen(pos_word);
            }
            
            while ((neg_ptr = strstr(neg_ptr, neg_word)) != NULL) {
                sentiment_score -= 3;
                neg_ptr += strlen(neg_word);
            }

            // Format the matching line with filename and line number
            snprintf(temp_result + strlen(temp_result), sizeof(temp_result) - strlen(temp_result),
                     "%d, %s, %d: %s",sentiment_score, filename, line_number, line );
        }
    }

    fclose(file);

    // Critical section to update shared memory
    sem_wait(&shm->semaphore);
    strncat(shm->results, temp_result, sizeof(shm->results) - shm->result_size - 1);
    shm->result_size += strlen(temp_result);
    sem_post(&shm->semaphore);
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <input_files...> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char* pos_word = argv[1];
    const char* neg_word = argv[2];
    int num_files = atoi(argv[3]);
    char* output_file = argv[4 + num_files];

    // Set up shared memory
    SharedMemory* shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap failed");
        return EXIT_FAILURE;
    }
    shm->result_size = 0;
    sem_init(&shm->semaphore, 1, 1); // Initialize unnamed semaphore

    // Start measuring execution time
    clock_t start_time = clock();

    pid_t pids[MAX_FILE_COUNT];
    for (int i = 0; i < num_files; i++) {
        if ((pids[i] = fork()) == 0) {
            // Child process
            search_in_file(argv[4 + i], pos_word, neg_word, shm);
            exit(0);
        }
    }

    // Wait for all children to finish
    for (int i = 0; i < num_files; i++) {
        waitpid(pids[i], NULL, 0);
    }

    // Write results to output file
    FILE* out_file = fopen(output_file, "w");
    if (!out_file) {
        perror("Error opening output file");
        return EXIT_FAILURE;
    }
    fprintf(out_file, "%s", shm->results);
    fclose(out_file);

    // End measuring execution time
    clock_t end_time = clock();
    double execution_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Execution Time: %.2f seconds\n", execution_time);

    // Cleanup
    sem_destroy(&shm->semaphore);
    munmap(shm, sizeof(SharedMemory));

    return EXIT_SUCCESS;
}