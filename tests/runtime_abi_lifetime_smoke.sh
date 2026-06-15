#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-runtime-abi-lifetime.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

fail() {
    echo "[runtime-abi-lifetime] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing runtime ABI lifetime source: $rel"
}

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq "$term" "$ROOT_DIR/$rel" || fail "$rel missing term: $term"
}

concat_runtime_text() {
    local out="$1"
    shift
    : > "$out"
    local rel
    for rel in "$@"; do
        require_file "$rel"
        cat "$ROOT_DIR/$rel" >> "$out"
        printf '\n' >> "$out"
    done
}

extract_function_body() {
    local source="$1"
    local name="$2"
    awk -v fn="$name" '
        BEGIN { in_body = 0; depth = 0; seen_open = 0 }
        {
            if (!in_body && $0 ~ ("(^|[^A-Za-z0-9_])" fn "[[:space:]]*\\(")) {
                in_body = 1
            }
            if (in_body) {
                print
                for (i = 1; i <= length($0); i++) {
                    ch = substr($0, i, 1)
                    if (ch == "{") { depth++; seen_open = 1 }
                    else if (ch == "}") { depth-- }
                }
                if (seen_open && depth <= 0)
                    exit
            }
        }
    ' "$source"
}

extract_macro_body() {
    local source="$1"
    local macro="$2"
    awk -v macro="$macro" '
        BEGIN { in_body = 0 }
        $0 ~ ("^#define[[:space:]]+" macro) { in_body = 1 }
        in_body {
            print
            if ($0 !~ /\\$/)
                exit
        }
    ' "$source"
}

forbidden_regex='(^|[^A-Za-z0-9_])(malloc|calloc|realloc|free|strdup|pgy_runtime_strdup|pgy_runtime_lib_strdup|pgy_intent_copy_string)([^A-Za-z0-9_]|$)'

check_borrowed_exports() {
    local label="$1"
    local source="$2"
    shift 2
    local fn body
    for fn in "$@"; do
        body="$(extract_function_body "$source" "$fn")"
        [[ -n "$body" ]] || fail "$label missing runtime ABI string export function $fn"
        grep -Fq "return" <<< "$body" || fail "$label:$fn has no return statement"
        if grep -Eq "$forbidden_regex" <<< "$body"; then
            fail "$label:$fn performs ownership-changing work"
        fi
    done
}

check_macro_borrowed_exports() {
    local label="$1"
    local source="$2"
    local macro="$3"
    shift 3
    local body fn
    body="$(extract_macro_body "$source" "$macro")"
    [[ -n "$body" ]] || fail "$label missing $macro macro"
    grep -Fq "return result" <<< "$body" ||
        fail "$label:$macro has no borrowed return"
    if grep -Eq "$forbidden_regex" <<< "$body"; then
        fail "$label:$macro performs ownership-changing work"
    fi
    for fn in "$@"; do
        grep -Fq "$macro($fn," "$source" ||
            fail "$label missing borrowed string macro export $fn"
    done
}

check_result_owned_strings() {
    local label="$1"
    local source="$2"
    local dup_helper="$3"
    shift 3
    local fn body
    for fn in "$@"; do
        body="$(extract_function_body "$source" "$fn")"
        [[ -n "$body" ]] || fail "$label missing result-owned string function $fn"
        if ! grep -Fq "$dup_helper" <<< "$body" \
            && ! grep -Fq "pgy_runtime_strdup_export" <<< "$body" \
            && ! grep -Fq "malloc" <<< "$body"; then
            fail "$label:$fn does not allocate/copy a result-owned string"
        fi
        for bad in 'return ""' 'return s;' 'return tmp;' 'return stack_buf;' 'return resolved;'; do
            if grep -Fq "$bad" <<< "$body"; then
                fail "$label:$fn returns borrowed/stack data in result-owned ABI: $bad"
            fi
        done
    done
}

check_result_owned_arrays() {
    local label="$1"
    local source="$2"
    local dup_helper="$3"
    shift 3
    local fn body
    for fn in "$@"; do
        body="$(extract_function_body "$source" "$fn")"
        [[ -n "$body" ]] || fail "$label missing result-owned array function $fn"
        if ! grep -Fq "PgyArray_String" <<< "$body" \
            && ! grep -Fq "pgy_array_new_String" <<< "$body"; then
            fail "$label:$fn does not materialize a string array payload"
        fi
        if ! grep -Fq "$dup_helper" <<< "$body" \
            && ! grep -Fq "pgy_runtime_strdup_export" <<< "$body" \
            && ! grep -Fq "malloc" <<< "$body"; then
            fail "$label:$fn does not allocate/copy string array payloads"
        fi
        if grep -Fq "pgy_array_push_String(&result, s)" <<< "$body" \
            || grep -Fq "pgy_array_push_String(&result, p)" <<< "$body"; then
            fail "$label:$fn pushes borrowed source strings into result-owned array"
        fi
    done
}

inline_text="$TMP_DIR/inline_runtime.txt"
llvm_text="$TMP_DIR/llvm_runtime.txt"
inline_string_text="$TMP_DIR/inline_string.txt"
llvm_string_text="$TMP_DIR/llvm_string.txt"

concat_runtime_text "$inline_text" \
    "src/runtime/pgy_runtime_intent_trace_inline.h" \
    "src/runtime/pgy_runtime_intent_active_exports.h" \
    "src/runtime/pgy_runtime_intent_history.h" \
    "src/runtime/pgy_runtime_intent_exit.h" \
    "src/runtime/pgy_runtime_intent_query_inline.h" \
    "src/runtime/pgy_runtime_panic_checked_inline.h" \
    "src/runtime/pgy_runtime_memory_array_slot_inline.h" \
    "src/runtime/pgy_runtime_slot_macros.h" \
    "src/runtime/pgy_runtime_channel_inline.h" \
    "src/runtime/pgy_runtime_scalar_std_inline.h" \
    "src/runtime/pgy_runtime_builtin_storage_inline.h" \
    "src/runtime/pgy_runtime_list_set_inline.h" \
    "src/runtime/pgy_runtime_pool_fsm_timer_inline.h" \
    "src/runtime/pgy_runtime_zone_result_option_inline.h"

