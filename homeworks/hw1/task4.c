#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

// Defining constraints
#define MAX_FILES 100
#define SHARED_DATA_SIZE 60000
#define MAX_LINE_LENGTH 1024
#define MAX_FILENAME_LENGTH 256

// Mutex for synchronizing access to the shared linked list
pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure to store the data to be passed to the thread
typedef struct
{
    const char *filename;
    const char *positive_word;
    const char *negative_word;
} ThreadData;

// Linked list node structure
struct node
{
    const char *filename;
    int line_number;
    const char *line;
    struct node *next;
};

// Function prototypes
void add_to_linked_list(const char *filename, int line_number, const char *line);
void write_linked_list_to_file(const char *output_file);
void free_linked_list(void);
void sort_linked_list(void);
int is_word_boundary(char c);
int count_word_in_line(const char *line, const char *word);
void *process_file(void *args);
int compare_nodes(const void *a, const void *b);

// Global head of the linked list
struct node *head = NULL;
long length = 0;

// Function to check if a character is a word boundary
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

// Function to compare two nodes for sorting (filename, then line_number)
int compare_nodes(const void *a, const void *b)
{
    struct node *nodeA = *(struct node **)a;
    struct node *nodeB = *(struct node **)b;

    // Compare by filename first
    int filename_comparison = strcmp(nodeA->filename, nodeB->filename);
    if (filename_comparison != 0)
    {
        return filename_comparison;
    }

    // If filenames are equal, compare by line_number
    return nodeA->line_number - nodeB->line_number;
}

// Function to sort the linked list by filename and line_number
void sort_linked_list(void)
{
    struct node *current = head;
   
    // If there are fewer than two nodes, no need to sort
    if (length < 2)
        return;

    // Create an array to store nodes for sorting
    struct node **node_array = (struct node **)malloc(length * sizeof(struct node *));
    current = head;
    for (int i = 0; i < length; i++)
    {
        node_array[i] = current;
        current = current->next;
    }

    // Sort the array of nodes using quicksort
    qsort(node_array, length, sizeof(struct node *), compare_nodes);

    // Rebuild the linked list in sorted order
    head = node_array[0];
    for (int i = 1; i < length; i++)
    {
        node_array[i - 1]->next = node_array[i];
    }
    node_array[length - 1]->next = NULL;

    // Free the array
    free(node_array);
}

void *process_file(void *args)
{
    ThreadData *data = (ThreadData *)args;

    FILE *inp_file = fopen(data->filename, "r");
    if (!inp_file)
    {
        fprintf(stderr, "Error while opening file: %s\n", data->filename);
        exit(1);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;
    int total_sentiment = 0;
    while (fgets(line, sizeof(line), inp_file))
    {
        // Calculate sentiment score for the line
        int num_positive = count_word_in_line(line, data->positive_word);
        int num_negative = count_word_in_line(line, data->negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0)
        {
            // Add result to shared linked list
            add_to_linked_list(data->filename, line_number, line);
        }
        total_sentiment += sentiment_score;
        line_number++;
    }

    printf("Total sentiment score for %s: %d\n", data->filename, total_sentiment);

    fclose(inp_file);
    return NULL;
}

// Function to create a new node and add it to the shared linked list
void add_to_linked_list(const char *filename, int line_number, const char *line)
{
    pthread_mutex_lock(&list_mutex); // Lock the shared list

    // Allocate memory for the new node
    struct node *new_node = (struct node *)malloc(sizeof(struct node));
    new_node->filename = filename;
    new_node->line_number = line_number;
    new_node->line = strdup(line); // Duplicate the line string
    new_node->next = head;         // Insert at the beginning
    head = new_node;
    length++;

    pthread_mutex_unlock(&list_mutex); // Unlock after modification
}

// Function to write the linked list to a file
void write_linked_list_to_file(const char *output_file)
{
    FILE *out_file = fopen(output_file, "w");
    if (!out_file)
    {
        fprintf(stderr, "Error while opening output file: %s\n", output_file);
        exit(1);
    }

    struct node *current = head;
    while (current != NULL)
    {
        fprintf(out_file, "%s, %d: %s", current->filename, current->line_number, current->line);
        current = current->next;
    }

    fclose(out_file);
}

// Function to free the memory used by the linked list
void free_linked_list()
{
    struct node *current = head;
    while (current != NULL)
    {
        struct node *temp = current;
        current = current->next;
        free((void *)temp->line); // Free the duplicated line
        free(temp);               // Free the node itself
    }
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> ... <fileN> <output_file>\n", argv[0]);
        exit(1);
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("---------------------------------------------------------------------\n");
    printf("sentimentCal4.c |\n");
    printf("----------------\n");

    char *positive_word = argv[1];
    char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    // Create list of threads and thread data
    pthread_t threads[MAX_FILES];
    ThreadData thread_data[MAX_FILES];

    // Create threads for each file
    for (int i = 0; i < num_files; i++)
    {
        thread_data[i].filename = argv[4 + i];
        thread_data[i].positive_word = positive_word;
        thread_data[i].negative_word = negative_word;
        pthread_create(&threads[i], NULL, process_file, &thread_data[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_files; i++)
    {
        pthread_join(threads[i], NULL);
    }

    sort_linked_list();

    write_linked_list_to_file(output_file);
    
    free_linked_list();

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);
    printf("---------------------------------------------------------------------\n");

    return 0;
}
