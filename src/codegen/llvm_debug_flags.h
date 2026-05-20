/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM debug flag policy owner. Keep developer trace environment reads here so
 * emission owners consume intent-specific helpers instead of raw getenv calls.
 */

#ifndef PGY_LLVM_DEBUG_FLAGS_H
#define PGY_LLVM_DEBUG_FLAGS_H

#include <stdbool.h>
#include <stdlib.h>

static inline bool
llvm_debug_detail_enabled(void)
{
    return getenv("PGY_DEBUG_LLVM_DETAIL") != NULL;
}

static inline bool
llvm_debug_stage_enabled(void)
{
    return getenv("PGY_DEBUG_LLVM_STAGE") != NULL;
}

static inline bool
llvm_debug_verify_enabled(void)
{
    return getenv("PGY_DEBUG_LLVM_VERIFY") != NULL;
}

#endif /* PGY_LLVM_DEBUG_FLAGS_H */
