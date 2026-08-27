#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
pgy_normalize_runner_env_paths_for_bash
PGY="$PGY_BIN"
if [[ -d "/c/Program Files/LLVM/bin" ]]; then
    PATH="/c/ProgramData/mingw64/mingw64/bin:/c/Program Files/LLVM/bin:$PATH"
fi
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "[mir-param-carriage] python is required" >&2
        exit 1
    fi
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-mir-param-carriage.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
MIR_JSON="$WORK_DIR/carriage.json"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    tests/cases/abi_param_carriage/main.pgy --backend=c >"$MIR_JSON")

"$PYTHON_BIN" - "$MIR_JSON" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    payload = json.load(handle)

expected = {
    "ValueParam": ("value", "direct"),
    "ReadParam": ("readonly-ref", "direct"),
    "ValueResultParam": ("value-result", "direct"),
    "OwnerParam": ("owner-handle", "direct"),
    "ReadAggregate": ("readonly-ref", "indirect"),
}
routines = {row.get("name"): row for row in payload.get("routines", [])}
syntax_ids = []
for name, (carriage, pass_shape) in expected.items():
    params = routines.get(name, {}).get("params", [])
    actual = (
        (params[0].get("carriage"), params[0].get("pass"))
        if len(params) == 1 else None
    )
    if actual != (carriage, pass_shape):
        raise SystemExit(
            f"{name}: expected {(carriage, pass_shape)!r}, got {actual!r}"
        )
    syntax_id = params[0].get("source_syntax_id")
    if not isinstance(syntax_id, int) or syntax_id <= 0:
        raise SystemExit(f"{name}: missing formal parameter syntax identity")
    syntax_ids.append(syntax_id)
if len(syntax_ids) != len(set(syntax_ids)):
    raise SystemExit(f"formal parameter syntax identities are reused: {syntax_ids}")
PY

BAD_JSON="$WORK_DIR/ref-write.json"
if (cd "$ROOT_DIR" && "$PGY" tests/cases/semantic/ref_readonly_write/main.pgy \
        --backend=c --error-format=json >"$WORK_DIR/ref-write.out" \
        2>"$BAD_JSON"); then
    echo "[mir-param-carriage] ref field write unexpectedly compiled" >&2
    exit 1
fi
"$PYTHON_BIN" - "$BAD_JSON" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    diagnostics = json.load(handle)
if not any(
    row.get("code") == "PGY_SEM_IMMUTABLE_FIELD_WRITE"
    and "read-only ref parameter" in row.get("message", "")
    for row in diagnostics
):
    raise SystemExit("missing structured read-only ref write diagnostic")
PY

grep -Fq "MIRParamAbiFact   *param_abi_facts" \
    "$ROOT_DIR/src/compiler/mir_types.h"
grep -Fq "records parameters without carriage facts" \
    "$ROOT_DIR/src/compiler/mir_program_fact_validate.c"
grep -Fq "mir_routine_param_carriage" \
    "$ROOT_DIR/src/codegen/transpiler_inventory_view.c"
grep -Fq "llvm_mir_routine_param_carriage" \
    "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
grep -Fq "Parameter carriage controls physical ABI only" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
if grep -Eq 'param_attr|carriage[[:space:]]*==[[:space:]]*MIR_PARAM_CARRIAGE_OWNER_HANDLE.*noalias' \
        "$ROOT_DIR/src/codegen/llvm_decl.c"; then
    echo "[mir-param-carriage] carriage must not imply LLVM pointer attributes" >&2
    exit 1
fi

echo "[mir-param-carriage] source modes and nominal readonly-ref ABI are MIR-owned and verifier-gated"
