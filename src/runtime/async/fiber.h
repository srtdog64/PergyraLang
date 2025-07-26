/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Fiber implementation for Pergyra's Structured Effect Async (SEA) model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_FIBER_H
#define PERGYRA_FIBER_H

#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#define FIBER_STACK_SIZE (1024 * 64) /* 64KB stack for each fiber */

/* Fiber states */
typedef enum {
    FIBER_STATE_NEW,
    FIBER_STATE_READY,
    FIBER_STATE_RUNNING,
    FIBER_STATE_SUSPENDED,
    FIBER_STATE_BLOCKED,
    FIBER_STATE_DONE,
    FIBER_STATE_ERROR
} FiberState;

/* Forward declarations */
typedef struct Fiber Fiber;
typedef struct Scheduler Scheduler;
typedef struct Effect Effect;

/* Fiber function signature */
typedef void (*FiberStartRoutine)(void* arg);

/* Fiber context for platform-specific context switching */
typedef struct FiberContext {
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
} FiberContext;

/* Fiber structure */
struct Fiber {
    uint64_t id;
    FiberState state;
    
    /* Context switching */
    FiberContext context;
    void* stackBase;
    size_t stackSize;
    
    /* Execution */
    FiberStartRoutine startRoutine;
    void* arg;
    void* result;
    
    /* Error handling */
    char* errorMessage;
    int errorCode;
    
    /* Scheduling */
    Scheduler* scheduler;
    Fiber* next;            /* For scheduler queues */
    uint32_t priority;
    
    /* Effect handling */
    Effect* pendingEffect;
    
    /* Cancellation */
    bool isCancelled;
    
    /* Parent-child relationship for structured concurrency */
    Fiber* parent;
    Fiber* firstChild;
    Fiber* nextSibling;
    
    /* Statistics */
    uint64_t switchCount;
    uint64_t cpuTimeNs;
};

/* Fiber management functions - BSD style with PascalCase */
Fiber* FiberCreate(FiberStartRoutine startRoutine, void* arg);
void FiberDestroy(Fiber* fiber);

/* Fiber control functions */
void FiberYield(void);
void FiberSuspend(Fiber* fiber);
void FiberResume(Fiber* fiber);
void FiberCancel(Fiber* fiber);

/* Context switching - Assembly implementation */
void FiberSwitchContext(FiberContext* oldContext, FiberContext* newContext);

/* Fiber query functions */
Fiber* FiberGetCurrent(void);
bool FiberIsCancelled(Fiber* fiber);
FiberState FiberGetState(Fiber* fiber);

/* Parent-child relationship */
void FiberAttachChild(Fiber* parent, Fiber* child);
void FiberDetachChild(Fiber* parent, Fiber* child);

#endif /* PERGYRA_FIBER_H */
