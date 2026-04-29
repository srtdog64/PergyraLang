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
    "src/runtime/pgy_runtime_panic_checked_inline.h" \
    "src/runtime/pgy_runtime_memory_array_slot_inline.h" \
    "src/runtime/pgy_runtime_slot_macros.h" \
    "src/runtime/pgy_runtime_builtin_storage_inline.h" \
    "src/runtime/pgy_runtime_list_set_inline.h" \
    "src/runtime/pgy_runtime_pool_fsm_timer_inline.h" \
    "src/runtime/pgy_runtime_zone_result_option_inline.h"

concat_runtime_text "$llvm_text" \
    "src/runtime/pgy_runtime_lib_core_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_collection_common_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_collection_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_map_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_queue_exports.h" \
    "src/runtime/pgy_runtime_lib_raw_set_exports.h" \
    "src/runtime/pgy_runtime_lib_set_intent_trace_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" \
    "src/runtime/pgy_runtime_lib_slot_exports.h" \
    "src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_secure_slot_exports.h" \
    "src/runtime/pgy_runtime_lib_device_slot_exports.h" \
    "src/runtime/pgy_runtime_lib_array_map_exports.h" \
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
    "src/runtime/pgy_runtime_lib_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_std_exports.h" \
    "src/runtime/pgy_runtime_lib_channel_quantum_exports.h"

check_borrowed_exports "inline-intent" "$inline_text" \
    pgy_intent_last_trace_export \
    pgy_intent_last_failure_export \
    pgy_intent_last_name_export \
    pgy_intent_history_step_name_export \
    pgy_intent_history_step_zone_export \
    pgy_intent_history_step_phase_export \
    pgy_intent_history_step_participant_export \
    pgy_intent_history_step_slot_export \
    pgy_intent_history_step_from_zone_export \
    pgy_intent_history_step_from_slot_export \
    pgy_intent_history_step_to_zone_export \
    pgy_intent_history_step_to_slot_export \
    pgy_intent_history_step_failure_export \
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
    pgy_intent_history_step_name_export \
    pgy_intent_history_step_zone_export \
    pgy_intent_history_step_phase_export \
    pgy_intent_history_step_participant_export \
    pgy_intent_history_step_slot_export \
    pgy_intent_history_step_from_zone_export \
    pgy_intent_history_step_from_slot_export \
    pgy_intent_history_step_to_zone_export \
    pgy_intent_history_step_to_slot_export \
    pgy_intent_history_step_failure_export \
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

check_result_owned_strings "inline-string-helpers" "$inline_string_text" \
    pgy_runtime_strdup \
    Substring StringReplace StringTrim ToUpper ToLower StringConcat StringJoin

check_result_owned_strings "llvm-string-helpers" "$llvm_string_text" \
    pgy_runtime_lib_strdup \
    pgy_file_read pgy_read_file pgy_input Substring StringReplace StringTrim \
    ToUpper ToLower StringConcat StringJoin

check_result_owned_arrays "inline-string-array-helpers" "$inline_string_text" \
    pgy_runtime_strdup StringSplit

check_result_owned_arrays "llvm-string-array-helpers" "$llvm_string_text" \
    pgy_runtime_lib_strdup StringSplit pgy_map_keys_raw_export

file_open_body="$(extract_function_body "$llvm_string_text" pgy_file_open)"
file_close_body="$(extract_function_body "$llvm_string_text" pgy_file_close)"
[[ -n "$file_open_body" ]] || fail "missing pgy_file_open"
[[ -n "$file_close_body" ]] || fail "missing pgy_file_close"
for term in \
    "for (int i = 3; i < PGY_MAX_OPEN_FILES; i++)" \
    "pgy_runtime_ftable[i] == NULL" \
    "fd = i" \
    "pgy_runtime_ftable[fd] = fp"; do
    grep -Fq "$term" <<< "$file_open_body" ||
        fail "pgy_file_open must reuse closed runtime-owned handle slots; missing $term"
done
grep -Fq "pgy_runtime_ftable[fd] = NULL" <<< "$file_close_body" ||
    fail "pgy_file_close must release the runtime-owned handle slot"

require_file "docs/semantics/04_ownership_abi.md"
for term in \
    "runtime-borrowed string" \
    "result-owned string" \
    "result-owned array" \
    "runtime-owned handle" \
    "caller must not free" \
    "caller owns" \
    "must eventually release" \
    "valid until the next mutation of the corresponding runtime registry" \
    "last/history/active/recent" \
    "runtime-abi-lifetime-test-smoke"; do
    require_term "docs/semantics/04_ownership_abi.md" "$term"
done

echo "[runtime-abi-lifetime] borrowed exports, result-owned payloads, and file handles are gated"