concat_runtime_text "$llvm_text" \
    "src/runtime/pgy_runtime_lib_core_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_collection_common_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_collection_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_map_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_map_key_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_queue_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_set_exports.h" \
    "src/runtime/pgy_runtime_lib_set_intent_trace_exports.h" \
    "src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" \
    "src/runtime/pgy_runtime_lib_intent_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_recent_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" \
    "src/runtime/pgy_runtime_lib_slot_exports.h" \
    "src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_secure_slot_exports.h" \
    "src/runtime/pgy_runtime_lib_device_slot_exports.h" \
    "src/runtime/pgy_runtime_lib_array_map_exports.h" \
    "src/runtime/pgy_runtime_lib_array_set_exports.h" \
    "src/runtime/pgy_runtime_lib_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_std_exports.h" \
    "src/runtime/pgy_runtime_lib_channel_quantum_exports.h" \
    "src/runtime/pgy_runtime_lib_quantum_exports.h" \
    "src/runtime/pgy_runtime_lib_authority_file_core.h"

concat_runtime_text "$inline_string_text" \
    "src/runtime/pgy_runtime_io_qubit_inline.h"

concat_runtime_text "$llvm_string_text" \
    "src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_array_map_exports.h" \
    "src/runtime/pgy_runtime_lib_array_set_exports.h" \
    "src/runtime/pgy_runtime_lib_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_std_exports.h" \
    "src/runtime/pgy_runtime_lib_channel_quantum_exports.h"

check_borrowed_exports "inline-intent" "$inline_text" \
    pgy_intent_last_trace_export \
    pgy_intent_last_failure_export \
    pgy_intent_last_name_export \
    pgy_intent_active_name_export \
    pgy_intent_active_trace_export \
    pgy_intent_active_failure_export \
    pgy_intent_active_step_name_export \
    pgy_intent_recent_name_export \
    pgy_intent_recent_trace_export \
    pgy_intent_recent_failure_export \
    pgy_zone_authority_last_zone_export \
    pgy_zone_authority_last_participant_export \
    pgy_zone_authority_last_code_export \
    pgy_zone_authority_last_reason_export \
    pgy_zone_authority_last_zone_rt_export \
    pgy_zone_authority_last_participant_rt_export \
    pgy_zone_authority_last_code_rt_export \
    pgy_zone_authority_last_reason_rt_export

check_borrowed_exports "llvm-intent-authority" "$llvm_text" \
    pgy_intent_last_trace_export \
    pgy_intent_last_failure_export \
    pgy_intent_last_name_export \
    pgy_intent_active_name_export \
    pgy_intent_active_trace_export \
    pgy_intent_active_failure_export \
    pgy_intent_active_step_name_export \
    pgy_intent_recent_name_export \
    pgy_intent_recent_trace_export \
    pgy_intent_recent_failure_export \
    pgy_zone_authority_last_zone_rt_export \
    pgy_zone_authority_last_participant_rt_export \
    pgy_zone_authority_last_code_rt_export \
    pgy_zone_authority_last_reason_rt_export

check_macro_borrowed_exports "inline-intent-active-step" "$inline_text" \
    PGY_INTENT_ACTIVE_STEP_STRING_EXPORT \
    pgy_intent_active_step_zone_export \
    pgy_intent_active_step_phase_export \
    pgy_intent_active_step_participant_export \
    pgy_intent_active_step_slot_export \
    pgy_intent_active_step_from_zone_export \
    pgy_intent_active_step_from_slot_export \
    pgy_intent_active_step_to_zone_export \
    pgy_intent_active_step_to_slot_export \
    pgy_intent_active_step_failure_export

check_macro_borrowed_exports "llvm-intent-active-step" "$llvm_text" \
    PGY_INTENT_ACTIVE_STEP_STRING_EXPORT \
    pgy_intent_active_step_zone_export \
    pgy_intent_active_step_phase_export \
    pgy_intent_active_step_participant_export \
    pgy_intent_active_step_slot_export \
    pgy_intent_active_step_from_zone_export \
    pgy_intent_active_step_from_slot_export \
    pgy_intent_active_step_to_zone_export \
    pgy_intent_active_step_failure_export

for term in \
    "pgy_intent_borrowed_snapshot(" \
    "static _Thread_local char *snapshots" \
    "pgy_intent_last_trace_export" \
    "pthread_mutex_lock(&pgy_intent_registry_mutex)" \
    "pgy_intent_active_entry_by_index_locked_export" \
    "pgy_intent_recent_entry_by_index_locked_export" \
    "pgy_intent_active_step_by_handle_locked_export" \
    "pgy_intent_find_active_entry_locked(" \
    "pgy_intent_history_step_name_export" \
    "pgy_intent_recent_name_export"; do
    grep -Fq "$term" "$inline_text" ||
        fail "inline intent borrowed query snapshots must be mutex-backed: $term"
done
for term in \
    "result = entry->name;" \
    "result = entry->trace;" \
    "result = entry->failure_reason;" \
    "result = step->name;" \
    "pgy_intent_active_entry_by_index_export(" \
    "pgy_intent_recent_entry_by_index_export(" \
    "pgy_intent_active_step_by_index_export(" \
    "pgy_intent_find_active_entry(" \
    "return pgy_intent_last_trace != NULL" \
    "return pgy_intent_last_steps[index]"; do
    if grep -Fq "$term" "$inline_text"; then
        fail "inline intent borrowed query must not return raw registry pointers: $term"
    fi
done
for term in \
    "pgy_intent_borrowed_snapshot_export(" \
    "static _Thread_local char *snapshots" \
    "pgy_intent_last_trace_export" \
    "pthread_mutex_lock(&pgy_intent_registry_mutex)" \
    "pgy_intent_active_entry_by_index_locked_export" \
    "pgy_intent_recent_entry_by_index_locked_export" \
    "pgy_intent_active_step_by_handle_locked_export" \
    "pgy_intent_find_active_entry_locked_export(" \
    "pgy_intent_history_step_name_export" \
    "pgy_intent_recent_name_export"; do
    grep -Fq "$term" "$llvm_text" ||
        fail "LLVM intent borrowed query snapshots must be mutex-backed: $term"
