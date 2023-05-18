#ifndef QUEUE_H_
#define QUEUE_H_
#include <vos.h>

typedef struct Queue_ {
    uint16 entry_count;
    uint16 entry_size;
    volatile uint16 write_index;
    volatile uint16 read_index;
    volatile uint8 *buffer;
    vos_mutex_t lock;
    vos_cond_var_t not_full_cond_var;
    vos_cond_var_t not_empty_cond_var;
} Queue;

Queue* create_queue(uint16 entry_count, uint16 entry_size);
void enqueue(Queue* queue, const void *data);
void dequeue(Queue* queue, void *data);
int try_dequeue(Queue *queue, void *data);

#endif
