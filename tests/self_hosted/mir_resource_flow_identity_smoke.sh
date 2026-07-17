#!/usr/bin/env bash
# The self-hosted MIR reader must consume the HIR-owned ResourceFlowUniverse
# projection and reject missing or inconsistent routine identity rows.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INDEX="$ROOT_DIR/src/self_hosted/mir_lower/routine_fact_index_owner.pgy"
LOWER="$ROOT_DIR/src/self_hosted/mir_lower/routine_lower.pgy"
DUMP="$ROOT_DIR/src/compiler/mir_json_dump_flow.c"

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-mir-resource-flow] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_resource_flow}"
mkdir -p "$BUILD_DIR"
RUNNER_TMP="$BUILD_DIR/c_runner_tmp"
mkdir -p "$RUNNER_TMP"
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) TMPDIR="$(pgy_path_for_windows_tool "$RUNNER_TMP")" ;;
    *) TMPDIR="$RUNNER_TMP" ;;
esac
export TMPDIR
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) SHELL="$(command -v bash)"; export SHELL ;;
esac

LOWER_BIN="$BUILD_DIR/mir_lower.exe"
MIR_JSON="$BUILD_DIR/resource_flow.mirjson"
VALID_OUT="$BUILD_DIR/resource_flow.out"
BAD_JSON="$BUILD_DIR/resource_flow.bad.mirjson"
BAD_OUT="$BUILD_DIR/resource_flow.bad.out"
LOWER_LOG="$BUILD_DIR/mir_lower.compile.log"

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "src/self_hosted/mir_lower/main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$LOWER_BIN")" \
    >"$LOWER_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    printf 'compile_rc=%s pgy=%s tmpdir=%s\n' "$compile_rc" "$PGY" "$TMPDIR" >>"$LOWER_LOG"
    echo "[self-host-mir-resource-flow] mir_lower rebuild failed" >&2
    cat "$LOWER_LOG" >&2
    exit 1
fi

CASE="tests/cases/function_param_flow_summary/main.pgy"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$CASE")" >"$MIR_JSON")
grep -Fq -- '"resource_flow_symbol_count"' "$MIR_JSON"
grep -Fq -- '"resource_flow_symbols"' "$MIR_JSON"
grep -Fq -- '"stable_index"' "$MIR_JSON"
grep -Fq -- '"declaration_syntax_id"' "$MIR_JSON"
grep -Fq -- '"symbol_kind"' "$MIR_JSON"
grep -Fq -- '"is_parameter"' "$MIR_JSON"
grep -Fq -- '"parameter_index"' "$MIR_JSON"
grep -Fq -- '"name":"slot"' "$MIR_JSON"

MIR_REL="${MIR_JSON#$ROOT_DIR/}"
if ! (cd "$ROOT_DIR" && "$LOWER_BIN" "$MIR_REL" >"$VALID_OUT" 2>&1); then
    echo "[self-host-mir-resource-flow] valid ResourceFlowUniverse MIR was rejected" >&2
    cat "$VALID_OUT" >&2
    exit 1
fi
grep -Fq -- "Function: Recur" "$VALID_OUT"

sed '0,/"resource_flow_symbol_count":1/s//"resource_flow_symbol_count":0/' \
    "$MIR_JSON" >"$BAD_JSON"
BAD_REL="${BAD_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_REL" >"$BAD_OUT" 2>&1); then
    echo "[self-host-mir-resource-flow] missing ResourceFlowUniverse row was accepted" >&2
    exit 1
fi
grep -Eq -- "routine ResourceFlowUniverse facts are incomplete|routine MIR fact index is incomplete" "$BAD_OUT"

grep -Fq -- 'resource_flow_symbol_count' "$DUMP"
grep -Fq -- 'resource_flow_symbols' "$DUMP"
grep -Fq -- 'MirRoutineFactIndexResourceFlowFactsValid' "$INDEX"
grep -Fq -- 'resource_flow_stable_indices' "$INDEX"
grep -Fq -- 'routine ResourceFlowUniverse facts are incomplete' "$LOWER"
grep -Fq -- 'MIRResourceFlowSymbol *resource_flow_symbols' \
    "$ROOT_DIR/src/compiler/mir_types.h"
grep -Fq -- 'mir_copy_resource_flow_symbols' \
    "$ROOT_DIR/src/compiler/mir_hir_fact_transfer.c"
grep -Fq -- 'mir_validate_resource_flow_symbols' \
    "$ROOT_DIR/src/compiler/mir_program_validate.c"

echo "[self-host-mir-resource-flow] MIR-owned ResourceFlowUniverse rows reach self-host identity and malformed counts fail closed"
