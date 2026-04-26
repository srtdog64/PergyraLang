#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 1
#endif

#ifndef PGY_RUNTIME_MAX_FILE_BYTES
#define PGY_RUNTIME_MAX_FILE_BYTES (64u * 1024u * 1024u)
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
extern int clock_gettime(int clk_id, struct timespec *tp);
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif
extern char *realpath(const char *path, char *resolved_path);
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "runtime/pgy_parallel.h"
#include "runtime/pgy_runtime_authority_contract.h"
#include "runtime/pgy_runtime_panic_contract.h"

static char *pgy_runtime_lib_strdup(const char *src);

_Thread_local bool pgy_zone_authority_last_ok = true;
_Thread_local char pgy_zone_authority_last_zone[128] = "";
_Thread_local char pgy_zone_authority_last_participant[128] = "";
_Thread_local char pgy_zone_authority_last_code[64] =
    PGY_ZONE_AUTHORITY_CODE_OK;
_Thread_local char pgy_zone_authority_last_reason[192] = "";

static void
pgy_runtime_record_zone_authority_last(bool ok,
                                       const char *zone_name,
                                       const char *participant_name,
                                       const char *code,
                                       const char *reason)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    pgy_zone_authority_last_ok = ok;
    snprintf(pgy_zone_authority_last_zone, sizeof(pgy_zone_authority_last_zone),
             "%s", resolved_zone);
    snprintf(pgy_zone_authority_last_participant,
             sizeof(pgy_zone_authority_last_participant),
             "%s", resolved_participant);
    snprintf(pgy_zone_authority_last_code, sizeof(pgy_zone_authority_last_code),
             "%s", code != NULL ? code
                                 : (ok ? PGY_ZONE_AUTHORITY_CODE_OK
                                       : PGY_ZONE_AUTHORITY_CODE_UNKNOWN));
    snprintf(pgy_zone_authority_last_reason, sizeof(pgy_zone_authority_last_reason),
             "%s", reason != NULL ? reason : "");
}

static bool
pgy_runtime_zone_authority_validate_core(void *zone_ptr, void *participant_ptr,
                                         const char *zone_name,
                                         const char *participant_name,
                                         bool emit_stderr)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (zone_ptr == NULL) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE,
            PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE,
                resolved_zone, resolved_participant);
        }
        return false;
    }
    if (participant_ptr == NULL) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
            PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT,
                resolved_zone, resolved_participant);
        }
        return false;
    }

    pgy_runtime_record_zone_authority_last(true, resolved_zone, resolved_participant,
        PGY_ZONE_AUTHORITY_CODE_OK, "");
    return true;
}

static bool
pgy_runtime_zone_authority_validate_token_core(void *zone_ptr,
                                               void *participant_ptr,
                                               int64_t expected_token,
                                               int64_t provided_token,
                                               const char *zone_name,
                                               const char *participant_name,
                                               bool emit_stderr)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (!pgy_runtime_zone_authority_validate_core(zone_ptr, participant_ptr,
            resolved_zone, resolved_participant, emit_stderr)) {
        return false;
    }

    if (expected_token <= 0 || provided_token <= 0
        || expected_token != provided_token) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone,
            resolved_participant, PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH,
            PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_TOKEN_MISMATCH,
                resolved_zone, resolved_participant);
        }
        return false;
    }

    pgy_runtime_record_zone_authority_last(true, resolved_zone,
        resolved_participant, PGY_ZONE_AUTHORITY_CODE_OK, "");
    return true;
}

static void
pgy_runtime_warn_invalid_channel(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][channel] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid channel operation");
}

static size_t
pgy_runtime_channel_capacity_or_default(const char *op, size_t cap)
{
    if (cap == 0) {
        pgy_runtime_warn_invalid_channel(op, "zero capacity requested; using capacity=1");
        return 1;
    }
    return cap;
}

static void
pgy_runtime_warn_invalid_intent_index(const char *op, int32_t index, int32_t count)
{
    fprintf(stderr, "[pgy][intent] %s: invalid index %d (count=%d)\n",
            op != NULL ? op : "<op>", (int)index, (int)count);
}

