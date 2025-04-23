#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define NUM_ENGINEERS 3
#define TIMEOUT 5

#define PRIORITY_LEVELS 4

// Shared resources
int availableEngineers = NUM_ENGINEERS;
pthread_mutex_t engineerMutex;
pthread_mutex_t queueMutex; 
sem_t newRequest;
int shutdownFlag = 0;

typedef struct {
    int id;
    int priority;
    sem_t handled_sem; // Per-satellite semaphore
} Satellite;

typedef struct Node {
    Satellite* satellite; // Store pointers instead of copies
    struct Node* next;
} Node;

typedef struct {
    Node* head;
} PriorityQueue;

PriorityQueue requestQueue;

void initQueue(PriorityQueue* pq) {
    pq->head = NULL;
}

void enqueue(PriorityQueue* pq, Satellite* satellite) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->satellite = satellite;
    newNode->next = NULL;

    // Case 1: Empty queue or new node has higher priority than head
    if (pq->head == NULL || pq->head->satellite->priority < satellite->priority) {
        newNode->next = pq->head;
        pq->head = newNode;
        return;
    }

    // Case 2: Same priority as head; insert after existing same-priority nodes
    Node* current = pq->head;
    while (current->next != NULL && 
           (current->next->satellite->priority > satellite->priority || 
            (current->next->satellite->priority == satellite->priority && current->next != NULL))) {
        current = current->next;
    }
    newNode->next = current->next;
    current->next = newNode;
}

Satellite* dequeue(PriorityQueue* pq) {
    if (pq->head == NULL) return NULL;
    Node* temp = pq->head;
    Satellite* satellite = temp->satellite;
    pq->head = pq->head->next;
    free(temp);
    return satellite;
}


void removeFromQueue(PriorityQueue* pq, int id) {
    Node** indirect = &pq->head;
    while (*indirect != NULL) {
        if ((*indirect)->satellite->id == id) {
            Node* to_remove = *indirect;
            *indirect = to_remove->next;
            free(to_remove);
            return;
        }
        indirect = &(*indirect)->next;
    }
}

void engineer_cleanup(void* arg) {
    int engineer_id = *((int*)arg);
    printf("[ENGINEER %d] Exiting...\n", engineer_id);
    free(arg);
}

void* satellite(void* arg) {
    Satellite* sat = (Satellite*)arg;
    printf("[SAIELLITE] Satellite %d requesting (priority %d)\n", sat->id, sat->priority);

    pthread_mutex_lock(&queueMutex);
    enqueue(&requestQueue, sat); // Pass pointer
    sem_post(&newRequest);
    pthread_mutex_unlock(&queueMutex);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += TIMEOUT;

    if (sem_timedwait(&sat->handled_sem, &ts) == -1) {
        pthread_mutex_lock(&queueMutex);
        removeFromQueue(&requestQueue, sat->id);
        pthread_mutex_unlock(&queueMutex);
        printf("[TIMEOUT] Satellite %d timeout %d second.\n", sat->id, TIMEOUT);
    }
    return NULL;
}

void* engineer(void* arg) {
    int* engineer_id_ptr = (int*)arg;
    int engineer_id = *engineer_id_ptr;
    pthread_cleanup_push(engineer_cleanup, engineer_id_ptr);

    while (1) {
        sem_wait(&newRequest);

        pthread_mutex_lock(&engineerMutex);
        if (shutdownFlag && requestQueue.head == NULL) {
            pthread_mutex_unlock(&engineerMutex);
            break;
        }
        if (requestQueue.head != NULL) {
            pthread_mutex_lock(&queueMutex);
            Satellite* sat = dequeue(&requestQueue); // Get pointer
            pthread_mutex_unlock(&queueMutex);
            availableEngineers--;
            
            sem_post(&sat->handled_sem); // cancel timeout

            pthread_mutex_unlock(&engineerMutex);

            printf("[ENGINEER %d] Handling Satellite %d (Priority %d)\n", engineer_id, sat->id, sat->priority);
            sleep(rand() % 3 + 1);

            pthread_mutex_lock(&engineerMutex);
            availableEngineers++;
            pthread_mutex_unlock(&engineerMutex);

            
            printf("[ENGINEER %d] Finished Satellite %d\n", engineer_id, sat->id);
        } else {
            pthread_mutex_unlock(&engineerMutex);
        }
    }

    pthread_cleanup_pop(1);
    return NULL;
}

int main() {
    pthread_t engineers[NUM_ENGINEERS];
    Satellite sat[5];
    srand(time(NULL));

    // Initialize satellite semaphores
    for (int i = 0; i < 5; i++) {
        sem_init(&sat[i].handled_sem, 0, 0); // Initialize here
        sat[i].id = i;
        sat[i].priority = rand() % PRIORITY_LEVELS + 1;
    }

    pthread_mutex_init(&engineerMutex, NULL);
    pthread_mutex_init(&queueMutex, NULL);
    sem_init(&newRequest, 0, 0);
    initQueue(&requestQueue);

    // Create engineers
    for (int i = 0; i < NUM_ENGINEERS; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&engineers[i], NULL, engineer, id);
    }

    // Create satellites
    pthread_t satellite_threads[5];
    for (int i = 0; i < 5; i++) {
        pthread_create(&satellite_threads[i], NULL, satellite, &sat[i]);
    }

    // Join satellites
    for (int i = 0; i < 5; i++) {
        pthread_join(satellite_threads[i], NULL);
    }

    // Signal shutdown
    pthread_mutex_lock(&engineerMutex);
    shutdownFlag = 1;
    pthread_mutex_unlock(&engineerMutex);

    // Wake engineers
    for (int i = 0; i < NUM_ENGINEERS; i++) sem_post(&newRequest);

    // Join engineers
    for (int i = 0; i < NUM_ENGINEERS; i++) pthread_join(engineers[i], NULL);

    // Cleanup
    for (int i = 0; i < 5; i++) sem_destroy(&sat[i].handled_sem);
    pthread_mutex_destroy(&engineerMutex);
    pthread_mutex_destroy(&queueMutex);
    sem_destroy(&newRequest);

    return 0;
}