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

for rel in \
    "src/runtime/pgy_runtime_intent_active_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_active_index_exports.c" \
    "src/runtime/pgy_runtime_lib_intent_active_index_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" \
    "src/runtime/pgy_runtime_lib_intent_exports.h" \
    "src/codegen/transpiler_intent_observability_builtin_emit.h" \
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

require_term "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.h" "BUILTIN_INTENT_ACTIVE_HANDLE"
require_term "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.h" "pgy_intent_active_handle_export"
require_term "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" "IntentActiveHandle"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c" "pgy_intent_find_active_registry_slot_export"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_intent_find_active_registry_slot_export(handle)"
require_term "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_intent_active_index_clear_export(handle)"

echo "[runtime-intent-observability-contract] active index/handle contract is gated"
