#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <math.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#define MAX_QUEUE 10
#define MAX_ORDERS 500
#define MAX_LOG 256
#define OVEN_CAPACITY 6
#define DELIVERY_CAPACITY 3
#define APPARATUS_COUNT 3

// Order structure to hold order details
typedef struct {
    int id;
    int address_x;
    int address_y;
    int shop_x;
    int shop_y;
    char status[50];
} Order;

// Cook structure for cook threads
typedef struct {
    int cook_id;
    pthread_t thread;
} Cook;

// Delivery structure for delivery threads
typedef struct {
    int delivery_id;
    pthread_t thread;
    int capacity;
    int speed;
} Delivery;

// Server structure to hold server details
typedef struct {
    int server_fd;
    struct sockaddr_in address;
    int addrlen;
} Server;

// Global variables
Order orders[MAX_ORDERS];
int order_count = 0;
int oven_slots = OVEN_CAPACITY;
sem_t oven_sem, apparatus_sem;
pthread_mutex_t order_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t order_cond = PTHREAD_COND_INITIALIZER;
int client_pid;
int total_orderNum = 0;
int cooked_count[MAX_ORDERS] = {0};
int server_running = 1;

// Function to log messages to a file
void log_message(const char *message) {
    FILE *logfile = fopen("pideshop.log", "a");
    if (logfile == NULL) {
        perror("Failed to open log file");
        return;
    }
    fprintf(logfile, "%s\n", message);
    fclose(logfile);
}

// Function to initialize the log file
void initialize_log() {
    FILE *logfile = fopen("pideshop.log", "w");
    if (logfile != NULL) {
        fprintf(logfile, "PideShop Log Initialized\n");
        fclose(logfile);
    } else {
        perror("Failed to initialize log file");
    }
}

// Function for cook threads
void *cook_function(void *arg) {
    Cook *cook = (Cook *)arg;
    while (1) {
        pthread_mutex_lock(&order_mutex);
        while (order_count == 0) {
            pthread_cond_wait(&order_cond, &order_mutex);
        }

        // Find the next order to prepare
        int order_index = -1;
        for (int i = 0; i < order_count; i++) {
            if (strcmp(orders[i].status, "received") == 0) {
                order_index = i;
                break;
            }
        }

        if (order_index == -1) {
            pthread_mutex_unlock(&order_mutex);
            continue;
        }

        Order order = orders[order_index];
        strcpy(orders[order_index].status, "preparing");

        // Log order preparation
        char log_msg[MAX_LOG];
        pthread_mutex_lock(&log_mutex);
        snprintf(log_msg, MAX_LOG, "Cook %d preparing order %d", cook->cook_id, order.id);
        log_message(log_msg);
        pthread_mutex_unlock(&log_mutex);
        pthread_mutex_unlock(&order_mutex);

        // Simulate preparation time
        usleep(100000);

        // Wait for apparatus and oven availability
        sem_wait(&apparatus_sem);
        sem_wait(&oven_sem);

        pthread_mutex_lock(&order_mutex);
        strcpy(orders[order_index].status, "cooking");
        oven_slots--;

        // Log order cooking
        pthread_mutex_lock(&log_mutex);
        snprintf(log_msg, MAX_LOG, "Order %d is in the oven", order.id);
        log_message(log_msg);
        pthread_mutex_unlock(&log_mutex);

        pthread_mutex_unlock(&order_mutex);

        // Simulate cooking time
        usleep(50000);

        // Release apparatus and oven slots
        sem_post(&oven_sem);
        sem_post(&apparatus_sem);

        pthread_mutex_lock(&order_mutex);
        strcpy(orders[order_index].status, "cooked");
        oven_slots++;

        // Log order completion
        pthread_mutex_lock(&log_mutex);
        snprintf(log_msg, MAX_LOG, "Cook %d finished order %d", cook->cook_id, order.id);
        log_message(log_msg);
        pthread_mutex_unlock(&log_mutex);

        cooked_count[cook->cook_id]++;
        pthread_mutex_unlock(&order_mutex);
    }
    return NULL;
}

// Function for delivery threads
void *delivery_function(void *arg) {
    Delivery *delivery = (Delivery *)arg;
    while (1) {
        pthread_mutex_lock(&order_mutex);
        
        while (order_count == 0) {
            pthread_cond_wait(&order_cond, &order_mutex);
        }

        int deliveries = 0;
        for (int i = 0; i < order_count && deliveries < DELIVERY_CAPACITY; i++) {
            if (strcmp(orders[i].status, "cooked") == 0) {
                deliveries++;
            }
        }

        if (deliveries > 0) {
            for (int i = 0; i < order_count && deliveries > 0; i++) {
                if (strcmp(orders[i].status, "cooked") == 0) {
                    Order order = orders[i];
                    strcpy(orders[i].status, "delivering");

                    // Log order delivery
                    char log_msg[MAX_LOG];

                    // Simulate delivery time based on distance
                    int distance = sqrt(pow(order.address_x - order.shop_x, 2) + pow(order.address_y - order.shop_y, 2));
                    usleep(distance / delivery->speed * 1000000);

                    pthread_mutex_lock(&log_mutex);
                    snprintf(log_msg, MAX_LOG, "Delivery %d delivered order %d", delivery->delivery_id, order.id);
                    log_message(log_msg);
                    pthread_mutex_unlock(&log_mutex);

                    // Remove the order from the list
                    for (int j = i; j < order_count - 1; j++) {
                        orders[j] = orders[j + 1];
                    }
                    order_count--;
                    i--;  // Adjust index after removal
                    deliveries--;
                }
            }
        }

        pthread_mutex_unlock(&order_mutex);
    }
    return NULL;
}

