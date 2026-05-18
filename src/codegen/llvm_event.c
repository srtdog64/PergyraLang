/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend ??event registry helpers
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMEventTypeEntry *
llvm_lookup_event(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->event_type_count; i++) {
        if (strcmp(ctx->event_types[i].event_name, name) == 0)
            return &ctx->event_types[i];
    }
    return NULL;
}

LLVMEventTypeEntry *
llvm_register_event(LLVMGenCtx *ctx, const char *name,
                    LLVMTypeRef struct_type,
                    int param_count, LLVMTypeRef *param_types)
{
    PGY_DYNARR_ENSURE_RET(ctx->event_types, ctx->event_type_count,
                          ctx->event_type_capacity, LLVMEventTypeEntry);

    LLVMEventTypeEntry *e = &ctx->event_types[ctx->event_type_count++];
    e->event_name  = name;
    e->struct_type = struct_type;
    e->param_count = param_count;
    for (int i = 0; i < param_count && i < MAX_EVENT_PARAMS; i++)
        e->param_types[i] = param_types[i];
    return e;
}

#endif
