#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR" || exit 2

DRIVER_BIN="${DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver.exe}"
OUT_DIR="${PGY_GRAMMAR_SELF_DRIVER_OUT:-$ROOT_DIR/build/grammar_self_driver}"
EXPECTED_COUNT=17

fail() {
    echo "[grammar-self-driver] $*" >&2
    exit 1
}

if [[ ! -x "$DRIVER_BIN" ]]; then
    fail "missing self-host driver: $DRIVER_BIN"
fi

mkdir -p "$OUT_DIR"
sources=()
while IFS= read -r source_path; do
    [[ -n "$source_path" ]] && sources+=("$source_path")
done < <(find grammar -type f -name '*.pgy' | sort)
[[ "${#sources[@]}" -eq "$EXPECTED_COUNT" ]] || \
    fail "fixture count drifted: ${#sources[@]} != $EXPECTED_COUNT"

for src in "${sources[@]}"; do
    # DRV-2 IO is repository-relative by contract.
    out_name="${src//\//_}.c"
    out_path="$OUT_DIR/$out_name"
    "$DRIVER_BIN" "$src" --emit-c-verified >"$out_path" || \
        fail "DRV-2 failed: $src"
    [[ -s "$out_path" ]] || fail "empty verified C: $src"
    grep -Fq '#include' "$out_path" || fail "C include surface missing: $src"
    grep -Eq 'int main\(' "$out_path" || fail "C main surface missing: $src"
done

echo "[grammar-self-driver] $EXPECTED_COUNT grammar fixtures pass DRV-2 verified C"
