/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_COMPILER_TOOLCHAIN_H
#define PGY_COMPILER_TOOLCHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "compiler.h"

typedef struct PgyCCompilerSelection {
    const char *cc;
    const char *target_flag;
    char        cc_storage[512];
    char        target_storage[256];
} PgyCCompilerSelection;

typedef PgyCCompilerSelection PgyLlvmIrCompilerSelection;

int pgy_exec_argv(const char *const argv[], bool verbose);
bool pgy_select_c_compiler(PgyCCompilerSelection *selection);
bool pgy_select_llvm_ir_compiler(PgyLlvmIrCompilerSelection *selection);
double compiler_now_seconds(void);
bool pgy_path_is_safe(const char *path);

#ifdef _WIN32
void pgy_win32_normalize_exec_path(const char *path, char *dst, size_t dst_cap);
#endif

#ifndef _WIN32
bool compiler_should_use_lld(void);
#endif

#ifdef PGY_LLVM_ENABLED
void compiler_debug_llvm_host_stage(const char *stage);
#endif

/* Runtime object cache (compiler_runtime_cache.c). Not gated on LLVM: the C
 * extern leg and textual/native LLVM legs consume distinct linkage objects
 * owned by the same freshness/publication boundary. */
bool compiler_runtime_cache_is_fresh(const char *cache_obj_path);

/* Build the runtime object through a per-process scratch path and publish it
 * into the shared cache in one step, so concurrent builders never observe a
 * missing or half-written object (docs/190 B2). Both legs must use these. */
char *compiler_runtime_object_scratch_path(const char *final_path);
bool compiler_publish_runtime_object(const char *tmp_path,
                                     const char *final_path);
char *compiler_runtime_prebuilt_object_path(PgyOptProfile opt_profile,
                                            bool uses_intent_observability);
char *compiler_runtime_cache_object_path(PgyOptProfile opt_profile,
                                         bool uses_intent_observability);
/* Resolve + (re)build the C-extern runtime object for the given profile. */
char *compiler_runtime_object_ensure(PgyOptProfile opt_profile,
                                     bool uses_intent_observability,
                                     bool verbose,
                                     const char **error_out);
/* Resolve + (re)build the canonical LLVM-callable runtime object. The caller
 * frees the returned path; compiled_out records whether this call published a
 * fresh cache object so a failed link may invalidate that new artifact. */
char *compiler_llvm_runtime_object_ensure(PgyOptProfile opt_profile,
                                          bool uses_intent_observability,
                                          bool verbose,
                                          bool *compiled_out,
                                          const char **error_out);

#endif /* PGY_COMPILER_TOOLCHAIN_H */
