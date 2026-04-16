#include "compiler.h"
#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>
#define PGY_STAT _stat
#define PGY_STAT_STRUCT struct _stat
#include <process.h>    /* _spawnvp */
#else
#include <sys/time.h>
#include <sys/stat.h>
#define PGY_STAT stat
#define PGY_STAT_STRUCT struct stat
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "../codegen/transpiler.h"
#include "../common/string_compat.h"

#ifndef PGY_SRC_DIR
#define PGY_SRC_DIR "src"
#endif

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
#endif

#ifdef PGY_LLVM_ENABLED
#include "../codegen/llvm_backend.h"
#endif

#ifdef _WIN32
#define PGY_CFLAGS_THREAD_LIB "-lwinpthread"
#else
#define PGY_CFLAGS_THREAD_LIB "-lpthread"
#endif

/* -----------------------------------------------------------------
 * Safe process execution (no shell — immune to command injection)
 *
 * argv must be NULL-terminated.
 * Returns process exit code, or -1 on failure.
 * ----------------------------------------------------------------- */
static int
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
static void
compiler_debug_llvm_host_stage(const char *stage)
{
    if (stage == NULL || getenv("PGY_DEBUG_LLVM_HOST") == NULL)
        return;
    printf("[llvm host] %s\n", stage);
    fflush(stdout);
}
#endif

#ifdef _WIN32
/* Silent probe via CreateProcess: stdout/stderr → NUL in the child only.
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
 * C compiler detection: PGY_CC env → clang → gcc → cc
 *
 * On Windows, clang may default to MSVC target which lacks pthread.h.
 * We detect this and store a --target flag for mingw if needed.
 * ----------------------------------------------------------------- */
static const char *pgy_cc_cached = NULL;
static const char *pgy_cc_target_flag = NULL; /* e.g. "--target=x86_64-w64-mingw32" */

