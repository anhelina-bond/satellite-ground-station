#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define NUM_ENGINEERS 3
#define TIMEOUT 5  // Maximum connection window in seconds

// Priority levels (higher number = higher priority)
#define PRIORITY_LEVELS 4

// Shared resources
int availableEngineers = NUM_ENGINEERS;
pthread_mutex_t engineerMutex;
sem_t newRequest;
sem_t requestHandled;
int shutdownFlag = 0;  

// Structure for Satellite
typedef struct {
    int id;
    int priority;
    sem_t handled_sem; 
} Satellite;

// Priority Queue Node
typedef struct Node {
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
        return empty;
    }
    Node* temp = pq->head;
    Satellite satellite = temp->satellite;
    pq->head = pq->head->next;
    free(temp);
    return satellite;
}


// Function to remove a satellite by ID from the queue
void removeFromQueue(PriorityQueue* pq, int id) {
    Node** indirect = &pq->head;
    while (*indirect != NULL) {
        if ((*indirect)->satellite.id == id) {
            Node* to_remove = *indirect;
            *indirect = to_remove->next;
            free(to_remove);
            return;
        }
        indirect = &(*indirect)->next;
    }
}

// Cleanup function for engineer threads
void engineer_cleanup(void* arg) {
    int engineer_id = *((int*)arg);
    printf("[ENGINEER %d] Exiting...\n", engineer_id);
    free(arg);
}

// Satellite thread function
void* satellite(void* arg) {
    Satellite* sat = (Satellite*)arg;
    printf("[SAIELLITE] Satellite %d requesting (priority %d)\n", sat->id, sat->priority);

    pthread_mutex_lock(&engineerMutex);
    enqueue(&requestQueue, *sat);
    sem_post(&newRequest);  // Notify engineers of new request
    pthread_mutex_unlock(&engineerMutex);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TIMEOUT;

    // Wait for THIS satellite's semaphore
    if (sem_timedwait(&sat->handled_sem, &ts) == -1) {
        pthread_mutex_lock(&engineerMutex);
        removeFromQueue(&requestQueue, sat->id);  // Remove self from queue
        pthread_mutex_unlock(&engineerMutex);
        printf("[TIMEOUT] Satellite %d timeout %d second.\n", sat->id, TIMEOUT);
    } else {
        printf("[SAIELLITE] Satellite %d update completed.\n", sat->id);
    }
    return NULL;
}

// Engineer thread function
void* engineer(void* arg) {
    int* engineer_id_ptr = (int*)arg;
    int engineer_id = *engineer_id_ptr;
    pthread_cleanup_push(engineer_cleanup, engineer_id_ptr);

    while (1) {
        sem_wait(&newRequest);

        pthread_mutex_lock(&engineerMutex);
        if (shutdownFlag && requestQueue.head == NULL) {
            pthread_mutex_unlock(&engineerMutex);
            break;  // Exit loop
        }
        if (requestQueue.head != NULL) {
            Satellite sat = dequeue(&requestQueue);
            availableEngineers--;
            pthread_mutex_unlock(&engineerMutex);

            printf("[ENGINEER %d] Handling Satellite %d (Priority %d)\n", engineer_id, sat.id, sat.priority);
            sleep(rand() % 3 + 1);

            pthread_mutex_lock(&engineerMutex);
            availableEngineers++;
            pthread_mutex_unlock(&engineerMutex);

            sem_post(&sat.handled_sem);
            printf("[ENGINEER %d] Finished Satellite %d\n", engineer_id, sat.id);
        } else {
            pthread_mutex_unlock(&engineerMutex);
        }
    }

    pthread_cleanup_pop(0);
    return NULL;
}

int main() {
    pthread_t engineers[NUM_ENGINEERS];
    pthread_t satellites[5];
    Satellite sat[5];
    srand(time(NULL));

    pthread_mutex_init(&engineerMutex, NULL);
    sem_init(&newRequest, 0, 0);
    sem_init(&requestHandled, 0, 0);
    initQueue(&requestQueue);

    // Create engineer threads with IDs
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&engineers[i], NULL, engineer, id);
    }

    // Create satellite threads
    for (int i = 0; i < 5; i++) {
        sat[i].id = i;
        sat[i].priority = rand() % PRIORITY_LEVELS + 1;
        pthread_create(&satellites[i], NULL, satellite, &sat[i]);
    }

    // Join satellite threads
    for (int i = 0; i < 5; i++) {
        pthread_join(satellites[i], NULL);
    }

    pthread_mutex_lock(&engineerMutex);
    shutdownFlag = 1;
    pthread_mutex_unlock(&engineerMutex);

    // Wake engineers to check shutdownFlag
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        sem_post(&newRequest);
    }

    // Join engineers (no cancellation)
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        pthread_join(engineers[i], NULL);
    }

    // Destroy semaphores and mutex AFTER engineers exit
    for (int i = 0; i < 5; i++) {
        sem_destroy(&sat[i].handled_sem);
    }
    pthread_mutex_destroy(&engineerMutex);
    sem_destroy(&newRequest);

    sem_destroy(&requestHandled);
    return 0;
}