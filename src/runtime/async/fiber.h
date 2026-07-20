/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * PgyMnFiber implementation for Pergyra's Structured Effect Async (SEA) model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_FIBER_H
#define PERGYRA_FIBER_H

#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#define FIBER_STACK_SIZE (1024 * 64) /* 64KB stack for each fiber */

/* PgyMnFiber states */
typedef enum {
    FIBER_STATE_NEW,
    FIBER_STATE_READY,
    FIBER_STATE_RUNNING,
    FIBER_STATE_SUSPENDED,
    FIBER_STATE_BLOCKED,
    FIBER_STATE_DONE,
    FIBER_STATE_ERROR
} PgyMnFiberState;

/* Forward declarations */
typedef struct PgyMnFiber PgyMnFiber;
typedef struct PgyMnScheduler PgyMnScheduler;
typedef struct Effect Effect;

/* PgyMnFiber function signature */
typedef void (*PgyMnFiberFn)(void* arg);

/* PgyMnFiber context for platform-specific context switching */
typedef struct PgyMnFiberContext {
    /* For prototyping with setjmp/longjmp */
    jmp_buf jmpBuf;
    
    /* For assembly implementation */
    void* stackPointer;     /* rsp */
    void* basePointer;      /* rbp */
    void* instructionPointer; /* rip */
    
    /* Callee-saved registers */
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    
    /* Extended state */
    void* extendedState;    /* For SSE/AVX state if needed */
} PgyMnFiberContext;

/* PgyMnFiber structure */
struct PgyMnFiber {
    uint64_t id;
    PgyMnFiberState state;
    
    /* Context switching */
    PgyMnFiberContext context;
    void* stackBase;
    size_t stackSize;
    
    /* Execution */
    PgyMnFiberFn startRoutine;
    void* arg;
    void* result;
    
    /* Error handling */
    char* errorMessage;
    int errorCode;
    
    /* Scheduling */
    PgyMnScheduler* scheduler;
    PgyMnFiber* next;            /* For scheduler queues */
    uint32_t priority;
    
    /* Effect handling */
    Effect* pendingEffect;
    
    /* Cancellation */
    bool isCancelled;
    
    /* Parent-child relationship for structured concurrency */
    PgyMnFiber* parent;
    PgyMnFiber* firstChild;
    PgyMnFiber* nextSibling;
    
    /* Statistics */
    uint64_t switchCount;
    uint64_t cpuTimeNs;
};

/* PgyMnFiber management functions - BSD style with PascalCase */
PgyMnFiber* pgy_mn_fiber_create(PgyMnFiberFn startRoutine, void* arg);
void pgy_mn_fiber_destroy(PgyMnFiber* fiber);

/* PgyMnFiber control functions */
void pgy_mn_fiber_yield(void);
void pgy_mn_fiber_suspend(PgyMnFiber* fiber);
void pgy_mn_fiber_resume(PgyMnFiber* fiber);
void pgy_mn_fiber_cancel(PgyMnFiber* fiber);

/* Context switching is intentionally ABSENT (run-to-completion depth,
 * docs/194 WO-MN-1 R2): workers run routines directly. The WO-MN-2 context
 * layer reintroduces a switch built on the platform fiber APIs, seeded with
 * a real entry trampoline -- referencing a switch before that rung lands is
 * a compile error by design. */

/* PgyMnFiber query functions */
PgyMnFiber* pgy_mn_fiber_get_current(void);
bool pgy_mn_fiber_is_cancelled(PgyMnFiber* fiber);
PgyMnFiberState pgy_mn_fiber_get_state(PgyMnFiber* fiber);

/* Parent-child relationship */
void pgy_mn_fiber_attach_child(PgyMnFiber* parent, PgyMnFiber* child);
void pgy_mn_fiber_detach_child(PgyMnFiber* parent, PgyMnFiber* child);

#endif /* PERGYRA_FIBER_H */
