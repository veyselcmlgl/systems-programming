#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>

// Order structure to hold order details
typedef struct {
    int id;
    int address_x;
    int address_y;
    int shop_x;
    int shop_y;
    char status[50];
} Order;

// Global variable for socket descriptor
int sock;

// Signal handler for graceful termination
void handle_signal() {
    Order cancel_order;
    printf("Client is terminating\n");
    strcpy(cancel_order.status, "cancel");
    send(sock, &cancel_order, sizeof(Order), 0);
    close(sock);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [portnumber] [ipAdress] [numberOfClients] [p] [q]\n", argv[0]);
        return 1;
    }
    
    // Print client PID
    pid_t pid = getpid();
    printf("> > Client PID: %d...\n> > ...\n", pid);

    // Parse command line arguments
    int port = atoi(argv[1]);
    char *ip = argv[2];
    int number_of_clients = atoi(argv[3]);
    int p = atoi(argv[4]);
    int q = atoi(argv[5]);

    // Set up signal handler for SIGINT
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set up SIGINT handler");
        exit(EXIT_FAILURE);
    }

    // Create socket
    struct sockaddr_in server_address;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip);

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    // Send client PID to server
    if (send(sock, &pid, sizeof(pid), 0) < 0) {
        perror("Send failed");
        close(sock);
        return 1;
    }

    // Send number of clients to server
    if (send(sock, &number_of_clients, sizeof(int), 0) < 0) {
        perror("Send failed");
        close(sock);
        return 1;
    }

    // Generate and send orders to server
    srand(time(NULL));
    for (int i = 0; i < number_of_clients; i++) {
        Order order;
        order.id = i + 1;
        order.address_x = rand() % p;
        order.address_y = rand() % q;
        order.shop_x = p / 2;
        order.shop_y = q / 2;
        strcpy(order.status, "new");

        if (send(sock, &order, sizeof(Order), 0) < 0) {
            perror("Send failed");
            close(sock);
            return 1;
        }
        usleep(50000); // Simulate delay between orders
    }

    printf("> > All customers served\n> > log file written ..\n");
    close(sock);
    return 0;
}
