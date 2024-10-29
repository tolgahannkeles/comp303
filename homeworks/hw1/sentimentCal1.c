#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024

// Function to calculate sentiment score and write to a temporary file
void calculate_score(const char* filename, const char* positive_word, const char* negative_word, int child_num) {
    FILE *input_file = fopen(filename, "r");
    if (!input_file) {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    char temp_filename[20];
    snprintf(temp_filename, 20, "temp_%d.txt", child_num);
    FILE *temp_file = fopen(temp_filename, "w");

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    while (fgets(line, MAX_LINE_LENGTH, input_file)) {
        line_number++;
        int score = 0;
        
        // Check for positive and negative words
        if (strstr(line, positive_word)) score += 5;
        if (strstr(line, negative_word)) score -= 3;

        if (score != 0) { // Only write matched lines
            fprintf(temp_file, "%s, %d: %s", filename, line_number, line);
        }
    }

    fclose(input_file);
    fclose(temp_file);
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

    pid_t pids[num_files];
    clock_t start = clock();

    for (int i = 0; i < num_files; i++) {
        pids[i] = fork();
        if (pids[i] == 0) { // Child process
            calculate_score(argv[4 + i], positive_word, negative_word, i);
            exit(0); // Exit child process
        }
    }

    for (int i = 0; i < num_files; i++) {
        waitpid(pids[i], NULL, 0); // Wait for all child processes to finish
    }

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Output file opening failed");
        exit(EXIT_FAILURE);
    }

    // Write results from temporary files to final output file
    for (int i = 0; i < num_files; i++) {
        char temp_filename[20];
        snprintf(temp_filename, 20, "temp_%d.txt", i);
        FILE *temp_file = fopen(temp_filename, "r");
        if (temp_file) {
            char line[MAX_LINE_LENGTH];
            while (fgets(line, MAX_LINE_LENGTH, temp_file)) {
                fputs(line, output_file);
            }
            fclose(temp_file);
            remove(temp_filename); // Remove temp file after use
        }
    }

    fclose(output_file);

    printf("Execution time: %.2f seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);
    return 0;
}
