#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.c"

#define SERVER_INFO "/tmp/server_fifo.%ld"
#define CLIENT_READ_FIFO_TEMPLATE "/tmp/fifo_client_reader.%ld"
#define CLIENT_WRITE_FIFO_TEMPLATE "/tmp/fifo_client_writer.%ld"

#define CLIENT_SHM_TEMPLATE "/shm_client.%ld"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_READ_FIFO_TEMPLATE) + 20)

#define SHM_SIZE 256

static char clientReaderFifo[CLIENT_FIFO_NAME_LEN];
static char clientWriterFifo[CLIENT_FIFO_NAME_LEN];

volatile sig_atomic_t running = 1;

void handle_signal(int signum) {
    
    if (signum == SIGUSR1)
        exit(EXIT_SUCCESS);
    
    running = 0;
}

static void removeFifo(void){
    unlink(clientReaderFifo);
    unlink(clientWriterFifo);
}

int main(int argc, char *argv[]) {

    int clientWriterFd, clientReaderFd;
    int server_pid;
    struct ServerConnection serverConn;
    struct Response response;
    char serverFifo[CLIENT_FIFO_NAME_LEN];


    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);



    if (argc != 3 || (strcmp(argv[1], "Connect") != 0 && strcmp(argv[1], "tryConnect") !=0 )) {
        printf("Usage: %s <Connect/tryConnect> <ServerPID>\n", argv[0]);
        return 1;
    }

    serverConn.connectionType = (strcmp(argv[1], "tryConnect") == 0);

    serverConn.processID = getpid();
    server_pid = atoi(argv[2]);

    if (server_pid <= 0) {
        fprintf(stderr, "Invalid ServerPID: %s\n", argv[2]);
        return 1;
    }
    if(serverConn.connectionType == 0)
        printf(">> Waiting for Que.. ");
    fflush(stdout);

    snprintf(clientReaderFifo, CLIENT_FIFO_NAME_LEN, CLIENT_READ_FIFO_TEMPLATE, (long)getpid());
    snprintf(clientWriterFifo, CLIENT_FIFO_NAME_LEN, CLIENT_WRITE_FIFO_TEMPLATE, (long)getpid());

    if (mkfifo(clientReaderFifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1  && errno != EEXIST){
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }
    if (mkfifo(clientWriterFifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1  && errno != EEXIST){
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    if (atexit(removeFifo) != 0){
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    snprintf(serverFifo, CLIENT_FIFO_NAME_LEN, SERVER_INFO,  (long) server_pid); // READER IS CLIENT
    
    int serverFd = open(serverFifo, O_WRONLY);
    if (serverFd == -1){
        perror("open server_FIFO");
        return -1;
    }

    if (write(serverFd, &serverConn, sizeof(struct ServerConnection)) == -1) {
        perror("write");
        close(serverFd);
        exit(EXIT_FAILURE);
    }

    clientReaderFd = open(clientReaderFifo, O_RDONLY);
    if (clientReaderFd == -1){
        perror("open FIFO");
        return -1;
    }

    clientWriterFd = open(clientWriterFifo, O_WRONLY);
    if (clientWriterFd == -1){
        perror("open FIFO");
        return -1;
    }

    printf("Connection established:\n");

    ssize_t bytes_read = 0;

    while (running)
    {
        char command[256];

        printf(">> Enter comment : ");
        fflush(stdout);

        if(fgets(command, sizeof(command), stdin) == NULL){
            break;
        }
        command[strcspn(command, "\n")] = '\0';

        if (strlen(command) == 0)
            continue;
        
        if (write(clientWriterFd, command, strlen(command)) == -1)
        {
            fprintf(stderr, "Error writing request!\n");
            break;
        }

        while ((bytes_read = read(clientReaderFd, &response, sizeof(response))) > 0)
        {
            if ((int) bytes_read == -1)
            {
                printf("Signal Received\n");
                break;
            }
            
            response.content[strlen(response.content)] = '\0';
            printf("%s \n", response.content);
            fflush(stdout);

            if (response.isFinished)
                break;
            memset(response.content, 0, sizeof(response.content));

        }
        memset(response.content, 0, sizeof(response.content));

        if (strcmp(command,"quit") == 0)
            break;

        if (strcmp(command,"killServer") == 0)
            break;

    }
    close(clientWriterFd);
    close(clientReaderFd);
    close(serverFd);

    unlink(clientReaderFifo);
    unlink(clientWriterFifo);

}