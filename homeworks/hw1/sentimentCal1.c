#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// Defining constraints
#define MAX_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

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

// Function to process a single file and write the result to a temporary output file
void process_file(const char *input_file, const char *positive_word, const char *negative_word, const char *temp_output_file)
{
    FILE *inp_file = fopen(input_file, "r");
    if (!inp_file)
    {
        fprintf(stderr, "Error while opening file: %s\n", input_file);
        exit(1);
    }

    FILE *out_file = fopen(temp_output_file, "w");
    if (!out_file)
    {
        fprintf(stderr, "Error while opening temp output file: %s\n", temp_output_file);
        fclose(inp_file);
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 1;
    int total_sentiment = 0;
    while (fgets(line, sizeof(line), inp_file))
    {
        // Calculate sentiment score for the line
        int num_positive = count_word_in_line(line, positive_word);
        int num_negative = count_word_in_line(line, negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0)
        {
            // Write result to temporary output file
            fprintf(out_file, "%s, %d: %s", input_file, line_number, line);
        }
        total_sentiment += sentiment_score;
        line_number++;
    }

    printf("Total sentiment score for %s: %d\n", input_file, total_sentiment);

    fclose(inp_file);
    fclose(out_file);
    exit(0);
}

// Function to combine the results from all temporary output files and write the final output file
void combine_results(int n, const char *output_file)
{
    FILE *outfile = fopen(output_file, "w");
    if (!outfile)
    {
        perror("Error opening output file");
        exit(1);
    }
    // Combine the results from all temporary files
    for (int i = 0; i < n; i++)
    {
        char temp_filename[256];
        sprintf(temp_filename, "task1_temp_output_%d.txt", i);
        FILE *temp_file = fopen(temp_filename, "r");
        if (!temp_file)
        {
            perror(("Error opening %s file", temp_filename));
            exit(1);
        }
        // Put the contents of the temporary file into the final output file
        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), temp_file))
        {
            fputs(line, outfile);
        }

        fclose(temp_file);
        if(remove(temp_filename) != 0)
        {
            perror("Error deleting temporary file");
            exit(1);
        }
    }

    fclose(outfile);
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> <file2> ... <output_file>\n", argv[0]);
        exit(1);
    }

    // Measure the start time
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal1.c |\n");
    printf("----------------\n");

    // Get the input arguments
    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int n = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    // Create child processes for each input file
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();

        // Check for fork failure
        if (pid == -1)
        {
            perror("Fork failed");
            exit(1);
        }

        // Child process
        if (pid == 0)
        {
            // Generate a temporary output file for this child and process the input file
            char temp_output_filename[MAX_FILENAME_LENGTH];
            snprintf(temp_output_filename, sizeof(temp_output_filename), "task1_temp_output_%d.txt", i);
            process_file(argv[i + 4], positive_word, negative_word, temp_output_filename);
        }
    }

    // Wait for all child processes to complete
    for (int i = 0; i < n; i++)
    {
        wait(NULL);
    }

    // Combine results from all children and create the final output file
    combine_results(n, output_file);

    // Measure the end time and calculate the execution time
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");
    
    return 0;
}
