#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export LC_ALL=C
LLVM_CASES="$(mktemp)"
COMPARE_CASES="$(mktemp)"
MISSING_CASES="$(mktemp)"
trap 'rm -f "$LLVM_CASES" "$COMPARE_CASES" "$MISSING_CASES"' EXIT

require_term() {
    local rel="$1"
    local term="$2"

    if ! grep -Fq "$term" "$ROOT_DIR/$rel"; then
        echo "backend-compare-llvm-coverage: $rel missing term: $term" >&2
        exit 1
    fi
}

require_term "tests/compare_backends.sh" \
    "run_windows_compiler_backend_fallback()"
require_term "tests/compare_backends.sh" \
    "run_compiler_backend()"
require_term "tests/compare_backends.sh" \
    "run_windows_compiler_backend_fallback \\"
require_term "tests/compare_backends.sh" \
    'if [[ "$rc" -eq 126 || "$rc" -eq 127 ]]; then'
require_term "tests/compare_backends.sh" \
    'if ! run_compiler_backend "$source_arg" "c" "$c_bin_arg" "$c_compile_log"; then'
require_term "tests/compare_backends.sh" \
    'if ! run_compiler_backend "$source_arg" "llvm" "$llvm_bin_arg" "$llvm_compile_log"; then'

sed -n 's/^run_case "\([^"]*\)".*/\1/p' \
    "$ROOT_DIR/tests/llvm_smoke.sh" \
    | sort -u > "$LLVM_CASES"

find "$ROOT_DIR/tests/cases/backend_compare" \
    -mindepth 1 -maxdepth 1 -type d \
    | sed 's#.*/##' \
    | sort -u > "$COMPARE_CASES"

comm -23 "$LLVM_CASES" "$COMPARE_CASES" > "$MISSING_CASES"

unexpected=0
while IFS= read -r case_name; do
    [ -n "$case_name" ] || continue
    case "$case_name" in
        qubit_slot)
            ;;
        *)
            echo "backend-compare-llvm-coverage: LLVM smoke case '$case_name' is missing backend-compare coverage" >&2
            unexpected=1
            ;;
    esac
done < "$MISSING_CASES"

if [ "$unexpected" -ne 0 ]; then
    echo "backend-compare-llvm-coverage: add the case under tests/cases/backend_compare or explicitly classify it out-of-beta" >&2
    exit 1
fi

if grep -qx 'qubit_slot' "$MISSING_CASES"; then
    echo "[backend-compare-llvm-coverage] LLVM-only allowlist ok: qubit_slot"
else
    echo "[backend-compare-llvm-coverage] all LLVM smoke cases have backend-compare coverage"
fi
