#include "Queue.h"

static uint16 cyclic_increment(uint16 i, uint16 limit)
{
    uint16 n;
	
	n = i + 1;
    if (n >= limit) return 0;
    return n;
}

Queue* create_queue(uint16 entry_count, uint16 entry_size)
{
    Queue *queue;
	
	queue = vos_malloc(sizeof(Queue) + (entry_count * entry_size));
    if (queue == NULL) return NULL;

    queue->entry_count = entry_count;
    queue->entry_size = entry_size;
    queue->write_index = 0;
    queue->read_index = 0;
    queue->buffer = ((uint8*)queue) + sizeof(Queue);
    vos_init_mutex(&(queue->lock), VOS_MUTEX_UNLOCKED);
    vos_init_cond_var(&(queue->not_full_cond_var));
    vos_init_cond_var(&(queue->not_empty_cond_var));

    return queue;
}

void enqueue(Queue* queue, const void *data)
{
    uint16 current_write_index, next_write_index, entry_size;

    vos_lock_mutex(&queue->lock);

    current_write_index = (uint16)queue->write_index;
    next_write_index = cyclic_increment(current_write_index, queue->entry_count);
    if (next_write_index == queue->read_index) {
        vos_wait_cond_var(&queue->not_full_cond_var, &queue->lock);
    }

    entry_size = queue->entry_size;
    vos_memcpy(queue->buffer +  entry_size * current_write_index, data, (short)entry_size);

    queue->write_index = next_write_index;
    if (current_write_index == queue->read_index) {
        vos_signal_cond_var(&queue->not_empty_cond_var);
    }

    vos_unlock_mutex(&queue->lock);
}

void dequeue(Queue* queue, void *data)
{
    uint16 current_read_index, entry_size;

    vos_lock_mutex(&queue->lock);
    current_read_index = queue->read_index;
    if (current_read_index == queue->write_index) {
        vos_wait_cond_var(&queue->not_empty_cond_var, &queue->lock);
    }

    entry_size = queue->entry_size;
    vos_memcpy(data, queue->buffer + entry_size * current_read_index, (short)entry_size);

    queue->read_index = cyclic_increment(queue->read_index, queue->entry_count);
    if (current_read_index == cyclic_increment(queue->write_index, queue->entry_count)) {
        vos_signal_cond_var(&queue->not_full_cond_var);
    }

    vos_unlock_mutex(&queue->lock);
}

int try_dequeue(Queue *queue, void *data)
{
    uint16 current_read_index, entry_size;

    if (vos_trylock_mutex(&queue->lock) != VOS_MUTEX_LOCKED) {
        return 0;
    }
    current_read_index = queue->read_index;
    if (current_read_index == queue->write_index) {
        vos_unlock_mutex(&queue->lock);
        return 0;
    }

    entry_size = queue->entry_size;
    vos_memcpy(data, queue->buffer + entry_size * current_read_index, (short)entry_size);

    queue->read_index = cyclic_increment(queue->read_index, queue->entry_count);
    if (current_read_index == cyclic_increment(queue->write_index, queue->entry_count)) {
        vos_signal_cond_var(&queue->not_full_cond_var);
    }
    vos_unlock_mutex(&queue->lock);

    return 1;
}

