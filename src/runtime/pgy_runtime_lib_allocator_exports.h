#ifndef PGY_RUNTIME_LIB_ALLOCATOR_EXPORTS_H
#define PGY_RUNTIME_LIB_ALLOCATOR_EXPORTS_H

#ifndef PGY_PANIC
#define PGY_PANIC(msg) \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, (msg))
#endif

#include "pgy_runtime_allocator_inline.h"

void
pgy_allocator_system_init(PgyAllocator *out)
{
    if (out != NULL)
        *out = pgy_allocator_system();
}

void
pgy_allocator_tracing_init(PgyAllocator *out)
{
    if (out != NULL)
        *out = pgy_allocator_tracing();
}

void
pgy_allocator_debug_init(PgyAllocator *out)
{
    if (out != NULL)
        *out = pgy_allocator_debug();
}

void
pgy_allocator_pool_init(PgyAllocator *out, size_t capacity)
{
    if (out != NULL)
        *out = pgy_allocator_pool(capacity);
}

void
pgy_allocator_destroy_export(PgyAllocator *alloc)
{
    pgy_allocator_destroy(alloc);
}

#endif /* PGY_RUNTIME_LIB_ALLOCATOR_EXPORTS_H */
