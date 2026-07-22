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
    uint64_t       instance_id;
    int            initialized;
} PgyRuntimeContext;

PGY_CONTEXT_GLOBAL _Thread_local PgyRuntimeContext *g_pgy_runtime_context_current;

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
    context->instance_id = instance_id;
    context->initialized = 1;
}
#else
;
#endif

PGY_CONTEXT_DECL PgyRuntimeContext *
pgy_runtime_context_default(void)
#if !defined(PGY_RUNTIME_BC_BUILD) && !defined(PGY_RUNTIME_DECLS_ONLY)
{
    static PgyRuntimeContext context;

    if (!context.initialized)
        pgy_runtime_context_init(&context, 0);
    return &context;
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
    if (context == NULL || !context->initialized)
        return false;
    g_pgy_runtime_context_current = context;
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
    return &pgy_runtime_context_current()->budget;
}
#else
;
#endif

#endif /* PGY_RUNTIME_CONTEXT_H */