static const char *
pgy_detect_c_compiler(void)
{
    const char *env_cc;
    if (pgy_cc_cached != NULL)
        return pgy_cc_cached;
    env_cc = getenv("PGY_CC");
    if (env_cc != NULL && env_cc[0] != '\0') {
        pgy_cc_cached = env_cc;
        return pgy_cc_cached;
    }
#ifdef _WIN32
    /* Try clang with mingw target first (MSVC-default clang lacks pthread.h,
     * and gcc on Windows frequently crashes via cc1.exe) */
    {
        const char *clang_mingw[] = { "clang", "--target=x86_64-w64-mingw32", "--version", NULL };
        if (pgy_exec_probe_argv_silent(clang_mingw) == 0) {
            pgy_cc_cached = "clang";
            pgy_cc_target_flag = "--target=x86_64-w64-mingw32";
            return pgy_cc_cached;
        }
    }
    /* Try gcc (mingw gcc has pthread.h built-in, but cc1 may crash) */
    {
        const char *gcc_ver[] = { "gcc", "--version", NULL };
        if (pgy_exec_probe_argv_silent(gcc_ver) == 0) {
            pgy_cc_cached = "gcc"; return pgy_cc_cached;
        }
    }
    /* Fallback: plain clang without mingw target */
    {
        const char *clang_ver[] = { "clang", "--version", NULL };
        if (pgy_exec_probe_argv_silent(clang_ver) == 0) {
            pgy_cc_cached = "clang"; return pgy_cc_cached; }
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
static const char *
pgy_cc_extra_target_flag(void)
{
    if (pgy_cc_cached == NULL)
        pgy_detect_c_compiler();
    return pgy_cc_target_flag;
}

static double
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

static bool
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

#ifdef PGY_LLVM_ENABLED

static const char *
compiler_temp_dir(void)
{
    const char *tmpdir = getenv("TMPDIR");

    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TMP");
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = ".";
#else
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = "/tmp";
#endif
    return tmpdir;
}

static bool
compiler_file_mtime(const char *path, time_t *mtime_out)
{
    PGY_STAT_STRUCT st;

    if (path == NULL || mtime_out == NULL)
        return false;
    if (PGY_STAT(path, &st) != 0)
        return false;
    *mtime_out = st.st_mtime;
    return true;
}

static bool
compiler_runtime_cache_is_fresh(const char *cache_obj_path)
{
    time_t cache_mtime;
    const char *deps[] = {
        PGY_RUNTIME_LIB_C,
        PGY_RUNTIME_DIR "/pgy_runtime.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_part_a.inc",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_part_b.inc",
        PGY_RUNTIME_DIR "/pgy_runtime_part_a.inc",
        PGY_RUNTIME_DIR "/pgy_runtime_part_b.inc",
        PGY_RUNTIME_DIR "/pgy_runtime_part_ba.inc",
        PGY_RUNTIME_DIR "/pgy_runtime_part_bb.inc",
        PGY_RUNTIME_DIR "/pgy_runtime_part_c.inc",
        NULL
    };

    if (!compiler_file_mtime(cache_obj_path, &cache_mtime))
        return false;
    for (size_t i = 0; deps[i] != NULL; i++) {
        time_t dep_mtime;

        if (!compiler_file_mtime(deps[i], &dep_mtime))
            return false;
        if (cache_mtime < dep_mtime)
            return false;
    }
    return true;
}

static char *
compiler_runtime_prebuilt_object_path(PgyOptProfile opt_profile,
                                      bool uses_intent_observability)
{
    char key[64];
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "RELEASE" : "DEV";
    const char *obs_name = uses_intent_observability ? "OBS1" : "OBS0";
    const char *value;

    snprintf(key, sizeof(key), "PGY_PREBUILT_RUNTIME_OBJ_%s_%s", opt_name, obs_name);
    value = getenv(key);
    if (value == NULL || value[0] == '\0')
        value = getenv("PGY_PREBUILT_RUNTIME_OBJ");
    if (value == NULL || value[0] == '\0')
        return NULL;
    return pergyra_strdup(value);
}

static char *
compiler_runtime_cache_object_path(PgyOptProfile opt_profile,
                                   bool uses_intent_observability)
{
    const char *tmpdir = compiler_temp_dir();
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "release" : "dev";
    const char *obs_name = uses_intent_observability ? "obs1" : "obs0";
    char buf[1024];
#ifdef _WIN32
    const char *ext = ".obj";
#else
    const char *ext = ".o";
#endif

    snprintf(buf, sizeof(buf), "%s/pgy_runtime_cache_%s_%s%s",
             tmpdir, opt_name, obs_name, ext);
    return pergyra_strdup(buf);
}

#endif /* PGY_LLVM_ENABLED */

/* Validate a path contains no shell metacharacters */
static bool
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

static CompilerResult *
compiler_result_create(void)
{
    return calloc(1, sizeof(CompilerResult));
}

static CompilerResult *
compiler_error(const char *message)
{
    CompilerResult *result = compiler_result_create();
    if (result == NULL)
        return NULL;

    result->success = false;
    result->exit_code = 1;
    result->error_message = pergyra_strdup(message);
    return result;
}

static CompilerResult *
compiler_success(const char *output_c_path, const char *output_binary_path)
{
    CompilerResult *result = compiler_result_create();
    if (result == NULL)
        return NULL;

    result->success = true;
    result->c_output_path = output_c_path != NULL ? pergyra_strdup(output_c_path) : NULL;
    result->binary_path = output_binary_path != NULL ? pergyra_strdup(output_binary_path) : NULL;
    return result;
}

static int
invoke_c_backend(const CompilerIRBundle *bundle,
                 const char *output_c_path,
                 char **error_message,
                 bool *uses_intent_observability)
{
    TranspileResult *transpile_result;
    if (error_message != NULL)
        *error_message = NULL;
    if (uses_intent_observability != NULL)
        *uses_intent_observability = false;
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("IR bundle is incomplete");
        return 1;
    }

    transpile_result = transpile_from_mir(bundle->mir, output_c_path);
    if (transpile_result == NULL) {
        *error_message = pergyra_strdup("Out of memory");
        return 1;
    }

    if (!transpile_result->success) {
        *error_message = transpile_result->error_message != NULL
            ? pergyra_strdup(transpile_result->error_message)
            : pergyra_strdup("C backend failed");
        transpile_result_destroy(transpile_result);
        return 1;
    }

    if (uses_intent_observability != NULL)
        *uses_intent_observability = transpile_result->uses_intent_observability;

    transpile_result_destroy(transpile_result);
    return 0;
}

CompilerResult *
compiler_emit_c(const CompilerIRBundle *bundle, const char *output_c_path)
{
    char *error_message = NULL;
    int rc = invoke_c_backend(bundle, output_c_path, &error_message, NULL);
    if (rc != 0) {
        CompilerResult *result = compiler_error(error_message != NULL
            ? error_message
            : "C backend failed");
        free(error_message);
        return result;
    }

    return compiler_success(output_c_path, NULL);
}

CompilerResult *
compiler_build_native(const CompilerIRBundle *bundle,
                      const char *output_c_path,
                      const char *output_binary_path,
                      bool verbose,
                      PgyOptProfile opt_profile)
{
    char *error_message = NULL;
    char *output_obj_path = NULL;
    double phase_start = compiler_now_seconds();
    bool uses_intent_observability = false;
    int rc = invoke_c_backend(bundle, output_c_path, &error_message,
                              &uses_intent_observability);
    if (rc != 0) {
        CompilerResult *result = compiler_error(error_message != NULL
            ? error_message
            : "C backend failed");
        if (result != NULL)
            result->backend_timings.codegen = compiler_now_seconds() - phase_start;
        free(error_message);
        return result;
    }

    output_obj_path = path_replace_extension(output_binary_path, ".o");
    if (output_obj_path == NULL)
        return compiler_error("Out of memory");

    if (!pgy_path_is_safe(output_c_path) ||
        !pgy_path_is_safe(output_binary_path) ||
        !pgy_path_is_safe(output_obj_path)) {
        free(output_obj_path);
        return compiler_error("Unsafe characters in file path");
    }

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *intent_observability_flag =
        uses_intent_observability
            ? "-DPGY_INTENT_OBSERVABILITY_ENABLED=1"
            : "-DPGY_INTENT_OBSERVABILITY_ENABLED=0";
    CompilerResult *result = NULL;
    const char *cc = pgy_detect_c_compiler();
    const char *cc_target = pgy_cc_extra_target_flag();
    {
        const char *compile_argv[24];
        int ci = 0;
#ifdef _WIN32
        const char *link_argv[24];
        int li = 0;
#endif
        compile_argv[ci++] = cc;
        if (cc_target != NULL) compile_argv[ci++] = cc_target;
        compile_argv[ci++] = "-std=c11";
        compile_argv[ci++] = "-Wall";
        compile_argv[ci++] = "-Wno-unused-function";
#ifdef _WIN32
        compile_argv[ci++] = "-Wno-unused-value";
        compile_argv[ci++] = "-Wno-parentheses-equality";
        compile_argv[ci++] = "-Wno-c23-extensions";
#endif
        compile_argv[ci++] = opt_flag;
#ifndef _WIN32
        compile_argv[ci++] = "-fopenmp";
#endif
        compile_argv[ci++] = intent_observability_flag;
        compile_argv[ci++] = "-I";
        compile_argv[ci++] = PGY_SRC_DIR;
        compile_argv[ci++] = "-I";
        compile_argv[ci++] = PGY_RUNTIME_DIR;
        compile_argv[ci++] = "-c";
        compile_argv[ci++] = output_c_path;
        compile_argv[ci++] = "-o";
        compile_argv[ci++] = output_obj_path;
        compile_argv[ci] = NULL;

#ifdef _WIN32
        link_argv[li++] = cc;
        if (cc_target != NULL) link_argv[li++] = cc_target;
        link_argv[li++] = "-std=c11";
        link_argv[li++] = "-Wall";
        link_argv[li++] = opt_flag;
        link_argv[li++] = output_obj_path;
        link_argv[li++] = "-o";
        link_argv[li++] = output_binary_path;
        link_argv[li++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[li++] = "-lm";
        link_argv[li] = NULL;
#endif

    result = compiler_success(output_c_path, output_binary_path);
    if (result == NULL) {
        free(output_obj_path);
        return NULL;
    }
    result->backend_timings.codegen = compiler_now_seconds() - phase_start;

    phase_start = compiler_now_seconds();
    rc = pgy_exec_argv(compile_argv, verbose);
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup("Native compilation failed");
        result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
        remove(output_obj_path);
        free(output_obj_path);
        return result;
    }
    result->backend_timings.native_compile = compiler_now_seconds() - phase_start;

    phase_start = compiler_now_seconds();
#ifndef _WIN32
    /* Linux: rebuild link_argv with extra flags (lld, build-id, fopenmp) */
    {
        const char *lnk[20];
        int lc = 0;
        lnk[lc++] = cc;
        if (cc_target != NULL) lnk[lc++] = cc_target;
        lnk[lc++] = "-std=c11";
        lnk[lc++] = "-Wall";
        lnk[lc++] = opt_flag;
        lnk[lc++] = "-fopenmp";
        if (compiler_should_use_lld())
            lnk[lc++] = "-fuse-ld=lld";
        lnk[lc++] = "-Wl,--build-id=none";
        lnk[lc++] = output_obj_path;
        lnk[lc++] = "-o";
        lnk[lc++] = output_binary_path;
        lnk[lc++] = PGY_CFLAGS_THREAD_LIB;
        lnk[lc++] = "-lm";
        lnk[lc] = NULL;
        rc = pgy_exec_argv(lnk, verbose);
    }
#else
    rc = pgy_exec_argv(link_argv, verbose);
#endif
    } /* close compile/link argv scope */
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup("Native link failed");
        result->backend_timings.link = compiler_now_seconds() - phase_start;
        remove(output_obj_path);
        free(output_obj_path);
        return result;
    }
    result->backend_timings.link = compiler_now_seconds() - phase_start;

    remove(output_obj_path);
    free(output_obj_path);
    return result;
}

