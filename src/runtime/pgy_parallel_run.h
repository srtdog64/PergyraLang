#include "pgy_runtime_linkage.h"
/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parallel block execution helpers.
 * BSD 3-Clause License
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_RUN_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_RUN_H

/* =================================================================
 * Parallel block helpers
 * ================================================================= */

typedef struct {
    void (*fn)(void);
} PgyParallelArg;

static void *
pgy_parallel_wrapper(void *raw)
{
    PgyParallelArg *parg = (PgyParallelArg *)raw;
    if (parg != NULL && parg->fn != NULL)
        parg->fn();
    return NULL;
}

PGY_RT_DECL void
pgy_parallel_run(void (**tasks)(void), size_t count)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (count == 0)
        return;
    if (tasks == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "parallel task array is null");
    }

    if (count == 1) {
        if (tasks[0] == NULL) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "parallel task is null");
        }
        tasks[0]();
        return;
    }

    if (!pgy_parallel_array_fits(count, sizeof(PgyTaskHandle))
        || !pgy_parallel_array_fits(count, sizeof(PgyParallelArg))) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }

    PgyTaskHandle *handles =
        (PgyTaskHandle *)calloc(count, sizeof(PgyTaskHandle));
    PgyParallelArg *args =
        (PgyParallelArg *)calloc(count, sizeof(PgyParallelArg));
    if (handles == NULL || args == NULL) {
        free(handles);
        free(args);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }

    for (size_t i = 0; i < count; i++) {
        if (tasks[i] == NULL) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "parallel task is null");
        }
        args[i].fn = tasks[i];
        handles[i] = pgy_spawn(pgy_parallel_wrapper, &args[i]);
        if (handles[i].task == NULL) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "parallel task spawn failed");
        }
    }

    for (size_t i = 0; i < count; i++)
        pgy_await(handles[i]);

    free(handles);
    free(args);
}
#else
;
#endif


#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_RUN_H */
