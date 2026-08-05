#!/usr/bin/env bash
#
# ability_coherence_smoke.sh — coherence of ability polymorphism
# (docs/semantics/10 SS4.1, closed 2026-07-09).
#
# Pergyra sidesteps the classic typeclass coherence problem structurally:
# there is no global instance space, so "which witness wins" is always the
# explicitly bound one. The residual obligation is local uniqueness --
# exactly one implementation per (role, ability):
#
#   - reject_duplicate_impl        one role, same ability twice -> semantic
#     reject on BOTH backends (before this gate the C backend leaked a gcc
#     redefinition error and the LLVM backend accepted it silently)
#   - allow_two_roles_same_ability two roles each implementing the same
#     ability + sequential rebind: legal, deterministic, runs on both
#     backends (explicit bind IS the coherence rule)

set -euo pipefail

# Subject of this gate:
#   the duplicate-impl diagnostic stopped being raised.
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
[[ -x "$PGY" ]] || { echo "[ability-coherence] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/ability_coherence"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[ability-coherence] FAIL: $*" >&2; exit 1; }

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
    # The uniqueness rule is semantic-layer: BOTH backends must refuse with
    # the same diagnostic (the LLVM silent-accept was the measured hole).
    local backend="$1" fixture="$2"
    if compile "$backend" "$fixture" "rej_${backend}_${fixture%.pgy}.exe"; then
        fail "$backend/$fixture compiled but must fail closed"
    fi
    grep -Fq "more than once" "$OUT_DIR/rej_${backend}_${fixture%.pgy}.exe.log" ||
        fail "$backend/$fixture failed without the duplicate-impl diagnostic"
}

expect_runs() {
    local backend="$1" fixture="$2" want="$3"
    local exe="run_${backend}_${fixture%.pgy}.exe"
    compile "$backend" "$fixture" "$exe" ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    local got
    got="$("$OUT_DIR/$exe" | tr -d '\r')" || fail "$backend/$fixture crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend/$fixture printed '$got', expected '$want'"
}

# C-only platforms (macOS CI, Windows C-only) narrow the voice set via env;
# default exercises both backends (the LLVM voice is load-bearing: the
# silent-accept hole this gate closed was LLVM-side).
BACKENDS="${PGY_ABILITY_COHERENCE_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    expect_reject "$backend" reject_duplicate_impl.pgy
    expect_runs   "$backend" allow_two_roles_same_ability.pgy "2"
done

echo "[ability-coherence] duplicate impl fails closed on: $BACKENDS; two-role same-ability with explicit rebind stays legal"
