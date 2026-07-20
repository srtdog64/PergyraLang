/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * PgyMnFiber implementation for Pergyra's SEA model
 * BSD Style + C# naming conventions
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "fiber.h"
#include "scheduler.h"
#include "../pgy_runtime_panic_contract.h"

/* Thread-local current fiber */
static __thread PgyMnFiber* tlsCurrentFiber = NULL;

/* PgyMnFiber ID counter */
static atomic_uint_least64_t fiberIdCounter = 0;

static void
fiber_warn(const char *op, const char *reason, PgyMnFiber *fiber)
{
    fprintf(stderr,
            "[pgy][fiber] %s failed: %s (fiber=%p)\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown",
            (void *)fiber);
}

PgyMnFiber* pgy_mn_fiber_create(PgyMnFiberFn startRoutine, void* arg)
{
    if (startRoutine == NULL) {
        fiber_warn("create", "start routine is null", NULL);
        return NULL;
    }
    
    PgyMnFiber* fiber = (PgyMnFiber*)calloc(1, sizeof(PgyMnFiber));
    if (fiber == NULL) {
        fiber_warn("create", "fiber allocation failed", NULL);
        return NULL;
    }
    
    /* Generate unique ID */
    fiber->id = atomic_fetch_add(&fiberIdCounter, 1);
    fiber->state = FIBER_STATE_NEW;
    
    /* Set execution parameters */
    fiber->startRoutine = startRoutine;
    fiber->arg = arg;
    
    /* Allocate stack */
    fiber->stackSize = FIBER_STACK_SIZE;
#ifdef _WIN32
    fiber->stackBase = VirtualAlloc(NULL, fiber->stackSize,
                                     MEM_COMMIT | MEM_RESERVE,
                                     PAGE_READWRITE);
    if (fiber->stackBase == NULL) {
        fiber_warn("create", "stack allocation failed", fiber);
        free(fiber);
        return NULL;
    }
#else
    int mapFlags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_STACK
    mapFlags |= MAP_STACK;
#endif
    fiber->stackBase = mmap(NULL, fiber->stackSize,
                           PROT_READ | PROT_WRITE,
                           mapFlags, -1, 0);
    if (fiber->stackBase == MAP_FAILED) {
        fiber_warn("create", "stack allocation failed", fiber);
        free(fiber);
        return NULL;
    }
#endif
    
    /* Initialize stack pointer to top of stack (stacks grow down) */
    fiber->context.stackPointer = (char*)fiber->stackBase + fiber->stackSize;

    /* Run-to-completion depth (docs/194 WO-MN-1): the fiber is READY and the
     * worker runs its routine directly on the worker stack. The setjmp dance
     * this replaced saved a context in THIS frame and "resumed" it after the
     * frame had returned -- measured as an immediate segfault the first time
     * the machine ever ran. Seeding fiber->context so the assembly switch can
     * start a NEW fiber on its own stack is the WO-MN-2 context layer. */
    fiber->state = FIBER_STATE_READY;

    return fiber;
}

void pgy_mn_fiber_destroy(PgyMnFiber* fiber)
{
    if (fiber == NULL) {
        return;
    }
    
    /* Cancel if still running */
    if (fiber->state == FIBER_STATE_RUNNING ||
        fiber->state == FIBER_STATE_READY ||
        fiber->state == FIBER_STATE_SUSPENDED) {
        pgy_mn_fiber_cancel(fiber);
    }
    
    /* Detach from parent */
    if (fiber->parent != NULL) {
        pgy_mn_fiber_detach_child(fiber->parent, fiber);
    }
    
    /* Cancel all children */
    PgyMnFiber* child = fiber->firstChild;
    while (child != NULL) {
        PgyMnFiber* next = child->nextSibling;
        pgy_mn_fiber_cancel(child);
        child = next;
    }
    
    /* Free stack */
    if (fiber->stackBase != NULL) {
#ifdef _WIN32
        VirtualFree(fiber->stackBase, 0, MEM_RELEASE);
#else
        munmap(fiber->stackBase, fiber->stackSize);
#endif
    }
    
    /* Free error message */
    if (fiber->errorMessage != NULL) {
        free(fiber->errorMessage);
    }
    
    /* Free pending effect */
    if (fiber->pendingEffect != NULL) {
        free(fiber->pendingEffect);
    }
    
    free(fiber);
}

