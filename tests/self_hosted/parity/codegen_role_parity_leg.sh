#!/usr/bin/env bash
# Positive role-operator leg sourced by codegen_parity.sh.

run_role_operator_parity() {
    local backend="$1"
    local tool_bin="$2"
    local base="role_operator"
    local source_rel
    source_rel="$(path_relative_to_root "$ROLE_SOURCE")"

    local oracle_exe="$ABS_BUILD/${base}_oracle.exe"
    local oracle_compile_out="$ABS_BUILD/${base}_oracle_compile.out"
    local oracle_compile_err="$ABS_BUILD/${base}_oracle_compile.err"
    if ! run_native_capture "$ROOT_DIR" "$oracle_compile_out" "$oracle_compile_err" "$PGY" \
        "$(path_relative_to_root "$ROLE_SOURCE")" --backend=c \
        -o "$(path_relative_to_root "$oracle_exe")"; then
        echo "[self-host-parity:codegen] role operator oracle failed to build" >&2
        cat "$oracle_compile_out" "$oracle_compile_err" >&2
        exit 1
    fi
    local oracle_raw="$ABS_BUILD/${base}_oracle.out.raw"
    local oracle_out="$ABS_BUILD/${base}_oracle.out"
    local oracle_err="$ABS_BUILD/${base}_oracle.err"
    if ! run_native_capture "$ROOT_DIR" "$oracle_raw" "$oracle_err" "$oracle_exe"; then
        echo "[self-host-parity:codegen] role operator oracle failed to run" >&2
        cat "$oracle_err" >&2
        exit 1
    fi
    tr -d '\r' < "$oracle_raw" > "$oracle_out"
    compare_run_output_with_owner "c-oracle" "$base" "$ROLE_EXPECTED" "$oracle_out" 0

    local c_file="$ABS_BUILD/${base}_${backend}.c"
    local tool_rc
    set +e
    run_native_capture "$ROOT_DIR" "$c_file.raw" "$c_file.err" \
        "$tool_bin" --source "$source_rel"
    tool_rc="$?"
    set -e
    tr -d '\r' < "$c_file.raw" > "$c_file"
    if [[ "$tool_rc" -ne 0 ]]; then
        echo "[self-host-parity:codegen] backend=$backend role codegen exit=$tool_rc" >&2
        cat "$c_file" "$c_file.err" >&2
        exit 1
    fi

    local exe="$ABS_BUILD/${base}_${backend}.exe"
    if ! "$CC" "$c_file" -o "$exe" 2>"$ABS_BUILD/${base}_${backend}_cc.err"; then
        echo "[self-host-parity:codegen] backend=$backend role emitted C failed" >&2
        cat "$ABS_BUILD/${base}_${backend}_cc.err" >&2
        exit 1
    fi
    local run_raw="$ABS_BUILD/${base}_${backend}.out.raw"
    local run_out="$ABS_BUILD/${base}_${backend}.out"
    local run_err="$ABS_BUILD/${base}_${backend}.err"
    if ! run_native_capture "$ROOT_DIR" "$run_raw" "$run_err" "$exe"; then
        echo "[self-host-parity:codegen] backend=$backend role executable failed" >&2
        cat "$run_err" >&2
        exit 1
    fi
    tr -d '\r' < "$run_raw" > "$run_out"
    compare_run_output_with_owner "$backend" "$base" "$ROLE_EXPECTED" "$run_out" 2
    echo "[self-host-parity:codegen] backend=$backend role operator run-stdout equal"
}
