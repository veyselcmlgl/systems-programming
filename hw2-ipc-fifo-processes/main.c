#define _POSIX_C_SOURCE 200809L // Define POSIX compliance for compatibility
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define FIFO1 "myfifo1" // Named FIFO pipe for inter-process communication
#define FIFO2 "myfifo2"

int proc_count = 0; // Process counter for child processes

// Signal handler for child process termination
void child_exit_handler(int sig) {
    int status;
    pid_t pid;
    // Handle all children that have terminated without blocking
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if(WIFEXITED(status)){
            printf("Child with PID %d terminated. exit status: %d\n", pid, WEXITSTATUS(status));
        }
        proc_count++;
    }
}

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <array size> (non-negative)\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int i;
    int sizeArr = atoi(argv[1]); // Convert string argument to integer
    int array[sizeArr]; // Dynamic array based on input size
    pid_t child_1, child_2;
    srand(time(NULL)); // Seed random number generator
    printf("Array: ");
    for(i = 0; i<sizeArr; i++) {
        array[i] = rand() % 10; // Fill array with random numbers
        printf("%d ", array[i]);
    }
    printf("\n");

    // Create two FIFO pipes
    if(mkfifo(FIFO1, 0666) == -1) {
        if(errno != EEXIST) {
            perror("FIFO1");
            exit(EXIT_FAILURE);
        }
    }
    if(mkfifo(FIFO2, 0666) == -1) {
        if(errno != EEXIST) {
            perror("FIFO2");
            exit(EXIT_FAILURE);
        }
    }

    // Fork first child process
    child_1 = fork();
    if(child_1 < 0) {
        perror("Failed to fork child 1");
        exit(EXIT_FAILURE);
    } else if (child_1 == 0) { // Child 1 process code
        for(i = 0; i<5; i++) {
            printf("Proceeding...\n");
            sleep(2);
        }
        int sum = 0, num;
        int fd2;
        int fd1 = open(FIFO1, O_RDONLY);
        if(fd1 == -1) {
            perror("Failed to open FIFO1");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < sizeArr; i++) {
            if(read(fd1, &num, sizeof(int)) < 0) {
                perror("Failed to read num");
                close(fd1);
                exit(EXIT_FAILURE);
            }
            sum += num; // Sum the elements read from the FIFO
        }

        printf("Sum of elements in array at CHLD1: %d\n", sum);
        close(fd1);

        fd2 = open(FIFO2, O_WRONLY);
        if(fd2 == -1) {
            perror("Failed to open FIFO2");
            exit(EXIT_FAILURE);
        }
        if(write(fd2, &sum, sizeof(sum)) < 0) {
            perror("Failed to write sum");
            close(fd2);
            exit(EXIT_FAILURE);
        }
        printf("Sum sent to CHLD2: %d\n", sum);
        close(fd2);
        return EXIT_SUCCESS;
    } 
    else { // Parent process code
        // Fork second child process
        child_2 = fork();
        if(child_2 < 0) {
            perror("Failed to fork child 2");
            exit(EXIT_FAILURE);
        } else if(child_2 == 0) { // Child 2 process code
            sleep(10); // Simulate delay for synchronization
            char command[9];
            int result = 1, num, sum;
            int fd = open(FIFO2, O_RDONLY);
            if(fd == -1) {
                perror("Failed to open FIFO2");
                exit(EXIT_FAILURE);
            }
            if(read(fd, command, sizeof(char)*8) < 0) {
                perror("Failed to read command");
                close(fd);
                exit(EXIT_FAILURE);
            }
            if(strcmp(command, "multiply") != 0) {
                fprintf(stderr, "Invalid command\n");
                close(fd);
                exit(EXIT_FAILURE);
            }
            for(i = 0; i < sizeArr; i++) {
                if(read(fd, &num, sizeof(int)) < 0) {
                    perror("Failed to read num");
                    close(fd);
                    exit(EXIT_FAILURE);
                }
                result *= num; // Multiply the elements read from the FIFO
            }

            sleep(2);
            if(read(fd, &sum, sizeof(sum)) < 0) {
                perror("Failed to read sum");
                close(fd);
                exit(EXIT_FAILURE);
            }
            printf("Sum from CHLD1 : %d\n", sum);
            printf("Result of multiplication: %d\n", result);
            printf("result of summation + result of mult: %d\n", sum+result);
            close(fd);
            return EXIT_SUCCESS;
        } else { // Handling for the parent process
            int fd1 = open(FIFO1, O_WRONLY);
            int fd2 = open(FIFO2, O_WRONLY);
            if(fd1 == -1) {
                perror("Failed to open FIFO1");
                exit(EXIT_FAILURE);
            }
            if(fd2 == -1) {
                perror("Failed to open FIFO2");
                exit(EXIT_FAILURE);
            }

            char command[] = "multiply";
            if(write(fd2, command, sizeof(char)*8) < 0) {
                perror("Failed to write command to FIFO2");
                close(fd2);
                exit(EXIT_FAILURE);
            }
            if(write(fd2, array, sizeof(int)*sizeArr) < 0) {
                perror("Failed to write array to FIFO2");
                close(fd2);
                exit(EXIT_FAILURE);
            } else {
                printf("Array sent to FIFO2\n");
            }

            if(write(fd1, array, sizeof(int)*sizeArr) < 0) {
                perror("Failed to write array to FIFO1");
                close(fd1);
                exit(EXIT_FAILURE);
            } else {
                printf("Array sent to FIFO1\n");
            }

            close(fd1);
            close(fd2);
        }
    }
    // Setup signal handler for SIGCHLD
    struct sigaction sa;
    sa.sa_handler = child_exit_handler; // Set handler function for SIGCHLD to manage child termination
    sa.sa_flags = SA_RESTART; // Ensure system calls restart instead of failing when signals are handled
    sigemptyset(&sa.sa_mask); // Initialize signal set to be empty, ensuring no signals are blocked

    if (sigaction(SIGCHLD, &sa, NULL) == -1) { // Set up signal handling for child process termination
        perror("sigaction"); // Print error if sigaction fails
        exit(EXIT_FAILURE); // Exit if cannot set signal action
    }

    while(proc_count < 2) { // Loop until both child processes have terminated
        printf("Proceeding...\n"); // Indicate progress in the parent process
        sleep(2); // Pause for 2 seconds, reducing CPU usage while waiting
    }
    printf("Parent exits\n"); // Inform that the parent process is terminating
    unlink(FIFO1); // Remove FIFO1 to clean up
    unlink(FIFO2); // Remove FIFO2 to clean up

    return EXIT_SUCCESS; // Exit program successfully
}
