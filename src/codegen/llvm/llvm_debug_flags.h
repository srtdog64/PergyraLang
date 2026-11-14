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

bool llvm_debug_detail_enabled(void);
bool llvm_debug_stage_enabled(void);
bool llvm_debug_verify_enabled(void);

#endif /* PGY_LLVM_DEBUG_FLAGS_H */
