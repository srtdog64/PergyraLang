#!/usr/bin/env bash
# The self-hosted MIR JSON reader must consume function-parameter flow rows
# from the routine-owned JSON facts and reject malformed identity/row shapes.

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

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-mir-function-param-flow] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_function_param_flow}"
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
MIR_JSON="$BUILD_DIR/function_param_flow.mirjson"
AST_OUT="$BUILD_DIR/function_param_flow.reast"
BAD_JSON="$BUILD_DIR/function_param_flow.bad.mirjson"
BAD_OUT="$BUILD_DIR/function_param_flow.bad.out"
LOWER_LOG="$BUILD_DIR/mir_lower.compile.log"

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "src/self_hosted/mir_lower/main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$LOWER_BIN")" \
    >"$LOWER_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    printf 'compile_rc=%s pgy=%s tmpdir=%s\n' "$compile_rc" "$PGY" "$TMPDIR" >>"$LOWER_LOG"
    echo "[self-host-mir-function-param-flow] mir_lower rebuild failed" >&2
    cat "$LOWER_LOG" >&2
    exit 1
fi

CASE="tests/cases/function_param_flow_summary/main.pgy"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$CASE")" >"$MIR_JSON")
MIR_REL="${MIR_JSON#$ROOT_DIR/}"
if ! (cd "$ROOT_DIR" && "$LOWER_BIN" "$MIR_REL" >"$AST_OUT" 2>&1); then
    echo "[self-host-mir-function-param-flow] valid summary MIR was rejected" >&2
    cat "$AST_OUT" >&2
    exit 1
fi
grep -Fq -- "Function: Recur" "$AST_OUT"
grep -Fq -- "Parameters:" "$AST_OUT"

sed '0,/"function_param_flow_summary_count":1/s//"function_param_flow_summary_count":0/' \
    "$MIR_JSON" >"$BAD_JSON"
BAD_REL="${BAD_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_REL" >"$BAD_OUT" 2>&1); then
    echo "[self-host-mir-function-param-flow] malformed summary row was accepted" >&2
    exit 1
fi
grep -Eq -- "routine function parameter flow summary facts are incomplete|routine MIR fact index is incomplete" "$BAD_OUT"

grep -Fq -- "function_param_flow_summaries" "$INDEX"
grep -Fq -- "MirRoutineFactIndexFunctionParamFlowFactsValid" "$INDEX"
grep -Fq -- "source_syntax_id" "$INDEX"
grep -Fq -- "MirRoutineFactIndexFunctionParamFlowFactsValid(index)" "$LOWER"
grep -Fq -- "routine function parameter flow summary facts are incomplete" "$LOWER"

echo "[self-host-mir-function-param-flow] live MIR JSON summary rows reach routine identity and malformed rows fail closed"
