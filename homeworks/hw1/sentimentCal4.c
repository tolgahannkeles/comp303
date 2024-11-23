#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

// Structure to store filename, line number, line and next node in Linked List
typedef struct LineInfo
{
    char filename[MAX_FILENAME_LENGTH];
    int line_number;
    char line[MAX_LINE_LENGTH];
    struct LineInfo *next;
} LineInfo;

// Global linked list to hold the results
LineInfo *head = NULL;
sem_t list_semaphore;

// Structure for thread arguments
typedef struct
{
    const char *filename;
    const char *possitive_word;
    const char *negative_word;
} ThreadArgs;
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

// Function to add a line to the linked list
void add_to_list(const char *filename, int line_number, const char *line)
{
    // Create a new node
    LineInfo *new_node = (LineInfo *)malloc(sizeof(LineInfo));
    if (!new_node)
    {
        // Memory allocation failed
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize the new node
    strncpy(new_node->filename, filename, sizeof(new_node->filename) - 1);
    new_node->line_number = line_number;
    strncpy(new_node->line, line, sizeof(new_node->line) - 1);
    new_node->next = NULL;

    // Critical section: Protect access to the linked list
    sem_wait(&list_semaphore);
    // Add the new node to the beginning of the linked list
    new_node->next = head;
    head = new_node;
    sem_post(&list_semaphore);
}

// Function for each thread to execute
void *process_file(void *args)
{
    // Extract arguments
    ThreadArgs *thread_args = (ThreadArgs *)args;
    const char *filename = thread_args->filename;
    const char *possitive_word = thread_args->possitive_word;
    const char *negative_word = thread_args->negative_word;

    // Open the file to read
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        // Exit the thread
        pthread_exit(NULL);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    int total_sentiment = 0;

    // Read the file line by line
    while (fgets(line, sizeof(line), file))
    {
        line_number++;
        // Calculate sentiment score for the line
        int num_positive = count_word_in_line(line, possitive_word);
        int num_negative = count_word_in_line(line, negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;
        if (sentiment_score != 0)
        {
            // Add the line to the linked list
            add_to_list(filename, line_number, line);
            total_sentiment += sentiment_score;
        }
    }

    printf("Total sentiment score for %s: %d\n", filename, total_sentiment);

    fclose(file);
    pthread_exit(NULL);
}

// Compare function for sorting linked list
int compare_lines(const void *a, const void *b)
{
    LineInfo *line1 = *(LineInfo **)a;
    LineInfo *line2 = *(LineInfo **)b;

    // Compare by filename
    int filename_cmp = strcmp(line1->filename, line2->filename);
    if (filename_cmp != 0)
    {
        return filename_cmp;
    }

    // If filenames are equal, compare by line number
    return line1->line_number - line2->line_number;
}

// Function to sort the linked list
void sort_linked_list(LineInfo **head_ref)
{
    // Count the number of nodes
    int count = 0;
    LineInfo *temp = *head_ref;
    while (temp)
    {
        count++;
        temp = temp->next;
    }

    // Create an array to hold pointers to each node
    LineInfo **array = (LineInfo **)malloc(count * sizeof(LineInfo *));
    // Fill the array with pointers to each node
    temp = *head_ref;
    for (int i = 0; i < count; i++)
    {
        array[i] = temp;
        temp = temp->next;
    }

    // Sort the array with qsort via compare_lines function
    qsort(array, count, sizeof(LineInfo *), compare_lines);

    // Reconstruct the linked list from the sorted array
    for (int i = 0; i < count - 1; i++)
    {
        array[i]->next = array[i + 1];
    }
    array[count - 1]->next = NULL;
    *head_ref = array[0];

    // Free the array
    free(array);
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <input_files...> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal4.c |\n");
    printf("----------------\n");

    // Get the input arguments
    const char *possitive_word = argv[1];
    const char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    char *output_file = argv[4 + num_files];

    // Initialize the semaphore with a value of 1 meaning it is unlocked 
    sem_init(&list_semaphore, 0, 1);


    // Create threads
    pthread_t threads[num_files];
    ThreadArgs thread_args[num_files];

    for (int i = 0; i < num_files; i++)
    {
        // Set the thread arguments
        thread_args[i].filename = argv[4 + i];
        thread_args[i].possitive_word = possitive_word;
        thread_args[i].negative_word = negative_word;
        // Create the thread
        if (pthread_create(&threads[i], NULL, process_file, &thread_args[i]) != 0)
        {
            // Thread creation failed
            perror("Thread creation failed");
            return EXIT_FAILURE;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_files; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Sort the linked list
    sort_linked_list(&head);

    // Write sorted results to output file
    FILE *out_file = fopen(output_file, "w");
    if (!out_file)
    {
        perror("Error opening output file");
        return EXIT_FAILURE;
    }

    // Write the sorted results to the output file
    LineInfo *current = head;
    while (current)
    {
        fprintf(out_file, "%s, %d: %s",
                current->filename,
                current->line_number,
                current->line);
        current = current->next;
    }

    fclose(out_file);

    // Cleanup
    sem_destroy(&list_semaphore);
    while (head)
    {
        LineInfo *temp = head;
        head = head->next;
        free(temp);
    }

    // Measure the end time
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    // Print the execution time
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");

    return EXIT_SUCCESS;
}