done
for term in \
    "result = entry->name;" \
    "result = entry->trace;" \
    "result = entry->failure_reason;" \
    "result = step->name;" \
    "pgy_intent_active_entry_by_index_export(" \
    "pgy_intent_recent_entry_by_index_export(" \
    "pgy_intent_active_step_by_index_export(" \
    "pgy_intent_find_active_entry_export(" \
    "return pgy_intent_last_trace != NULL" \
    "return pgy_intent_last_steps[index]"; do
    if grep -Fq "$term" "$llvm_text"; then
        fail "LLVM intent borrowed query must not return raw registry pointers: $term"
    fi
done

check_result_owned_strings "inline-string-helpers" "$inline_string_text" \
    pgy_runtime_strdup \
    pgy_input Substring StringReplace StringTrim ToUpper ToLower StringConcat StringJoin

check_result_owned_strings "llvm-string-helpers" "$llvm_string_text" \
    pgy_runtime_lib_strdup \
    pgy_file_read pgy_read_file pgy_input Substring StringReplace StringTrim \
    ToUpper ToLower StringConcat StringJoin

check_result_owned_strings "inline-scalar-string-helpers" "$inline_text" \
    pergyra_strdup_printf \
    pgy_int_to_string pgy_long_to_string pgy_float_to_string pgy_double_to_string

check_result_owned_strings "llvm-scalar-string-helpers" "$llvm_text" \
    pergyra_strdup_printf \
    pgy_int_to_string pgy_long_to_string pgy_float_to_string pgy_double_to_string

check_result_owned_arrays "inline-string-array-helpers" "$inline_string_text" \
    pgy_runtime_strdup StringSplit

check_result_owned_arrays "llvm-string-array-helpers" "$llvm_string_text" \
    pgy_runtime_lib_strdup StringSplit pgy_map_keys_raw_export

for term in \
    "PGY_ARRAY_DEFINE(String, char*)" \
    "arr->data[arr->length++] = value" \
    "arr->data[index] = value"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_memory_array_slot_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_builtin_storage_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
        fail "generic Array<String> must remain pointer-storage unless a producer explicitly copies payloads: $term"
done
for term in \
    "array push on null array" \
    "array get on array without backing storage" \
    "slice get on slice without backing storage" \
    "RcClone strong count overflow" \
    "BoxArray capacity overflow on drop"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_memory_array_slot_inline.h" ||
        fail "memory array/rc inline null/range guard missing: $term"
done
for term in \
    "array get on array without backing storage" \
    "array set on array without backing storage" \
    "slice get on slice without backing storage"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
        fail "raw array export backing-storage guard missing: $term"
done
if grep -Fq "pgy_array_push_String(&result, s)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h"; then
    fail "StringSplit must not push borrowed input strings into result-owned arrays"
fi
for term in \
    "PgyArray_String result = pgy_array_new_String(8)" \
    "pgy_array_push_String(&result, pgy_runtime_strdup(s))" \
    "pgy_array_push_String(&result, pgy_runtime_strdup(p))"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" ||
        fail "inline StringSplit must match result-owned LLVM runtime contract; missing $term"
done
for term in \
    "PgyArray_String result = pgy_array_new_String(8)" \
    "pgy_array_push_String(&result, pgy_runtime_lib_strdup(s))" \
    "pgy_array_push_String(&result, pgy_runtime_lib_strdup(p))"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h" ||
        fail "LLVM StringSplit must keep result-owned string tokens; missing $term"
done
grep -Fq '{ "Split", "stdlib string", "StringSplit", 2 }' \
    "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" ||
    fail "LLVM Split alias must lower through the StringSplit runtime export"
grep -Fq '{ "StringSplit", ctx->array_type_String,' \
    "$ROOT_DIR/src/codegen/llvm_runtime_core_builtin_decl.c" ||
    fail "LLVM runtime registry must declare StringSplit as Array<String>"
grep -Fq "dup_key = pgy_runtime_strdup" \
    "$ROOT_DIR/src/runtime/pgy_runtime_map_string_inline.h" ||
    fail "inline MapKeys<String> must duplicate keys before pushing into Array<String>"
grep -Fq "dup_value = pgy_runtime_strdup" \
    "$ROOT_DIR/src/runtime/pgy_runtime_list_set_inline.h" ||
    fail "inline SetValues<String> must duplicate values before pushing into Array<String>"
for term in \
    "pgy_set_values_raw_string_export" \
    "pgy_array_push_String(out, value != NULL ? value : \"\")"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_set_exports.h" ||
        fail "LLVM SetValues<String> must keep result-owned string values; missing $term"
done

file_open_body="$(extract_function_body "$llvm_string_text" pgy_try_file_open_result)"
file_close_body="$(extract_function_body "$llvm_string_text" pgy_file_close)"
[[ -n "$file_open_body" ]] || fail "missing pgy_try_file_open_result"
[[ -n "$file_close_body" ]] || fail "missing pgy_file_close"
for term in \
    "resolved = pgy_runtime_resolve_file_path(path, for_write)" \
    "free(resolved)" \
    "for (int i = 3; i < PGY_MAX_OPEN_FILES; i++)" \
    "pgy_runtime_ftable[i] == NULL" \
    "pthread_mutex_lock(&pgy_runtime_ftable_mutex)" \
    "fd = i" \
    "pgy_runtime_ftable[fd] = fp"; do
    grep -Fq "$term" <<< "$file_open_body" ||
        fail "pgy_file_open must reuse closed runtime-owned handle slots; missing $term"
done
if grep -Fq "ftable_next" \
    "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h"; then
    fail "runtime file handle allocation must not keep a dead ftable_next cursor"
fi
grep -Fq "pgy_runtime_ftable[fd] = NULL" <<< "$file_close_body" ||
    fail "pgy_file_close must release the runtime-owned handle slot"
