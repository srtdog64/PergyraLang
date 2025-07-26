/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Fiber implementation for Pergyra's SEA model
 * BSD Style + C# naming conventions
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include "fiber.h"
#include "scheduler.h"

/* Thread-local current fiber */
static __thread Fiber* tlsCurrentFiber = NULL;

/* Fiber ID counter */
static atomic_uint64_t fiberIdCounter = 0;

/* Internal fiber entry point wrapper */
static void FiberEntryPoint(Fiber* fiber)
{
    assert(fiber != NULL);
    assert(fiber->startRoutine != NULL);
    
    /* Set current fiber */
    tlsCurrentFiber = fiber;
    
    /* Update state */
    fiber->state = FIBER_STATE_RUNNING;
    
    /* Execute the fiber function */
    fiber->startRoutine(fiber->arg);
    
    /* Mark as done */
    fiber->state = FIBER_STATE_DONE;
    
    /* Return control to scheduler */
    SchedulerYield();
    
    /* Should never reach here */
    assert(0 && "Fiber returned after completion");
}

Fiber* FiberCreate(FiberStartRoutine startRoutine, void* arg)
{
    if (startRoutine == NULL) {
        return NULL;
    }
    
    Fiber* fiber = (Fiber*)calloc(1, sizeof(Fiber));
    if (fiber == NULL) {
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
    fiber->stackBase = mmap(NULL, fiber->stackSize,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                           -1, 0);
    
    if (fiber->stackBase == MAP_FAILED) {
        free(fiber);
        return NULL;
    }
    
    /* Initialize stack pointer to top of stack (stacks grow down) */
    fiber->context.stackPointer = (char*)fiber->stackBase + fiber->stackSize;
    
    /* Setup initial context */
    if (setjmp(fiber->context.jmpBuf) == 0) {
        /* First time - will be resumed later */
        fiber->state = FIBER_STATE_READY;
    } else {
        /* Resumed - start execution */
        FiberEntryPoint(fiber);
    }
    
    return fiber;
}

void FiberDestroy(Fiber* fiber)
{
    if (fiber == NULL) {
        return;
    }
    
    /* Cancel if still running */
    if (fiber->state == FIBER_STATE_RUNNING ||
        fiber->state == FIBER_STATE_READY ||
        fiber->state == FIBER_STATE_SUSPENDED) {
        FiberCancel(fiber);
    }
    
    /* Detach from parent */
    if (fiber->parent != NULL) {
        FiberDetachChild(fiber->parent, fiber);
    }
    
    /* Cancel all children */
    Fiber* child = fiber->firstChild;
    while (child != NULL) {
        Fiber* next = child->nextSibling;
        FiberCancel(child);
        child = next;
    }
    
    /* Free stack */
    if (fiber->stackBase != NULL) {
        munmap(fiber->stackBase, fiber->stackSize);
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

void FiberYield(void)
{
    Fiber* current = FiberGetCurrent();
    if (current == NULL) {
        return;
    }
    
    /* Save current context and switch to scheduler */
    if (setjmp(current->context.jmpBuf) == 0) {
        /* Saved context - switch to scheduler */
        SchedulerYield();
    } else {
        /* Resumed - continue execution */
        return;
    }
}

void FiberSuspend(Fiber* fiber)
{
    if (fiber == NULL || fiber->state != FIBER_STATE_RUNNING) {
        return;
    }
    
    fiber->state = FIBER_STATE_SUSPENDED;
    
    if (fiber == FiberGetCurrent()) {
        FiberYield();
    }
}

void FiberResume(Fiber* fiber)
{
    if (fiber == NULL || fiber->state != FIBER_STATE_SUSPENDED) {
        return;
    }
    
    fiber->state = FIBER_STATE_READY;
    
    /* Schedule for execution */
    Scheduler* scheduler = fiber->scheduler;
    if (scheduler != NULL) {
        SchedulerUnblock(fiber);
    }
}

void FiberCancel(Fiber* fiber)
{
    if (fiber == NULL) {
        return;
    }
    
    /* Mark as cancelled */
    fiber->isCancelled = true;
    
    /* Cancel all children recursively */
    Fiber* child = fiber->firstChild;
    while (child != NULL) {
        FiberCancel(child);
        child = child->nextSibling;
    }
    
    /* If suspended or blocked, mark as done */
    if (fiber->state == FIBER_STATE_SUSPENDED ||
        fiber->state == FIBER_STATE_BLOCKED) {
        fiber->state = FIBER_STATE_DONE;
    }
}

Fiber* FiberGetCurrent(void)
{
    return tlsCurrentFiber;
}

bool FiberIsCancelled(Fiber* fiber)
{
    return fiber != NULL && fiber->isCancelled;
}

FiberState FiberGetState(Fiber* fiber)
{
    return fiber != NULL ? fiber->state : FIBER_STATE_ERROR;
}

void FiberAttachChild(Fiber* parent, Fiber* child)
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

void FiberDetachChild(Fiber* parent, Fiber* child)
{
    if (parent == NULL || child == NULL || child->parent != parent) {
        return;
    }
    
    /* Remove from parent's child list */
    if (parent->firstChild == child) {
        parent->firstChild = child->nextSibling;
    } else {
        Fiber* prev = parent->firstChild;
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

/* Assembly implementation for context switching */
#ifdef __x86_64__
__asm__(
    ".global FiberSwitchContext\n"
    "FiberSwitchContext:\n"
    "    # Save callee-saved registers\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    \n"
    "    # Save old stack pointer\n"
    "    movq %rsp, 8(%rdi)\n"   /* oldContext->stackPointer */
    "    \n"
    "    # Load new stack pointer\n"
    "    movq 8(%rsi), %rsp\n"   /* newContext->stackPointer */
    "    \n"
    "    # Restore callee-saved registers\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbx\n"
    "    popq %rbp\n"
    "    \n"
    "    # Return to new context\n"
    "    ret\n"
);
#else
/* Fallback to setjmp/longjmp for non-x86_64 platforms */
void FiberSwitchContext(FiberContext* oldContext, FiberContext* newContext)
{
    if (setjmp(oldContext->jmpBuf) == 0) {
        longjmp(newContext->jmpBuf, 1);
    }
}
#endif
