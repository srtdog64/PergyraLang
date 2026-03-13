#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_H

#include <stddef.h>

typedef struct
{
    void *result;
} PgyTaskHandle;

static inline void
pgy_pool_init(size_t worker_count)
{
    (void)worker_count;
}

static inline void
pgy_pool_shutdown(void)
{
}

static inline PgyTaskHandle
pgy_spawn(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle;
    handle.result = fn != NULL ? fn(arg) : NULL;
    return handle;
}

static inline void *
pgy_await(PgyTaskHandle handle)
{
    return handle.result;
}

#endif
