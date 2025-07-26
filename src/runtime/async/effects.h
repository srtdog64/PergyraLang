/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Effect system for Pergyra's Structured Effect Async (SEA) model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_EFFECTS_H
#define PERGYRA_EFFECTS_H

#include <stdint.h>
#include <stddef.h>
#include "../slot_manager.h"

/* Forward declarations */
typedef struct Channel Channel;
typedef struct Fiber Fiber;

/* Effect types */
typedef enum {
    /* I/O Effects */
    EFFECT_TYPE_IO_READ,
    EFFECT_TYPE_IO_WRITE,
    EFFECT_TYPE_IO_ACCEPT,
    EFFECT_TYPE_IO_CONNECT,
    
    /* Channel Effects */
    EFFECT_TYPE_CHANNEL_SEND,
    EFFECT_TYPE_CHANNEL_RECV,
    EFFECT_TYPE_CHANNEL_SELECT,
    
    /* Time Effects */
    EFFECT_TYPE_SLEEP,
    EFFECT_TYPE_TIMEOUT,
    
    /* Fiber Effects */
    EFFECT_TYPE_SPAWN,
    EFFECT_TYPE_JOIN,
    
    /* Slot Effects */
    EFFECT_TYPE_SLOT_READ,
    EFFECT_TYPE_SLOT_WRITE,
    EFFECT_TYPE_SLOT_CLAIM,
    EFFECT_TYPE_SLOT_RELEASE,
    
    /* Synchronization Effects */
    EFFECT_TYPE_MUTEX_LOCK,
    EFFECT_TYPE_MUTEX_UNLOCK,
    EFFECT_TYPE_SEMAPHORE_ACQUIRE,
    EFFECT_TYPE_SEMAPHORE_RELEASE,
    
    /* Custom Effects */
    EFFECT_TYPE_CUSTOM
} EffectType;

/* Effect structure - describes an operation without performing it */
typedef struct Effect {
    EffectType type;
    
    /* Effect payload */
    union {
        /* I/O operations */
        struct {
            int fd;
            void* buffer;
            size_t count;
            int flags;
        } io;
        
        /* Channel operations */
        struct {
            Channel* channel;
            void* data;
            size_t size;
        } channel;
        
        /* Channel select */
        struct {
            Channel** channels;
            size_t channelCount;
            void** dataBuffers;
            int* selectedIndex;
        } channelSelect;
        
        /* Time operations */
        struct {
            uint64_t nanoseconds;
        } time;
        
        /* Fiber operations */
        struct {
            void (*routine)(void*);
            void* arg;
            Fiber** fiberHandle;
        } spawn;
        
        struct {
            Fiber* fiber;
            void** result;
        } join;
        
        /* Slot operations */
        struct {
            SlotHandle slot;
            void* data;
            SecurityToken token;
        } slot;
        
        /* Custom effect */
        struct {
            uint32_t effectId;
            void* data;
            size_t dataSize;
        } custom;
    } payload;
    
    /* Result handling */
    void* result;
    int errorCode;
    char* errorMessage;
    
    /* Continuation */
    void (*continuation)(struct Effect* effect);
    void* continuationData;
} Effect;

/* Effect handlers - BSD style with PascalCase */
typedef void (*EffectHandler)(Effect* effect);

/* Effect registration for custom effects */
void EffectRegisterHandler(uint32_t effectId, EffectHandler handler);

/* Effect execution - called by the compiler-generated code */
void* PerformEffect(Effect* effect);

/* Async effect execution - returns immediately, result via continuation */
void PerformEffectAsync(Effect* effect);

/* Built-in effect constructors */
Effect* EffectIoRead(int fd, void* buffer, size_t count);
Effect* EffectIoWrite(int fd, const void* buffer, size_t count);
Effect* EffectChannelSend(Channel* channel, void* data, size_t size);
Effect* EffectChannelRecv(Channel* channel, void* buffer, size_t size);
Effect* EffectSleep(uint64_t nanoseconds);
Effect* EffectSpawn(void (*routine)(void*), void* arg);

/* Effect composition */
typedef struct EffectChain {
    Effect** effects;
    size_t count;
    size_t capacity;
} EffectChain;

EffectChain* EffectChainCreate(void);
void EffectChainAdd(EffectChain* chain, Effect* effect);
void EffectChainExecute(EffectChain* chain);
void EffectChainDestroy(EffectChain* chain);

/* Effect debugging and tracing */
typedef enum {
    EFFECT_TRACE_NONE,
    EFFECT_TRACE_BASIC,
    EFFECT_TRACE_DETAILED,
    EFFECT_TRACE_VERBOSE
} EffectTraceLevel;

void EffectSetTraceLevel(EffectTraceLevel level);
const char* EffectTypeToString(EffectType type);

#endif /* PERGYRA_EFFECTS_H */