grep -Fq "pthread_mutex_lock(&pgy_runtime_ftable_mutex)" <<< "$file_close_body" ||
    fail "pgy_file_close must guard the runtime-owned handle table"
for term in \
    "pgy_runtime_resolve_file_path(path, for_write)" \
    "free(resolved)"; do
    grep -Fq "$term" \
        "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h" ||
        fail "FileOpen must share resolved path ownership on both runtime surfaces; missing $term"
done
for term in \
    "static pthread_mutex_t pgy_runtime_ftable_mutex = PTHREAD_MUTEX_INITIALIZER" \
    "pthread_mutex_lock(&_pgy_ftable_mutex)" \
    "pthread_mutex_lock(&pgy_runtime_ftable_mutex)"; do
    grep -Fq "$term" \
        "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h" ||
        fail "runtime file handle table must be mutex-protected; missing $term"
done

read_file_body="$(extract_function_body "$llvm_string_text" pgy_try_read_file_result)"
write_file_body="$(extract_function_body "$llvm_string_text" pgy_try_write_file_result)"
[[ -n "$read_file_body" ]] || fail "missing pgy_try_read_file_result"
[[ -n "$write_file_body" ]] || fail "missing pgy_try_write_file_result"
grep -Fq "free(resolved)" <<< "$read_file_body" ||
    fail "pgy_try_read_file_result must release resolved paths on all result-owned exits"
grep -Fq "free(resolved)" <<< "$write_file_body" ||
    fail "pgy_try_write_file_result must release resolved paths on all exits"

# Pool object handles share the runtime-owned handle contract: pgy_pool_spawn
# returns a numeric int32_t slot index that the runtime owns until
# pgy_pool_despawn releases it. Like file descriptors, the slot allocator must
# reuse freed slots (no monotonic-growth bug) and the release surface must
# clear the alive flag through a validated index check.
pool_text="$TMP_DIR/pool_text.txt"
concat_runtime_text "$pool_text" \
    "src/runtime/pgy_runtime_pool_fsm_timer_inline.h"
pool_spawn_body="$(extract_function_body "$pool_text" pgy_pool_spawn)"
pool_despawn_body="$(extract_function_body "$pool_text" pgy_pool_despawn)"
pool_get_body="$(extract_function_body "$pool_text" pgy_pool_get)"
[[ -n "$pool_spawn_body" ]] || fail "missing pgy_pool_spawn"
[[ -n "$pool_despawn_body" ]] || fail "missing pgy_pool_despawn"
[[ -n "$pool_get_body" ]] || fail "missing pgy_pool_get"
for term in \
    "p == NULL || item == NULL || p->data == NULL || p->alive == NULL" \
    "for (size_t i = 0; i < p->capacity; i++)" \
    "if (!p->alive[i])" \
    "p->alive[i] = 1" \
    "p->count++" \
    "return (int32_t)i" \
    "return -1"; do
    grep -Fq "$term" <<< "$pool_spawn_body" ||
        fail "pgy_pool_spawn must reuse freed slots as runtime-owned handles; missing $term"
done
for term in \
    "p == NULL || p->alive == NULL" \
    "(size_t)index < p->capacity && p->alive[index]" \
    "p->alive[index] = 0" \
    "p->count--"; do
    grep -Fq "$term" <<< "$pool_despawn_body" ||
        fail "pgy_pool_despawn must release the runtime-owned handle slot; missing $term"
done
for term in \
    "p == NULL || p->data == NULL || p->alive == NULL" \
    "(size_t)index >= p->capacity || !p->alive[index]"; do
    grep -Fq "$term" <<< "$pool_get_body" ||
        fail "pgy_pool_get must validate the runtime-owned handle before access; missing $term"
done

for term in \
    "raw_len > (size_t)INT32_MAX" \
    "size_t count = 0" \
    "result_len = source_len + count * delta" \
    "if (len > slen - start)" \
    "if (new_len > old_len)" \
    "result_len = source_len" \
    "if (la > ((size_t)-1) - lb || la + lb > ((size_t)-1) - 1)"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_io_string_exports.h" ||
        fail "LLVM string helper missing overflow/lifetime guard: $term"
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" ||
        fail "inline string helper missing overflow/lifetime guard: $term"
done

grep -Fq "if (item_len > ((size_t)-1) - total)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_std_exports.h" ||
    fail "LLVM StringJoin must guard result length overflow"
grep -Fq "if (sl > ((size_t)-1) - total)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_io_qubit_inline.h" ||
    fail "inline StringJoin must guard result length overflow"
grep -Fq "static PGY_RUNTIME_NOINLINE char *pgy_runtime_strdup" \
    "$ROOT_DIR/src/runtime/pgy_runtime_platform_io_core.h" ||
    fail "runtime string duplicate declaration must remain noinline for MinGW O3 Array<String> stability"
grep -Fq "static PGY_RUNTIME_NOINLINE char *" \
    "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" ||
    fail "runtime string duplicate definition must remain noinline for MinGW O3 Array<String> stability"
grep -Fq "pgy_runtime_strdup(const char *src)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" ||
    fail "runtime string duplicate noinline definition must stay attached to pgy_runtime_strdup"
grep -Fq "if (start > arr->length || len > arr->length - start)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
    fail "LLVM array slice export must avoid start+len overflow"
grep -Fq "slice.data = NULL;" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
    fail "LLVM array slice export must initialize zero-length slices as null-backed"
grep -Fq "if (len == 0)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
    fail "LLVM array slice export must return before deriving backing pointers for empty slices"
grep -Fq "slice.data = len == 0 ? NULL : arr->data + start" \
    "$ROOT_DIR/src/runtime/pgy_runtime_memory_array_slot_inline.h" ||
    fail "inline array slice helper must keep empty slices null-backed"
grep -Fq "pgy_slice_copy_##Suffix" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
    fail "LLVM slice copy export must remain available for SliceCopy"
grep -Fq "pgy_slice_copy_##SuffixName" \
    "$ROOT_DIR/src/runtime/pgy_runtime_memory_array_slot_inline.h" ||
    fail "inline slice copy helper must remain available for SliceCopy"
