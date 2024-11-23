#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>

// Defining constraints
#define MAX_LINE_LENGTH 1024
#define MAX_FILES 10
#define PIPE_BUFFER_SIZE 4096
#define MAX_LINES 99999

// Structure to store filename, line number, and the actual line in the Pipeline
typedef struct
{
    char filename[MAX_LINE_LENGTH];
    int line_number;
    char line[MAX_LINE_LENGTH];
} PipeData;

// Function to check if a character is a word boundary by checking if the character is an alphabet or not
int is_word_boundary(char c)
{
    return !isalnum(c);
}

// Function to count the number of occurrences of a word in a line
int count_word_in_line(const char *line, const char *word)
{
    int count = 0;
    const char *current_position = line;
    size_t word_length = strlen(word);

    while ((current_position = strstr(current_position, word)) != NULL)
    {
        // Check if the found word is a separate word
        int is_start_of_word = (current_position == line || is_word_boundary(*(current_position - 1)));
        int is_end_of_word = is_word_boundary(*(current_position + word_length));

        if (is_start_of_word && is_end_of_word)
        {
            count++;
        }

        // Move past the current match to continue searching
        current_position += word_length;
    }

    return count;
}

void process_file(const char *filename, const char *positive_word, const char *negative_word, int pipe_fd)
{
    // Open input file
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    char buffer[PIPE_BUFFER_SIZE];

    int total_sentiment = 0;

    PipeData data;
    // Read the file line by line
    while (fgets(line, MAX_LINE_LENGTH, file))
    {
        line_number++;

        // Calculate sentiment score for the line
        int num_positive = count_word_in_line(line, positive_word);
        int num_negative = count_word_in_line(line, negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0)
        {
            // Fill the struct data with the filename, line number, and the actual line
            snprintf(data.filename, MAX_LINE_LENGTH, "%s", filename);
            data.line_number = line_number;
            snprintf(data.line, MAX_LINE_LENGTH, "%s", line);

            // Write the struct data to the pipe
            if (write(pipe_fd, &data, sizeof(PipeData)) == -1)
            {
                // If write fails, print an error message and exit
                perror("write");
                exit(1);
            }

            total_sentiment += sentiment_score;
        }
    }

    printf("Total sentiment score for %s: %d\n", filename, total_sentiment);

    fclose(file);
}

// Comparison function for sorting PipeData
int compare_file_lines(const void *a, const void *b)
{
    PipeData *line_a = (PipeData *)a;
    PipeData *line_b = (PipeData *)b;

    // First, compare by filename
    int filename_comparison = strcmp(line_a->filename, line_b->filename);
    if (filename_comparison != 0)
        return filename_comparison;

    // If filenames are the same, compare by line number
    return line_a->line_number - line_b->line_number;
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> [<file2> ... <fileN>] <output_file>\n", argv[0]);
        exit(1);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal3.c |\n");
    printf("----------------\n");

    // Get the input arguments
    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    // Create a pipe to communicate between parent and child processes 0: read end, 1: write end
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    // Create child processes to process each file
    pid_t pids[MAX_FILES];
    for (int i = 0; i < num_files; i++)
    {
        if ((pids[i] = fork()) == 0)
        {
            // Child process: close the read end of the pipe and process the file
            close(pipe_fd[0]); // Close the read end of the pipe
            process_file(argv[i + 4], positive_word, negative_word, pipe_fd[1]);
            close(pipe_fd[1]); // Close the write end of the pipe
            exit(0);
        }
    }

    // Parent process: close the write end of the pipe and read data
    close(pipe_fd[1]);

    // Dynamically allocate memory for storing PipeData
    PipeData *all_data = NULL;
    int data_count = 0;
    int buffer_size = 10;

    // Allocate initial memory for the data array
    all_data = malloc(buffer_size * sizeof(PipeData));
    if (!all_data)
    {
        perror("malloc");
        exit(1);
    }

    // Read the data from the pipe into the array
    while (1)
    {
        PipeData data;
        ssize_t bytes_read = read(pipe_fd[0], &data, sizeof(PipeData));

        // Break the loop if no more data to read
        if (bytes_read == 0)
        {
            break;
        }
        // Check for read error
        if (bytes_read == -1)
        {
            perror("read");
            exit(1);
        }

        // Resize the array if we run out of space
        if (data_count >= buffer_size)
        {
            // Double the buffer size when we run out of space
            buffer_size *= 2; 
            // Reallocate memory for the array
            all_data = realloc(all_data, buffer_size * sizeof(PipeData));
            if (!all_data)
            {
                // If realloc fails, print an error message and exit
                perror("realloc");
                exit(1);
            }
        }
        // Add the data to the array
        all_data[data_count++] = data;
    }

    // Close the read end of the pipe
    close(pipe_fd[0]);

    // Sort the collected data with qsort via compare_file_lines function
    qsort(all_data, data_count, sizeof(PipeData), compare_file_lines);

    // Write sorted data to the output file
    FILE *outfile = fopen(output_file, "w");
    if (outfile == NULL)
    {
        perror("fopen");
        exit(1);
    }

    // Write the sorted data to the output file line by line
    for (int i = 0; i < data_count; i++)
    {
        fprintf(outfile, "%s,%d:%s", all_data[i].filename, all_data[i].line_number, all_data[i].line);
    }

    fclose(outfile);

    // Free dynamically allocated memory
    free(all_data);

    // Wait for all child processes to finish
    for (int i = 0; i < num_files; i++)
    {
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