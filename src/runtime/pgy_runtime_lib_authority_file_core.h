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

#include "pgy_runtime_lib_file_path_core.h"
