#!/usr/bin/env bash
#
# duration_literal_smoke.sh — the docs/181 SS2.3 duration literal:
#
#   - duration_literal (compare corpus) prints the exact normalized
#     nanosecond values on both backends:
#     1500000000 / 5000000000 / 250000 / 90 / 120000000000 / 3000000000
#   - reject_duration_int_mix     Int = 5s          -> type error
#     (Duration is DISTINCT from Int/Long; a bare number cannot pose
#     as time and time cannot silently collapse into a bare number)
#   - reject_duration_fractional  1.5s              -> parse error
#   - reject_duration_overflow    counts past 2^53ns -> parse error
#     (number literals carry a double; past the exactly-representable
#     range the value would drift silently, so it fails closed)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[duration-literal] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

SRC="$ROOT_DIR/tests/cases/backend_compare/duration_literal/main.pgy"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[duration-literal] FAIL: $*" >&2; exit 1; }

compile() {
    local backend="$1" src_path="$2" out_name="$3"
    local src out rc
    src="$(pgy_path_for_compiler "$PGY" "$src_path")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$out_name")"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$out_name.log" 2>&1
    rc=$?
    set -e
    return $rc
}

WANT="$(printf '1500000000\n5000000000\n250000\n90\n120000000000\n3000000000')"
BACKENDS="${PGY_DURATION_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    compile "$backend" "$SRC" "dur_$backend.exe" ||
        fail "$backend must compile: $(tail -2 "$OUT_DIR/dur_$backend.exe.log")"
    got="$("$OUT_DIR/dur_$backend.exe" | tr -d '\r')" ||
        fail "$backend crashed at runtime"
    [ "$got" = "$WANT" ] || fail "$backend printed '$got'"
done

expect_reject() {
    local name="$1" source="$2" needle="$3"
    printf '%s' "$source" > "$OUT_DIR/$name.pgy"
    if compile c "$OUT_DIR/$name.pgy" "rej_$name.exe"; then
        fail "$name compiled but must fail closed"
    fi
    grep -Fq "$needle" "$OUT_DIR/rej_$name.exe.log" ||
        fail "$name failed without the expected diagnostic: $needle"
}

expect_reject duration_int_mix \
    'func Main() -> Void { let x: Int = 5s; }' \
    "Type mismatch"
expect_reject duration_fractional \
    'func Main() -> Void { let d: Duration = 1.5s; }' \
    "bare integer count"
expect_reject duration_overflow \
    'func Main() -> Void { let d: Duration = 999999999999999s; }' \
    "exactly-representable nanosecond range"

echo "[duration-literal] 5-unit nanosecond normalization on: $BACKENDS; 3 reject shapes fail closed"
