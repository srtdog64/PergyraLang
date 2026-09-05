#!/usr/bin/env bash
#
# border_registry_smoke.sh — docs/154's executable half. src/ compartment
# faces are a CONTRACT: an include crossing a border must be a registered
# face, or this gate fails. Pure text (no compiler) — always load-bearing
# in CI, in the run_budget_twin_parity.sh lineage.
#
# Census baseline (2026-07-04): all borders below measured clean after the
# B-1 surgery. An unregistered crossing appearing later is drift, not
# baseline — fix the crossing or register the face IN THE SAME COMMIT as
# docs/154.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$ROOT_DIR/src"
DOC="$ROOT_DIR/docs/154_border_registry.md"

fail() { echo "[border-registry] FAIL: $*" >&2; exit 1; }

check_runtime_twin_include_boundary() {
    local runtime_dir="$1" input hits status
    local inline_inputs=("$runtime_dir/pgy_runtime.h" "$runtime_dir/"*inline*.h)
    local extern_input="$runtime_dir/pgy_runtime_lib_authority_file_core.h"
    local inline_twin="$runtime_dir/pgy_runtime_panic_checked_inline.h"
    for input in "${inline_inputs[@]}" "$inline_twin" "$extern_input"; do
        [[ -f "$input" ]] || fail "missing runtime twin input: $input"
    done
    if hits="$(grep -lE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^">]*authority_file_core\.h[">]' "${inline_inputs[@]}" 2>&1)"; then
        fail "inline twin includes the extern twin: $hits"
    else
        status=$?
        [[ "$status" -eq 1 ]] || fail "cannot inspect inline twin includes: $hits"
    fi
    if hits="$(grep -nE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^">]*panic_checked_inline\.h[">]' "$extern_input" 2>&1)"; then
        fail "extern twin includes the inline twin: $hits"
    else
        status=$?
        [[ "$status" -eq 1 ]] || fail "cannot inspect extern twin includes: $hits"
    fi
}

bash "$ROOT_DIR/tests/border_registry_checker_smoke.sh"
[ -f "$DOC" ] || fail "missing docs/154_border_registry.md"

# --- backend border: transpiler_* and llvm_* never include each other ----
cd "$SRC/codegen"
hits="$(grep -l '#include "transpiler' llvm_*.c llvm_*.h 2>/dev/null || true)"
[ -z "$hits" ] || fail "llvm_* includes transpiler_* (use a codegen_* neutral face): $hits"
hits="$(grep -l '#include "llvm' transpiler_*.c transpiler_*.h 2>/dev/null || true)"
[ -z "$hits" ] || fail "transpiler_* includes llvm_*: $hits"
cd "$ROOT_DIR"

# --- stage borders: hard zeros ---------------------------------------------
hits="$(grep -rl '#include "../codegen' "$SRC/semantic" 2>/dev/null || true)"
[ -z "$hits" ] || fail "semantic includes codegen: $hits"
hits="$(grep -rl '#include "../codegen' "$SRC/parser" 2>/dev/null || true)"
[ -z "$hits" ] || fail "parser includes codegen: $hits"
hits="$(grep -rlE '#include "\.\./(codegen|semantic|parser)' "$SRC/runtime" 2>/dev/null || true)"
[ -z "$hits" ] || fail "runtime includes an upstream stage: $hits"

# --- parser -> semantic: registered faces only ------------------------------
bad="$(grep -rho '#include "../semantic/[^"]*"' "$SRC/parser" \
    | sort -u \
    | grep -v '"../semantic/diag_codes.h"' \
    | grep -Fv '"../semantic/callable_contract_vocabulary.h"' \
    | grep -v '"../semantic/type_system.h"' || true)"
[ -z "$bad" ] || fail "parser -> semantic crossing outside registered faces: $bad"

# --- codegen -> semantic: registered faces only -----------------------------
bad="$(grep -rho '#include "../semantic/[^"]*"' "$SRC/codegen" \
    | sort -u \
    | grep -v '"../semantic/diag_codes.h"' \
    | grep -v '"../semantic/builtin_kind.h"' \
    | grep -v '"../semantic/lifecycle_state.h"' || true)"
[ -z "$bad" ] || fail "codegen -> semantic crossing outside registered faces: $bad"

# --- runtime twin border -----------------------------------------------------
check_runtime_twin_include_boundary "$SRC/runtime"

# --- AIR border: codegen must not read verification-only IR -----------------
# AIR headers live at src/compiler/air*.h (air.h, air_internal.h, ...).
hits="$(grep -rl '#include "\.\./compiler/air' "$SRC/codegen" 2>/dev/null || true)"
[ -z "$hits" ] || fail "codegen includes AIR headers (verification-only): $hits"

# --- registry text stays honest ---------------------------------------------
grep -Fq "사인된 face" "$DOC" || fail "docs/154 lost the registered-face table"
grep -Fq '`callable_contract_vocabulary.h`' "$DOC" || fail "docs/154 lost the callable vocabulary face"
grep -Fq "B-2" "$DOC" || fail "docs/154 lost the rung ladder"

echo "[border-registry] all crossings registered (backend/stage/twin/AIR faces clean)"