static void
pgy_runtime_warn_intent_enter_failure(const char *name, const char *reason,
                                      int32_t priority, bool is_concurrent)
{
    fprintf(stderr, "[pgy][intent] enter %s failed: %s (priority=%d concurrent=%s)\n",
            name != NULL ? name : "<intent>",
            reason != NULL ? reason : "unknown reason",
            (int)priority,
            is_concurrent ? "true" : "false");
}

static void
pgy_runtime_warn_invalid_collection(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][collection] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid collection operation");
}

static bool
pgy_runtime_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return false;
#ifdef _WIN32
    if ((path[0] == '/' || path[0] == '\\')
        || (isalpha((unsigned char)path[0]) && path[1] == ':'))
        return true;
#else
    if (path[0] == '/')
        return true;
#endif
    return false;
}

void
pgy_zone_authority_check_export(void *zone_ptr, void *participant_ptr,
                                const char *zone_name,
                                const char *participant_name)
{
    if (!pgy_runtime_zone_authority_validate_core(zone_ptr, participant_ptr,
            zone_name, participant_name, true)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,
                          PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);
    }
}

void
pgy_zone_authority_check_token_export(void *zone_ptr, void *participant_ptr,
                                      int64_t expected_token,
                                      int64_t provided_token,
                                      const char *zone_name,
                                      const char *participant_name)
{
    if (!pgy_runtime_zone_authority_validate_token_core(zone_ptr, participant_ptr,
            expected_token, provided_token, zone_name, participant_name, true)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,
                          PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);
    }
}

void
pgy_runtime_panic_internal_invariant_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      reason != NULL ? reason : "runtime invariant failed");
}

int32_t
pgy_checked_div_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs / rhs;
}

int64_t
pgy_checked_div_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs / rhs;
}

int32_t
pgy_checked_mod_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs % rhs;
}

int64_t
pgy_checked_mod_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs % rhs;
}

bool
pgy_zone_authority_validate_flags_export(bool has_zone, bool has_participant,
                                         char *zone_name,
                                         char *participant_name)
{
    void *zone_ptr = has_zone ? (void *)&pgy_zone_authority_last_ok : NULL;
    void *participant_ptr =
        has_participant ? (void *)pgy_zone_authority_last_zone : NULL;
    return pgy_runtime_zone_authority_validate_core(zone_ptr, participant_ptr,
        zone_name, participant_name, false);
}

bool
pgy_zone_authority_validate_token_flags_export(bool has_zone,
                                               bool has_participant,
                                               int64_t expected_token,
                                               int64_t provided_token,
                                               char *zone_name,
                                               char *participant_name)
{
    void *zone_ptr = has_zone ? (void *)&pgy_zone_authority_last_ok : NULL;
    void *participant_ptr =
        has_participant ? (void *)pgy_zone_authority_last_zone : NULL;
    return pgy_runtime_zone_authority_validate_token_core(zone_ptr,
        participant_ptr, expected_token, provided_token, zone_name,
        participant_name, false);
}

bool
pgy_zone_authority_last_ok_rt_export(void)
{
    return pgy_zone_authority_last_ok;
}

char *
pgy_zone_authority_last_zone_rt_export(void)
{
    return pgy_zone_authority_last_zone;
}

char *
pgy_zone_authority_last_participant_rt_export(void)
{
    return pgy_zone_authority_last_participant;
}

char *
pgy_zone_authority_last_code_rt_export(void)
{
    return pgy_zone_authority_last_code;
}

char *
pgy_zone_authority_last_reason_rt_export(void)
{
    return pgy_zone_authority_last_reason;
}

static bool
pgy_runtime_path_has_parent_ref(const char *path)
{
    const char *seg = path;

    if (path == NULL)
        return true;

    while (*seg != '\0') {
        const char *end = seg;
        while (*end != '\0' && *end != '/' && *end != '\\')
            end++;
        if ((size_t)(end - seg) == 2 && seg[0] == '.' && seg[1] == '.')
            return true;
        if (*end == '\0')
            break;
        seg = end + 1;
    }

    return false;
}