grep -Fq "PGY_ARRAY_COPY_VALUE_String(value)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_memory_array_slot_inline.h" ||
    fail "inline SliceCopy(String) must duplicate string payloads"
grep -Fq "PGY_ARRAY_EXPORT_COPY_VALUE_String(value)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h" ||
    fail "LLVM SliceCopy(String) export must duplicate string payloads"
grep -Fq "\"slice_copy\"" "$ROOT_DIR/src/codegen/llvm_runtime.c" ||
    fail "LLVM runtime declarations must register pgy_slice_copy_<T>"
grep -Fq "_pgy_start_%d > _pgy_slice_%d.length || _pgy_len_%d > _pgy_slice_%d.length - _pgy_start_%d" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" ||
    fail "C backend generated Slice<T>.Slice code must avoid start+len overflow"
grep -Fq "PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" ||
    fail "C backend generated Slice<T>.Slice code must use out-of-bounds panic class"
if grep -Fq "_pgy_start_%d + _pgy_len_%d > _pgy_slice_%d.length" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"; then
    fail "C backend generated Slice<T>.Slice code reintroduced start+len overflow"
fi
for term in \
    "pgy_queue_push_string_raw_export" \
    "pgy_runtime_strdup_export(value != NULL ? value : \"\")" \
    "pgy_queue_pop_string_raw_export"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_queue_exports.h" ||
        fail "raw Queue<String> export missing result-owned string term: $term"
done
grep -Fq "\"pgy_queue_push_string_raw_export\"" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c" ||
    fail "LLVM Queue<String> push must use string-owned raw queue export"
grep -Fq "\"pgy_queue_pop_string_raw_export\"" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c" ||
    fail "LLVM Queue<String> pop must use string-owned raw queue export"
for term in \
    "pgy_channel_dup_String" \
    "pgy_channel_free_pending_String" \
    "pgy_channel_string_is_initialized" \
    "pgy_channel_string_size_to_i32" \
    "channel is not initialized" \
    "ch->buffer[ch->head] = NULL"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_string_exports.h" ||
        fail "LLVM Channel<String> export missing owned-transfer term: $term"
done
for term in \
    "pgy_channel_int_is_initialized" \
    "pgy_channel_int_size_to_i32" \
    "size_t used = ch->count <= ch->capacity ? ch->count : ch->capacity"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_int_exports.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_string_exports.h" ||
        fail "exported Channel<Int/String> initialized/range guard missing: $term"
done
for term in \
    "pgy_map_set_string_value_raw_export" \
    "pgy_map_get_string_value_raw_export" \
    "pgy_map_remove_string_value_raw_export" \
    "pgy_runtime_strdup_export(value != NULL ? value : \"\")"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_map_exports.h" ||
        fail "raw HashMap<K,String> export missing owned string-value term: $term"
done
for term in \
    "pgy_hashmap_key_raw_string_value_export_name" \
    "pgy_map_set_string_value_raw_i32_export" \
    "pgy_map_get_string_value_raw_bool_export"; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen"/*.c "$ROOT_DIR/src/codegen"/*.h ||
        fail "LLVM HashMap<K,String> lowering missing string-value export term: $term"
done
for term in \
    "llvm_constructed_arg_name_copy" \
    "llvm_constructed_arg_name_write" \
    "out[0] = '\\0'" \
    "return llvm_constructed_arg_name_write(type_name, arg_index, out, out_size)"; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c" \
        "$ROOT_DIR/src/codegen/llvm_internal_api.h" ||
        fail "LLVM constructed type arg helper must expose caller-owned copy seam: $term"
done
grep -Fq "llvm_required_constructed_arg_name_copy" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" ||
    fail "LLVM type map must consume constructed generic args through caller-owned copy seam"
if grep -Fq "llvm_constructed_arg_name_at(type_name" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"; then
    fail "LLVM type map must not retain static constructed generic arg pointers"
fi
if grep -Fq "llvm_constructed_arg_name_at(const char" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_render.c"; then
    fail "LLVM constructed type arg helper must not expose static-return compatibility seam"
fi
for term in \
    "argc > (size_t)UINT_MAX" \
    "argc > SIZE_MAX / sizeof(LLVMValueRef)" \
    "argument count exceeds backend ABI limits"; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_expr_call_args.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c" ||
        fail "LLVM argument lowering must guard arena sizing and unsigned ABI arity: $term"
done
for term in \
    "arg_count > (size_t)UINT_MAX - 1U" \
    "arg_count > (SIZE_MAX / sizeof(LLVMValueRef)) - 1U" \
    "param_count > SIZE_MAX / sizeof(LLVMTypeRef)"; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c" ||
        fail "LLVM event/callable lowering must guard arena sizing and unsigned ABI arity: $term"
done
for term in \
    "pgy_arena_fmt(&ctx->scratch" \
    "ctx->tmp_counter++"; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_backend_generic.c" ||
        fail "LLVM tmp name helper must use the context scratch arena: $term"
done
for term in \
    "static char bufs[8][32]" \
    "slot++ %"; do
    if grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_backend_generic.c"; then
        fail "LLVM tmp name helper regressed to mutable static ring storage: $term"
    fi
done
for term in \
    "role count is nonzero but role array is null" \
    "party_context_role_abilities_valid" \
    "ability count is nonzero but ability array is null" \
    "shared field count is nonzero but shared field array is null" \
    "party_stats_add_u64" \
    "stats->totalExecutions != UINT64_MAX" \
    "stats->errorCount != UINT32_MAX" \
    "g_fiberStatsMutex" \
    "fiber_stats_rebuild_index" \
    "fiber_stats_lookup(roleId)"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/party_runtime.c" \
        "$ROOT_DIR/src/runtime/party_runtime_stats.c" ||
        fail "party runtime context/stat guards missing: $term"
done
for term in \
    "atomic_bool completed" \
    "atomic_store_explicit(&handle->completed, true, memory_order_release)" \
    "atomic_load_explicit(&handle->completed, memory_order_acquire)"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/world_roster_async.c" ||
        fail "world roster async wait handle must publish completion/result through atomics: $term"
