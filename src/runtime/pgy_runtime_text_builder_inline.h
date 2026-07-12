#ifndef PGY_RUNTIME_TEXT_BUILDER_INLINE_H
#define PGY_RUNTIME_TEXT_BUILDER_INLINE_H

/*
 * Owned text assembly substrate. The builder keeps intermediate bytes in one
 * growable allocation and makes promotion to a result allocator explicit.
 * This owner is intentionally below the Pergyra builtin surface for now.
 */
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool finished;
} PgyTextBuilder;

static inline PgyTextBuilder
pgy_text_builder_new(int64_t initial_capacity)
{
    PgyTextBuilder builder;
    size_t capacity;

    if (initial_capacity < 0
        || (uint64_t)initial_capacity > (uint64_t)SIZE_MAX)
        PGY_PANIC("TextBuilder capacity must be non-negative and target-sized");
    capacity = initial_capacity > 0 ? (size_t)initial_capacity : 64;

    memset(&builder, 0, sizeof(builder));
    builder.data = (char *)pgy_alloc(NULL, capacity, _Alignof(char));
    builder.capacity = capacity;
    builder.data[0] = '\0';
    return builder;
}

static inline void
pgy_text_builder_reserve(PgyTextBuilder *builder, size_t required)
{
    size_t capacity;
    char *grown;

    if (builder == NULL || builder->finished)
        PGY_PANIC("TextBuilder reserve after finish or drop");
    if (required <= builder->capacity)
        return;

    capacity = builder->capacity > 0 ? builder->capacity : 64;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    if (capacity < required)
        PGY_PANIC("TextBuilder capacity overflow");

    grown = (char *)pgy_realloc(NULL, builder->data,
                               builder->capacity, capacity);
    builder->data = grown;
    builder->capacity = capacity;
    builder->data[builder->length] = '\0';
}

static inline void
pgy_text_builder_append_n(PgyTextBuilder *builder,
                          const char *text,
                          size_t text_length)
{
    size_t required;

    if (builder == NULL || builder->finished)
        PGY_PANIC("TextBuilder append after finish or drop");
    if (text == NULL || text_length == 0)
        return;
    if (builder->length == SIZE_MAX
        || text_length > SIZE_MAX - builder->length - 1)
        PGY_PANIC("TextBuilder length overflow");

    required = builder->length + text_length + 1;
    pgy_text_builder_reserve(builder, required);
    memcpy(builder->data + builder->length, text, text_length);
    builder->length += text_length;
    builder->data[builder->length] = '\0';
}

static inline void
pgy_text_builder_append(PgyTextBuilder *builder, const char *text)
{
    pgy_text_builder_append_n(builder, text,
                              text != NULL ? strlen(text) : 0);
}

static inline char *
pgy_text_builder_finish(PgyTextBuilder *builder, PgyAllocator *result_allocator)
{
    char *result;
    size_t result_size;

    if (builder == NULL || builder->finished)
        PGY_PANIC("TextBuilder finish after finish or drop");
    if (builder->length == SIZE_MAX)
        PGY_PANIC("TextBuilder result length overflow");

    result_size = builder->length + 1;
    if (result_allocator == NULL || result_allocator->pool == NULL) {
        result = builder->data;
        pgy_allocator_record_alloc(result_allocator, result_size);
    } else {
        result = (char *)pgy_alloc(result_allocator, result_size,
                                  _Alignof(char));
        memcpy(result, builder->data, result_size);
        pgy_free(NULL, builder->data, builder->capacity);
    }
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
    builder->finished = true;
    return result;
}

static inline void
pgy_text_builder_drop(PgyTextBuilder *builder)
{
    if (builder == NULL || builder->finished)
        PGY_PANIC("TextBuilder drop after finish or drop");
    pgy_free(NULL, builder->data, builder->capacity);
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
    builder->finished = true;
}

#endif /* PGY_RUNTIME_TEXT_BUILDER_INLINE_H */
