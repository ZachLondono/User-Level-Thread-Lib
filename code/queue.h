#ifndef QUEUE
#define QUEUE

#include <stdlib.h>

typedef struct node {
    struct node* next;
    void* data;
} QueueNode;

typedef struct _Queue {
    QueueNode* head;
    QueueNode* tail;
} Queue;

void initQueue(Queue* queue);
void enqueue(Queue* queue, void* data);
void* dequeue(Queue* queue);
int queuesize(Queue* queue);
void** queuecontents(Queue* queue);

#endif