done
for term in \
    "world_roster_array_fits" \
    "len > SIZE_MAX - 1U" \
    "party count is nonzero but party slot array is null" \
    "roster count is nonzero but roster array is null" \
    "world_roster_increment_frame" \
    "party slot size overflow" \
    "roster slot size overflow" \
    "result allocation size overflow"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/world_roster.c" ||
        fail "world/roster runtime dynamic arrays must guard allocation sizes: $term"
done
for term in \
    "world_roster_array_fits" \
    "party count is nonzero but party slot array is null" \
    "roster count is nonzero but roster array is null" \
    "world_roster_saturating_increment_size" \
    "roster plan size overflow" \
    "party plan size overflow" \
    "role count overflow" \
    "roster stats size overflow" \
    "role stats size overflow" \
    "buffer size overflow"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/world_roster_plan_stats.c" ||
        fail "world/roster plan/stat dynamic arrays must guard allocation sizes: $term"
done
for term in \
    "secure_slot_scope_array_fits" \
    "sizeof(SlotHandle)" \
    "sizeof(TokenCapability)" \
    "SecureMemoryWipe(scope->tokens, scope->capacity * sizeof(*scope->tokens))"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/slot_manager_scope.c" ||
        fail "secure slot scope arrays must guard allocation and wipe sizes: $term"
done
for term in \
    "maxSlots > SIZE_MAX / sizeof(SlotEntry)" \
    "manager->slotTable = calloc(maxSlots, sizeof(SlotEntry))"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/slot_manager.c" ||
        fail "slot manager table allocation must guard maxSlots sizing: $term"
done
for term in \
    "queue_size_increment" \
    "queue_size_decrement" \
    "queue tail is null" \
    "queue head is null" \
    "current != SIZE_MAX" \
    "current > 0"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/async/concurrent_queue.c" ||
        fail "concurrent queue must guard head/tail and size counter boundaries: $term"
done
for term in \
    "scheduler_array_fits" \
    "worker count is nonzero but worker array is null" \
    "worker array size overflow" \
    "scheduler->workers = (WorkerThread*)calloc(scheduler->numWorkers, sizeof(WorkerThread))"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/async/scheduler.c" ||
        fail "async scheduler worker arrays must guard allocation sizes: $term"
done
for term in \
    "scheduler global queue is null" \
    "worker or local queue is null" \
    "scheduler worker array is not initialized" \
    "victim local queue is null" \
    "scheduler failed to unblock fiber" \
    "scheduler failed to enqueue stolen fiber"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/async/scheduler_fiber_ops.c" ||
        fail "async scheduler fiber ops must guard queues/workers: $term"
done
grep -Fq "scheduler failed to requeue ready fiber" \
    "$ROOT_DIR/src/runtime/async/scheduler.c" ||
    fail "async scheduler worker loop must fail closed on ready-fiber requeue failure"
for term in \
    "pgy_parallel_array_fits" \
    "atomic_bool   g_pgy_pool_active" \
    "atomic_bool   g_pgy_pool_shutting_down" \
    "g_pgy_pool_lifecycle_mutex" \
    "atomic_store_explicit(&g_pgy_pool_shutting_down, true" \
    "pthread_t *workers = g_pgy_pool.workers" \
    "atomic_load_explicit(&g_pgy_pool_active" \
    "atomic_store_explicit(&g_pgy_pool_active" \
    "worker array size overflow"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_parallel.h" ||
        fail "parallel runtime task/worker arrays must guard null and allocation sizes: $term"
done
for term in \
    "parallel task array is null" \
    "parallel task spawn failed" \
    "PgyTaskHandle *handles =" \
    "(PgyTaskHandle *)calloc(count, sizeof(PgyTaskHandle))" \
    "PgyParallelArg *args =" \
    "(PgyParallelArg *)calloc(count, sizeof(PgyParallelArg))"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_parallel_run.h" ||
        fail "parallel block runner must guard null and allocation sizes: $term"
done
for term in \
    "atomic_bool   g_pgy_blocking_pool_active" \
    "atomic_bool   g_pgy_blocking_pool_shutting_down" \
    "g_pgy_blocking_pool_lifecycle_mutex" \
    "atomic_store_explicit(&g_pgy_blocking_pool_shutting_down, true" \
    "pthread_t *workers = g_pgy_blocking_pool.workers" \
    "atomic_load_explicit(&g_pgy_blocking_pool_active" \
    "atomic_store_explicit(&g_pgy_blocking_pool_active"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_parallel_blocking.h" ||
        fail "blocking parallel pool lifecycle must be atomic/mutex guarded: $term"
done
if grep -Fq "g_securityContext" "$ROOT_DIR/src/runtime/slot_security.c"; then
    fail "slot security context ownership must stay explicit, not process-global"
fi
if grep -Fq "g_pergyraSlotManager" \
    "$ROOT_DIR/src/runtime/slot_manager_scope.c" \
    "$ROOT_DIR/src/test_security.c" \
    "$ROOT_DIR/src/tests/security/test_security_runtime_1.cases.h" \
    "$ROOT_DIR/src/tests/security/test_security_runtime_2.cases.h"; then
    fail "secure slot wrapper ownership must stay slot-carried, not process-global"
fi
for term in \
    "SlotManager *manager;" \
    "slot->manager = manager" \
    "slot->manager = pscope->manager" \
    "SlotWriteSecure(slot->manager" \
    "SlotReadSecure(slot->manager" \
    "SlotReleaseSecure(slot->manager"; do
    grep -Fq "$term" \
        "$ROOT_DIR/src/runtime/slot_manager.h" \
        "$ROOT_DIR/src/runtime/slot_manager_scope.c" ||
        fail "secure slot wrapper must carry its manager owner: $term"
done
for term in \
    "PGY_RUNTIME_ELEM_CAPACITY_FITS" \
    "(capacity) <= (size_t)INT32_MAX" \
    "(capacity) <= SIZE_MAX / sizeof(CType)"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_list_set_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_queue_inline.h" ||
        fail "inline List/Queue capacity must stay within Int size API and element allocation bounds: $term"
