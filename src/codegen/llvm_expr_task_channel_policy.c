/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "llvm_expr_task_channel_policy.h"

#include <stdlib.h>
#include <string.h>

typedef struct LLVMTaskChannelNameSpec {
    const char *name;
} LLVMTaskChannelNameSpec;

typedef struct LLVMChannelQueryOpSpec {
    const char *name;
    const char *runtime_op;
} LLVMChannelQueryOpSpec;

typedef struct LLVMTaskChannelOpSpec {
    const char *name;
    size_t argc;
    LLVMTaskChannelOp op;
} LLVMTaskChannelOpSpec;

static int
llvm_task_channel_name_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMTaskChannelNameSpec *spec =
        (const LLVMTaskChannelNameSpec *)entry;

    return strcmp(name, spec->name);
}

static int
llvm_channel_query_op_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMChannelQueryOpSpec *spec =
        (const LLVMChannelQueryOpSpec *)entry;

    return strcmp(name, spec->name);
}

static int
llvm_task_channel_op_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMTaskChannelOpSpec *spec =
        (const LLVMTaskChannelOpSpec *)entry;

    return strcmp(name, spec->name);
}

bool
llvm_is_task_channel_builtin_name(const char *callee_name)
{
    static const LLVMTaskChannelNameSpec specs[] = {
        { "ChannelCapacity" },
        { "ChannelClose" },
        { "ChannelClosed" },
        { "ChannelFull" },
        { "ChannelLength" },
        { "ChannelReady" },
        { "ChannelSpace" },
        { "RecvTimeout" },
        { "SendTimeout" },
        { "SendTimeoutStatus" },
        { "TryRecv" },
        { "TrySend" },
        { "TrySendStatus" },
    };

    if (callee_name == NULL)
        return false;

    return bsearch(&callee_name, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), llvm_task_channel_name_compare) != NULL;
}

const char *
llvm_channel_query_runtime_op(const char *callee_name)
{
    static const LLVMChannelQueryOpSpec specs[] = {
        { "ChannelCapacity", "capacity" },
        { "ChannelClosed", "closed" },
        { "ChannelFull", "full" },
        { "ChannelLength", "length" },
        { "ChannelReady", "ready" },
        { "ChannelSpace", "space" },
    };
    const LLVMChannelQueryOpSpec *match;

    if (callee_name == NULL)
        return NULL;

    match = (const LLVMChannelQueryOpSpec *)bsearch(
        &callee_name, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), llvm_channel_query_op_compare);
    return match != NULL ? match->runtime_op : NULL;
}

LLVMTaskChannelOp
llvm_task_channel_op_lookup(const char *callee_name, size_t argc)
{
    static const LLVMTaskChannelOpSpec kTaskChannelOpSpecs[] = {
        { "ChannelClose", 1, LLVM_TASK_CHANNEL_OP_CLOSE },
        { "RecvTimeout", 2, LLVM_TASK_CHANNEL_OP_RECV_TIMEOUT },
        { "SendTimeout", 3, LLVM_TASK_CHANNEL_OP_SEND_TIMEOUT },
        { "SendTimeoutStatus", 3, LLVM_TASK_CHANNEL_OP_SEND_TIMEOUT_STATUS },
        { "TryRecv", 1, LLVM_TASK_CHANNEL_OP_TRY_RECV },
        { "TrySend", 2, LLVM_TASK_CHANNEL_OP_TRY_SEND },
        { "TrySendStatus", 2, LLVM_TASK_CHANNEL_OP_TRY_SEND_STATUS },
    };
    const LLVMTaskChannelOpSpec *match;

    if (callee_name == NULL)
        return LLVM_TASK_CHANNEL_OP_NONE;

    match = (const LLVMTaskChannelOpSpec *)bsearch(
        &callee_name, kTaskChannelOpSpecs,
        sizeof(kTaskChannelOpSpecs) / sizeof(kTaskChannelOpSpecs[0]),
        sizeof(kTaskChannelOpSpecs[0]), llvm_task_channel_op_compare);
    if (match == NULL || match->argc != argc)
        return LLVM_TASK_CHANNEL_OP_NONE;
    return match->op;
}
