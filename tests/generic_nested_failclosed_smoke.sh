#!/usr/bin/env bash
#
# generic_nested_failclosed_smoke.sh — generic functions over constructed
# types must never reach the native compiler as broken source. This locks
# BOTH backends' voices on the same fixtures (measured 2026-07-03):
#
#   - C: signature guard rejects Option<T> in param AND return position with
#     "uses type parameter ... inside a constructed type".
#   - LLVM: rejects Option<T> PARAMS ("requires concrete argument ... type
#     metadata"), but genuinely supports Option<T> RETURNS and body-locals —
#     that capability asymmetry is asserted here so it cannot drift silently.
#   - C body-local Option<T> (bare-T signature) is a known diagnostic-quality
#     gap: pgy still fails (rc!=0, native stage), never a silent bad binary.
#     Real nested substitution is the separate feature decision on the board.
#   - bare-T generics must keep compiling AND running on both backends
#     (no-false-positive leg).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[generic-nested] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/generic_nested_failclosed"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[generic-nested] FAIL: $*" >&2; exit 1; }

compile() {
    # compile <backend> <fixture> <out-name>; echoes rc, captures log.
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
    if compile "$backend" "$fixture" "rej_${backend}_${fixture%.pgy}.exe"; then
        fail "$backend/$fixture compiled but must fail closed"
    fi
    grep -Fq "$needle" "$OUT_DIR/rej_${backend}_${fixture%.pgy}.exe.log" ||
        fail "$backend/$fixture failed without the expected diagnostic: $needle"
}

expect_reject_any() {
    local backend="$1" fixture="$2"
    if compile "$backend" "$fixture" "rejany_${backend}_${fixture%.pgy}.exe"; then
        fail "$backend/$fixture compiled but must fail (known gap: never a silent bad binary)"
    fi
}

expect_runs() {
    local backend="$1" fixture="$2" want="$3"
    local exe="run_${backend}_${fixture%.pgy}.exe"
    compile "$backend" "$fixture" "$exe" ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    local got
    got="$("$OUT_DIR/$exe")" || fail "$backend/$fixture crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend/$fixture printed '$got', expected '$want'"
}

# C guard: constructed-over-T rejected in both positions, with the diagnostic.
expect_reject c nested_param.pgy  "inside a constructed type"
expect_reject c nested_return.pgy "inside a constructed type"

# LLVM voice: params need concrete metadata; returns genuinely work.
expect_reject llvm nested_param.pgy "requires concrete argument"
expect_runs   llvm nested_return.pgy "7"
expect_runs   llvm body_local.pgy    "9"

# C body-local: known diagnostic-quality gap — must still FAIL, never emit
# a silent bad binary. (Real substitution = board feature decision.)
expect_reject_any c body_local.pgy

# No false positives: bare-T generics stay green on both backends.
expect_runs c    bare_ok.pgy "42"
expect_runs llvm bare_ok.pgy "42"

echo "[generic-nested] fail-closed + capability asymmetry locked (c/llvm)"