int
compiler_run_binary(const char *binary_path, bool verbose)
{
    if (!pgy_path_is_safe(binary_path))
        return -1;

    char *resolved_path = path_resolve_runnable_binary(binary_path);
    if (resolved_path == NULL || !pgy_path_is_safe(resolved_path)) {
        free(resolved_path);
        return -1;
    }

    char safe_path[512];
#ifdef _WIN32
    snprintf(safe_path, sizeof(safe_path), "%s", resolved_path);
    for (char *p = safe_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#else
    if (resolved_path[0] == '/' || strncmp(resolved_path, "./", 2) == 0) {
        snprintf(safe_path, sizeof(safe_path), "%s", resolved_path);
    } else {
        snprintf(safe_path, sizeof(safe_path), "./%s", resolved_path);
    }
#endif

    const char *run_argv[] = { safe_path, NULL };

    if (verbose) {
        printf("--- output ---\n");
        fflush(stdout);
    }
    int rc = pgy_exec_argv(run_argv, verbose);
    if (verbose) {
        printf("--- end ---\n");
        fflush(stdout);
    }
    free(resolved_path);
    return rc;
}

#ifdef PGY_LLVM_ENABLED

CompilerResult *
compiler_emit_llvm_ir(const CompilerIRBundle *bundle, const char *module_name)
{
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    LLVMGenResult *gen = llvm_codegen_from_mir(bundle->mir, module_name);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error(gen->error_message != NULL
            ? gen->error_message
            : "LLVM codegen failed");
        llvm_gen_result_destroy(gen);
        return result;
    }

    /* Print IR to stdout */
    if (gen->ir_text != NULL)
        printf("%s", gen->ir_text);

    llvm_gen_result_destroy(gen);
    return compiler_success(NULL, NULL);
}

