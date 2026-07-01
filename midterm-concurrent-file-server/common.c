#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
 
// Structure to represent a queue
struct CustomQueue {
    int front;
    int back;
    int size;
    unsigned capacity;
    int* elements;
};

struct Response {
    char content[4096];
    int isFinished;
};

typedef enum {
  Connect,
  TryConnect
} ConnectionType;

struct ServerConnection {
    pid_t processID;
    ConnectionType connectionType;
};
 
// Function to create a queue
struct CustomQueue* createCustomQueue(unsigned capacity)
{
    struct CustomQueue* queue = (struct CustomQueue*)malloc(
        sizeof(struct CustomQueue));
    if (queue == NULL) {
        fprintf(stderr, "Memory allocation failed for queue structure\n");
        exit(EXIT_FAILURE);
    }
    queue->capacity = capacity;
    queue->size = 0;
    queue->front = 0;
    // Set back to the last index initially
    queue->back = capacity - 1;
    queue->elements = (int*)malloc(
        queue->capacity * sizeof(int));
    if (queue->elements == NULL) {
        fprintf(stderr, "Memory allocation failed for elements array\n");
        free(queue); // Clean up allocated memory for queue structure
        exit(EXIT_FAILURE);
    }
    return queue;
}

// Function to get the nth element in the queue
int getElementAtIndex(struct CustomQueue* queue, int index)
{
    int i;
    int elementIndex = queue->front;
    if (index >= queue->size || index < 0)
        return INT_MIN;
    for(i = 0; i<index; i++) {
        elementIndex = (elementIndex + 1) % queue->capacity;
    }
    return queue->elements[elementIndex];
}

// Add an item to the queue
void enqueueElement(struct CustomQueue* queue, int item)
{
    if ((unsigned)queue->size == queue->capacity) {
        // fprintf(stderr, "Queue is full. Cannot enqueue.\n");
        return;
    }
    queue->back = (queue->back + 1) % queue->capacity;
    queue->elements[queue->back] = item;
    queue->size++;
}
 
// Remove an item from the queue
int dequeueElement(struct CustomQueue* queue)
{
    if (queue->size == 0)
        return INT_MIN;
    int item = queue->elements[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size--;
    return item;
}

// Get the front element of the queue
int peekFront(struct CustomQueue* queue)
{
    if (queue->size == 0)
        return INT_MIN;
    return queue->elements[queue->front];
}
 
// Get the back element of the queue
int peekBack(struct CustomQueue* queue)
{
    if (queue->size == 0)
        return INT_MIN;
    return queue->elements[queue->back];
}