#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_AUTOMOBILE 8
#define MAX_PICKUP 4

// Semaphores for synchronizing vehicle arrivals and parking attendants
sem_t newPickup, inChargeforPickup;
sem_t newAutomobile, inChargeforAutomobile;

// Counters for parking space availability
int mFree_automobile = MAX_AUTOMOBILE;
int mFree_pickup = MAX_PICKUP;

// Function prototypes
void* carOwner(void* arg);
void* carAttendant(void* arg);

int main() {
    srand(time(NULL));

    // Initialize semaphores
    if (sem_init(&newPickup, 0, 0) != 0) {
        perror("Failed to initialize newPickup semaphore");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&inChargeforPickup, 0, 1) != 0) {
        perror("Failed to initialize inChargeforPickup semaphore");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&newAutomobile, 0, 0) != 0) {
        perror("Failed to initialize newAutomobile semaphore");
        exit(EXIT_FAILURE);
    }

    if (sem_init(&inChargeforAutomobile, 0, 1) != 0) {
        perror("Failed to initialize inChargeforAutomobile semaphore");
        exit(EXIT_FAILURE);
    }

    int forAuto = 1;
    int forPickup = 2;

    // Create threads
    pthread_t threads[4];

    if (pthread_create(&threads[0], NULL, carOwner, NULL) != 0) {
        perror("Failed to create carOwner thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&threads[1], NULL, carOwner, NULL) != 0) {
        perror("Failed to create carOwner thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&threads[2], NULL, carAttendant, &forAuto) != 0) {
        perror("Failed to create carAttendant thread for automobiles");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&threads[3], NULL, carAttendant, &forPickup) != 0) {
        perror("Failed to create carAttendant thread for pickups");
        exit(EXIT_FAILURE);
    }

    // Join threads
    for (int i = 0; i < 4; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Failed to join thread");
            exit(EXIT_FAILURE);
        }
    }

    // Destroy semaphores
    if (sem_destroy(&newPickup) != 0) {
        perror("Failed to destroy newPickup semaphore");
    }

    if (sem_destroy(&inChargeforPickup) != 0) {
        perror("Failed to destroy inChargeforPickup semaphore");
    }

    if (sem_destroy(&newAutomobile) != 0) {
        perror("Failed to destroy newAutomobile semaphore");
    }

    if (sem_destroy(&inChargeforAutomobile) != 0) {
        perror("Failed to destroy inChargeforAutomobile semaphore");
    }

    return 0;
}

void* carOwner(void* arg) {
    while (1) {
        sleep(rand() % 5 + 1); // Simulate time taken for new vehicle to arrive
        int vehicleType = rand() % 2; // Generate random number (0 or 1) for vehicle type
        if (vehicleType == 0) { // Automobile
            if (mFree_automobile > 0) {
                sem_post(&newAutomobile);
                printf("Automobile owner arrives. Available automobile spots: %d\n", mFree_automobile);
                sem_wait(&inChargeforAutomobile);
            } else {
                printf("Automobile owner leaves. No available parking spots.\n");
                if (mFree_pickup == 0) {
                    printf("No available parking spots. Exiting program.\n");
                    exit(0);
                }
            }
        } else { // Pickup
            if (mFree_pickup > 0) {
                sem_post(&newPickup);
                printf("Pickup owner arrives. Available pickup spots: %d\n", mFree_pickup);
                sem_wait(&inChargeforPickup);
            } else {
                printf("Pickup owner leaves. No available parking spots.\n");
                if (mFree_automobile == 0) {
                    printf("No available parking spots. Exiting program.\n");
                    exit(0);
                }
            }
        }
    }
    return NULL;
}

void* carAttendant(void* arg) {
    int type = *(int*)arg; // 1 for automobile, 2 for pickup

    while (1) {
        if (type == 1) { // Automobile
            sem_wait(&newAutomobile);
            mFree_automobile--;
            printf("Automobile attendant parks a car. Remaining automobile spots: %d\n", mFree_automobile);
            sleep(rand() % 3 + 1); // Simulate parking time
            sem_post(&inChargeforAutomobile);
        } else { // Pickup
            sem_wait(&newPickup);
            mFree_pickup--;
            printf("Pickup attendant parks a pickup. Remaining pickup spots: %d\n", mFree_pickup);
            sleep(rand() % 3 + 1); // Simulate parking time
            sem_post(&inChargeforPickup);
        }
    }
    return NULL;
}