done
grep -Fq "((capacity) != 0 && (capacity) <= (size_t)INT32_MAX)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_list_set_inline.h" ||
    fail "inline Set capacity must stay within Int size API range"
for term in \
    "capacity <= (size_t)INT32_MAX" \
    "elem_size <= SIZE_MAX / capacity"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_list_raw_exports.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_queue_exports.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_map_exports.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h" ||
        fail "LLVM raw collection capacity must stay within Int size API and element allocation bounds: $term"
done
for term in \
    "pgy_queue_raw_is_initialized" \
    "\"queue_push_string\"" \
    "\"queue pop on uninitialized queue\"" \
    "\"queue_size\", \"queue is not initialized\"" \
    "\"queue_empty\", \"queue is not initialized\""; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_queue_exports.h" ||
        fail "raw Queue initialized guard missing: $term"
done
for term in \
    "ch == NULL || ch->buf == NULL || ch->cap == 0" \
    "INT32_MAX" \
    "raw_space" \
    "used = (ch->count <= ch->cap)" \
    "pgy_channel_try_send_status_" \
    "pgy_channel_send_timeout_status_" \
    "pgy_channel_recv_result_" \
    "pgy_channel_try_recv_result_" \
    "pgy_channel_recv_timeout_result_" \
    "pgy_channel_ready_" \
    "pgy_channel_space_" \
    "pgy_channel_string_inline_dup" \
    "pgy_channel_string_inline_free_pending" \
    "ch->buf[ch->head] = NULL"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_channel_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_channel_string_inline.h" ||
        fail "inline Channel initialized/range guard missing: $term"
done
for term in \
    "pgy_runtime_rng_mutex" \
    "pthread_mutex_lock(&pgy_runtime_rng_mutex)" \
    "pthread_mutex_unlock(&pgy_runtime_rng_mutex)"; do
    grep -Fq "$term" \
        "$ROOT_DIR/src/runtime/pgy_runtime_platform_io_core.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_scalar_std_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_qubit_inline.h" ||
        fail "inline runtime RNG state must be mutex-guarded: $term"
done
for term in \
    "pgy_runtime_lib_rng_mutex" \
    "pthread_mutex_lock(&pgy_runtime_lib_rng_mutex)" \
    "pthread_mutex_unlock(&pgy_runtime_lib_rng_mutex)"; do
    grep -Fq "$term" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_authority_file_core.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_std_exports.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_quantum_exports.h" ||
        fail "LLVM runtime RNG state must be mutex-guarded: $term"
done
for term in \
    "static pthread_mutex_t _pgy_qubit_mutex = PTHREAD_MUTEX_INITIALIZER" \
    "pthread_mutex_lock(&_pgy_qubit_mutex)" \
    "pthread_mutex_unlock(&_pgy_qubit_mutex)"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_qubit_inline.h" ||
        fail "inline Qubit state must be mutex-guarded: $term"
done
for term in \
    "static pthread_mutex_t          pgy_qubit_rt_mutex = PTHREAD_MUTEX_INITIALIZER" \
    "pthread_mutex_lock(&pgy_qubit_rt_mutex)" \
    "pthread_mutex_unlock(&pgy_qubit_rt_mutex)"; do
    grep -Fq "$term" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_qubit_state_exports.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_lib_quantum_exports.h" ||
        fail "LLVM Qubit state must be mutex-guarded: $term"
done
for term in \
    "q->data == NULL || q->capacity == 0" \
    "q->capacity > (size_t)INT32_MAX" \
    "q != NULL ? (int32_t)q->count : 0" \
    "q == NULL || q->count == 0"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_queue_inline.h" ||
        fail "inline Queue initialized/null guard missing: $term"
done
for term in \
    "(capacity) <= (size_t)INT32_MAX" \
    "capacity <= (size_t)INT32_MAX"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_builtin_hashmap_inline.h" \
        "$ROOT_DIR/src/runtime/pgy_runtime_map_string_inline.h" ||
        fail "inline HashMap capacity must stay within Int size API range: $term"
done
for term in \
    "pgy_map_string_is_initialized" \
    "pgy_map_string_capacity_fits(m->capacity)" \
    "if (!pgy_map_string_is_initialized(m))" \
    "return pgy_map_string_is_initialized(m) ? (int32_t)m->count : 0;"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_map_string_inline.h" ||
        fail "inline HashMap<String> initialized guard missing: $term"
done
for term in \
    "pgy_map_int_is_initialized" \
    "PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m->capacity, int32_t)" \
    "if (!pgy_map_int_is_initialized(m))" \
    "\"map_set_int\", \"null key\""; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_builtin_storage_inline.h" ||
        fail "inline HashMap<Int> initialized/null-key guard missing: $term"
done
grep -Fq "return pgy_map_int_is_initialized(m) ? (int32_t)m->count : 0;" \
    "$ROOT_DIR/src/runtime/pgy_runtime_map_int_key_inline.h" ||
    fail "inline HashMap<Int> size must not dereference invalid map"
for term in \
    "PGY_RUNTIME_HASHMAP_IS_INITIALIZED" \
    "PGY_RUNTIME_HASHMAP_CAPACITY_FITS((map)->capacity, CType)" \
    "if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType))" \
    "return 0; \\"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_builtin_hashmap_inline.h" ||
        fail "inline HashMap initialized guard missing: $term"
done
for term in \
    "pgy_set_add_string_raw_export" \
    "pgy_runtime_strdup_export(value != NULL ? value : \"\")"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h" ||
        fail "raw Set<String> add export missing result-owned string term: $term"
done
for term in \
    "pgy_set_raw_string_hash_value" \
    "pgy_set_raw_string_slot_eq" \
    "pgy_set_raw_rehash_string"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h" ||
        fail "raw Set<String> must use string-specific hash/equality/rehash: $term"
done
if grep -Fq "elem_size == (int64_t)sizeof(char *)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h"; then
    fail "generic raw Set<T> must not classify pointer-sized values as String"
