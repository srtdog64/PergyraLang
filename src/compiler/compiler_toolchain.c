/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "compiler_toolchain.h"
#include "compiler_process.h"
#include "path_utils.h"
#include "../common/env_flags.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef PGY_LLVM_ENABLED
void
compiler_debug_llvm_host_stage(const char *stage)
{
    if (stage == NULL || getenv("PGY_DEBUG_LLVM_HOST") == NULL)
        return;
    printf("[llvm host] %s\n", stage);
    fflush(stdout);
}
#endif

/* -----------------------------------------------------------------
 * C compiler detection: PGY_CC env -> clang -> gcc -> cc
 *
 * On Windows, clang may default to MSVC target which lacks pthread.h. The
 * caller owns PgyCCompilerSelection storage, so detection does not need a
 * process-global mutable cache.
 * ----------------------------------------------------------------- */
static bool
pgy_parse_compiler_command(PgyCCompilerSelection *selection,
                           const char *command)
{
    const char *cursor = command;
    const char *token_start;
    const char *token_end;
    size_t token_len;
    char quote = '\0';

    if (selection == NULL || command == NULL)
        return false;

    while (*cursor != '\0' && isspace((unsigned char)*cursor))
        cursor++;
    if (*cursor == '\0')
        return false;

    token_start = cursor;
    if (*cursor == '"' || *cursor == '\'') {
        quote = *cursor++;
        token_start = cursor;
        while (*cursor != '\0' && *cursor != quote)
            cursor++;
        token_end = cursor;
        if (*cursor == quote)
            cursor++;
    } else {
        while (*cursor != '\0' && !isspace((unsigned char)*cursor))
            cursor++;
        token_end = cursor;
    }

    token_len = (size_t)(token_end - token_start);
    if (token_len == 0 || token_len >= sizeof(selection->cc_storage))
        return false;

    memcpy(selection->cc_storage, token_start, token_len);
    selection->cc_storage[token_len] = '\0';
    selection->cc = selection->cc_storage;
    selection->target_flag = NULL;

    while (*cursor != '\0') {
        const char *flag = cursor;
        const char *value = NULL;
        const char *flag_end;
        size_t flag_len;

        while (*flag != '\0' && isspace((unsigned char)*flag))
            flag++;
        if (*flag == '\0')
            break;

        flag_end = flag;
        while (*flag_end != '\0' && !isspace((unsigned char)*flag_end))
            flag_end++;
        flag_len = (size_t)(flag_end - flag);

        if (flag_len > strlen("--target=")
            && strncmp(flag, "--target=", strlen("--target=")) == 0) {
            if (flag_len >= sizeof(selection->target_storage))
                return false;
            memcpy(selection->target_storage, flag, flag_len);
            selection->target_storage[flag_len] = '\0';
            selection->target_flag = selection->target_storage;
            break;
        }

        if (flag_len == strlen("--target")
            && strncmp(flag, "--target", flag_len) == 0) {
            value = flag_end;
            while (*value != '\0' && isspace((unsigned char)*value))
                value++;
            if (*value == '\0')
                return false;

            flag_end = value;
            while (*flag_end != '\0' && !isspace((unsigned char)*flag_end))
                flag_end++;
            flag_len = (size_t)(flag_end - value);
            if (flag_len == 0
                || flag_len + strlen("--target=") >= sizeof(selection->target_storage)) {
                return false;
            }
            memcpy(selection->target_storage, "--target=", strlen("--target="));
            memcpy(selection->target_storage + strlen("--target="), value,
                   flag_len);
            selection->target_storage[strlen("--target=") + flag_len] = '\0';
            selection->target_flag = selection->target_storage;
            break;
        }

        cursor = flag_end;
    }

    return true;
}

bool
pgy_select_c_compiler(PgyCCompilerSelection *selection)
{
    const char *env_cc;
    const char *make_cc;
    if (selection == NULL)
        return false;
    memset(selection, 0, sizeof(*selection));

    env_cc = getenv("PGY_CC");
    if (env_cc != NULL && env_cc[0] != '\0') {
        if (pgy_parse_compiler_command(selection, env_cc))
            return true;
    }
    make_cc = getenv("CC");
    if (make_cc != NULL && make_cc[0] != '\0') {
        if (pgy_parse_compiler_command(selection, make_cc))
            return true;
    }
#ifdef _WIN32
    /* Prefer an actual MinGW GCC driver on Windows.
     *
     * The native backend and LLVM object-link path both rely on the same
     * thread/runtime model as the rest of the MinGW toolchain. Picking
     * clang --target=x86_64-w64-mingw32 here looks attractive, but in
     * practice it can drift into a different runtime/thread model and leave
     * libgcc_eh/emutls unresolved against pthread_* during final link.
     *
     * The repository already builds pgy itself through MinGW GCC on Windows,
     * so use the same driver first for emitted program builds as well.
     */
    {
        const char *mingw_gcc_ver[] = { "x86_64-w64-mingw32-gcc", "--version", NULL };
        if (pgy_exec_probe_argv_silent(mingw_gcc_ver) == 0) {
            selection->cc = "x86_64-w64-mingw32-gcc";
            return true;
        }
    }
    {
        const char *gcc_ver[] = { "gcc", "--version", NULL };
        if (pgy_exec_probe_argv_silent(gcc_ver) == 0) {
            selection->cc = "gcc";
            return true;
        }
    }
    /* Fallback: clang with explicit MinGW target. */
    {
        const char *clang_mingw[] = { "clang", "--target=x86_64-w64-mingw32", "--version", NULL };
        if (pgy_exec_probe_argv_silent(clang_mingw) == 0) {
            selection->cc = "clang";
            selection->target_flag = "--target=x86_64-w64-mingw32";
            return true;
        }
    }
    /* Last fallback: plain clang. */
    {
        const char *clang_ver[] = { "clang", "--version", NULL };
        if (pgy_exec_probe_argv_silent(clang_ver) == 0) {
            selection->cc = "clang";
            return true;
        }
    }
#else
    {
        const char *candidates[] = { "gcc", "clang", "cc", NULL };
        for (int i = 0; candidates[i] != NULL; i++) {
            const char *test_argv[] = { candidates[i], "--version", NULL };
            if (pgy_exec_probe_argv_silent(test_argv) == 0) {
                selection->cc = candidates[i];
                return true;
            }
        }
    }
#endif
    return false;
}

double
compiler_now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
#endif
}

#ifndef _WIN32
static bool
compiler_env_truthy(const char *name)
{
    return pgy_env_value_is_truthy(getenv(name));
}

bool
compiler_should_use_lld(void)
{
    const char *value = getenv("PGY_USE_LLD");

    if (value != NULL && value[0] != '\0')
        return compiler_env_truthy("PGY_USE_LLD");
    return access("/usr/bin/ld.lld", X_OK) == 0
        || access("/usr/local/bin/ld.lld", X_OK) == 0
        || access("/bin/ld.lld", X_OK) == 0;
}
#endif

/* Validate a path contains no shell metacharacters */
bool
pgy_path_is_safe(const char *path)
{
    for (const char *p = path; *p; p++) {
        switch (*p) {
        case ';': case '&': case '|': case '`':
        case '$': case '(': case ')': case '{':
        case '}': case '<': case '>': case '!':
        case '\n': case '\r':
            return false;
        default:
            break;
        }
    }
    return true;
}