CompilerResult *
compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                              const char *module_name,
                              const char *output_ir_path)
{
    if (!pgy_path_is_safe(output_ir_path))
        return compiler_error("Unsafe characters in file path");
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    LLVMGenResult *gen = llvm_codegen_from_mir(bundle->mir, module_name);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error(gen->error_message != NULL
            ? gen->error_message
            : "LLVM codegen failed");
        llvm_gen_result_destroy(gen);
        return result;
    }

    FILE *out = fopen(output_ir_path, "w");
    if (out == NULL) {
        llvm_gen_result_destroy(gen);
        return compiler_error("Cannot open LLVM IR output file");
    }

    if (gen->ir_text != NULL)
        fputs(gen->ir_text, out);
    fclose(out);

    llvm_gen_result_destroy(gen);
    return compiler_success(output_ir_path, NULL);
}

CompilerResult *
compiler_build_native_llvm(const CompilerIRBundle *bundle,
                           const char *output_obj_path,
                           const char *output_binary_path,
                           bool verbose,
                           PgyOptProfile opt_profile)
{
    char *runtime_obj_path = NULL;
    double phase_start;
    CompilerResult *result = NULL;
    bool compiled_runtime = false;
    bool uses_intent_observability = false;

    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }
    if (verbose)
        printf("pgy: LLVM codegen → %s\n", output_obj_path);

    compiler_debug_llvm_host_stage("codegen_begin");
    phase_start = compiler_now_seconds();
    LLVMGenResult *gen = llvm_codegen_to_object_from_mir(
        bundle->mir,
        "pergyra_module",
        output_obj_path,
        opt_profile == PGY_OPT_RELEASE);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *error_result = compiler_error(gen->error_message != NULL
            ? gen->error_message
            : "LLVM codegen failed");
        if (error_result != NULL)
            error_result->backend_timings.codegen = compiler_now_seconds() - phase_start;
        llvm_gen_result_destroy(gen);
        return error_result;
    }
    uses_intent_observability = gen->uses_intent_observability;
    llvm_gen_result_destroy(gen);
    compiler_debug_llvm_host_stage("codegen_done");

    /* Link object file with GCC + runtime library */
    if (!pgy_path_is_safe(output_binary_path) ||
        !pgy_path_is_safe(output_obj_path)) {
        return compiler_error("Unsafe characters in file path");
    }

    runtime_obj_path = compiler_runtime_prebuilt_object_path(
        opt_profile,
        uses_intent_observability);
    bool using_prebuilt_runtime = (runtime_obj_path != NULL);
    if (runtime_obj_path == NULL) {
        runtime_obj_path = compiler_runtime_cache_object_path(
            opt_profile,
            uses_intent_observability);
    }
    if (runtime_obj_path == NULL)
        return compiler_error("Out of memory");
    if (!pgy_path_is_safe(runtime_obj_path)) {
        free(runtime_obj_path);
        return compiler_error("Unsafe characters in file path");
    }

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *intent_observability_flag =
        uses_intent_observability
            ? "-DPGY_INTENT_OBSERVABILITY_ENABLED=1"
            : "-DPGY_INTENT_OBSERVABILITY_ENABLED=0";
    const char *cc = pgy_detect_c_compiler();
    const char *cc_target = pgy_cc_extra_target_flag();
    result = compiler_success(output_obj_path, output_binary_path);
    if (result == NULL) {
        free(runtime_obj_path);
        return NULL;
    }
    result->backend_timings.codegen = compiler_now_seconds() - phase_start;
    compiler_debug_llvm_host_stage("runtime_prepare");