// Function to find and print the cook with the maximum orders
void find_max_cook() {
    int max = 0;
    int max_cook = 0;
    for (int i = 0; i < (int)(sizeof(cooked_count) / sizeof(cooked_count[0])); i++) {
        if (cooked_count[i] > max) {
            max = cooked_count[i];
            max_cook = i;
        }
    }
    printf("> > Cook %d cooked the most orders\n", max_cook);
}

// Signal handler function
void handle_signal() {
    pthread_mutex_lock(&log_mutex);
    log_message("Received termination signal, shutting down...");
    pthread_mutex_unlock(&log_mutex);
    printf(".. Upps quitting.. writing log file\n");
    server_running = 0;
    find_max_cook();
    exit(0);
}

// Function to handle client connections
void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    // Receive client PID and total number of orders
    recv(client_socket, &client_pid, sizeof(client_pid), 0);
    recv(client_socket, &total_orderNum, sizeof(total_orderNum), 0);

    // Log client connection
    char log_msg[MAX_LOG];
    pthread_mutex_lock(&log_mutex);
    snprintf(log_msg, MAX_LOG, "Accepted connection from client PID %d", client_pid);
    log_message(log_msg);
    pthread_mutex_unlock(&log_mutex);
    printf("> > Accepted connection from client PID %d\n", client_pid);
    printf("> > %d new customers.. Serving\n", total_orderNum);

    while (1) {
        Order order;
        int read_size = recv(client_socket, &order, sizeof(Order), 0);
        if (read_size <= 0) {
            printf("> > done serving client @ XXX PID %d\n", client_pid);
            printf("> > active waiting for connections ..\n");
            close(client_socket);
            return NULL;
        }

        pthread_mutex_lock(&order_mutex);
        strcpy(order.status, "received");
        orders[order_count++] = order;

        // Log order reception
        pthread_mutex_lock(&log_mutex);
        snprintf(log_msg, MAX_LOG, "Received order %d from client, adress_x=%d, adress_y=%d", order.id, order.address_x, order.address_y);
        log_message(log_msg);
        pthread_mutex_unlock(&log_mutex);
        usleep(10000);

        pthread_cond_signal(&order_cond);
        pthread_mutex_unlock(&order_mutex);
    }

    return NULL;
}

// Main function to initialize server and create threads
int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s [portnumber] [CookthreadPoolSize] [DeliveryPoolSize] [k]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int cook_pool_size = atoi(argv[2]);
    int delivery_pool_size = atoi(argv[3]);
    int delivery_speed = atoi(argv[4]);

    // Set up signal handler
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set up SIGINT handler");
        exit(EXIT_FAILURE);
    }

    // Initialize log file
    initialize_log();
    log_message("PideShop active, waiting for connections...");
    printf("> PideShop active waiting for connection ...\n");

    // Initialize server
    Server server;
    server.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server.server_fd == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server.address.sin_family = AF_INET;
    server.address.sin_addr.s_addr = INADDR_ANY;
    server.address.sin_port = htons(port);
    server.addrlen = sizeof(server.address);

    if (bind(server.server_fd, (struct sockaddr *)&server.address, sizeof(server.address)) < 0) {
        perror("Bind failed");
        close(server.server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server.server_fd, MAX_QUEUE) < 0) {
        perror("Listen failed");
        close(server.server_fd);
        exit(EXIT_FAILURE);
    }

    // Retrieve and print the server's IP address
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    if (gethostname(hostbuffer, sizeof(hostbuffer)) == -1) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }
    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    printf("Server IP Address: %s\n", IPbuffer);

    // Initialize semaphores
    sem_init(&oven_sem, 0, OVEN_CAPACITY);
    sem_init(&apparatus_sem, 0, APPARATUS_COUNT);

    // Create cook threads
    Cook cooks[cook_pool_size];
    for (int i = 0; i < cook_pool_size; i++) {
        cooks[i].cook_id = i + 1;
        if (pthread_create(&cooks[i].thread, NULL, cook_function, &cooks[i]) != 0) {
            perror("Failed to create cook thread");
            exit(EXIT_FAILURE);
        }
    }

    // Create delivery threads
    Delivery deliveries[delivery_pool_size];
    for (int i = 0; i < delivery_pool_size; i++) {
        deliveries[i].delivery_id = i + 1;
        deliveries[i].capacity = DELIVERY_CAPACITY;
        deliveries[i].speed = delivery_speed;
        if (pthread_create(&deliveries[i].thread, NULL, delivery_function, &deliveries[i]) != 0) {
            perror("Failed to create delivery thread");
            exit(EXIT_FAILURE);
        }
    }

    // Accept client connections
    while (server_running) {
        int client_socket = accept(server.server_fd, (struct sockaddr *)&server.address, (socklen_t *)&server.addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            perror("Failed to allocate memory for client socket");
            continue;
        }
        *new_sock = client_socket;
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, (void *)new_sock) != 0) {
            perror("Failed to create client handler thread");
            free(new_sock);
            continue;
        }
    }

    // Join cook threads
    for (int i = 0; i < cook_pool_size; i++) {
        pthread_join(cooks[i].thread, NULL);
    }

    // Join delivery threads
    for (int i = 0; i < delivery_pool_size; i++) {
        pthread_join(deliveries[i].thread, NULL);
    }

    // Destroy semaphores and mutexes
    sem_destroy(&oven_sem);
    sem_destroy(&apparatus_sem);
    pthread_mutex_destroy(&order_mutex);
    pthread_cond_destroy(&order_cond);

    close(server.server_fd);

    return 0;
}
