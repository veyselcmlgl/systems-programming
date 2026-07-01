#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <errno.h>
#include "common.c"

#define SERVER_INFO "/tmp/server_fifo.%ld"
#define LOG_NAME "%s/log_file_%ld.txt"
#define CLIENT_READ_FIFO_TEMPLATE "/tmp/fifo_client_reader.%ld" 
#define CLIENT_WRITE_FIFO_TEMPLATE "/tmp/fifo_client_writer.%ld"  
#define CLIENT_FIFO_NAME_LEN 256
#define CLIENT_SHM_TEMPLATE "/shm_child.%ld"
#define CLIENT_SEM_RW "/sem.%s"
#define SHM_SIZE 256
#define MAX_CLIENTS 100
#define MAX_TOKENS 4
#define CHUNK_SIZE 4096  //1MB


static char *dirname;

volatile sig_atomic_t running = 1;
volatile sig_atomic_t running_child = 1;


void handle_client(char* clientReadFifo,char* clientWriteFifo, int client_num);
void readF(char *filename, int line_num, int clientReaderFd);
// void writeT(char *filename, int line_num, int clientReaderFd, char* content);
void uploadFile(char *filename, int clientReaderFd);
void downloadFile(char *filename, int clientReaderFd);
void list();
void help(char *token);
void errorRequestForOpenedFile(char *message);
void write_to_Log(char *message);


struct child_comm
{
    struct CustomQueue * queue;
    int counter;
};

pid_t server_pid;
pid_t child_pids[50];
pid_t client_pids[50];
int log_fd;
int num_children = 0;
int clientReaderFd, clientWriterFd; 
char message[256];

void handle_kill_signal() {

    running_child = 0;

    if (num_children > 0)
    {
        printf(">> bye...\n");
        running = 0;

        for (int i = 0; i < num_children; i++)
        {
            int status;
            kill(client_pids[i], SIGINT);
            waitpid(child_pids[i], &status, 0);
        }
    }
}