#ifdef _WIN32
    const char *compile_runtime_argv[24];
    int compile_runtime_argc = 0;
    compile_runtime_argv[compile_runtime_argc++] = cc;
    if (cc_target != NULL)
        compile_runtime_argv[compile_runtime_argc++] = cc_target;
    compile_runtime_argv[compile_runtime_argc++] = "-std=c11";
    compile_runtime_argv[compile_runtime_argc++] = "-Wall";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-unused-value";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-parentheses-equality";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-c23-extensions";
    compile_runtime_argv[compile_runtime_argc++] = opt_flag;
    compile_runtime_argv[compile_runtime_argc++] = intent_observability_flag;
    compile_runtime_argv[compile_runtime_argc++] = "-DPGY_LLVM_ENABLED";
    compile_runtime_argv[compile_runtime_argc++] = "-I";
    compile_runtime_argv[compile_runtime_argc++] = PGY_SRC_DIR;
    compile_runtime_argv[compile_runtime_argc++] = "-c";
    compile_runtime_argv[compile_runtime_argc++] = PGY_RUNTIME_LIB_C;
    compile_runtime_argv[compile_runtime_argc++] = "-o";
    compile_runtime_argv[compile_runtime_argc++] = runtime_obj_path;
    compile_runtime_argv[compile_runtime_argc] = NULL;
