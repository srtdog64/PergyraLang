/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "llvm_expr_call_inline_policy.h"

#include <stdlib.h>
#include <string.h>

typedef struct LLVMCallInlineSpec {
    const char *name;
    size_t argc;
    LLVMCallInlineOp op;
} LLVMCallInlineSpec;

static int
llvm_call_inline_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMCallInlineSpec *spec = (const LLVMCallInlineSpec *)entry;

    return strcmp(name, spec->name);
}

LLVMCallInlineOp
llvm_call_inline_lookup(const char *callee_name, size_t argc)
{
    static const LLVMCallInlineSpec kLLVMCallInlineSpecs[] = {
        { "Clone", 1, LLVM_CALL_INLINE_OP_CLONE },
        { "ToObject", 2, LLVM_CALL_INLINE_OP_TO_OBJECT },
        { "ToTObject", 2, LLVM_CALL_INLINE_OP_TO_OBJECT },
    };
    const LLVMCallInlineSpec *match;

    if (callee_name == NULL)
        return LLVM_CALL_INLINE_OP_NONE;

    match = (const LLVMCallInlineSpec *)bsearch(&callee_name,
        kLLVMCallInlineSpecs,
        sizeof(kLLVMCallInlineSpecs) / sizeof(kLLVMCallInlineSpecs[0]),
        sizeof(kLLVMCallInlineSpecs[0]), llvm_call_inline_spec_compare);
    if (match == NULL || match->argc != argc)
        return LLVM_CALL_INLINE_OP_NONE;
    return match->op;
}
