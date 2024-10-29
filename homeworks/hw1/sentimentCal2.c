#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILES 10

typedef struct {
    char content[MAX_LINE_LENGTH];
    int line_number;
} SharedData;

void process_file(const char* filename, const char* positive_word, const char* negative_word, SharedData *shared_data, sem_t *sem) {
    FILE *file = fopen(filename, "r");
    char line[MAX_LINE_LENGTH];
    int line_number = 0;

    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_number++;
        int score = 0;
        if (strstr(line, positive_word)) score += 5;
        if (strstr(line, negative_word)) score -= 3;

        if (score != 0) {
            sem_wait(sem);
            snprintf(shared_data->content, MAX_LINE_LENGTH, "%s, %d: %s", filename, line_number, line);
            shared_data->line_number = line_number;
            sem_post(sem);
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s positive_word negative_word num_files file1 ... output_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    char *output_filename = argv[4 + num_files];

    sem_t *sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(sem, 1, 1);

    SharedData *shared_data = mmap(NULL, sizeof(SharedData) * num_files, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    pid_t pids[MAX_FILES];
    clock_t start = clock();

    for (int i = 0; i < num_files; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            process_file(argv[4 + i], positive_word, negative_word, &shared_data[i], sem);
            exit(0);
        }
    }

    for (int i = 0; i < num_files; i++) {
        waitpid(pids[i], NULL, 0);
    }

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Output file opening failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_files; i++) {
        fprintf(output_file, "%s", shared_data[i].content);
    }

    fclose(output_file);

    sem_destroy(sem);
    munmap(sem, sizeof(sem_t));
    munmap(shared_data, sizeof(SharedData) * num_files);

    printf("Execution time: %.2f seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);
    return 0;
}
