#!/usr/bin/env bash
# RIR must own the routine-local ResourceFlow identity projection after the
# HIR adapter, and every malformed identity must fail closed in RIR validation.

set -euo pipefail

SCRIPT_DIR="${BASH_SOURCE[0]%/*}"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH
RIR_TEST="${RIR_TEST_BIN:-$ROOT_DIR/bin/test_rir}"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ -n "${RIR_TEST_BIN:-}" ]]; then
    :
elif [[ ! -x "$RIR_TEST" && -x "$ROOT_DIR/bin/test_rir.exe" ]]; then
    RIR_TEST="$ROOT_DIR/bin/test_rir.exe"
fi

if [[ ! -x "$RIR_TEST" ]]; then
    echo "[rir-resource-flow] missing RIR test binary: $RIR_TEST" >&2
    exit 1
fi
if [[ ! -x "$PGY" ]]; then
    echo "[rir-resource-flow] missing compiler binary: $PGY" >&2
    exit 1
fi
PYTHON_BIN=""
for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v "$candidate")"
        break
    fi
done
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[rir-resource-flow] python3/python is required for JSON validation" >&2
    exit 1
fi

OUTPUT="$($RIR_TEST)"
[[ "$OUTPUT" == *"RIR validation rejects a ResourceFlow fact with unknown stable identity"* ]]
[[ "$OUTPUT" == *"RIR validation rejects duplicate ResourceFlow stable identity"* ]]

RIR_FLOW="$(< "$ROOT_DIR/src/compiler/rir_flow.c")"
RIR_RESOURCE_FLOW="$(< "$ROOT_DIR/src/compiler/rir_resource_flow_symbols.c")"
RIR_HEADER="$(< "$ROOT_DIR/src/compiler/rir.h")"
RIR_VALIDATOR="$(< "$ROOT_DIR/src/compiler/rir_validation.c")"
RIR_PUBLIC="$(< "$ROOT_DIR/src/compiler/rir_public_surface.c")"
[[ "$RIR_HEADER" == *"RIRResourceFlowSymbol *resource_flow_symbols"* ]]
[[ "$RIR_RESOURCE_FLOW" == *"rir_copy_resource_flow_symbols"* ]]
[[ "$RIR_VALIDATOR" == *"rir_validate_resource_flow_symbols"* ]]
[[ "$RIR_PUBLIC" == *"resource_flow_symbol_count"* ]]

"$PYTHON_BIN" - "$ROOT_DIR/src/compiler/rir_flow.c" <<'PY'
import pathlib
import sys

text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
start = text.index("rir_attach_resource_flow_identity")
end = text.index("rir_attach_function_param_flow_summaries", start)
body = text[start:end]
for forbidden in ("HIRResourceFlowSymbol", "hir_resource_flow_symbol_count",
                  "hir_resource_flow_symbol_at"):
    if forbidden in body:
        raise SystemExit(f"RIR identity consumer reopened HIR rows: {forbidden}")
PY

WORK_DIR="$ROOT_DIR/.tmp/rir_resource_flow_identity"
mkdir -p "$WORK_DIR"
RIR_JSON="$WORK_DIR/summary-hit.rir.json"
"$PGY" --rir-json "$ROOT_DIR/tests/cases/semantic_loop_flow/summary_hit.pgy" \
    >"$RIR_JSON"
"$PYTHON_BIN" - "$RIR_JSON" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    payload = json.load(handle)
scopes = payload.get("scopes", [])
if not scopes:
    raise SystemExit("RIR JSON has no scopes")
if not any(
    scope.get("resource_flow_symbol_count", 0) == len(
        scope.get("resource_flow_symbols", [])
    ) and scope.get("resource_flow_symbol_count", 0) >= 2
    for scope in scopes
):
    raise SystemExit("RIR JSON did not carry the RIR-owned ResourceFlow rows")
PY

echo "[rir-resource-flow] RIR-owned ResourceFlow rows reach JSON and malformed identity fails closed"
