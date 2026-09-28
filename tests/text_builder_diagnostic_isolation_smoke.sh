#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here text-builder-diagnostic-isolation "$PGY"
cd "$ROOT_DIR"

TMP_ROOT="$ROOT_DIR/.tmp/tests"
mkdir -p "$TMP_ROOT"
WORK="$(mktemp -d "$TMP_ROOT/text-builder-diagnostic-isolation.XXXXXX")"
trap 'rm -rf -- "$WORK"' EXIT
VALID="tests/cases/text_builder_owner/finish_after_loop_branch.pgy"
INVALID="tests/cases/text_builder_owner/finish_after_prior_error.pgy"
printf 'x\n' >"$WORK/expected.out"

for backend in c llvm; do
    "$PGY" --native-pipeline --backend="$backend" "$VALID" \
        -o "$WORK/$backend.exe"
    "$WORK/$backend.exe" >"$WORK/$backend.raw.out" \
        2>"$WORK/$backend.err"
    tr -d '\r' <"$WORK/$backend.raw.out" >"$WORK/$backend.out"
    cmp "$WORK/expected.out" "$WORK/$backend.out"
    [[ ! -s "$WORK/$backend.err" ]]
done
cmp "$WORK/c.out" "$WORK/llvm.out"

status=0
"$PGY" --native-pipeline --mir-json --error-format=json "$INVALID" \
    >"$WORK/invalid.out" 2>"$WORK/invalid.err" || status=$?
error_count="$(grep -o '"severity":"error"' "$WORK/invalid.err" \
    | wc -l | tr -d ' ')"
if [[ "$status" -ne 1 || "$error_count" -ne 1 ]] \
    || ! grep -Fq 'PGY_SEM_BORROW_ESCAPE' "$WORK/invalid.err" \
    || grep -Fq '"pgy.mir.v1"' "$WORK/invalid.out" \
    || grep -Fq 'PGY_SEM_OWNER_NOT_CONSUMED' "$WORK/invalid.err"; then
    echo '[text-builder-diagnostic-isolation] prior error poisoned later Finish' >&2
    exit 1
fi

echo '[text-builder-diagnostic-isolation] C/LLVM Finish and prior-error isolation PASS'
