typedef struct {
    void   *data;
    size_t  head;
    size_t  tail;
    size_t  count;
    size_t  capacity;
} PgyQueueRaw;

void
pgy_queue_new_raw_export(void *queue_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_new", "null queue");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_new", "non-positive element size");
        return;
    }
    queue->capacity = 16;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->data = calloc(queue->capacity, (size_t)elem_size);
    if (queue->data == NULL) {
        queue->capacity = 0;
        pgy_runtime_warn_invalid_collection("queue_new", "allocation failed");
    }
}

void
pgy_queue_push_raw_export(void *queue_ptr, void *value_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push", "null queue");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_push", "non-positive element size");
        return;
    }
    if (queue->data == NULL && queue->capacity == 0) {
        pgy_runtime_warn_invalid_collection("queue_push", "queue is not initialized");
        return;
    }
    if (queue->count >= queue->capacity) {
        size_t new_capacity = queue->capacity == 0 ? 16 : queue->capacity * 2;
        void *new_data = calloc(new_capacity, (size_t)elem_size);
        if (new_data == NULL) {
            pgy_runtime_warn_invalid_collection("queue_push", "allocation failed");
            return;
        }
        for (size_t i = 0; i < queue->count; i++) {
            memcpy((char *)new_data + (i * (size_t)elem_size),
                   (char *)queue->data + (((queue->head + i) % queue->capacity) * (size_t)elem_size),
                   (size_t)elem_size);
        }
        free(queue->data);
        queue->data = new_data;
        queue->head = 0;
        queue->tail = queue->count;
        queue->capacity = new_capacity;
    }
    memcpy((char *)queue->data + (queue->tail * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
}

void
pgy_queue_pop_raw_export(void *queue_ptr, void *out_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on null output");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop with invalid element size");
    }
    memset(out_ptr, 0, (size_t)elem_size);
    if (queue == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on null queue");
    }
    if (queue->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "queue pop from empty queue");
    }
    memcpy(out_ptr,
           (char *)queue->data + (queue->head * (size_t)elem_size),
           (size_t)elem_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
}

int32_t
pgy_queue_size_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_size", "null queue");
        return 0;
    }
    return (int32_t)queue->count;
}

bool
pgy_queue_empty_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL)
        pgy_runtime_warn_invalid_collection("queue_empty", "null queue");
    return queue == NULL || queue->count == 0;
}
