#define _DEFAULT_SOURCE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAX_FD_LIMIT 1000

// Mutex and condition variable declarations for thread synchronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t new_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t total_bytes_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t new_fd_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

pthread_t *consumer_threads; // Array to hold consumer thread IDs
pthread_t producer_thread;   // Producer thread ID

pthread_barrier_t barrier; // Barrier for synchronizing threads

struct timeval start,end;   // Variables to hold the start and end time for statistics

int done = 0; // Flag to indicate if processing is done
int signal_received = 0; // Flag to indicate if a signal has been received
long total_bytes = 0; // Variable to keep track of total bytes copied
int created_dir = 0; // Counter for created directories
int copied_reg_file = 0; // Counter for copied regular files
int copied_fifo_file = 0; // Counter for copied FIFO files
unsigned int fd_limit; // File descriptor limit
unsigned int num_of_opened_fd = 0; // Number of currently opened file descriptors

// Struct to hold file information
typedef struct {
    int source_fd;
    int destination_fd;
    char filepath[4096];
} FileInformations;

// Struct for buffer implementation (circular queue)
typedef struct {
    int front, rear, size;
    unsigned capacity;
    FileInformations** array;
} Buffer;

Buffer* buffer; // Global buffer variable

//Function prototypes
Buffer* createQueue(unsigned capacity);
int isFull(Buffer* queue);
int isEmpty(Buffer* queue);
void enqueue(Buffer* queue, int source_file, int destination_file, const char* file_name);
FileInformations *dequeue(Buffer* queue);

void handle_signal(int signal);
int initialize(int buffer_size, int num_consumer, char **directories);
int freeResources(int num_consumers);
void store_file_descriptors(const char* source_file_path, const char* destination_file_path, const char *filename);
void copy_directory(char* source_dir_path, char* destination_dir_path);
int destroyThreads(int num_consumers);

void* producer(void *arg);
void* consumer(void *arg);

void printStatistics(int num_consumers, int buffer_size);

int is_path_exists(const char* path, char* parentDir);
int directory_checking(const char* source, const char* destination);

// Function to create a queue of given capacity. It initializes size of queue as 0
Buffer* createQueue(unsigned capacity) {
    Buffer* queue = (Buffer*)malloc(sizeof(Buffer));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (FileInformations**)malloc(queue->capacity * sizeof(FileInformations*));
    return queue;
}

// Queue is full when size becomes equal to the capacity
int isFull(Buffer* queue) {
    return (queue->size == (int)queue->capacity);
}

// Queue is empty when size is 0
int isEmpty(Buffer* queue) {
    return (queue->size == 0);
}

// Function to add an item to the queue. It changes rear and size
void enqueue(Buffer* queue, int source_file, int destination_file, const char* filepath) {
    if (isFull(queue)) {
        fprintf(stderr, "Queue is full. Cannot enqueue.\n");
        return;
    }

    FileInformations* data = malloc(sizeof(FileInformations));
    if (!data) {
        fprintf(stderr, "Failed to allocate memory for FileInformations.\n");
        return;
    }

    data->source_fd = source_file;
    data->destination_fd = destination_file;
    strncpy(data->filepath, filepath, 4096);
    data->filepath[4095] = '\0'; 

    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = data;
    queue->size = queue->size + 1;
}

