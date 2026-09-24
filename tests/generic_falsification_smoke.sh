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
# Falsifications that SUCCEEDED (= implementation coordinates, now fixed):
#   - unification conflict and unbound-T died in MIR lowering after the
#     native semantic pass had admitted them ("MIR generic call cannot
#     resolve consistent actual/formal bindings", no semantic diagnostic).
#     The native semantic pass now refuses both at the call
#     (type_checker_call_generic_where.c), on C and LLVM alike.
#   - default type args (<T = Int>) bind on both native backends (f_default).
#   - `return None;` inside a generic body emitted None_T -> the option
#     context copy now substitutes bindings.
# Still open, not asserted here: f_where_g1 (a where-bound T instantiated
# with a subject, wrapped in Option<T>) fails in code generation on both
# native backends, and so does the same Some(subject) without generics.
#
# 2026-09-24: this gate ran on the default route after the default route
# became the self-hosted front end, so it had been red and unrun since; it
# now names its native subject and runs in the Linux push shard.

set -euo pipefail

# Subject of this gate:
#   the native semantic generic call checks changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

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

# --- unification + unbound diagnostics (semantic, both backends) ---------
for backend in c llvm; do
    expect_reject "$backend" f_unify.pgy \
        "binds generic parameter 'T' to both 'Int' (argument 1) and 'String' (argument 2)"
    expect_reject "$backend" f_return_only.pgy \
        "cannot bind generic parameter 'T': no parameter of 'Make' mentions it"
done

# --- known open: Option<T> over a subject fails in code generation ------
# Recorded, not endorsed: when either line changes, update the header above.
expect_reject c    f_where_g1.pgy "pgy_option_some_Card"
expect_reject llvm f_where_g1.pgy "LLVM type 'Sortable' is not registered"

# --- default type args and a satisfied where-bound run on both -----------
for backend in c llvm; do
    expect_runs "$backend" f_default.pgy "fresh-none"
    expect_runs "$backend" f_where_ability_ok.pgy "1"
done

echo "[generic-falsification] constraint, unification and unbound-T claims hold at native semantic on c and llvm; default args and satisfied where-bounds run on both"