fi
if grep -Fq "pgy_set_add_raw_export(set_ptr, &owned" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h"; then
    fail "raw Set<String> add must not route through pointer-sized generic raw set add"
fi
for term in \
    "pgy_set_has_string_raw_export" \
    "pgy_set_remove_string_raw_export" \
    "\"set_size\", \"set is not initialized\"" \
    "free(*owned)" \
    "PGY_SET_RAW_DELETED"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_raw_exports.h" ||
        fail "raw Set<String> query/remove export missing ownership/tombstone term: $term"
done
grep -Fq "pgy_set_raw_is_initialized" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h" ||
    fail "raw Set initialized guard helper missing"
grep -Fq "set->capacity <= (size_t)INT32_MAX" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_set_exports.h" ||
    fail "raw Set initialized guard must enforce Int-sized capacity"
for term in \
    "\"pgy_set_add_string_raw_export\"" \
    "\"pgy_set_has_string_raw_export\"" \
    "\"pgy_set_remove_string_raw_export\""; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c" ||
        fail "LLVM Set<String> lowering must use string-owned raw set export: $term"
done
for term in \
    "PGY_MAP_RAW_DELETED" \
    "map->occupied[h] = PGY_MAP_RAW_DELETED" \
    "map->occupied[h] == PGY_MAP_RAW_LIVE" \
    "pgy_map_raw_is_initialized" \
    "map->capacity <= (size_t)INT32_MAX" \
    "\"map_has\", \"map is not initialized\"" \
    "\"map_size\", \"map is not initialized\"" \
    "\"map remove on uninitialized map\"" \
    "\"map string remove on uninitialized map\"" \
    "static bool" \
    "pgy_map_grow_raw_export(PgyHashMapRaw *map" \
    "&& !pgy_map_grow_raw_export(map, value_size)" \
    "&& !pgy_map_grow_raw_export(map, (int64_t)sizeof(char *))" \
    "\"map is full\""; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_raw_map_exports.h" ||
        fail "raw HashMap removal must preserve probe chains with tombstones: $term"
done
for term in \
    "pgy_list_push_string_raw_export" \
    "pgy_list_get_string_raw_export" \
    "pgy_list_set_string_raw_export" \
    "pgy_list_remove_string_raw_export" \
    "pgy_list_raw_is_initialized" \
    "\"list_push_string\"" \
    "\"list_size\", \"list is not initialized\"" \
    "\"list remove string on uninitialized list\"" \
    "pgy_runtime_strdup_export(value != NULL ? value : \"\")" \
    "free(*slot)"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_lib_list_raw_exports.h" ||
        fail "raw List<String> export missing ownership term: $term"
done
for term in \
    "pgy_list_set_string" \
    "pgy_list_remove_string" \
    "free(l->data[index])" \
    "PGY_SET_INLINE_DELETED"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_list_set_inline.h" ||
        fail "inline List/Set string ownership or tombstone term missing: $term"
done
for term in \
    "PGY_RUNTIME_LIST_IS_INITIALIZED" \
    "PGY_RUNTIME_SET_IS_INITIALIZED" \
    "PGY_RUNTIME_STRING_SET_IS_INITIALIZED" \
    "list get on invalid list" \
    "return PGY_RUNTIME_LIST_IS_INITIALIZED(l, int32_t) ? (int32_t)l->count : 0;" \
    "return PGY_RUNTIME_STRING_SET_IS_INITIALIZED(s) ? (int32_t)s->count : 0;" \
    "if (!PGY_RUNTIME_SET_IS_INITIALIZED(s, CType)) return;"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_list_set_inline.h" ||
        fail "inline List/Set initialized guard missing: $term"
done
for term in \
    "if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType))" \
    "list get on invalid list" \
    "return PGY_RUNTIME_LIST_IS_INITIALIZED(l, CType) ? (int32_t)l->count : 0;"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_list_generic_inline.h" ||
        fail "generic inline List initialized guard missing: $term"
done
for term in \
    "capacity > (size_t)INT32_MAX" \
    "if (f == NULL)" \
    "input < 0 || input >= PGY_FSM_MAX_STATES" \
    "return f != NULL ? f->current : -1;" \
    "return t == NULL || t->done;" \
    "return c == NULL || c->remaining <= 0;"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_pool_fsm_timer_inline.h" ||
        fail "pool/FSM/timer inline null/range guard missing: $term"
done
for term in \
    "pool->capacity == 0 || pool->occupied == NULL" \
    "pool->freeListTop > pool->capacity" \
    "pool->freeListTop >= pool->capacity" \
    "usage = pool->capacity == 0"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/slot_pool.c" ||
        fail "slot pool initialized/cursor guard missing: $term"
done
for term in \
    "PGY_HASHMAP_DELETED" \
    "m->occupied[h] = PGY_HASHMAP_DELETED" \
    "m->occupied[h] == PGY_HASHMAP_LIVE"; do
    grep -Fq "$term" "$ROOT_DIR/src/runtime/pgy_runtime_builtin_hashmap_inline.h" ||
        fail "inline HashMap removal must preserve probe chains with tombstones: $term"
done
for term in \
    "\"pgy_list_push_string_raw_export\"" \
    "\"pgy_list_get_string_raw_export\"" \
    "\"pgy_list_set_string_raw_export\"" \
    "\"pgy_list_remove_string_raw_export\""; do
    grep -Fq "$term" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c" ||
        fail "LLVM List<String> lowering must use string-owned raw list export: $term"
done

require_file "docs/semantics/04_ownership_abi.md"
for term in \
    "runtime-borrowed string" \
    "result-owned string" \
    "result-owned array" \
    "runtime-owned handle" \
    "caller must not free" \
    "caller owns" \
    "must eventually release" \
    "thread-local borrowed snapshots" \
    "later borrowed string query on the same thread" \
    "until the next authority validation updates that thread's snapshot" \
    "last/history/active/recent" \
    "runtime-abi-lifetime-test-smoke"; do
    require_term "docs/semantics/04_ownership_abi.md" "$term"
done

echo "[runtime-abi-lifetime] borrowed exports, result-owned payloads, and file handles are gated"