// Function to remove an item from queue. It changes front and size
FileInformations* dequeue(Buffer* queue) {
    FileInformations *emptyFileInformations = { 0 };
    if (isEmpty(queue)) {
        fprintf(stderr, "Queue is empty. Cannot dequeue.\n");
        return emptyFileInformations;
    }
    FileInformations* item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

// Signal handler function
void handle_signal(int signal) {
    switch (signal) {
        case SIGINT:
            fprintf(stderr, "\t Received SIGINT signal. Terminating...\n");
            break;
        case SIGTSTP:
            fprintf(stderr, "\t Received SIGSTP signal. Terminating...\n");
            break;
    }

    done = 1;
    signal_received = 1;
    pthread_cond_broadcast(&full);
    pthread_cond_broadcast(&empty); 
}

// Function to print statistics
void printStatistics(int num_consumers, int buffer_size) {
    // Calculate total time in minutes, seconds, and milliseconds
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;

    if (microseconds < 0) {
        seconds -= 1;
        microseconds += 1000000;
    }

    long minutes = seconds / 60;
    seconds = seconds % 60;
    long milliseconds = microseconds / 1000;

    printf("\n---------------STATISTICS--------------------\n");
    printf("Consumers: %d - Buffer Size: %d\n", num_consumers, buffer_size);
    printf("Number of Regular Files: %d\n", copied_reg_file);
    printf("Number of FIFO Files: %d\n", copied_fifo_file);
    printf("Number of Directories: %d\n", created_dir);
    printf("TOTAL BYTES COPIED: %ld\n", total_bytes);
    printf("TOTAL TIME: %02ld:%02ld.%03ld (min:sec.milli)\n", minutes, seconds, milliseconds);

}

// Function to initialize the program
int initialize(int buffer_size, int num_consumers, char **directories) {
    struct rlimit limit;
    buffer = createQueue(buffer_size);
    
    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit");
        return 0;
    }
    fd_limit = limit.rlim_cur;

    if (gettimeofday(&start, NULL) == -1){
        perror("gettimeofday");
        return -1;
    }

    // Initialize barrier
    if (pthread_barrier_init(&barrier, NULL, num_consumers + 1) != 0) {
        perror("pthread_barrier_init");
        return 0;
    }

    //Create consumer threads
    consumer_threads = (pthread_t*)malloc(num_consumers * sizeof(pthread_t));
    if (!consumer_threads) {
        fprintf(stderr, "Failed to allocate memory for consumer threads.\n");
        return 0;
    }

    for (int i = 0; i < num_consumers; i++) {
       if (pthread_create(&consumer_threads[i], NULL, consumer, NULL) != 0) {
            fprintf(stderr, "Failed to create consumer thread %d.\n", i);
            return 0;
        }
    }

    // Create producer thread
    if (pthread_create(&producer_thread, NULL, producer, (void*)directories) != 0) {
        fprintf(stderr, "Failed to create producer thread.\n");
        return 0;
    }

    return 1;
}

// Function to destroy threads
int destroyThreads(int num_consumers) {
    for (int i = 0; i < num_consumers; i++) {
        if (pthread_join(consumer_threads[i], NULL) != 0) {
            perror("Joining consumer thread ");
            return 0;
        }
    }

    if (pthread_join(producer_thread, NULL) != 0) {
        perror("Joining producer thread.\n");
        return 0;
    }

    if (gettimeofday(&end, NULL) == -1) {
        perror("gettimeofday");
        return 0;
    }

    if (pthread_mutex_destroy(&mutex) != 0
        || pthread_mutex_destroy(&stdout_mutex) != 0
        || pthread_mutex_destroy(&total_bytes_mutex) != 0 // Add this line
        || pthread_cond_destroy(&full) != 0
        || pthread_cond_destroy(&empty) != 0
        || pthread_barrier_destroy(&barrier) != 0) { // Destroy the barrier
        perror("Destroy");
        return 0;
    }

    free(consumer_threads);
    return 1;
}

// Function to free resources
int freeResources(int num_consumers) {
    destroyThreads(num_consumers);
    free(buffer->array);
    free(buffer);
    return 1;
}

// Function to store file descriptors and manage the buffer
void store_file_descriptors(const char* source_file_path, const char* destination_file_path, const char *filename) {
    pthread_mutex_lock(&new_fd_mutex);

    while ((num_of_opened_fd + 2) > MAX_FD_LIMIT) {
        pthread_cond_wait(&new_fd_cond, &new_fd_mutex);
    }

    int source_fd = open(source_file_path, O_RDONLY);
    if (source_fd == -1) {
        pthread_mutex_lock(&stdout_mutex);
        fprintf(stderr, "Failed to open source file '%s'.\n", source_file_path);
        pthread_mutex_unlock(&stdout_mutex);
        pthread_mutex_unlock(&new_fd_mutex);
        return;
    }

    int destination_fd = open(destination_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destination_fd == -1) {
        pthread_mutex_lock(&stdout_mutex);
        fprintf(stderr, "Failed to open destination file '%s'.\n", destination_file_path);
        pthread_mutex_unlock(&stdout_mutex);
        close(source_fd);
        pthread_mutex_unlock(&new_fd_mutex);
        return;
    }

    num_of_opened_fd += 2;

    pthread_mutex_unlock(&new_fd_mutex);

    pthread_mutex_lock(&mutex);

    while (isFull(buffer)) {
        pthread_cond_wait(&empty, &mutex);
    }

    enqueue(buffer, source_fd, destination_fd, filename);
    pthread_cond_signal(&full);

    pthread_mutex_unlock(&mutex);
}

