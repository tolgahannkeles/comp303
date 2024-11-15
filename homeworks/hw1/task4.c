#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define MAX_FILES 10

typedef struct Node {
    char *line;
    struct Node *next;
} Node;

typedef struct {
    const char *filename;
    const char *positive_word;
    const char *negative_word;
} ThreadData;

Node *head = NULL;
sem_t sem;

int is_word_boundary(char c) {
    return !isalnum(c);
}

int count_word_occurrences(const char *line, const char *word) {
    int count = 0;
    const char *current_position = line;
    size_t word_length = strlen(word);

    while ((current_position = strstr(current_position, word)) != NULL) {
        int is_start_of_word = (current_position == line || is_word_boundary(*(current_position - 1)));
        int is_end_of_word = is_word_boundary(*(current_position + word_length));

        if (is_start_of_word && is_end_of_word) {
            count++;
        }

        current_position += word_length;
    }

    return count;
}

void add_to_list(const char *line) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->line = strdup(line);
    new_node->next = NULL;

    sem_wait(&sem);
    new_node->next = head;
    head = new_node;
    sem_post(&sem);
}

void *process_file(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    FILE *file = fopen(data->filename, "r");
    if (!file) {
        perror("Error opening file");
        pthread_exit(NULL);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;

    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_number++;
        int num_positive = count_word_occurrences(line, data->positive_word);
        int num_negative = count_word_occurrences(line, data->negative_word);
        int sentiment_score = num_positive * 5 - num_negative * 3;

        if (sentiment_score != 0) {
            char buffer[MAX_LINE_LENGTH + 100];
            snprintf(buffer, sizeof(buffer), "%s, %d, score: %d -> %s", data->filename, line_number, sentiment_score, line);
            add_to_list(buffer);
        }
    }

    fclose(file);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    

    if (argc < 5) {
        fprintf(stderr, "Usage: %s <positive_word> <negative_word> <num_files> <file1> [<file2> ... <fileN>]\n", argv[0]);
        exit(1);
    }

    const char *positive_word = argv[1];
    const char *negative_word = argv[2];
    int num_files = atoi(argv[3]);
    const char *output_file = argv[argc - 1];

    pthread_t threads[MAX_FILES];
    ThreadData thread_data[MAX_FILES];

    sem_init(&sem, 0, 1);

    for (int i = 0; i < num_files; i++) {
        thread_data[i].filename = argv[i + 4];
        thread_data[i].positive_word = positive_word;
        thread_data[i].negative_word = negative_word;
        pthread_create(&threads[i], NULL, process_file, &thread_data[i]);
    }

    for (int i = 0; i < num_files; i++) {
        pthread_join(threads[i], NULL);
    }

    FILE *outfile = fopen(output_file, "w");
    if (outfile == NULL) {
        perror("fopen");
        exit(1);
    }

    Node *current = head;
    while (current != NULL) {
        fprintf(outfile, "%s", current->line);
        Node *temp = current;
        current = current->next;
        free(temp->line);
        free(temp);
    }

    fclose(outfile);
    sem_destroy(&sem);

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time.tv_nsec;
    long total_microseconds = seconds * 1000000L + nanoseconds / 1000L;
    printf("Execution time: %ld µs\n", total_microseconds);

    return 0;
}