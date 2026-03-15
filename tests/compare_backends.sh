#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
TMP_BASE="${TMPDIR:-/tmp}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_backend_compare.XXXXXX")"

cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

if [[ ! -x "$PGY_BIN" ]]; then
    echo "backend-compare: missing compiler binary: $PGY_BIN" >&2
    exit 1
fi

run_case() {
    local source_rel="$1"
    local case_name
    local source_copy
    local c_bin
    local llvm_bin
    local c_compile_log
    local llvm_compile_log
    local c_out
    local llvm_out
    local c_err
    local llvm_err
    local c_rc=0
    local llvm_rc=0

    case_name="$(basename "$source_rel" .pgy)"
    source_copy="$WORK_DIR/${case_name}.pgy"
    c_bin="$WORK_DIR/${case_name}_c"
    llvm_bin="$WORK_DIR/${case_name}_llvm"
    c_compile_log="$WORK_DIR/${case_name}_c.compile.log"
    llvm_compile_log="$WORK_DIR/${case_name}_llvm.compile.log"
    c_out="$WORK_DIR/${case_name}_c.stdout"
    llvm_out="$WORK_DIR/${case_name}_llvm.stdout"
    c_err="$WORK_DIR/${case_name}_c.stderr"
    llvm_err="$WORK_DIR/${case_name}_llvm.stderr"

    cp "$ROOT_DIR/$source_rel" "$source_copy"

    if ! "$PGY_BIN" "$source_copy" --backend=c -o "$c_bin" \
        >"$c_compile_log" 2>&1; then
        echo "backend-compare: C backend compile failed for $source_rel" >&2
        cat "$c_compile_log" >&2
        return 1
    fi

    if ! "$PGY_BIN" "$source_copy" --backend=llvm -o "$llvm_bin" \
        >"$llvm_compile_log" 2>&1; then
        echo "backend-compare: LLVM backend compile failed for $source_rel" >&2
        cat "$llvm_compile_log" >&2
        return 1
    fi

    if ! "$c_bin" >"$c_out" 2>"$c_err"; then
        c_rc=$?
    fi
    if ! "$llvm_bin" >"$llvm_out" 2>"$llvm_err"; then
        llvm_rc=$?
    fi

    if [[ "$c_rc" -ne "$llvm_rc" ]]; then
        echo "backend-compare: exit code mismatch for $source_rel (C=$c_rc LLVM=$llvm_rc)" >&2
        return 1
    fi

    if ! diff -u "$c_out" "$llvm_out" >/dev/null; then
        echo "backend-compare: stdout mismatch for $source_rel" >&2
        diff -u "$c_out" "$llvm_out" >&2 || true
        return 1
    fi

    if ! diff -u "$c_err" "$llvm_err" >/dev/null; then
        echo "backend-compare: stderr mismatch for $source_rel" >&2
        diff -u "$c_err" "$llvm_err" >&2 || true
        return 1
    fi

    echo "backend-compare: PASS $source_rel"
}

main() {
    local cases=(
        "examples/hello.pgy"
        "examples/minimal.pgy"
        "examples/slots.pgy"
    )

    if [[ "$#" -gt 0 ]]; then
        cases=("$@")
    fi

    for source_rel in "${cases[@]}"; do
        run_case "$source_rel"
    done
}

main "$@"
