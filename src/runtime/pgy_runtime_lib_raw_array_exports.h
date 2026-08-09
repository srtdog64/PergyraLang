/* Raw byte-array exports for LLVM Array<T> storage operations.
 * The view layout intentionally matches PgyArray_<T>:
 * data pointer, length, capacity, allocator pointer. */

typedef struct {
    void  *data;
    size_t length;
    size_t capacity;
    void  *allocator;
} PgyRawArrayExport;

static void
pgy_array_raw_require_elem_size(size_t elem_size)
{
    if (elem_size == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "raw array element size is zero");
    }
}

static void
pgy_array_raw_reserve(PgyRawArrayExport *arr,
                      size_t new_capacity,
                      size_t elem_size)
{
    size_t new_size;
    void *next_data;

    if (arr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "raw array reserve on null array");
    }
    pgy_array_raw_require_elem_size(elem_size);
    if (new_capacity <= arr->capacity)
        return;
    if (new_capacity > SIZE_MAX / elem_size) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    new_size = elem_size * new_capacity;
    next_data = arr->data == NULL ? malloc(new_size)
                                  : realloc(arr->data, new_size);
    if (next_data == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    arr->data = next_data;
    arr->capacity = new_capacity;
}

void
pgy_array_new_raw_export(void *arr_ptr, size_t capacity, size_t elem_size)
{
    PgyRawArrayExport *arr = (PgyRawArrayExport *)arr_ptr;

    if (arr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "raw array new on null array");
    }
    pgy_array_raw_require_elem_size(elem_size);
    arr->data = NULL;
    arr->length = 0;
    arr->capacity = 0;
    arr->allocator = NULL;
    pgy_array_raw_reserve(arr, capacity, elem_size);
}

void
pgy_array_push_raw_export(void *arr_ptr, const void *value_ptr,
                          size_t elem_size)
{
    PgyRawArrayExport *arr = (PgyRawArrayExport *)arr_ptr;
    size_t next_capacity;
    char *dst;

    if (arr == NULL || value_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "raw array push on null argument");
    }
    pgy_array_raw_require_elem_size(elem_size);
    if (arr->length == arr->capacity) {
        if (arr->capacity == 0) {
            next_capacity = 4;
        } else {
            if (arr->capacity > SIZE_MAX / 2) {
                PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                                  PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
            }
            next_capacity = arr->capacity * 2;
        }
        pgy_array_raw_reserve(arr, next_capacity, elem_size);
    }
    dst = (char *)arr->data + (arr->length * elem_size);
    memcpy(dst, value_ptr, elem_size);
    arr->length++;
}

void
pgy_array_get_raw_export(void *arr_ptr, size_t index, void *out_ptr,
                         size_t elem_size)
{
    PgyRawArrayExport *arr = (PgyRawArrayExport *)arr_ptr;
    const char *src;

    if (arr == NULL || out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "raw array get on null argument");
    }
    pgy_array_raw_require_elem_size(elem_size);
    if (index >= arr->length) {
        pgy_runtime_panic_out_of_bounds_export(
            PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS);
    }
    src = (const char *)arr->data + (index * elem_size);
    memcpy(out_ptr, src, elem_size);
}

void
pgy_array_set_raw_export(void *arr_ptr, size_t index, const void *value_ptr,
                         size_t elem_size)
{
    PgyRawArrayExport *arr = (PgyRawArrayExport *)arr_ptr;
    char *dst;

    if (arr == NULL || value_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "raw array set on null argument");
    }
    pgy_array_raw_require_elem_size(elem_size);
    if (index >= arr->length) {
        pgy_runtime_panic_out_of_bounds_export(
            PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS);
    }
    dst = (char *)arr->data + (index * elem_size);
    memcpy(dst, value_ptr, elem_size);
}

void
pgy_array_pop_raw_export(void *arr_ptr)
{
    PgyRawArrayExport *arr = (PgyRawArrayExport *)arr_ptr;

    if (arr == NULL)
        return;
    if (arr->length > 0)
        arr->length--;
}

void
pgy_array_drop_storage_raw_export(void *arr_ptr, size_t elem_size)
{
    PgyRawArrayExport *arr = (PgyRawArrayExport *)arr_ptr;

    if (arr == NULL)
        return;
    pgy_array_raw_require_elem_size(elem_size);
    if (arr->data != NULL) {
        if (arr->capacity > SIZE_MAX / elem_size) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "raw array drop capacity overflow");
        }
        pgy_free(arr->allocator, arr->data, arr->capacity * elem_size);
    }
    arr->data = NULL;
    arr->length = 0;
    arr->capacity = 0;
}
