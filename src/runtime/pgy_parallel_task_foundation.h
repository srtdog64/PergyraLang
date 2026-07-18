#include "pgy_runtime_linkage.h"
/*
 * Shared task foundation for pgy_parallel.h.
 *
 * This owner keeps task diagnostics, allocation bounds, cancellation-node
 * lifetime, and the runtime cancellation probe together. PgyCancelNode and
 * pgy_current_cancel_node are declared by the parent before this header.
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_TASK_FOUNDATION_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_TASK_FOUNDATION_H

PGY_RT_DECL void
pgy_parallel_warn(const char *op, const char *reason)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    fprintf(stderr,
            "[pgy][parallel] %s failed: %s\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown");
}
#else
;
#endif


PGY_RT_DECL bool
pgy_parallel_array_fits(size_t count, size_t elem_size)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}
#else
;
#endif


PGY_RT_DECL void
pgy_cancel_retain(PgyCancelNode *node)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (node != NULL)
        (void)atomic_fetch_add_explicit(&node->refcount, 1, memory_order_relaxed);
}
#else
;
#endif


PGY_RT_DECL PgyCancelNode *
pgy_cancel_node_create(PgyCancelNode *parent)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyCancelNode *node = (PgyCancelNode *)calloc(1, sizeof(PgyCancelNode));
    if (node == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    node->parent = parent;
    atomic_init(&node->refcount, 1);
    atomic_init(&node->cancelled, false);
    pgy_cancel_retain(parent);
    return node;
}
#else
;
#endif


PGY_RT_DECL void
pgy_cancel_release(PgyCancelNode *node)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (node == NULL)
        return;

    if (atomic_fetch_sub_explicit(&node->refcount, 1, memory_order_acq_rel) != 1)
        return;

    PgyCancelNode *parent = node->parent;
    free(node);
    pgy_cancel_release(parent);
}
#else
;
#endif


PGY_RT_DECL void
pgy_cancel_request(PgyCancelNode *node)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (node != NULL)
        atomic_store_explicit(&node->cancelled, true, memory_order_release);
}
#else
;
#endif


PGY_RT_DECL bool
pgy_cancel_is_requested(PgyCancelNode *node)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    while (node != NULL) {
        if (atomic_load_explicit(&node->cancelled, memory_order_acquire))
            return true;
        node = node->parent;
    }
    return false;
}
#else
;
#endif


/* Channel waits call the installed hook, while this object-local adapter reads
 * the current task's cancellation chain. */
#ifndef PGY_RUNTIME_DECLS_ONLY
static bool
pgy_parallel_cancel_probe(void)
{
    return pgy_cancel_is_requested(pgy_current_cancel_node());
}
#endif

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_TASK_FOUNDATION_H */
