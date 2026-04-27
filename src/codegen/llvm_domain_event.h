/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_CODEGEN_LLVM_DOMAIN_EVENT_H
#define PGY_CODEGEN_LLVM_DOMAIN_EVENT_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

void llvm_emit_domain_event_helpers(LLVMGenCtx *ctx,
    ASTNode **events,
    size_t event_count);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_CODEGEN_LLVM_DOMAIN_EVENT_H */
