#ifndef PGY_RUNTIME_CONTEXT_H
#define PGY_RUNTIME_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "pgy_runtime_linkage.h"
#include "pgy_runtime_capability.h"
#include "pgy_runtime_budget.h"

/* The LLVM bitcode twin must reference the native runtime object's context,
 * otherwise an inlined gate can silently create a second state. The C cext and
 * LLVM runtime objects both provide the external definitions; ordinary
 * self-contained C keeps the historical static-inline form. */
#if defined(PGY_RUNTIME_BC_BUILD) || defined(PGY_RUNTIME_DECLS_ONLY)
#  define PGY_CONTEXT_DECL extern
#  define PGY_CONTEXT_GLOBAL extern
#elif defined(PGY_RUNTIME_LIB_INTERNAL) || defined(PGY_RUNTIME_EXTERN_DEFS)
#  define PGY_CONTEXT_DECL
#  define PGY_CONTEXT_GLOBAL
#else
#  define PGY_CONTEXT_DECL static inline
#  define PGY_CONTEXT_GLOBAL static
#endif

typedef struct {
    uint32_t manifest;
    uint32_t env;
    int      env_latched;
} PgyCapMasks;

/* Capability and quantitative budget state share one explicitly bound
 * instance. Other runtime authorities are separate owners until their
 * migration reaches this boundary. */
typedef struct {
    PgyCapMasks    capabilities;
    PgyBudgetState budget;
    /* Task contexts snapshot capability masks but share the parent's
     * quantitative budget authority.  Pointing at the owning state prevents
     * each worker TLS from resetting counters and bypassing a process/content
     * ceiling.  Root contexts point this field at their inline `budget`. */
    PgyBudgetState *budget_owner;
    uint64_t       instance_id;
    int            initialized;
} PgyRuntimeContext;

PGY_CONTEXT_GLOBAL _Thread_local PgyRuntimeContext *g_pgy_runtime_context_current;
PGY_CONTEXT_GLOBAL PgyRuntimeContext g_pgy_runtime_context_default
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
    = {0}
#endif
;
PGY_CONTEXT_GLOBAL pthread_once_t g_pgy_runtime_context_default_once
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
    = PTHREAD_ONCE_INIT
#endif
;

PGY_CONTEXT_DECL void
pgy_runtime_context_init(PgyRuntimeContext *context, uint64_t instance_id)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    unsigned env_mask;

    if (context == NULL)
        return;
    context->capabilities.manifest = PGY_CAP_ALL;
    context->capabilities.env = PGY_CAP_ALL;
    context->capabilities.env_latched = 1;
    if (pgy_cap_env_grant(&env_mask))
        context->capabilities.env = env_mask;
    pgy_budget_state_init(&context->budget);
    context->budget_owner = &context->budget;
    context->instance_id = instance_id;
    context->initialized = 1;
}
#else
;
#endif

#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
static void
pgy_runtime_context_default_init(void)
{
    pgy_runtime_context_init(&g_pgy_runtime_context_default, 0);
}
#endif

PGY_CONTEXT_DECL PgyRuntimeContext *
pgy_runtime_context_default(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    if (pthread_once(&g_pgy_runtime_context_default_once,
                     pgy_runtime_context_default_init) != 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "runtime default context initialization failed");
    }
    return &g_pgy_runtime_context_default;
}
#else
;
#endif

PGY_CONTEXT_DECL PgyRuntimeContext *
pgy_runtime_context_current(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    if (g_pgy_runtime_context_current == NULL)
        g_pgy_runtime_context_current = pgy_runtime_context_default();
    return g_pgy_runtime_context_current;
}
#else
;
#endif

PGY_CONTEXT_DECL bool
pgy_runtime_context_bind(PgyRuntimeContext *context)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    if (context == NULL || !context->initialized ||
        context->budget_owner == NULL || !context->budget_owner->initialized)
        return false;
    g_pgy_runtime_context_current = context;
    return true;
}
#else
;
#endif

/* Capture the current authority at task creation. Capability masks are copied
 * so a child cannot observe an executor thread's broader default grant. The
 * budget state stays shared so charges across children contribute to the one
 * parent-owned ceiling instead of opening per-task counter authorities. */
PGY_CONTEXT_DECL bool
pgy_runtime_context_capture_task(PgyRuntimeContext *task_context)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    PgyRuntimeContext *parent;

    if (task_context == NULL)
        return false;
    parent = pgy_runtime_context_current();
    if (parent == NULL || !parent->initialized ||
        parent->budget_owner == NULL || !parent->budget_owner->initialized)
        return false;
    task_context->capabilities = parent->capabilities;
    task_context->budget_owner = parent->budget_owner;
    task_context->instance_id = parent->instance_id;
    task_context->initialized = 1;
    return true;
}
#else
;
#endif

PGY_CONTEXT_DECL void
pgy_runtime_context_unbind(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    g_pgy_runtime_context_current = pgy_runtime_context_default();
}
#else
;
#endif

PGY_CONTEXT_DECL uint64_t
pgy_runtime_context_instance_id(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    return pgy_runtime_context_current()->instance_id;
}
#else
;
#endif

PGY_CONTEXT_DECL PgyCapMasks *
pgy_cap_masks_slot(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    return &pgy_runtime_context_current()->capabilities;
}
#else
;
#endif

PGY_CONTEXT_DECL PgyBudgetState *
pgy_budget_state_slot(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    PgyRuntimeContext *context = pgy_runtime_context_current();
    return context->budget_owner;
}
#else
;
#endif

#endif /* PGY_RUNTIME_CONTEXT_H */