static bool
pgy_runtime_path_is_within_root(const char *path, const char *root)
{
    size_t root_len;

    if (path == NULL || root == NULL || root[0] == '\0')
        return false;

    root_len = strlen(root);
    if (strncmp(path, root, root_len) != 0)
        return false;
    return path[root_len] == '\0'
        || path[root_len] == '/'
        || path[root_len] == '\\';
}

static bool
pgy_runtime_file_path_allowed(const char *path)
{
    const char *root;
    const char *allow_abs;

    if (path == NULL || path[0] == '\0')
        return false;
    if (pgy_runtime_path_has_parent_ref(path))
        return false;

    root = getenv("PGY_IO_ROOT");
    if (root != NULL && root[0] != '\0') {
        if (pgy_runtime_path_is_absolute(path))
            return pgy_runtime_path_is_within_root(path, root);
        return true;
    }

    if (!pgy_runtime_path_is_absolute(path))
        return true;

    allow_abs = getenv("PGY_IO_ALLOW_ABSOLUTE");
    return allow_abs != NULL && strcmp(allow_abs, "1") == 0;
}

static char *
pgy_runtime_path_join_dup(const char *dir, const char *path)
{
    size_t dlen;
    size_t plen;
    bool needs_sep;
    char *result;

    if (dir == NULL || path == NULL)
        return NULL;

    dlen = strlen(dir);
    plen = strlen(path);
    needs_sep = dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\';
    result = (char *)malloc(dlen + (needs_sep ? 1 : 0) + plen + 1);
    if (result == NULL)
        return NULL;

    memcpy(result, dir, dlen);
    if (needs_sep)
        result[dlen++] = '/';
    memcpy(result + dlen, path, plen + 1);
    return result;
}

#ifdef _WIN32
static void
pgy_runtime_normalize_path_separators(char *path)
{
    if (path == NULL)
        return;
    while (*path != '\0') {
        if (*path == '\\')
            *path = '/';
        path++;
    }
}
#endif

static char *
pgy_runtime_path_dirname_dup(const char *path)
{
    const char *last_sep;
    const char *last_bsep;
    size_t len;
    char *dir;

    if (path == NULL)
        return NULL;

    last_sep = strrchr(path, '/');
    last_bsep = strrchr(path, '\\');
    if (last_bsep != NULL && (last_sep == NULL || last_bsep > last_sep))
        last_sep = last_bsep;

    if (last_sep == NULL)
        return pgy_runtime_lib_strdup(".");

    len = (size_t)(last_sep - path);
    dir = (char *)malloc(len + 1);
    if (dir == NULL)
        return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static bool
pgy_runtime_prefix_match_path(const char *root, const char *path)
{
    if (root == NULL || path == NULL)
        return false;
    {
        size_t i = 0;
        while (root[i] != '\0') {
            char a = root[i];
            char b = path[i];
#ifdef _WIN32
            if (a == '\\')
                a = '/';
            if (b == '\\')
                b = '/';
            if (tolower((unsigned char)a) != tolower((unsigned char)b))
                return false;
#else
            if (a != b)
                return false;
#endif
            i++;
        }
        return path[i] == '\0'
            || path[i] == '/'
            || path[i] == '\\';
    }
}

static char *
pgy_runtime_normalize_full_path_dup(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return NULL;
#ifdef _WIN32
    DWORD needed = GetFullPathNameA(path, 0, NULL, NULL);
    char *buf;

    if (needed == 0)
        return NULL;
    buf = (char *)malloc((size_t)needed + 2);
    if (buf == NULL)
        return NULL;
    if (GetFullPathNameA(path, needed + 1, buf, NULL) == 0) {
        free(buf);
        return NULL;
    }
    pgy_runtime_normalize_path_separators(buf);
    return buf;
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL)
        return NULL;
    return pgy_runtime_lib_strdup(resolved);
#endif
}

