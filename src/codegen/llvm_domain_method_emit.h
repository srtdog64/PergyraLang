/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_CODEGEN_LLVM_DOMAIN_METHOD_EMIT_H
#define PGY_CODEGEN_LLVM_DOMAIN_METHOD_EMIT_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool llvm_emit_domain_sync_and_method_bodies(LLVMGenCtx *ctx,
    ASTNode ***domain_groups,
    const size_t *domain_group_counts,
    size_t domain_group_count);

#endif /* PGY_LLVM_ENABLED */

#endif /* PGY_CODEGEN_LLVM_DOMAIN_METHOD_EMIT_H */