void pgy_mn_fiber_yield(void)
{
    PgyMnFiber* current = pgy_mn_fiber_get_current();
    if (current == NULL) {
        fiber_warn("yield", "no current fiber", NULL);
        return;
    }
    
    /* Save current context and switch to scheduler */
    if (setjmp(current->context.jmpBuf) == 0) {
        /* Saved context - switch to scheduler */
        pgy_mn_scheduler_yield();
    } else {
        /* Resumed - continue execution */
        return;
    }
}

void pgy_mn_fiber_suspend(PgyMnFiber* fiber)
{
    if (fiber == NULL || fiber->state != FIBER_STATE_RUNNING) {
        return;
    }
    
    fiber->state = FIBER_STATE_SUSPENDED;
    
    if (fiber == pgy_mn_fiber_get_current()) {
        pgy_mn_fiber_yield();
    }
}

void pgy_mn_fiber_resume(PgyMnFiber* fiber)
{
    if (fiber == NULL || fiber->state != FIBER_STATE_SUSPENDED) {
        fiber_warn("resume", "fiber is null or not suspended", fiber);
        return;
    }
    
    fiber->state = FIBER_STATE_READY;
    
    /* Schedule for execution */
    PgyMnScheduler* scheduler = fiber->scheduler;
    if (scheduler != NULL) {
        pgy_mn_scheduler_unblock(fiber);
    } else {
        fiber_warn("resume", "fiber has no scheduler", fiber);
    }
}

void pgy_mn_fiber_cancel(PgyMnFiber* fiber)
{
    if (fiber == NULL) {
        return;
    }
    
    /* Mark as cancelled */
    fiber->isCancelled = true;
    
    /* Cancel all children recursively */
    PgyMnFiber* child = fiber->firstChild;
    while (child != NULL) {
        pgy_mn_fiber_cancel(child);
        child = child->nextSibling;
    }
    
    /* If suspended or blocked, mark as done */
    if (fiber->state == FIBER_STATE_SUSPENDED ||
        fiber->state == FIBER_STATE_BLOCKED) {
        fiber->state = FIBER_STATE_DONE;
    }
}

PgyMnFiber* pgy_mn_fiber_get_current(void)
{
    return tlsCurrentFiber;
}

bool pgy_mn_fiber_is_cancelled(PgyMnFiber* fiber)
{
    return fiber != NULL && fiber->isCancelled;
}

PgyMnFiberState pgy_mn_fiber_get_state(PgyMnFiber* fiber)
{
    return fiber != NULL ? fiber->state : FIBER_STATE_ERROR;
}

void pgy_mn_fiber_attach_child(PgyMnFiber* parent, PgyMnFiber* child)
{
    if (parent == NULL || child == NULL) {
        return;
    }
    
    /* Set parent */
    child->parent = parent;
    
    /* Add to parent's child list */
    child->nextSibling = parent->firstChild;
    parent->firstChild = child;
}

void pgy_mn_fiber_detach_child(PgyMnFiber* parent, PgyMnFiber* child)
{
    if (parent == NULL || child == NULL || child->parent != parent) {
        return;
    }
    
    /* Remove from parent's child list */
    if (parent->firstChild == child) {
        parent->firstChild = child->nextSibling;
    } else {
        PgyMnFiber* prev = parent->firstChild;
        while (prev != NULL && prev->nextSibling != child) {
            prev = prev->nextSibling;
        }
        if (prev != NULL) {
            prev->nextSibling = child->nextSibling;
        }
    }
    
    /* Clear parent reference */
    child->parent = NULL;
    child->nextSibling = NULL;
}

/* The context switch was DELETED with the run-to-completion landing
 * (docs/194 WO-MN-1 R2). Three independent reasons, each sufficient:
 *   - it had no caller once workers run routines directly;
 *   - create() never seeded an entry frame on the fiber stack, so switching
 *     to a fresh fiber jumped to garbage (measured segfault on first run);
 *   - as module-level global assembly it survived the bitcode twin's
 *     internalize pass verbatim and collided with the runtime cache object
 *     at native link ("multiple definition of pgy_mn_fiber_switch_context").
 * The WO-MN-2 context layer replaces it with the platform fiber APIs the
 * coroutine layer already proves in-house (CreateFiber/SwitchToFiber /
 * ucontext), seeded with a real entry trampoline. */