int main(int argc, char *argv[]){

    int serverFd, otherFd;
    int client_num = 0;
    char serverFifo[CLIENT_FIFO_NAME_LEN];
    char clientReaderFifo[CLIENT_FIFO_NAME_LEN];
    char clientWriterFifo[CLIENT_FIFO_NAME_LEN];
    char log_path[256];
    struct child_comm *comm;
    int shmid;
    key_t key = 1234;

    //Client - Server Connection struct
    struct ServerConnection serverConn;

    // Set up signal handler using sigaction
    struct sigaction sa;
    sa.sa_handler = handle_kill_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL); // ctrl c
    sigaction(SIGTSTP, &sa, NULL); // ctrl z

    server_pid = getpid();

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <dirname> <max. #ofClients>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    dirname = argv[1];
    int max_clients = atoi(argv[2]);
    if (max_clients <= 0 || max_clients > MAX_CLIENTS) {
        fprintf(stderr, "Invalid number of Clients\n");
        exit(EXIT_FAILURE);
    }

    if (mkdir(dirname, 0777) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    snprintf(log_path, sizeof(log_path), LOG_NAME, dirname, (long) getpid()); // READER IS CLIENT
    log_fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_fd == -1) {
        perror("open log file");
        exit(EXIT_FAILURE);
    }

    snprintf(serverFifo, CLIENT_FIFO_NAME_LEN, SERVER_INFO,  (long) getpid()); // READER IS CLIENT
    if (mkfifo(serverFifo,  0666) == -1 && errno != EEXIST){
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    //shared memory 
    if ((shmid = shmget(key, sizeof(struct child_comm), IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    comm = (struct child_comm *) shmat(shmid, NULL, 0);
    // Attach shared memory segment to process
    if ((void *) comm == (void *) -1) {
        perror("shmat");
        exit(1);
    }

    comm->queue = createCustomQueue(1000);
    comm->counter = 0;
    sem_t *sem_queue = NULL;
    if(sem_queue == NULL) {
        sem_queue = sem_open("/isGone", O_CREAT, 0666, 0);
        if (sem_queue == SEM_FAILED) {
            perror("sem_open");
            exit(EXIT_FAILURE);
        }
    }

    printf(">> Server Started PID %d...\n>> waiting for clients...\n", (int) getpid());

    serverFd = open(serverFifo, O_RDONLY);
    if (serverFd == -1){
        perror("open FIFO");
        return -1;
    }

    otherFd = open(serverFifo, O_WRONLY);
    if (otherFd == -1){
        perror("open FIFO");
        return -1;
    }

    while (running)
    {
        ssize_t bytes_read;

        bytes_read = read(serverFd, &serverConn, sizeof(struct ServerConnection));
        if ((int)bytes_read == -1 ) {
            // fprintf(stderr, "Error reading request in parent! \n");
            perror("read");
            break;
        }
    
        if (serverConn.connectionType && comm->counter >= max_clients)
        {
            kill(serverConn.processID, SIGUSR1);
            continue;
        }
        client_pids[client_num++] = serverConn.processID;


        enqueueElement(comm->queue, (int) serverConn.processID);
        comm->counter++;

        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (child_pid == 0) {
            num_children = 0;

            dequeueElement(comm->queue);
            if (comm->counter > max_clients){
                printf(">> Connection request PID %ld... Que FULL\n", (long) serverConn.processID);
                sem_wait(sem_queue);
            }

            sprintf(message,">> Client PID %ld connected as \"client%02d\"\n", (long) serverConn.processID, client_num);
            write_to_Log(message);
            printf("%s", message);

            snprintf(clientReaderFifo,  CLIENT_FIFO_NAME_LEN, CLIENT_READ_FIFO_TEMPLATE,  (long) serverConn.processID); // READER IS CLIENT
            snprintf(clientWriterFifo,  CLIENT_FIFO_NAME_LEN, CLIENT_WRITE_FIFO_TEMPLATE, (long) serverConn.processID); //WRITER IS CLIENT

            handle_client(clientReaderFifo,clientWriterFifo, client_num);

            comm->counter--;

            //close the semaphore
            if(sem_queue != NULL) {
                sem_post(sem_queue);
                sem_close(sem_queue);
                sem_unlink("/isGone");
            }

            exit(EXIT_SUCCESS);
        }
        else{
            child_pids[num_children++] = child_pid;
        }
    }
    write_to_Log("End of the Server");

    close(serverFd);
    close(otherFd);
    close(log_fd);

    sem_close(sem_queue);
    sem_unlink("/isGone");


    // Detach from the shared memory segment
    shmdt((void *) comm);
    // Destroy the shared memory segment
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

void handle_client(char* clientReaderFifo, char* clientWriterFifo, int client_num)
{
    clientReaderFd = open(clientReaderFifo, O_WRONLY);
    if (clientReaderFd == -1){
        perror("Open Fifo Write:");
        exit(EXIT_FAILURE);
    }

    clientWriterFd = open(clientWriterFifo, O_RDONLY);
    if (clientWriterFd == -1){
        perror("Open Fifo Read:");
        exit(EXIT_FAILURE);
    }

    write_to_Log("Comments:\n");
    
    while (running_child)
    {
        struct Response response;
        char *tokens[MAX_TOKENS];
        char *cmd;
        char *filename;
        char *line_number_str;
        int i = 0;
        char buf[256];
        ssize_t bytes_read = read(clientWriterFd, buf, sizeof(buf));

        if ((int)bytes_read == -1 )
        {
            fprintf(stderr, "Error reading request! \n");
            close(clientReaderFd);
            close(clientWriterFd);
            break;
        } else if (bytes_read ==0) {
            // Client disconnected (EOF)
            sprintf(message, ">> client%02d disconnected..\n", client_num);
            write_to_Log(message);
            printf("%s", message);
            break;
        }
        buf[bytes_read] = '\0';

        write_to_Log(buf);
        write_to_Log("\n");

        
        if (strcmp(buf, "quit") == 0){
            sprintf(message,">> client%02d disconnected..\n",client_num);
            write_to_Log(message);
            printf("%s",message);
            break;
        }

        //parse the given string
        tokens[i] = strtok(buf, " ");
        while (tokens[i] != NULL && i < MAX_TOKENS - 1){
            i++;
            tokens[i] = strtok(NULL, " ");
        }
        if (i < MAX_TOKENS - 1)
            tokens[i] = NULL;
        
        response.isFinished = 0;
        if (tokens[0] != NULL) {

            cmd = tokens[0];
            filename = tokens[1];
            line_number_str = tokens[2];

            if (strcmp(cmd, "readF") == 0 && filename != NULL ) {
                if (line_number_str != NULL)
                {
                    int line_number = atoi(line_number_str);
                    readF(filename, line_number, clientReaderFd);
                }
                else
                    readF(filename, 0, clientReaderFd);
            } 
            else if (strcmp(cmd, "upload") == 0 && filename != NULL) {
                uploadFile(filename, clientReaderFd);
            }
            else if (strcmp(cmd, "download") == 0 && filename != NULL) {
                downloadFile(filename, clientReaderFd);
            }
            else if (strcmp(cmd, "help") == 0) {
                if (tokens[1] == NULL)
                    help("Y");
                else
                    help(tokens[1]);
            }
            else if (strcmp(cmd, "list") == 0) {
                list();
            }
            else if (strcmp(cmd, "quit") == 0) {
                running_child = 0;
            }
            else if(strcmp(cmd, "killServer") == 0 && filename == NULL ){
                printf(">> kill signal from client%02d.. terminating...\n",client_num);
                kill(server_pid, SIGINT);
            }
            else {
                strcpy(response.content, "Invalid Command!");
                response.isFinished = 1;
                write(clientReaderFd, &response, sizeof(response));
                memset(response.content, 0, sizeof(response.content));
            }
        }
    }
    close(clientReaderFd);
    close(clientWriterFd);
}

void help(char *token){

    struct Response response;

    if (strcmp(token, "readF") == 0)
        strcpy(response.content,
             "readF <file> <line #> \n\tdisplay the #th line of the <file>, returns with an error if <file> does not exists");
    else if (strcmp(token, "list") == 0)
        strcpy(response.content,
             "list \n\tSends a request to display the list of files in Servers directory");
    else if (strcmp(token, "upload") == 0)
        strcpy(response.content,
             "upload <file> \n\tuploads the file from the current working directory of client to the Servers directory (beware of the cases no file in clients current working directory and file with the same name on Servers side)");
    else if (strcmp(token, "download") == 0)
        strcpy(response.content,
             "download <file> \n\trequest to receive <file> from Servers directory to client side");
    else if (strcmp(token, "quit") == 0)
        strcpy(response.content, "quit \n\tSend write request to Server side log file and quits");
    else if (strcmp(token, "killServer") == 0)
        strcpy(response.content, "killServer \n\tSends a kill request to the Server");
    else
        strcpy(response.content,"Available comments are :\n help, list, readF, upload, download, quit, killServer");

    response.isFinished = 1;
    write(clientReaderFd, &response, sizeof(response));
}

void list(){
    pid_t pid;
    int status;
    int pipefd[2];
    struct Response response;
    pipe(pipefd);
    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        errorRequestForOpenedFile("Error creating child process for listing files!");
        return;
    }
    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        execl("/bin/ls", "ls", dirname, NULL);
        perror("execl");  // Should not reach here if execl succeeds
        exit(EXIT_FAILURE);
    }
    else
    {
        close(pipefd[1]);

        char buffer[256];
        read(pipefd[0], buffer, 256);
        
        strcpy(response.content, buffer);
        response.isFinished = 1;
        
        write(clientReaderFd, &response, sizeof(response));
        memset(response.content, 0, sizeof(response.content));
        
        wait(&status);
    }
}

void readF(char *filename, int line_num, int clientReaderFd) {
    int fp;
    char *fullpath = malloc(strlen(dirname) + strlen(filename) + 2); // allocate memory for full path
    int curr_line = 0;
    char chunk[CHUNK_SIZE];
    struct Response response;
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), CLIENT_SEM_RW, filename);

    sem_t *sem_file = sem_open(sem_name, O_CREAT, 0666, 1);
    if (sem_file == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sprintf(fullpath, "%s/%s", dirname, filename); // concatenate directory and filename with a slash separator

    sem_wait(sem_file);
    fp = open(fullpath, O_RDONLY);
    if (fp == -1) {
        errorRequestForOpenedFile("Cannot open file");
        sem_post(sem_file);
        return;
    }

    ssize_t bytes_read;
    size_t total_bytes_written = 0;

    while ((bytes_read = read(fp, chunk, sizeof(chunk))) > 0) {
        if (line_num == 0) {
            if (bytes_read < 4096)
                response.isFinished = 1;
            else {
                response.isFinished = 0;
            }

            memcpy(response.content, chunk, bytes_read);
            write(clientReaderFd, &response, sizeof(response));
            memset(response.content, 0, sizeof(response.content));

        } else {
            ssize_t remaining_bytes = bytes_read;
            ssize_t line_start_index = 0;

            for (ssize_t i = 0; i < bytes_read; i++) {
                if (chunk[i] == '\n') {
                    curr_line++;
                    if (curr_line == line_num) {
                        response.isFinished = 1;
                        strncpy(response.content, chunk + line_start_index, i - line_start_index + 1);
                        write(clientReaderFd, &response, sizeof(response));
                        memset(response.content, 0, sizeof(response.content));

                        break;
                    }
                    line_start_index = i + 1;
                }
                remaining_bytes--;
            }
        }

        if (line_num != 0 && curr_line >= line_num) {
            break;
        }
    }

    if (response.isFinished == 0) {
        strcpy(response.content, "End of the file");
        response.isFinished = 1;
        write(clientReaderFd, &response, sizeof(response));
    }

    if (line_num == 0 && total_bytes_written == 0) {
        while ((bytes_read = read(fp, chunk, sizeof(chunk))) > 0) {
            write(clientReaderFd, chunk, bytes_read);
            total_bytes_written += bytes_read;
        }
    } else if (curr_line < line_num) {
        snprintf(response.content, sizeof(response.content), "Error: file '%s' has only %d lines", filename, curr_line);
        response.isFinished = 1;
        write(clientReaderFd, &response, sizeof(response));
    }

    free(fullpath);
    close(fp);
    sem_post(sem_file);
    sem_close(sem_file);
    sem_unlink(sem_name);
}

//upload file
void uploadFile(char *filename, int clientReaderFd) {
    int fpr, fpw;
    ssize_t bytes_read;
    size_t total_uploaded_bytes = 0;
    char chunk[CHUNK_SIZE];
    char filepath[256];
    char info[256];
    struct Response response;
    char sem_name[256];

    snprintf(sem_name, sizeof(sem_name), CLIENT_SEM_RW, filename);

    sem_t *sem_file = sem_open(sem_name, O_CREAT, 0666, 1);
    if (sem_file == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sem_wait(sem_file);
    snprintf(filepath, sizeof(filepath), "%s/%s", dirname, filename);
    strcpy(response.content, "File transfer request received. Beginning file transfer:");
    response.isFinished = 0;
    write(clientReaderFd, &response, sizeof(response));

    fpr = open(filename, O_RDONLY);
    if (fpr == -1) {
        // perror("open");
        errorRequestForOpenedFile("File doesn't exist !");
        sem_post(sem_file);
        sem_close(sem_file);
        sem_unlink(sem_name);
        return;
    }

    fpw = open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fpw == -1) {
        // perror("open");
        errorRequestForOpenedFile("Target file exist !");
        sem_post(sem_file);
        sem_close(sem_file);
        sem_unlink(sem_name);
        return;
    }

    while ((bytes_read = read(fpr, chunk, sizeof(chunk))) > 0) {
        write(fpw, chunk, bytes_read);
        total_uploaded_bytes += bytes_read;
    }

    sprintf(info, "%zu bytes transferred\n", total_uploaded_bytes);
    memset(response.content, 0, sizeof(response.content));
    strcpy(response.content, info);
    response.isFinished = 1;
    write(clientReaderFd, &response, sizeof(response));

    close(fpr);
    close(fpw);
    sem_post(sem_file);
    sem_close(sem_file);
    sem_unlink(sem_name);
}

void downloadFile(char *filename, int clientReaderFd) {
    int fpr, fpw;
    ssize_t bytes_read;
    size_t total_uploaded_bytes = 0;
    char chunk[CHUNK_SIZE];
    char filepath[256];
    char info[256];
    struct Response response;
    char sem_name[256];

    snprintf(sem_name, sizeof(sem_name), CLIENT_SEM_RW, filename);

    sem_t *sem_file = sem_open(sem_name, O_CREAT, 0666, 1);
    if (sem_file == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sem_wait(sem_file);

    snprintf(filepath, sizeof(filepath), "%s/%s", dirname, filename);

    strcpy(response.content, "File transfer request received. Beginning file transfer:");
    response.isFinished = 0;
    write(clientReaderFd, &response, sizeof(response));

    fpr = open(filepath, O_RDONLY);
    if (fpr == -1) {
        //perror("open");
        errorRequestForOpenedFile("File doesn't exist !");
        sem_post(sem_file);
        sem_close(sem_file);
        sem_unlink(sem_name);
        return;
    }

    fpw = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fpw == -1) {
        errorRequestForOpenedFile("Target file exist !");
        sem_post(sem_file);
        sem_close(sem_file);
        sem_unlink(sem_name);
        return;
    }

    while ((bytes_read = read(fpr, chunk, sizeof(chunk))) > 0) {
        write(fpw, chunk, bytes_read);
        total_uploaded_bytes += bytes_read;
    }

    sprintf(info, "%zu bytes transferred\n", total_uploaded_bytes);
    memset(response.content, 0, sizeof(response.content));
    strcpy(response.content, info);
    response.isFinished = 1;
    write(clientReaderFd, &response, sizeof(response));

    close(fpr);
    close(fpw);
    sem_post(sem_file);
    sem_close(sem_file);
    sem_unlink(sem_name);
}

void errorRequestForOpenedFile(char *message){
    struct Response response;
    strcpy(response.content, message);
    response.isFinished = 1;
    write(clientReaderFd, &response, sizeof(response));
}

void write_to_Log(char *message){
    if (write(log_fd, message, strlen(message)) == -1) {
        perror("write log message");
        exit(EXIT_FAILURE);
    }
}