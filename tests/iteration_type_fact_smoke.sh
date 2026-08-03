#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[iteration-type-fact] missing $file contract: $text" >&2
        exit 1
    fi
}

require_text src/semantic/iteration_type_fact.h "iteration_syntax_id"
require_text src/compiler/hir_iteration_flow_facts.c \
    "hir_attach_iteration_type_facts"
require_text src/compiler/mir_hir_fact_transfer.c \
    "mir_copy_iteration_type_facts"
require_text src/compiler/mir_iteration_type_facts.c \
    "mir_routine_iteration_type_fact"
require_text src/compiler/mir_source_local_types.c \
    "mir_routine_iteration_type_fact"
require_text src/compiler/mir_json_dump_flow.c "iteration_type_fact_count"
require_text src/compiler/mir_json_dump_flow.c "iteration_type_facts"
if grep -Fq 'mir_source_local_for_loop_variable_type_name(' \
        "$ROOT_DIR/src/compiler/mir_source_local_types.c"; then
    echo "[iteration-type-fact] source-local capture reopened AST type lookup" >&2
    exit 1
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[iteration-type-fact] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_iteration_fact.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
JSON="$WORK_DIR/loop.json"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/break_continue.pgy")" \
    >"$JSON")
grep -Fq '"iteration_type_fact_count":1' "$JSON"
grep -Fq '"iteration_type_facts":[{"function_syntax_id":' "$JSON"
grep -Fq '"binding_type":"Int"' "$JSON"
grep -Fq '"iterable_type":"Int"' "$JSON"

echo "[iteration-type-fact] for-loop type fact survives semantic, MIR, and JSON anchors"
