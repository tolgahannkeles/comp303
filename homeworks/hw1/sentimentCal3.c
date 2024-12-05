#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define MAX_FILES 10

int is_standalone_word(const char *str, const char *word) {
    const char *pos = strstr(str, word);
    while (pos != NULL) {
        if ((pos == str || !isalnum((unsigned char)pos[-1])) &&
            !isalnum((unsigned char)pos[strlen(word)])) {
            return 1;
        }
        pos = strstr(pos + 1, word);
    }
    return 0;
}

void process_input_file(char *input_file, char *positive_word, char *negative_word, int pipe_fd) {
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
    char *buffer = NULL;
    size_t buffer_size = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        line_number++;
        char *original_line = strdup(line);
        if (is_standalone_word(line, positive_word)) {
            sentiment_score += 5;
        }
        if (is_standalone_word(line, negative_word)) {
            sentiment_score -= 3;
        }
        if (sentiment_score != 0) {
            size_t needed_size = snprintf(NULL, 0, "%s, %d: %sSentiment Score: %d\n", input_file, line_number, original_line, sentiment_score) + 1;
            buffer = realloc(buffer, buffer_size + needed_size);
            if (buffer == NULL) {
                printf("Error: realloc failed\n");
                free(original_line);
                free(line);
                fclose(file);
                exit(1);
            }
            snprintf(buffer + buffer_size, needed_size, "%s, %d: %sSentiment Score: %d\n", input_file, line_number, original_line, sentiment_score);
            buffer_size += needed_size - 1;
        }
        free(original_line);
        sentiment_score = 0; // Reset sentiment score for the next line
    }
    free(line);
    write(pipe_fd, buffer, buffer_size);
    free(buffer);
    fclose(file);
    close(pipe_fd); // Close the write end of the pipe
}

void collect_results(int num_files, char *final_output_file, int pipes[][2]) {
    FILE *final_output = fopen(final_output_file, "w");
    if (final_output == NULL) {
        printf("Error: Could not open final output file\n");
        exit(1);
    }

    for (int i = 0; i < num_files; i++) {
        close(pipes[i][1]); // Close the write end of the pipe in the parent process
        char *buffer = NULL;
        size_t buffer_size = 0;
        ssize_t bytes_read;
        while (1) {
            buffer = realloc(buffer, buffer_size + 1024);
            if (buffer == NULL) {
                printf("Error: realloc failed\n");
                fclose(final_output);
                exit(1);
            }
            bytes_read = read(pipes[i][0], buffer + buffer_size, 1024);
            if (bytes_read <= 0) {
                break;
            }
            buffer_size += bytes_read;
        }
        if (fwrite(buffer, 1, buffer_size, final_output) != buffer_size) {
            printf("Error: fwrite failed\n");
            free(buffer);
            fclose(final_output);
            exit(1);
        }
        free(buffer);
        close(pipes[i][0]); // Close the read end of the pipe
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

    int pipes[MAX_FILES][2];

    for (int i = 0; i < num_files; i++) {
        if (pipe(pipes[i]) < 0) {
            printf("Error: Pipe creation failed\n");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0) {
            printf("Error: Fork failed\n");
            exit(1);
        } else if (pid == 0) {
            close(pipes[i][0]); // Close the read end of the pipe in the child process
            process_input_file(argv[4 + i], positive_word, negative_word, pipes[i][1]);
            exit(0);
        }
    }

    for (int i = 0; i < num_files; i++) {
        wait(NULL); // Wait for all child processes to finish
    }

    collect_results(num_files, final_output_file, pipes);

    clock_t end_time = clock();
    double execution_time = ((double) (end_time - start_time)) / CLOCKS_PER_SEC;
    printf("Execution time: %.10f seconds\n", execution_time);

    return 0;
}