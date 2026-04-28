/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "compiler_toolchain.h"
#include "path_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* -----------------------------------------------------------------
 * Safe process execution (no shell; immune to command injection)
 *
 * argv must be NULL-terminated.
 * Returns process exit code, or -1 on failure.
 * ----------------------------------------------------------------- */
int
pgy_exec_argv(const char *const argv[], bool verbose)
{
    if (verbose) {
        printf("pgy:");
        for (const char *const *p = argv; *p; p++)
            printf(" %s", *p);
        printf("\n");
    }

#ifdef _WIN32
    intptr_t rc = _spawnvp(_P_WAIT, argv[0], argv);
    return (int)rc;
#else
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
#endif
}

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

#ifdef _WIN32
void
pgy_win32_normalize_exec_path(const char *path, char *dst, size_t dst_cap)
{
    const char *tmpbase = NULL;
    size_t pos = 0;

    if (dst == NULL || dst_cap == 0)
        return;
    dst[0] = '\0';
    if (path == NULL || path[0] == '\0')
        return;

    if (path[0] == '/' && path[1] != '\0' && path[2] == '/'
        && ((path[1] >= 'a' && path[1] <= 'z')
            || (path[1] >= 'A' && path[1] <= 'Z'))) {
        if (dst_cap < 4)
            return;
        dst[pos++] = path[1];
        dst[pos++] = ':';
        dst[pos++] = '\\';
        path += 3;
    } else if (strncmp(path, "/tmp/", 5) == 0 || strcmp(path, "/tmp") == 0) {
        tmpbase = getenv("TMPDIR");
        if (tmpbase == NULL || tmpbase[0] == '\0')
            tmpbase = getenv("TMP");
        if (tmpbase == NULL || tmpbase[0] == '\0')
            tmpbase = getenv("TEMP");
        if (tmpbase != NULL && tmpbase[0] != '\0') {
            size_t base_len = strlen(tmpbase);
            if (base_len >= dst_cap)
                base_len = dst_cap - 1;
            memcpy(dst, tmpbase, base_len);
            pos = base_len;
            if (pos > 0 && dst[pos - 1] != '\\' && dst[pos - 1] != '/') {
                if (pos + 1 >= dst_cap) {
                    dst[dst_cap - 1] = '\0';
                    return;
                }
                dst[pos++] = '\\';
            }
            path += (path[4] == '\0') ? 4 : 5;
        }
    }

    while (*path != '\0' && pos + 1 < dst_cap) {
        dst[pos++] = (*path == '/') ? '\\' : *path;
        path++;
    }
    dst[pos] = '\0';
}

/* Silent probe via CreateProcess: stdout/stderr go to NUL in the child only.
 * Parent's file descriptors are never touched. */
static bool
pgy_win32_quote_arg(char *dst, size_t dst_cap, size_t *pos_io, const char *arg)
{
    size_t pos = pos_io != NULL ? *pos_io : 0;
    bool needs_quotes = false;
    const char *p;

    if (dst == NULL || dst_cap == 0 || pos_io == NULL || arg == NULL)
        return false;

    for (p = arg; *p != '\0'; p++) {
        if (*p == ' ' || *p == '\t' || *p == '"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        size_t n = strlen(arg);
        if (pos + n + 1 >= dst_cap)
            return false;
        memcpy(dst + pos, arg, n);
        pos += n;
        dst[pos] = '\0';
        *pos_io = pos;
        return true;
    }

    if (pos + 2 >= dst_cap)
        return false;
    dst[pos++] = '"';

    p = arg;
    while (*p != '\0') {
        size_t slash_count = 0;
        while (*p == '\\') {
            slash_count++;
            p++;
        }
        if (*p == '"') {
            while (slash_count-- > 0) {
                if (pos + 2 >= dst_cap)
                    return false;
                dst[pos++] = '\\';
                dst[pos++] = '\\';
            }
            if (pos + 2 >= dst_cap)
                return false;
            dst[pos++] = '\\';
            dst[pos++] = '"';
            p++;
            continue;
        }
        if (*p == '\0') {
            while (slash_count-- > 0) {
                if (pos + 2 >= dst_cap)
                    return false;
                dst[pos++] = '\\';
                dst[pos++] = '\\';
            }
            break;
        }
        while (slash_count-- > 0) {
            if (pos + 1 >= dst_cap)
                return false;
            dst[pos++] = '\\';
        }
        if (pos + 1 >= dst_cap)
            return false;
        dst[pos++] = *p++;
    }

    if (pos + 2 >= dst_cap)
        return false;
    dst[pos++] = '"';
    dst[pos] = '\0';
    *pos_io = pos;
    return true;
}

static int
pgy_exec_probe_argv_silent(const char *const argv[])
{
    char cmdline[1024];
    size_t pos = 0;
    for (const char *const *p = argv; *p != NULL; p++) {
        if (pos > 0) {
            if (pos + 1 >= sizeof(cmdline))
                return -1;
            cmdline[pos++] = ' ';
            cmdline[pos] = '\0';
        }
        if (!pgy_win32_quote_arg(cmdline, sizeof(cmdline), &pos, *p))
            return -1;
    }
    cmdline[pos] = '\0';

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
                             &sa, OPEN_EXISTING, 0, NULL);
    if (nul == INVALID_HANDLE_VALUE) return -1;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = nul;
    si.hStdError  = nul;

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) { CloseHandle(nul); return -1; }

    DWORD wait_rc = WaitForSingleObject(pi.hProcess, 5000);
    DWORD exit_code = 1;
    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        WaitForSingleObject(pi.hProcess, 1000);
        exit_code = 124;
    } else if (wait_rc == WAIT_OBJECT_0) {
        if (!GetExitCodeProcess(pi.hProcess, &exit_code))
            exit_code = 1;
    } else {
        exit_code = 1;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(nul);
    return (int)exit_code;
}
#else
static int
pgy_exec_probe_argv(const char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        FILE *devnull = fopen("/dev/null", "w");
        if (devnull != NULL) {
            dup2(fileno(devnull), STDOUT_FILENO);
            dup2(fileno(devnull), STDERR_FILENO);
            fclose(devnull);
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
}
#endif

/* -----------------------------------------------------------------
 * C compiler detection: PGY_CC env -> clang -> gcc -> cc
 *
 * On Windows, clang may default to MSVC target which lacks pthread.h.
 * We detect this and store a --target flag for mingw if needed.
 * ----------------------------------------------------------------- */
static const char *pgy_cc_cached = NULL;
static const char *pgy_cc_target_flag = NULL; /* e.g. "--target=x86_64-w64-mingw32" */
static char pgy_cc_cached_storage[512];
static char pgy_cc_target_storage[256];

static bool
pgy_cache_compiler_command(const char *command)
{
    const char *cursor = command;
    const char *token_start;
    const char *token_end;
    size_t token_len;
    char quote = '\0';

    if (command == NULL)
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
    if (token_len == 0 || token_len >= sizeof(pgy_cc_cached_storage))
        return false;

    memcpy(pgy_cc_cached_storage, token_start, token_len);
    pgy_cc_cached_storage[token_len] = '\0';
    pgy_cc_cached = pgy_cc_cached_storage;
    pgy_cc_target_flag = NULL;

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
            if (flag_len >= sizeof(pgy_cc_target_storage))
                return false;
            memcpy(pgy_cc_target_storage, flag, flag_len);
            pgy_cc_target_storage[flag_len] = '\0';
            pgy_cc_target_flag = pgy_cc_target_storage;
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
                || flag_len + strlen("--target=") >= sizeof(pgy_cc_target_storage)) {
                return false;
            }
            memcpy(pgy_cc_target_storage, "--target=", strlen("--target="));
            memcpy(pgy_cc_target_storage + strlen("--target="), value, flag_len);
            pgy_cc_target_storage[strlen("--target=") + flag_len] = '\0';
            pgy_cc_target_flag = pgy_cc_target_storage;
            break;
        }

        cursor = flag_end;
    }

    return true;
}

