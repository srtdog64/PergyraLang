#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void   *data;
    size_t  head;
    size_t  tail;
    size_t  count;
    size_t  capacity;
} PgyQueueRaw;

static bool
pgy_queue_raw_shape_fits(size_t capacity, size_t elem_size)
{
    return capacity != 0
        && capacity <= (size_t)INT32_MAX
        && elem_size != 0
        && elem_size <= SIZE_MAX / capacity;
}

static bool
pgy_queue_raw_is_initialized(const PgyQueueRaw *queue)
{
    return queue != NULL
        && queue->capacity != 0
        && queue->capacity <= (size_t)INT32_MAX
        && queue->data != NULL;
}

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
    if (!pgy_queue_raw_shape_fits(queue->capacity, (size_t)elem_size)) {
        queue->capacity = 0;
        pgy_runtime_warn_invalid_collection("queue_new", "allocation size overflow");
        return;
    }
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
    if (!pgy_queue_raw_is_initialized(queue)) {
        pgy_runtime_warn_invalid_collection("queue_push", "queue is not initialized");
        return;
    }
    if (queue->count >= queue->capacity) {
        size_t elem_bytes = (size_t)elem_size;
        size_t new_capacity;
        void *new_data;
        if (queue->capacity == 0) {
            new_capacity = 16;
        } else {
            if (queue->capacity > SIZE_MAX / 2) {
                pgy_runtime_warn_invalid_collection("queue_push", "capacity overflow");
                return;
            }
            new_capacity = queue->capacity * 2;
        }
        if (!pgy_queue_raw_shape_fits(new_capacity, elem_bytes)) {
            pgy_runtime_warn_invalid_collection("queue_push", "allocation size overflow");
            return;
        }
        new_data = calloc(new_capacity, elem_bytes);
        if (new_data == NULL) {
            pgy_runtime_warn_invalid_collection("queue_push", "allocation failed");
            return;
        }
        for (size_t i = 0; i < queue->count; i++) {
            memcpy((char *)new_data + (i * elem_bytes),
                   (char *)queue->data + (((queue->head + i) % queue->capacity) * elem_bytes),
                   elem_bytes);
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
pgy_queue_push_string_raw_export(void *queue_ptr, const char *value)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    size_t before_count;
    char *owned;

    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push_string", "null queue");
        return;
    }
    if (!pgy_queue_raw_is_initialized(queue)
        || !pgy_queue_raw_shape_fits(queue->capacity, sizeof(char *))) {
        pgy_runtime_warn_invalid_collection("queue_push_string",
            "queue is not initialized");
        return;
    }
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push_string", "string duplication failed");
        return;
    }
    before_count = queue->count;
    pgy_queue_push_raw_export(queue_ptr, &owned, (int64_t)sizeof(char *));
    if (queue->count == before_count)
        free(owned);
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
    if (!pgy_queue_raw_is_initialized(queue)
        || !pgy_queue_raw_shape_fits(queue->capacity, (size_t)elem_size)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on uninitialized queue");
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

void
pgy_queue_pop_string_raw_export(void *queue_ptr, char **out_ptr)
{
    char *owned = NULL;

    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop string on null output");
    }
    pgy_queue_pop_raw_export(queue_ptr, &owned, (int64_t)sizeof(char *));
    *out_ptr = owned != NULL ? owned : pgy_runtime_strdup_export("");
}

int32_t
pgy_queue_size_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_size", "null queue");
        return 0;
    }
    if (!pgy_queue_raw_is_initialized(queue)) {
        pgy_runtime_warn_invalid_collection("queue_size", "queue is not initialized");
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
    if (queue == NULL)
        return true;
    if (!pgy_queue_raw_is_initialized(queue)) {
        pgy_runtime_warn_invalid_collection("queue_empty", "queue is not initialized");
        return true;
    }
    return queue->count == 0;
}
