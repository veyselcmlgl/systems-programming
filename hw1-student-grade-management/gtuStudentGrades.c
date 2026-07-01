#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define MAX_LENGTH 100

// Function to log actions with timestamps
void logAction(char *action) {
    // Open or create the log file
    int fd = open("system.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }

    // Get current time and format it
    time_t now = time(NULL);
    struct tm *tm_struct = localtime(&now);
    
    char timeBuffer[200];
    strftime(timeBuffer, sizeof(timeBuffer), "[%Y-%m-%d %H:%M:%S] ", tm_struct);

    // Write time stamp to the log file
    if (write(fd, timeBuffer, strlen(timeBuffer)) < 0) {
        perror("Failed to write to log file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Write action to the log file
    if (write(fd, action, strlen(action)) < 0) {
        perror("Failed to write to log file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Write newline character to the log file
    if (write(fd, "\n", 1) < 0) {
        perror("Failed to write to log file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Close the log file
    close(fd);
}

// Function to display manual to the user
void displayManual() {
    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Manual content
        char manual[] = "\nMANUAL:\n"
                        "\ngtuStudentGrades \"filename\"\n"
                        "-Creates a new file with the given filename\n\n"
                        "addStudentGrade \"Name Surname\" \"Grade\" \"filename\"\n"
                        "-Adds a new student with the given name and grade to the file\n"
                        "-Grade options: AA, BA, BB, CB, CC, DC, DD, FF, NA, VF\n\n"
                        "searchStudent  \"Name Surname\" \"filename\"\n"
                        "-Searches the file for the given student and displays name, grade of student\n\n"
                        "showAll \"filename\"\n"
                        "-Displays all the students and their grades\n\n"
                        "listGrades \"filename\"\n"
                        "-Displays the first 5 students and their grades\n\n"
                        "listSome \"numofEntries\" \"pageNumber\" \"filename\"\n"
                        "-Displays the grades of the students in a specific page\n"
                        "\nWARNING: \" \" characters in commands must be used for each argument!\n\n";
        
        // Write manual content to STDOUT
        int bytes_written = write(STDOUT_FILENO, manual, sizeof(manual) - 1);
        if (bytes_written < 0) {
            perror("Failed to write to STDOUT");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Child process finished. PID: %d, Displayed Manual", (int)pid);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

// Function to create a new file
void createFile(char *filename) {
    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Open or create the specified file
        int fd = open(filename, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (fd < 0) {
            perror("Failed to create file");
            exit(EXIT_FAILURE);
        }
        // Close the file descriptor
        close(fd);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Child process finished. PID: %d, File created: %s", (int)pid, filename);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

void addStudentGrade(char *studentName, char *grade, char *filename) {
    // Fork a child process
    pid_t pid = fork();
    
    if (pid == 0) { // Child process
        // Open the file for writing, creating it if it doesn't exist
        int fd = open(filename, O_WRONLY | O_APPEND, 0644);
        if (fd < 0) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }
        // Prepare data to write to the file
        char buffer[256];
        int length = snprintf(buffer, sizeof(buffer), "%s, %s\n", studentName, grade);
        // Write data to the file
        if (write(fd, buffer, length) != length) {
            perror("Failed to write to file");
            close(fd);
            exit(EXIT_FAILURE);
        }
        // Close the file descriptor
        close(fd);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Child process finished. PID: %d, Grade added: %s, %s", (int)pid, studentName, grade);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

void searchStudent(char *studentName, char *filename) {
    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Open the file for reading
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open file for searching");
            exit(EXIT_FAILURE);
        }

        char buffer[256];
        ssize_t bytes_read;
        int found = 0;

        // Loop through the file content line by line
        while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the string
            char *start = buffer;
            char *end = NULL;
            // Search for the student name in each line
            while ((end = strchr(start, '\n')) != NULL) {
                *end = '\0'; // Replace newline with null terminator to end the string
                if (strstr(start, studentName) != NULL) {
                    printf("%s\n", start); // Print the line if the student is found
                    found = 1;
                    break; // Exit inner loop if the student is found
                }
                start = end + 1; // Move to the start of the next line
            }
            if (found) break; // Exit outer loop if the student is found
        }

        // Print message if student is not found
        if (!found) {
            printf("Student not found.\n");
        }

        if (bytes_read < 0) {
            perror("Failed to read file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Close the file descriptor
        close(fd);
        exit(found ? EXIT_SUCCESS : EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Child process finished. PID: %d, Search request done for %s", (int)pid, studentName);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

void showAll(char *filename) {
    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Open the file for reading
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        char buffer[1024];
        ssize_t bytes_read;
        // Read and display the entire content of the file
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
                perror("Failed to write to stdout");
                close(fd);
                exit(EXIT_FAILURE);
            }
        }

        if (bytes_read < 0) {
            perror("Failed to read file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Close the file descriptor
        close(fd);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Child process finished. PID: %d, Displayed all student grades", (int)pid);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

void listGrades(char *filename) {
    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Open the file for reading
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        char buffer[1024];
        ssize_t bytes_read;
        int lines_to_print = 5;

        // Loop through the file content
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            // Iterate through each character in the buffer
            for (int i = 0; i < bytes_read; i++) {
                // Write the character to stdout
                write(STDOUT_FILENO, &buffer[i], 1);
                // Decrement lines_to_print if newline character is encountered
                if (buffer[i] == '\n') {
                    lines_to_print--;
                    // Break the loop if required number of lines have been printed
                    if (lines_to_print == 0)
                        break;
                }
            }
            // Break the loop if required number of lines have been printed
            if (lines_to_print == 0)
                break;
        }

        if (bytes_read < 0) {
            perror("Failed to read file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Close the file descriptor
        close(fd);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Child process finished. PID: %d, Listed first 5 student grades", (int)pid);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

void listSome(int numEntries, int pageNum, char *filename) {
    // Fork a child process
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Open the file for reading
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        char buffer[1024];
        int count = 0;
        int start = (pageNum - 1) * numEntries;
        int end = pageNum * numEntries;

        ssize_t bytes_read;
        // Loop through the file content
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            // Iterate through each character in the buffer
            for (int i = 0; i < bytes_read; i++) {
                // Check if the current line number falls within the required range
                if (count >= start && count < end) {
                    putchar(buffer[i]); // Print the character
                }
                // Increment count if newline character is encountered
                if (buffer[i] == '\n') {
                    count++;
                }
                // Exit the loop if required number of lines have been printed
                if (count == end) {
                    close(fd);
                    exit(EXIT_SUCCESS);
                }
            }
        }
        if (bytes_read < 0) {
            perror("Failed to read file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Close the file descriptor
        close(fd);
        exit(EXIT_SUCCESS);
    } else if (pid > 0) { // Parent process
        // Wait for the child process to complete
        wait(NULL);
        // Log the action
        char logMessage[256];
        snprintf(logMessage, sizeof(logMessage), "Listed entries %d to %d", (pageNum - 1) * numEntries + 1, pageNum * numEntries);
        logAction(logMessage);
    } else { // Fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

// Function to check if a grade is valid
int checkGrade(char *grade) {
    // Compare the grade with valid options
    if((strcmp(grade, "AA") == 0) || (strcmp(grade, "BA") == 0) || (strcmp(grade, "BB") == 0) || 
        (strcmp(grade, "CB") == 0) || (strcmp(grade, "CC") == 0) || (strcmp(grade, "DC") == 0) ||
        (strcmp(grade, "DD") == 0) || (strcmp(grade, "FF") == 0) || (strcmp(grade, "VF") == 0) ||
        (strcmp(grade, "NA") == 0)) {
            return 1; // Grade is valid
        }
    return 0; // Grade is invalid
}

int main() {
    // Declare variables
    char input[MAX_LENGTH];
    char name[MAX_LENGTH];
    char grade[MAX_LENGTH];
    char filename[MAX_LENGTH];
    char *parse;
    int numEntries;
    int pageNum;

    // Display initial message
    printf("\nFor usage type 'gtuStudentGrades'");
    // Main loop to handle user input
    while (1) {
        printf("\nEnter command (or 'q' to quit):\n");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0'; // remove newline character

        // Check if user wants to quit
        if (strcmp(input, "q") == 0) {
            printf("Exiting...\n");
            break;
        }

        // Tokenize the input
        parse = strtok(input, " ");
        
        // Process each token
        while (parse != NULL) {
            if(strcmp(parse, "gtuStudentGrades") == 0) {
                // If the command is 'gtuStudentGrades', display manual or create file
                parse = strtok(NULL, "\"");
                if(parse != NULL) {
                    strcpy(filename, parse);
                    createFile(filename);
                }
                if(parse == NULL) {
                    displayManual();
                }
            }
            else if (strcmp(parse, "addStudentGrade") == 0) {
                // If the command is 'addStudentGrade', add student grade
                parse = strtok(NULL, "\"");
                if(parse != NULL)   strcpy(name, parse);
                parse = strtok(NULL, " \"");
                if(parse != NULL)   strcpy(grade, parse);
                if(checkGrade(grade) == 1) {
                    parse = strtok(NULL, " \"");
                    if(parse != NULL) {
                        strcpy(filename, parse);
                        addStudentGrade(name, grade, filename);
                    }
                }
                else    printf("Please enter a valid grade!\n");
            }
            else if (strcmp(parse, "searchStudent") == 0) {
                // If the command is 'searchStudent', search for student
                parse = strtok(NULL, "\"");
                if(parse != NULL)   strcpy(name, parse);
                parse = strtok(NULL, " \"");
                if(parse != NULL) {
                    strcpy(filename, parse);
                    searchStudent(name, filename);
                }
            }
            else if (strcmp(parse, "showAll") == 0) {
                // If the command is 'showAll', display all student grades
                parse = strtok(NULL, "\"");
                if(parse != NULL) {
                    strcpy(filename, parse);
                    showAll(filename);
                }
            }
            else if (strcmp(parse, "listGrades") == 0) {
                // If the command is 'listGrades', list first 5 student grades
                parse = strtok(NULL, "\"");
                if(parse != NULL) {
                    strcpy(filename, parse);
                    listGrades(filename);
                }
            }
            else if (strcmp(parse, "listSome") == 0) {
                // If the command is 'listSome', list specified number of entries in a page
                parse = strtok(NULL, "\"");
                if(parse != NULL)   numEntries = atoi(parse);
                parse = strtok(NULL, " \"");
                if(parse != NULL)   pageNum = atoi(parse);
                parse = strtok(NULL, " \"");
                if(parse != NULL) {
                    strcpy(filename, parse);
                    listSome(numEntries, pageNum, filename);
                }
            }
            else {
                // Invalid command
                printf("Not a valid command!\n");
                break;
            }
            parse = strtok (NULL, " ");
        }
    }
    return EXIT_SUCCESS;
}
