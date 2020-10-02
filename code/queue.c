#include "queue.h"

void initQueue(Queue* queue) {
    queue->head = NULL;
    queue->tail = NULL;    
}

void enqueue(Queue *queue,void* data)  {
    QueueNode* newnode = malloc(sizeof(QueueNode));
    newnode->data = data;
    newnode->next = NULL;
    if (queue->tail == NULL) queue->head = newnode;
    else queue->tail->next = newnode;
    queue->tail = newnode;
}

void* dequeue(Queue *queue) {
    if (queue->head == NULL) {
        return NULL;
    } else {
        void *ret = queue->head->data;
        QueueNode *temp = queue->head;
        queue->head = queue->head->next;
        if (queue->head == NULL) queue->tail = NULL;
        free(temp);
        return ret;
    }
}

int queuesize(Queue* queue) {

    if (queue->head == NULL) return 0;
    
    int count = 0;
    QueueNode* node = queue->head;
    while(node != NULL) {
        count++;
        node = node->next;
    }

    return count;

}

void** queuecontents(Queue* queue) {

    int size = 0;
    if ((size = queuesize(queue)) == 0) return NULL;
    
    void** contents = malloc(sizeof(void*) * size);

    int i = 0;
    QueueNode* node = queue->head;
    while(node != NULL) {
        contents[i] = node->data;
        node = node->next;
        i++;
    }

    return contents;
}