// Function to copy directories and files
void copy_directory(char* source_dir_path, char* destination_dir_path){
    DIR *source_dir = opendir(source_dir_path);
    if (!source_dir) {
        pthread_mutex_lock(&stdout_mutex);
        fprintf(stderr, "Failed to open source directory '%s'.\n", source_dir_path);
        pthread_mutex_unlock(&stdout_mutex);
        done = 1;
        return;
    }

    mkdir(destination_dir_path, 0777);
    struct dirent* dir_entry;

    while ((dir_entry = readdir(source_dir)) != NULL && !done)
    {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        if (dir_entry->d_type == DT_DIR ) // DIRECTORY
        {
            char destination_subdir[512];
            char source_subdir[512];

            snprintf(destination_subdir, sizeof(destination_subdir), "%s/%s", destination_dir_path, (dir_entry->d_name));
            snprintf(source_subdir, sizeof(source_subdir), "%s/%s", source_dir_path, dir_entry->d_name);

            ++created_dir; 

            pthread_mutex_lock(&stdout_mutex);
            printf("|\t\033[1;31m# %-42s  directory created  #\033[0m\n", destination_subdir);
            pthread_mutex_unlock(&stdout_mutex);

            copy_directory(source_subdir, destination_subdir);
        }

        else if (dir_entry->d_type == DT_FIFO ) // FIFO
        {
            ++copied_fifo_file;
            mkfifo(destination_dir_path, 0777);

            pthread_mutex_lock(&stdout_mutex);
            printf("|\t\033[1;31m# %-30s  FIFO copied        #\033[0m\t\t|\n", destination_dir_path);
            pthread_mutex_unlock(&stdout_mutex);
        }

        else if (dir_entry->d_type == DT_REG ) // REGULAR
        {
            ++copied_reg_file;
            char destination_file_path[256];
            char source_file_path[256];
            char *filename = dir_entry->d_name;

            snprintf(destination_file_path, sizeof(destination_file_path), "%s/%s", destination_dir_path, filename);
            snprintf(source_file_path, sizeof(source_file_path), "%s/%s", source_dir_path, filename);

            store_file_descriptors(source_file_path, destination_file_path, filename);
        }
    }
    closedir(source_dir);

}

// Producer thread function
void* producer(void *arg) {
    char** directories = (char**)arg;
    char *source_dir_path = directories[0];
    char *destination_dir_path = directories[1];

    pthread_mutex_lock(&stdout_mutex);
    printf(" \n\033[1;32m --- Directory copying process started between '%s' to '%s' ---\033[0m\n\n", directories[0], directories[1]);
    pthread_mutex_unlock(&stdout_mutex);

    struct stat st;
    if (stat(destination_dir_path, &st) == -1) {
        // Destination directory does not exist, create it
        mkdir(destination_dir_path, 0777);
    }
    else {
        char *lastSlash = strrchr(source_dir_path, '/');
        char destination_subdir[512];
        snprintf(destination_subdir, sizeof(destination_subdir), "%s%s", destination_dir_path, lastSlash);
        destination_dir_path = destination_subdir;
    }

    copy_directory(source_dir_path, destination_dir_path);

    done = 1;
    pthread_cond_broadcast(&full); 
    return NULL;
} 