#ifdef _WIN32
static bool
pgy_runtime_path_has_reparse_component(const char *path)
{
    char *full;
    size_t len;
    size_t i;

    full = pgy_runtime_normalize_full_path_dup(path);
    if (full == NULL)
        return true;

    len = strlen(full);
    for (i = 0; i <= len; ++i) {
        char saved = full[i];
        DWORD attrs;

        if (saved != '/' && saved != '\0')
            continue;
        full[i] = '\0';
        if (full[0] != '\0'
            && !(isalpha((unsigned char)full[0]) && full[1] == ':' && full[2] == '\0')) {
            attrs = GetFileAttributesA(full);
            if (attrs != INVALID_FILE_ATTRIBUTES
                && (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                free(full);
                return true;
            }
        }
        full[i] = saved;
    }

    free(full);
    return false;
}
#endif

static char *
pgy_runtime_resolve_file_path(const char *path, bool for_write)
{
    const char *root = getenv("PGY_IO_ROOT");
    const char *allow_abs;
    char *candidate = NULL;

    if (!pgy_runtime_file_path_allowed(path))
        return NULL;

    if (root != NULL && root[0] != '\0') {
        if (pgy_runtime_path_is_absolute(path))
            candidate = pgy_runtime_lib_strdup(path);
        else
            candidate = pgy_runtime_path_join_dup(root, path);
    } else if (pgy_runtime_path_is_absolute(path)) {
        allow_abs = getenv("PGY_IO_ALLOW_ABSOLUTE");
        if (allow_abs == NULL || strcmp(allow_abs, "1") != 0)
            return NULL;
        candidate = pgy_runtime_lib_strdup(path);
    } else {
        candidate = pgy_runtime_lib_strdup(path);
    }

#ifndef _WIN32
    if (candidate != NULL && root != NULL && root[0] != '\0') {
        char *root_real;
        char *check_real;
        char *check_path = NULL;

        root_real = pgy_runtime_normalize_full_path_dup(root);
        if (root_real == NULL) {
            free(candidate);
            return NULL;
        }

        if (for_write)
            check_path = pgy_runtime_path_dirname_dup(candidate);
        else
            check_path = pgy_runtime_lib_strdup(candidate);

        check_real = check_path != NULL
            ? pgy_runtime_normalize_full_path_dup(check_path)
            : NULL;

        if (check_real == NULL
            || !pgy_runtime_prefix_match_path(root_real, check_real)) {
            free(check_path);
            free(check_real);
            free(root_real);
            free(candidate);
            return NULL;
        }

        free(check_path);
        free(check_real);
        free(root_real);
    }
#else
    if (candidate != NULL && root != NULL && root[0] != '\0') {
        char *root_real = pgy_runtime_normalize_full_path_dup(root);
        char *check_path = NULL;
        char *check_real = NULL;
        char *candidate_real = NULL;

        if (root_real == NULL) {
            free(candidate);
            return NULL;
        }

        if (for_write)
            check_path = pgy_runtime_path_dirname_dup(candidate);
        else
            check_path = pgy_runtime_lib_strdup(candidate);

        check_real = check_path != NULL
            ? pgy_runtime_normalize_full_path_dup(check_path)
            : NULL;
        candidate_real = pgy_runtime_normalize_full_path_dup(candidate);

        if (check_real == NULL
            || candidate_real == NULL
            || !pgy_runtime_prefix_match_path(root_real, check_real)
            || pgy_runtime_path_has_reparse_component(check_path)) {
            free(candidate_real);
            free(check_real);
            free(check_path);
            free(root_real);
            free(candidate);
            return NULL;
        }

        free(candidate);
        candidate = candidate_real;
        free(check_path);
        free(check_real);
        free(root_real);
    } else if (candidate != NULL && pgy_runtime_path_is_absolute(candidate)) {
        char *normalized = pgy_runtime_normalize_full_path_dup(candidate);
        if (normalized == NULL) {
            free(candidate);
            return NULL;
        }
        free(candidate);
        candidate = normalized;
    }
#endif

    return candidate;
}
