#!/usr/bin/env bash
#
# generic_falsification_smoke.sh — docs/151 §8's falsification battery
# (BDFL method, 2026-07-04): every claim behind G-2/G-6 got a kernel built
# to REFUTE it; the measured voices are locked here so they cannot drift.
#
# Falsifications that FAILED (= the claims are real):
#   - where type-bound / func-level ability-bound / ability multi-bound are
#     STATIC semantic rejections with named constraints (G-6: constraints
#     are the one static channel carrying axes through T — enforced).
#   - where-bound composes with G-1 on C, including a NON-builtin Option
#     payload (Option<Card>).
# Falsifications that SUCCEEDED (= implementation coordinates, now fixed):
#   - unification conflict and unbound-T used to die at the native/verify
#     stage with no diagnostic -> C now says "cannot bind generic
#     parameter(s)" (LLVM had its own diagnostics already).
#   - default type args (<T = Int>) were dead at C call-site binding ->
#     now bind (f_default runs); LLVM side still rejects (G-2L).
#   - `return None;` inside a generic body emitted None_T -> the option
#     context copy now substitutes bindings.
# LLVM G-2L cluster (locked as rejects until that rung lands): constructed
# params, subject-typed args, default-arg binding all need argument type
# metadata on the LLVM side.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[generic-falsification] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/generic_falsification"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[generic-falsification] FAIL: $*" >&2; exit 1; }

compile() {
    local backend="$1" fixture="$2" out_name="$3"
    local src out rc
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$out_name")"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$out_name.log" 2>&1
    rc=$?
    set -e
    return $rc
}

expect_reject() {
    local backend="$1" fixture="$2" needle="$3"
    local tag="rej_${backend}_${fixture%.pgy}.exe"
    if compile "$backend" "$fixture" "$tag"; then
        fail "$backend/$fixture compiled but the measured verdict is a reject"
    fi
    grep -Fq "$needle" "$OUT_DIR/$tag.log" ||
        fail "$backend/$fixture rejected without the measured diagnostic: $needle"
}

expect_runs() {
    local backend="$1" fixture="$2" want="$3"
    local tag="run_${backend}_${fixture%.pgy}.exe"
    compile "$backend" "$fixture" "$tag" ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$tag.log")"
    local got
    got="$("$OUT_DIR/$tag" | tr -d '\r')" || fail "$backend/$fixture crashed"
    [ "$got" = "$want" ] || fail "$backend/$fixture printed '$got', expected '$want'"
}

# --- constraint enforcement (semantic layer, backend-independent) ---------
expect_reject c f_where_type_bad.pgy    "does not satisfy constraint 'UserId'"
expect_reject c f_where_ability_bad.pgy "does not satisfy constraint 'Sortable'"
expect_reject c f_multibound_bad.pgy    "does not satisfy"

# --- unification + unbound diagnostics ------------------------------------
expect_reject c    f_unify.pgy       "cannot bind generic parameter"
expect_reject llvm f_unify.pgy       "Call parameter type does not match"
expect_reject c    f_return_only.pgy "cannot bind generic parameter"
expect_reject llvm f_return_only.pgy "requires argument 1 to bind"

# --- default type args: C binds, LLVM is G-2L ------------------------------
expect_runs   c    f_default.pgy "fresh-none"
expect_reject llvm f_default.pgy "requires argument 1 to bind"

# --- where-bound satisfied paths: C runs; LLVM subject-arg metadata = G-2L -
expect_runs   c    f_where_ability_ok.pgy "1"
expect_reject llvm f_where_ability_ok.pgy "requires concrete argument"
expect_runs   c    f_where_g1.pgy "held"
expect_reject llvm f_where_g1.pgy "requires concrete argument"

echo "[generic-falsification] claims verified, refuted claims fixed, voices locked (c/llvm)"
