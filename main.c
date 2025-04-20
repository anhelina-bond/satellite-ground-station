#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <queue.h>

#define NUM_ENGINEERS 3
#define TIMEOUT 5  // Maximum connection window in seconds

// Priority levels (higher number = higher priority)
#define PRIORITY_LEVELS 4

// Shared resources
int availableEngineers = NUM_ENGINEERS;
pthread_mutex_t engineerMutex;
sem_t newRequest;
sem_t requestHandled;

// Structure for Satellite
typedef struct {
    int id;
    int priority;
} Satellite;

// Priority Queue Node
typedef struct {
    Satellite satellite;
    struct Node* next;
} Node;

// Priority Queue
typedef struct {
    Node* head;
} PriorityQueue;

PriorityQueue requestQueue;

// Function to initialize the priority queue
void initQueue(PriorityQueue* pq) {
    pq->head = NULL;
}

// Function to insert into the priority queue based on priority
void enqueue(PriorityQueue* pq, Satellite satellite) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->satellite = satellite;
    newNode->next = NULL;

    if (pq->head == NULL || pq->head->satellite.priority < satellite.priority) {
        newNode->next = pq->head;
        pq->head = newNode;
    } else {
        Node* current = pq->head;
        while (current->next != NULL && current->next->satellite.priority >= satellite.priority) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
}

// Function to remove the highest priority satellite from the queue
Satellite dequeue(PriorityQueue* pq) {
    if (pq->head == NULL) {
        Satellite empty = { -1, -1 };
        return empty;  // Return an empty satellite if queue is empty
    }
    Node* temp = pq->head;
    Satellite satellite = temp->satellite;
    pq->head = pq->head->next;
    free(temp);
    return satellite;
}

// Satellite thread function
void* satellite(void* arg) {
    Satellite* sat = (Satellite*)arg;
    printf("Satellite %d with priority %d requesting update.\n", sat->id, sat->priority);

    // Try to acquire the mutex to access shared resources
    pthread_mutex_lock(&engineerMutex);
    enqueue(&requestQueue, *sat);
    sem_post(&newRequest);  // Signal that a new request has arrived
    pthread_mutex_unlock(&engineerMutex);

    // Wait for an engineer to handle the request with a timeout
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TIMEOUT;

    if (sem_timedwait(&requestHandled, &ts) == -1) {
        printf("Satellite %d timed out and is leaving.\n", sat->id);
        return NULL;
    }

    printf("Satellite %d update completed.\n", sat->id);
    return NULL;
}

// Engineer thread function
void* engineer(void* arg) {
    while (1) {
        sem_wait(&newRequest);  // Wait for a new request

        pthread_mutex_lock(&engineerMutex);
        if (requestQueue.head != NULL) {
            Satellite sat = dequeue(&requestQueue);
            availableEngineers--;
            pthread_mutex_unlock(&engineerMutex);

            // Simulate handling the request
            printf("Engineer handling Satellite %d.\n", sat.id);
            sleep(rand() % 3 + 1);  // Simulate time taken to process the update

            pthread_mutex_lock(&engineerMutex);
            availableEngineers++;
            pthread_mutex_unlock(&engineerMutex);
            sem_post(&requestHandled);  // Signal that a request has been handled
        } else {
            pthread_mutex_unlock(&engineerMutex);
        }
    }
}

int main() {
    pthread_t engineers[NUM_ENGINEERS];
    pthread_t satellites[5];
    Satellite sat[5];
    srand(time(NULL));

    // Initialize mutex and semaphores
    pthread_mutex_init(&engineerMutex, NULL);
    sem_init(&newRequest, 0, 0);
    sem_init(&requestHandled, 0, 0);
    initQueue(&requestQueue);

    // Create engineer threads
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        pthread_create(&engineers[i], NULL, engineer, NULL);
    }

    // Create and start satellite threads
    for (int i = 0; i < 5; i++) {
        sat[i].id = i;
        sat[i].priority = rand() % PRIORITY_LEVELS + 1;  // Assign random priority
        pthread_create(&satellites[i], NULL, satellite, &sat[i]);
    }

    // Wait for all satellites to finish
    for (int i = 0; i < 5; i++) {
        pthread_join(satellites[i], NULL);
    }

    // Clean up
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        pthread_cancel(engineers[i]);  // Stop engineer threads
        pthread_join(engineers[i], NULL);
    }

    pthread_mutex_destroy(&engineerMutex);
    sem_destroy(&newRequest);
    sem_destroy(&requestHandled);
    return 0;
}