const char *
pgy_detect_c_compiler(void)
{
    const char *env_cc;
    const char *make_cc;
    if (pgy_cc_cached != NULL)
        return pgy_cc_cached;
    env_cc = getenv("PGY_CC");
    if (env_cc != NULL && env_cc[0] != '\0') {
        if (pgy_cache_compiler_command(env_cc))
            return pgy_cc_cached;
    }
    make_cc = getenv("CC");
    if (make_cc != NULL && make_cc[0] != '\0') {
        if (pgy_cache_compiler_command(make_cc))
            return pgy_cc_cached;
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
            pgy_cc_cached = "x86_64-w64-mingw32-gcc";
            pgy_cc_target_flag = NULL;
            return pgy_cc_cached;
        }
    }
    {
        const char *gcc_ver[] = { "gcc", "--version", NULL };
        if (pgy_exec_probe_argv_silent(gcc_ver) == 0) {
            pgy_cc_cached = "gcc";
            pgy_cc_target_flag = NULL;
            return pgy_cc_cached;
        }
    }
    /* Fallback: clang with explicit MinGW target. */
    {
        const char *clang_mingw[] = { "clang", "--target=x86_64-w64-mingw32", "--version", NULL };
        if (pgy_exec_probe_argv_silent(clang_mingw) == 0) {
            pgy_cc_cached = "clang";
            pgy_cc_target_flag = "--target=x86_64-w64-mingw32";
            return pgy_cc_cached;
        }
    }
    /* Last fallback: plain clang. */
    {
        const char *clang_ver[] = { "clang", "--version", NULL };
        if (pgy_exec_probe_argv_silent(clang_ver) == 0) {
            pgy_cc_cached = "clang";
            pgy_cc_target_flag = NULL;
            return pgy_cc_cached;
        }
    }
#else
    {
        const char *candidates[] = { "gcc", "clang", "cc", NULL };
        for (int i = 0; candidates[i] != NULL; i++) {
            const char *test_argv[] = { candidates[i], "--version", NULL };
            if (pgy_exec_probe_argv(test_argv) == 0) {
                pgy_cc_cached = candidates[i];
                return pgy_cc_cached;
            }
        }
    }
#endif
    pgy_cc_cached = "gcc";
    return pgy_cc_cached;
}

/* Returns extra target flag for the detected compiler, or NULL */
const char *
pgy_cc_extra_target_flag(void)
{
    if (pgy_cc_cached == NULL)
        pgy_detect_c_compiler();
    return pgy_cc_target_flag;
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
    const char *value = getenv(name);

    if (value == NULL || value[0] == '\0')
        return false;
    if (strcmp(value, "0") == 0
        || strcmp(value, "false") == 0
        || strcmp(value, "FALSE") == 0
        || strcmp(value, "off") == 0
        || strcmp(value, "OFF") == 0
        || strcmp(value, "no") == 0
        || strcmp(value, "NO") == 0) {
        return false;
    }
    return true;
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