#else
    const char *compile_runtime_argv[20];
    int compile_runtime_argc = 0;
    compile_runtime_argv[compile_runtime_argc++] = cc;
    if (cc_target != NULL)
        compile_runtime_argv[compile_runtime_argc++] = cc_target;
    compile_runtime_argv[compile_runtime_argc++] = "-std=c11";
    compile_runtime_argv[compile_runtime_argc++] = "-Wall";
    compile_runtime_argv[compile_runtime_argc++] = opt_flag;
    compile_runtime_argv[compile_runtime_argc++] = "-fopenmp";
    compile_runtime_argv[compile_runtime_argc++] = intent_observability_flag;
    compile_runtime_argv[compile_runtime_argc++] = "-DPGY_LLVM_ENABLED";
    compile_runtime_argv[compile_runtime_argc++] = "-I";
    compile_runtime_argv[compile_runtime_argc++] = PGY_SRC_DIR;
    compile_runtime_argv[compile_runtime_argc++] = "-c";
    compile_runtime_argv[compile_runtime_argc++] = PGY_RUNTIME_LIB_C;
    compile_runtime_argv[compile_runtime_argc++] = "-o";
    compile_runtime_argv[compile_runtime_argc++] = runtime_obj_path;
    compile_runtime_argv[compile_runtime_argc] = NULL;
#endif
    phase_start = compiler_now_seconds();
    if (using_prebuilt_runtime && !compiler_runtime_cache_is_fresh(runtime_obj_path)) {
        result->success = false;
        result->exit_code = 1;
        free(result->error_message);
        result->error_message = pergyra_strdup(
            "Prebuilt LLVM runtime object is stale or missing");
        result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
        free(runtime_obj_path);
        return result;
    }
    if (!using_prebuilt_runtime && !compiler_runtime_cache_is_fresh(runtime_obj_path)) {
        compiler_debug_llvm_host_stage("runtime_compile");
        int rc = pgy_exec_argv(compile_runtime_argv, verbose);
        compiled_runtime = true;
        if (rc != 0) {
            result->success = false;
            result->exit_code = rc;
            free(result->error_message);
            result->error_message = pergyra_strdup("LLVM runtime compilation failed");
            result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
            remove(runtime_obj_path);
            free(runtime_obj_path);
            return result;
        }
    }
    result->backend_timings.native_compile = compiler_now_seconds() - phase_start;

    compiler_debug_llvm_host_stage("link_begin");
    phase_start = compiler_now_seconds();
    int rc;
