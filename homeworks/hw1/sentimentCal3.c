#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILES 10

void calculate_score(const char* filename, const char* positive_word, const char* negative_word, int pipe_fd) {
    FILE *input_file = fopen(filename, "r");
    if (!input_file) {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    char result[MAX_LINE_LENGTH];

    while (fgets(line, MAX_LINE_LENGTH, input_file)) {
        line_number++;
        int score = 0;
        
        if (strstr(line, positive_word)) score += 5;
        if (strstr(line, negative_word)) score -= 3;

        if (score != 0) {
            snprintf(result, MAX_LINE_LENGTH, "%s, %d: %s", filename, line_number, line);
            write(pipe_fd, result, strlen(result) + 1);
        }
    }

    fclose(input_file);
    close(pipe_fd); // Close the write end of the pipe
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
    clock_t start = clock();

    int pipes[MAX_FILES][2];
    pid_t pids[MAX_FILES];

    // Create pipes and fork processes
    for (int i = 0; i < num_files; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe failed");
            exit(EXIT_FAILURE);
        }

        pids[i] = fork();
        if (pids[i] == 0) { // Child process
            close(pipes[i][0]); // Close read end
            calculate_score(argv[4 + i], positive_word, negative_word, pipes[i][1]);
            exit(0);
        } else {
            close(pipes[i][1]); // Close write end in the parent
        }
    }

    // Wait for all children and read from pipes
    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Output file opening failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_files; i++) {
        waitpid(pids[i], NULL, 0);

        char buffer[MAX_LINE_LENGTH];
        while (read(pipes[i][0], buffer, MAX_LINE_LENGTH) > 0) {
            fputs(buffer, output_file);
        }
        close(pipes[i][0]);
    }

    fclose(output_file);

    printf("Execution time: %.2f seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);
    return 0;
}