// Consumer thread function
void* consumer(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);

        // Wait the buffer has an item
        while (isEmpty(buffer) && !done)
            pthread_cond_wait(&full, &mutex);

        if (isEmpty(buffer) && done) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        FileInformations *fileInfos = dequeue(buffer);

        // Signal that the buffer is not full
        pthread_cond_signal(&empty);

        // Release the lock
        pthread_mutex_unlock(&mutex);

        int source_fd = fileInfos->source_fd;
        int destination_fd = fileInfos->destination_fd;
        char *filename = fileInfos->filepath;

        // Copy the contents of the source file to the dest file using chunks
        char buff[4096];
        size_t bytes_read;
        size_t bytes_written;
        size_t total_bytes_for_a_file = 0;

        while ((bytes_read = read(source_fd, buff, sizeof(buff))) > 0 && !signal_received) {
            bytes_written = write(destination_fd, buff, bytes_read);
            if (bytes_written != bytes_read) {
                pthread_mutex_lock(&stdout_mutex);
                perror("Failed to write to destination file");
                pthread_mutex_unlock(&stdout_mutex);
                break;
            }
            pthread_mutex_lock(&total_bytes_mutex);
            total_bytes += bytes_written;
            pthread_mutex_unlock(&total_bytes_mutex);
            total_bytes_for_a_file += bytes_written;
        }

        pthread_mutex_lock(&stdout_mutex);
        printf("|\033[1;36m#  %-30s  %10ld bytes copied successfully #\033[0m\t\t\n", filename, (long)total_bytes_for_a_file);
        pthread_mutex_unlock(&stdout_mutex);

        pthread_mutex_lock(&new_fd_mutex);
        close(source_fd);
        close(destination_fd);    
        free(fileInfos);
        num_of_opened_fd -= 2; 
        pthread_cond_signal(&new_fd_cond);
        pthread_mutex_unlock(&new_fd_mutex);
    }

    pthread_barrier_wait(&barrier); // Wait for all threads to reach this point
    return NULL;
}


// Function to check if a path exists
int is_path_exists(const char* path, char* parentDir) {
    const char* lastSlash = strrchr(path, '/'); // Find the last occurrence of '/'
    char parentDir_absolute[4096];
    if (lastSlash != NULL) {
        size_t length = lastSlash - path;

        strncpy(parentDir, path, length);
        parentDir[length] = '\0'; // Null-terminate the string
    } else 
        parentDir[0] = '\0'; // Empty string

    if (realpath(parentDir, parentDir_absolute) == NULL) {
        return 0;
    }

    strcpy(parentDir, parentDir_absolute);
    return 1;
}

// Function to check directories
int directory_checking(const char* source, const char* destination) {
    char dest_parent[4096];
    char source_absolute[4096];
    char dest_absolute[4096];

    // Get the absolute path of the source file/directory
    if (realpath(source, source_absolute) == NULL) {
        perror("Source Path");
        return 0;
    }

    if (realpath(destination, dest_absolute) == NULL) {

        if (!is_path_exists(destination, dest_parent)){
            perror("Destination Path");
            return 0;
        }
        strcpy(dest_absolute ,dest_parent);
    }

    // Compare the directory paths
    if(strncmp(source_absolute, dest_absolute, strlen(source_absolute)) == 0)
    {
        printf("Cannot copy into itself\n");
        return 0;
    }

    return 1;
}

int main(int argc, char *argv[]) {
    char* directories[2];
    struct sigaction sa;

    // Register the signal handler
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    if (argc != 5) {
        fprintf(stderr, "Usage: %s buffer_size num_consumers source_dir destination_dir\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (atoi(argv[1]) < 1 || atoi(argv[2]) < 1 )
    {
        fprintf(stderr, "Buffer size and Number of consumer must be greater than 0 \n");
        return EXIT_FAILURE;
    }
    
    directories[0] = argv[3];
    directories[1] = argv[4];
    int buffer_size = atoi(argv[1]);
    int num_consumers = atoi(argv[2]);

    if(directory_checking(directories[0], directories[1]) == 0) {
        return EXIT_FAILURE;
    }

    if(!initialize(buffer_size, num_consumers, directories)){
        return EXIT_FAILURE;
    }

    pthread_barrier_wait(&barrier); // Wait for all threads to reach this point

    freeResources(num_consumers);
    printStatistics(num_consumers, buffer_size);

    return EXIT_SUCCESS;
}

