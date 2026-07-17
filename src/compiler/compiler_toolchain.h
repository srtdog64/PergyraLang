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

int pgy_exec_argv(const char *const argv[], bool verbose);
bool pgy_select_c_compiler(PgyCCompilerSelection *selection);
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
 * leg in extern mode (PGY_RUNTIME_DECLS_ONLY) links the runtime object too. */
bool compiler_runtime_cache_is_fresh(const char *cache_obj_path);
char *compiler_runtime_prebuilt_object_path(PgyOptProfile opt_profile,
                                            bool uses_intent_observability);
char *compiler_runtime_cache_object_path(PgyOptProfile opt_profile,
                                         bool uses_intent_observability);
/* Resolve + (re)build a fresh runtime object for the given profile; returns a
 * malloc'd path (caller frees) or NULL with *error_out set. Shared by both
 * backends so the linked runtime cannot drift. */
char *compiler_runtime_object_ensure(PgyOptProfile opt_profile,
                                     bool uses_intent_observability,
                                     bool verbose,
                                     const char **error_out);

#endif /* PGY_COMPILER_TOOLCHAIN_H */
