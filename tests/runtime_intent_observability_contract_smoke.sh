#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[runtime-intent-observability-contract] $*" >&2
    exit 1
}

require_term() {
    local path="$1"
    local term="$2"
    grep -Fq -- "$term" "$path" || fail "$path missing term: $term"
}

require_step_ok_guard_before_history_write() {
    local path="$1"
    local guard_line
    local write_line

    guard_line="$(
        awk '
            /pgy_intent_trace_step_ok_export/ { in_fn = 1 }
            in_fn && /if \(!PGY_INTENT_OBSERVABILITY_ENABLED\)/ {
                print NR
                exit
            }
        ' "$path"
    )"
    write_line="$(
        grep -nF "entry->steps[entry->step_count - 1].ok = true" "$path" |
            head -n 1 | cut -d: -f1
    )"

    if [[ -z "$guard_line" || -z "$write_line" || "$guard_line" -ge "$write_line" ]]; then
        fail "$path: step_ok must no-trace guard before history write"
    fi
}

for rel in \
    "src/runtime/pgy_runtime_intent_active_exports.h" \
    "src/runtime/pgy_runtime_intent_active_index_inline.h" \
    "src/runtime/pgy_runtime_intent_query_inline.h" \
    "src/runtime/pgy_runtime_intent_trace_inline.h" \
    "src/runtime/pgy_runtime_lib_intent_active_index_exports.c" \
    "src/runtime/pgy_runtime_lib_intent_active_index_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_trace_events_exports.c" \
    "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_recent_exports.h" \
    "src/codegen/transpiler_intent_observability_builtin_emit.c" \
    "src/codegen/llvm_expr_intent_observability_calls.c" \
    "src/codegen/llvm_expr_intent_observability_calls.h"; do
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing contract input: $rel"
done

for rel in \
    "src/runtime/pgy_runtime_intent_active_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_exports.h"; do
    path="$ROOT_DIR/$rel"
    require_term "$path" "pgy_intent_active_entry_by_index_export(index)"
    require_term "$path" "pgy_intent_active_entry_by_handle_export(intent_index)"

    for fn in \
        pgy_intent_active_name_export \
        pgy_intent_active_priority_export \
        pgy_intent_active_trace_id_export \
        pgy_intent_active_concurrent_export \
        pgy_intent_active_trace_export \
        pgy_intent_active_parent_handle_export \
        pgy_intent_active_subject_count_export \
        pgy_intent_active_failed_export \
        pgy_intent_active_failure_export; do
        awk -v fn="$fn" '
            $0 ~ fn "\\(int32_t index\\)" { in_fn = 1 }
            in_fn && /}/ { in_fn = 0 }
            in_fn && /pgy_intent_active_entry_by_handle_export\(index\)/ { bad = 1 }
            END { exit bad ? 1 : 0 }
        ' "$path" || fail "$rel: $fn must use active index lookup, not handle lookup"
    done
done

require_term "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c" "BUILTIN_INTENT_ACTIVE_HANDLE"
require_term "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c" "pgy_intent_active_handle_export"
require_term "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" "IntentActiveHandle"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c" "pgy_intent_find_active_registry_slot_export"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "PGY_INTENT_ACTIVE_INDEX_MAX must stay a power of two"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c" "PGY_INTENT_ACTIVE_INDEX_MAX must stay a power of two"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h" "pgy_intent_active_index_handles[first_tombstone] = handle"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c" "pgy_intent_active_index_handles[first_tombstone] = handle"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h" "pgy_intent_active_index_set(handle, i)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c" "pgy_intent_active_index_set_export(handle, i)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_intent_find_active_registry_slot_export(handle)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_intent_active_index_clear_export(handle)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "#include \"pgy_runtime_intent_active_index_inline.h\""
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "#include \"pgy_runtime_lib_intent_trace_events_exports.c\""

require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "pgy_intent_next_positive_counter"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "pgy_intent_next_unused_handle"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "pgy_intent_find_active_entry(candidate) == NULL"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "intent handle space exhausted"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "pgy_intent_next_positive_counter(&pgy_intent_next_trace_id)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h" "name = PGY_INTENT_OBSERVABILITY_ENABLED"

require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "pgy_intent_next_positive_counter_export"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "pgy_intent_next_unused_handle_export"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "pgy_intent_find_active_entry_export(candidate) == NULL"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "intent handle space exhausted"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "pgy_intent_next_positive_counter_export(&pgy_intent_next_trace_id)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "(size_t)subject_count > SIZE_MAX / sizeof(void *)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" "name = PGY_INTENT_OBSERVABILITY_ENABLED"
require_step_ok_guard_before_history_write "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_events_inline.h"
require_step_ok_guard_before_history_write "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_trace_events_exports.c"

echo "[runtime-intent-observability-contract] active index/handle contract is gated"
