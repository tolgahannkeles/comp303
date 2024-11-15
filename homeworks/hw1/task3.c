#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>


#define MAX_LINE_LENGTH 1024
#define MAX_FILES 10
#define PIPE_BUFFER_SIZE 4096

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

void process_file(const char* filename, const char* positive_word, const char* negative_word, int pipe_fd) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    char buffer[PIPE_BUFFER_SIZE];

    int total_sentiment = 0;

    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_number++;
      
        int num_positive = count_word_occurrences(line, positive_word);
        int num_negative = count_word_occurrences(line, negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0) {
            snprintf(buffer, PIPE_BUFFER_SIZE, "%s, %d: %s", filename, line_number, line);
            write(pipe_fd, buffer, strlen(buffer));
            total_sentiment += sentiment_score;
        }
    }

    printf("Total sentiment score for %s: %d\n", filename, total_sentiment);

    fclose(file);
    close(pipe_fd); // Close the write end of the pipe
}

int main(int argc, char *argv[]) {
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal3.c |\n");
    printf("----------------\n");


    if (argc < 5) {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> [<file2> ... <fileN>]\n", argv[0]);
        exit(1);
    }

    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pids[MAX_FILES];
    for (int i = 0; i < num_files; i++) {
        if ((pids[i] = fork()) == 0) {
            // Child process
            close(pipe_fd[0]); // Close the read end of the pipe
            process_file(argv[i + 4], positive_word, negative_word, pipe_fd[1]);
            exit(0);
        }
    }

    // Parent process
    close(pipe_fd[1]); // Close the write end of the pipe

    // Write results to output file
    FILE *outfile = fopen(output_file, "w");
    if (outfile == NULL) {
        perror("fopen");
        exit(1);
    }

    char buffer[PIPE_BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(pipe_fd[0], buffer, PIPE_BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytes_read, outfile);
    }

    close(pipe_fd[0]); // Close the read end of the pipe
    fclose(outfile);

    // Wait for all child processes to finish
    for (int i = 0; i < num_files; i++) {
        waitpid(pids[i], NULL, 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");

    return 0;
}