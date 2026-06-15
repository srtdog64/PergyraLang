#ifndef PGY_RUNTIME_LIB_BOX_ARRAY_EXPORTS_H
#define PGY_RUNTIME_LIB_BOX_ARRAY_EXPORTS_H

#define PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(Suffix, CType)                           \
typedef struct {                                                                 \
    PgyArray_##Suffix array;                                                      \
    CType storage[];                                                             \
} PgyBoxArrayExportStorage_##Suffix;                                             \
                                                                                 \
void *pgy_box_array_new_ptr_##Suffix(size_t capacity, PgyAllocator *alloc)       \
{                                                                                \
    size_t bytes;                                                                \
    PgyBoxArrayExportStorage_##Suffix *storage;                                  \
    if (capacity > (SIZE_MAX - sizeof(PgyBoxArrayExportStorage_##Suffix))        \
            / sizeof(CType)) {                                                   \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,                           \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);           \
    }                                                                            \
    bytes = sizeof(PgyBoxArrayExportStorage_##Suffix) + sizeof(CType) * capacity;\
    storage = (PgyBoxArrayExportStorage_##Suffix *)                              \
        pgy_alloc(alloc, bytes, _Alignof(PgyBoxArrayExportStorage_##Suffix));    \
    storage->array.data = storage->storage;                                      \
    storage->array.length = 0;                                                   \
    storage->array.capacity = capacity;                                          \
    storage->array.allocator = alloc;                                            \
    return &storage->array;                                                      \
}

PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(Int, int32_t)
PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(Long, int64_t)
PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(Float, float)
PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(Double, double)
PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(Bool, bool)
PGY_DEFINE_BOX_ARRAY_PTR_EXPORT(String, char*)

#undef PGY_DEFINE_BOX_ARRAY_PTR_EXPORT

#endif /* PGY_RUNTIME_LIB_BOX_ARRAY_EXPORTS_H */
