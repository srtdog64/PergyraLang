/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_COMPILER_TOOLCHAIN_H
#define PGY_COMPILER_TOOLCHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include "compiler.h"

int pgy_exec_argv(const char *const argv[], bool verbose);
const char *pgy_detect_c_compiler(void);
const char *pgy_cc_extra_target_flag(void);
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
bool compiler_runtime_cache_is_fresh(const char *cache_obj_path);
char *compiler_runtime_prebuilt_object_path(PgyOptProfile opt_profile,
                                            bool uses_intent_observability);
char *compiler_runtime_cache_object_path(PgyOptProfile opt_profile,
                                         bool uses_intent_observability);
#endif

#endif /* PGY_COMPILER_TOOLCHAIN_H */
