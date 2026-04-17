#!/usr/bin/env bash
set -euo pipefail

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

files_equal() {
    local left="$1"
    local right="$2"

    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$left" "$right"
        return $?
    fi

    if command -v git >/dev/null 2>&1; then
        git diff --no-index --quiet -- "$left" "$right"
        return $?
    fi

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import pathlib, sys
left = pathlib.Path(sys.argv[1]).read_bytes()
right = pathlib.Path(sys.argv[2]).read_bytes()
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi

    [[ "$(cat "$left")" == "$(cat "$right")" ]]
}

show_diff() {
    local left="$1"
    local right="$2"

    if command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
        return 0
    fi

    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index --no-prefix -- "$left" "$right" || true
        return 0
    fi

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import difflib, pathlib, sys
left = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines(True)
right = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace").splitlines(True)
sys.stdout.writelines(difflib.unified_diff(left, right, fromfile=sys.argv[1], tofile=sys.argv[2]))
PY
        return 0
    fi

    echo "--- left ---"
    cat "$left"
    echo "--- right ---"
    cat "$right"
    return 0
}

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
    PGY_BIN="${PGY_BIN}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_backend_compare.XXXXXX")"

cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

if [[ ! -x "$PGY_BIN" ]]; then
    echo "backend-compare: missing compiler binary: $PGY_BIN" >&2
    exit 1
fi

if [[ "${PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS:-0}" != "0" ]]; then
    ABI_PIPELINE_BIN="${PGY_ABI_PIPELINE_TEST_BIN:-}"
    if [[ -z "$ABI_PIPELINE_BIN" ]]; then
        echo "backend-compare: PGY_ABI_PIPELINE_TEST_BIN is required when same-process precheck is enabled" >&2
        exit 1
    fi
    if [[ ! -x "$ABI_PIPELINE_BIN" && -x "${ABI_PIPELINE_BIN}.exe" ]]; then
        ABI_PIPELINE_BIN="${ABI_PIPELINE_BIN}.exe"
    fi
    if [[ ! -x "$ABI_PIPELINE_BIN" ]]; then
        echo "backend-compare: missing ABI pipeline test binary: $ABI_PIPELINE_BIN" >&2
        exit 1
    fi
    PGY_ABI_PIPELINE_SAME_PROCESS=1 \
    PGY_ABI_PIPELINE_BACKEND=llvm \
    "$ABI_PIPELINE_BIN"
fi

resolve_native_bin() {
    local path="$1"
    if [[ -x "$path" ]]; then
        printf '%s\n' "$path"
    elif [[ "$path" != *.exe && -x "${path}.exe" ]]; then
        printf '%s.exe\n' "$path"
    else
        printf '%s\n' "$path"
    fi
}

run_case() {
    local case_ref="$1"
    local case_name
    local case_src_dir=""
    local source_rel=""
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

    if [[ -d "$ROOT_DIR/$case_ref" ]]; then
        case_name="$(basename "$case_ref")"
        case_src_dir="$ROOT_DIR/$case_ref"
        source_rel="$case_ref/main.pgy"
        mkdir -p "$WORK_DIR/$case_name"
        cp -R "$case_src_dir"/. "$WORK_DIR/$case_name/"
        source_copy="$WORK_DIR/$case_name/main.pgy"
    else
        source_rel="$case_ref"
        case_name="$(basename "$source_rel" .pgy)"
        source_copy="$WORK_DIR/${case_name}.pgy"
        cp "$ROOT_DIR/$source_rel" "$source_copy"
    fi

    c_bin="$WORK_DIR/${case_name}_c"
    llvm_bin="$WORK_DIR/${case_name}_llvm"
    c_compile_log="$WORK_DIR/${case_name}_c.compile.log"
    llvm_compile_log="$WORK_DIR/${case_name}_llvm.compile.log"
    c_out="$WORK_DIR/${case_name}_c.stdout"
    llvm_out="$WORK_DIR/${case_name}_llvm.stdout"
    c_err="$WORK_DIR/${case_name}_c.stderr"
    llvm_err="$WORK_DIR/${case_name}_llvm.stderr"

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

    c_bin="$(resolve_native_bin "$c_bin")"
    llvm_bin="$(resolve_native_bin "$llvm_bin")"

    if (cd "$(dirname "$source_copy")" && "$c_bin" >"$c_out" 2>"$c_err"); then
        c_rc=0
    else
        c_rc=$?
    fi
    if (cd "$(dirname "$source_copy")" && "$llvm_bin" >"$llvm_out" 2>"$llvm_err"); then
        llvm_rc=0
    else
        llvm_rc=$?
    fi

    if [[ "$c_rc" -ne "$llvm_rc" ]]; then
        echo "backend-compare: exit code mismatch for $source_rel (C=$c_rc LLVM=$llvm_rc)" >&2
        return 1
    fi

    if ! files_equal "$c_out" "$llvm_out"; then
        echo "backend-compare: stdout mismatch for $source_rel" >&2
        show_diff "$c_out" "$llvm_out" >&2
        return 1
    fi

    if ! files_equal "$c_err" "$llvm_err"; then
        echo "backend-compare: stderr mismatch for $source_rel" >&2
        show_diff "$c_err" "$llvm_err" >&2
        return 1
    fi

    echo "backend-compare: PASS $source_rel"
}

main() {
    local cases=(
        "tests/cases/backend_compare/basic"
        "tests/cases/backend_compare/slot_sugar"
        "tests/cases/backend_compare/break_continue"
        "tests/cases/backend_compare/array_enum"
        "tests/cases/backend_compare/dynamic_array"
        "tests/cases/backend_compare/string_io"
        "tests/cases/backend_compare/module_namespace"
        "tests/cases/backend_compare/role_operator"
        "tests/cases/backend_compare/host_method_class_return"
        "tests/cases/backend_compare/boilerplate_reduction"
        "tests/cases/backend_compare/intent_decl_overlay"
        "tests/cases/backend_compare/intent_conflict_runtime"
        "tests/cases/backend_compare/intent_trace_compensate"
        "tests/cases/backend_compare/intent_failure_observability_strings"
        "tests/cases/backend_compare/intent_zone_binding"
        "tests/cases/backend_compare/intent_cross_world_transfer"
        "tests/cases/backend_compare/intent_rich_history_identity"
        "tests/cases/backend_compare/zone_param_mutation"
        "tests/cases/backend_compare/zone_host_method_abi_combo"
        "tests/cases/backend_compare/ownership_forwarding"
        "tests/cases/backend_compare/generic_default_contracts"
        "tests/cases/backend_compare/generic_multi_bound_defaults"
        "tests/cases/backend_compare/intent_header_interleaved"
        "tests/cases/backend_compare/map_keys"
        "tests/cases/backend_compare/world_zone_projection_visibility"
        "tests/cases/backend_compare/relation_effect_propagation"
    )

    if [[ "$#" -gt 0 ]]; then
        cases=("$@")
    fi

    for source_rel in "${cases[@]}"; do
        run_case "$source_rel"
    done
}

main "$@"
