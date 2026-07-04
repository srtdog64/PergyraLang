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
hits="$(grep -l 'authority_file_core' "$SRC/runtime/pgy_runtime.h" \
    "$SRC/runtime/"*inline*.h 2>/dev/null || true)"
[ -z "$hits" ] || fail "inline twin chain references the extern twin: $hits"
if grep -q 'panic_checked_inline' "$SRC/runtime/authority_file_core.h" 2>/dev/null; then
    fail "extern twin includes the inline twin"
fi

# --- AIR border: codegen must not read verification-only IR -----------------
# AIR headers live at src/compiler/air*.h (air.h, air_internal.h, ...).
hits="$(grep -rl '#include "\.\./compiler/air' "$SRC/codegen" 2>/dev/null || true)"
[ -z "$hits" ] || fail "codegen includes AIR headers (verification-only): $hits"

# --- registry text stays honest ---------------------------------------------
grep -Fq "사인된 face" "$DOC" || fail "docs/154 lost the registered-face table"
grep -Fq "B-2" "$DOC" || fail "docs/154 lost the rung ladder"

echo "[border-registry] all crossings registered (backend/stage/twin/AIR faces clean)"
