/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Channel implementation for Pergyra's SEA model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_CHANNEL_H
#define PERGYRA_CHANNEL_H

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "fiber.h"

/* Channel states */
typedef enum {
    CHANNEL_STATE_OPEN,
    CHANNEL_STATE_CLOSED,
    CHANNEL_STATE_ERROR
} ChannelState;

/* Channel element */
typedef struct ChannelElement {
    void* data;
    size_t size;
    struct ChannelElement* next;
} ChannelElement;

/* Waiting fiber queue */
typedef struct WaitingFiber {
    Fiber* fiber;
    void* buffer;
    size_t bufferSize;
    struct WaitingFiber* next;
} WaitingFiber;

/* Channel structure */
typedef struct Channel {
    /* Configuration */
    size_t capacity;          /* 0 for unbuffered */
    size_t elementSize;       /* Size of each element */
    
    /* State */
    atomic_int state;
    
    /* Buffer management */
    ChannelElement* head;
    ChannelElement* tail;
    atomic_size_t count;
    
    /* Synchronization */
    pthread_mutex_t mutex;
    
    /* Waiting queues */
    WaitingFiber* sendersHead;
    WaitingFiber* sendersTail;
    WaitingFiber* receiversHead;
    WaitingFiber* receiversTail;
    
    /* Statistics */
    atomic_uint64_t totalSends;
    atomic_uint64_t totalReceives;
    atomic_uint64_t totalBlocks;
} Channel;

/* Channel result types */
typedef enum {
    CHANNEL_OK,
    CHANNEL_CLOSED,
    CHANNEL_FULL,
    CHANNEL_EMPTY,
    CHANNEL_ERROR,
    CHANNEL_TIMEOUT
} ChannelResult;

/* Channel operations - BSD style with PascalCase */
Channel* ChannelCreate(size_t elementSize, size_t capacity);
void ChannelDestroy(Channel* channel);

/* Sending operations */
ChannelResult ChannelSend(Channel* channel, const void* data);
ChannelResult ChannelTrySend(Channel* channel, const void* data);
ChannelResult ChannelSendTimeout(Channel* channel, const void* data, uint64_t timeoutNs);

/* Receiving operations */
ChannelResult ChannelReceive(Channel* channel, void* buffer);
ChannelResult ChannelTryReceive(Channel* channel, void* buffer);
ChannelResult ChannelReceiveTimeout(Channel* channel, void* buffer, uint64_t timeoutNs);

/* Channel control */
void ChannelClose(Channel* channel);
bool ChannelIsClosed(Channel* channel);
size_t ChannelLength(Channel* channel);
size_t ChannelCapacity(Channel* channel);

/* Select operations - for waiting on multiple channels */
typedef struct SelectCase {
    Channel* channel;
    void* data;           /* For send cases */
    void* buffer;         /* For receive cases */
    bool isSend;
    bool isDefault;       /* Default case */
} SelectCase;

typedef struct SelectResult {
    int selectedIndex;
    ChannelResult result;
} SelectResult;

SelectResult ChannelSelect(SelectCase* cases, size_t caseCount);
SelectResult ChannelSelectTimeout(SelectCase* cases, size_t caseCount, uint64_t timeoutNs);

/* Channel utilities */

/* Fan-out: one sender, multiple receivers */
typedef struct FanOutChannel {
    Channel* source;
    Channel** destinations;
    size_t destinationCount;
    AsyncScope* scope;
} FanOutChannel;

FanOutChannel* FanOutChannelCreate(Channel* source, size_t destinationCount);
void FanOutChannelStart(FanOutChannel* fanOut);
void FanOutChannelStop(FanOutChannel* fanOut);
void FanOutChannelDestroy(FanOutChannel* fanOut);

/* Fan-in: multiple senders, one receiver */
typedef struct FanInChannel {
    Channel** sources;
    size_t sourceCount;
    Channel* destination;
    AsyncScope* scope;
} FanInChannel;

FanInChannel* FanInChannelCreate(size_t sourceCount, Channel* destination);
void FanInChannelStart(FanInChannel* fanIn);
void FanInChannelStop(FanInChannel* fanIn);
void FanInChannelDestroy(FanInChannel* fanIn);

/* Pipeline: chain of processing stages */
typedef void* (*PipelineStage)(void* input);

typedef struct Pipeline {
    PipelineStage* stages;
    size_t stageCount;
    Channel** channels;
    AsyncScope* scope;
} Pipeline;

Pipeline* PipelineCreate(PipelineStage* stages, size_t stageCount, size_t bufferSize);
void PipelineStart(Pipeline* pipeline);
void PipelineStop(Pipeline* pipeline);
Channel* PipelineGetInput(Pipeline* pipeline);
Channel* PipelineGetOutput(Pipeline* pipeline);
void PipelineDestroy(Pipeline* pipeline);

#endif /* PERGYRA_CHANNEL_H */
