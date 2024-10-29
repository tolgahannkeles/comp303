#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_LINE_LENGTH 1024

typedef struct Node {
    char data[MAX_LINE_LENGTH];
    struct Node* next;
} Node;

Node *head = NULL;
pthread_mutex_t list_mutex;

typedef struct {
    const char *filename;
    const char *positive_word;
    const char *negative_word;
} ThreadData;

void* process_file(void *arg) {
    ThreadData *data = (ThreadData*) arg;
    FILE *file = fopen(data->filename, "r");

    if (!file) {
        perror("File opening failed");
        pthread_exit(NULL);
    }

    char line[MAX_LINE_LENGTH];
    int line_number = 0;

    while (fgets(line, MAX_LINE_LENGTH, file)) {
        line_number++;
        int score = 0;
        
        if (strstr(line, data->positive_word)) score += 5;
        if (strstr(line, data->negative_word)) score -= 3;

        if (score != 0) {
            Node *new_node = (Node*) malloc(sizeof(Node));
            snprintf(new_node->data, MAX_LINE_LENGTH, "%s, %d: %s", data->filename, line_number, line);
            new_node->next = NULL;

            pthread_mutex_lock(&list_mutex);
            new_node->next = head;
            head = new_node;
            pthread_mutex_unlock(&list_mutex);
        }
    }

    fclose(file);
    pthread_exit(NULL);
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

    pthread_t threads[num_files];
    ThreadData thread_data[num_files];
    clock_t start = clock();

    pthread_mutex_init(&list_mutex, NULL);

    // Create threads
    for (int i = 0; i < num_files; i++) {
        thread_data[i].filename = argv[4 + i];
        thread_data[i].positive_word = positive_word;
        thread_data[i].negative_word = negative_word;
        pthread_create(&threads[i], NULL, process_file, &thread_data[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < num_files; i++) {
        pthread_join(threads[i], NULL);
    }

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Output file opening failed");
        exit(EXIT_FAILURE);
    }

    // Write the results from the linked list to the output file
    Node *current = head;
    while (current != NULL) {
        fputs(current->data, output_file);
        Node *temp = current;
        current = current->next;
        free(temp);
    }

    fclose(output_file);

    pthread_mutex_destroy(&list_mutex);
    
    printf("Execution time: %.2f seconds\n", (double)(clock() - start) / CLOCKS_PER_SEC);
    return 0;
}