#ifdef _WIN32
    {
        const char *link_argv[18];
        int link_argc = 0;
        link_argv[link_argc++] = cc;
        if (cc_target != NULL)
            link_argv[link_argc++] = cc_target;
        link_argv[link_argc++] = "-std=c11";
        link_argv[link_argc++] = opt_flag;
        link_argv[link_argc++] = "-mconsole";
        link_argv[link_argc++] = "-DPGY_LLVM_ENABLED";
        link_argv[link_argc++] = "-I";
        link_argv[link_argc++] = PGY_SRC_DIR;
        link_argv[link_argc++] = "-o";
        link_argv[link_argc++] = output_binary_path;
        link_argv[link_argc++] = output_obj_path;
        link_argv[link_argc++] = runtime_obj_path;
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[link_argc++] = "-lm";
        link_argv[link_argc] = NULL;
        rc = pgy_exec_argv(link_argv, verbose);
    }
#else
    {
        const char *link_argv[22];
        int link_argc = 0;
        link_argv[link_argc++] = cc;
        if (cc_target != NULL)
            link_argv[link_argc++] = cc_target;
        link_argv[link_argc++] = "-std=c11";
        link_argv[link_argc++] = opt_flag;
        if (opt_profile == PGY_OPT_RELEASE) {
            link_argv[link_argc++] = "-march=native";
            link_argv[link_argc++] = "-mtune=native";
        }
        link_argv[link_argc++] = "-fopenmp";
        if (compiler_should_use_lld())
            link_argv[link_argc++] = "-fuse-ld=lld";
        link_argv[link_argc++] = "-no-pie";
        link_argv[link_argc++] = "-Wl,--build-id=none";
        link_argv[link_argc++] = "-DPGY_LLVM_ENABLED";
        link_argv[link_argc++] = "-I";
        link_argv[link_argc++] = PGY_SRC_DIR;
        link_argv[link_argc++] = "-o";
        link_argv[link_argc++] = output_binary_path;
        link_argv[link_argc++] = output_obj_path;
        link_argv[link_argc++] = runtime_obj_path;
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[link_argc++] = "-lm";
        link_argv[link_argc] = NULL;
        rc = pgy_exec_argv(link_argv, verbose);
    }
#endif
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup("LLVM link failed");
        result->backend_timings.link = compiler_now_seconds() - phase_start;
        if (compiled_runtime)
            remove(runtime_obj_path);
        free(runtime_obj_path);
        return result;
    }
    result->backend_timings.link = compiler_now_seconds() - phase_start;

    compiler_debug_llvm_host_stage("link_done");
    free(runtime_obj_path);
    compiler_debug_llvm_host_stage("return");
    return result;
}

#endif /* PGY_LLVM_ENABLED */

#ifndef PGY_LLVM_ENABLED

CompilerResult *
compiler_emit_llvm_ir(const CompilerIRBundle *bundle, const char *module_name)
{
    (void)bundle;
    (void)module_name;
    return compiler_error("LLVM backend not available in this build");
}

CompilerResult *
compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                              const char *module_name,
                              const char *output_ir_path)
{
    (void)bundle;
    (void)module_name;
    (void)output_ir_path;
    return compiler_error("LLVM backend not available in this build");
}

CompilerResult *
compiler_build_native_llvm(const CompilerIRBundle *bundle,
                           const char *output_obj_path,
                           const char *output_binary_path,
                           bool verbose,
                           PgyOptProfile opt_profile)
{
    (void)bundle;
    (void)output_obj_path;
    (void)output_binary_path;
    (void)verbose;
    (void)opt_profile;
    return compiler_error("LLVM backend not available in this build");
}

#endif /* !PGY_LLVM_ENABLED */

void
compiler_result_destroy(CompilerResult *result)
{
    if (result == NULL)
        return;

    free(result->error_message);
    free(result->c_output_path);
    free(result->binary_path);
    free(result);
}
