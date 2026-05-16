/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "llvm_stmt_let_collection_policy.h"

#include <stdlib.h>
#include <string.h>

typedef struct LLVMStmtLetCallSpec {
    const char *callee_name;
    LLVMStmtLetCallOp op;
} LLVMStmtLetCallSpec;

static int
llvm_stmt_collection_ctor_compare(const void *key, const void *entry)
{
    const char *callee_name = *(const char * const *)key;
    const LLVMStmtCollectionCtorSpec *spec =
        (const LLVMStmtCollectionCtorSpec *)entry;

    return strcmp(callee_name, spec->callee_name);
}

const LLVMStmtCollectionCtorSpec *
llvm_stmt_collection_ctor_lookup(const char *callee_name)
{
    static const LLVMStmtCollectionCtorSpec kLLVMStmtCollectionCtorSpecs[] = {
        { "ListNew", "List", "pgy_list_new_raw_export",
          LLVM_STMT_COLLECTION_CTOR_LIST },
        { "MapNew", "HashMap", "pgy_map_new_raw_export",
          LLVM_STMT_COLLECTION_CTOR_HASH_MAP },
        { "QueueNew", "Queue", "pgy_queue_new_raw_export",
          LLVM_STMT_COLLECTION_CTOR_QUEUE },
        { "SetNew", "Set", "pgy_set_new_raw_export",
          LLVM_STMT_COLLECTION_CTOR_SET },
    };

    if (callee_name == NULL)
        return NULL;

    return (const LLVMStmtCollectionCtorSpec *)bsearch(&callee_name,
        kLLVMStmtCollectionCtorSpecs,
        sizeof(kLLVMStmtCollectionCtorSpecs)
            / sizeof(kLLVMStmtCollectionCtorSpecs[0]),
        sizeof(kLLVMStmtCollectionCtorSpecs[0]),
        llvm_stmt_collection_ctor_compare);
}

static int
llvm_stmt_let_call_compare(const void *key, const void *entry)
{
    const char *callee_name = *(const char * const *)key;
    const LLVMStmtLetCallSpec *spec = (const LLVMStmtLetCallSpec *)entry;

    return strcmp(callee_name, spec->callee_name);
}

LLVMStmtLetCallOp
llvm_stmt_let_call_lookup(const char *callee_name)
{
    static const LLVMStmtLetCallSpec kLLVMStmtLetCallSpecs[] = {
        { "Channel", LLVM_STMT_LET_CALL_CHANNEL },
        { "ToObject", LLVM_STMT_LET_CALL_TO_OBJECT },
    };
    const LLVMStmtLetCallSpec *match;

    if (callee_name == NULL)
        return LLVM_STMT_LET_CALL_NONE;

    match = (const LLVMStmtLetCallSpec *)bsearch(&callee_name,
        kLLVMStmtLetCallSpecs,
        sizeof(kLLVMStmtLetCallSpecs) / sizeof(kLLVMStmtLetCallSpecs[0]),
        sizeof(kLLVMStmtLetCallSpecs[0]), llvm_stmt_let_call_compare);
    return match != NULL ? match->op : LLVM_STMT_LET_CALL_NONE;
}
