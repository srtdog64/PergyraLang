#!/usr/bin/env bash
# Negative codegen leg sourced by codegen_parity.sh.

run_codegen_reject_case() {
    local backend="$1"
    local tool_bin="$2"
    local base="$3"
    local source="$4"
    local expected="$5"
    local label="$6"
    local reject_raw="$ABS_BUILD/${base}_${backend}.out.raw"
    local reject_norm="$ABS_BUILD/${base}_${backend}.out"
    local reject_err="$ABS_BUILD/${base}_${backend}.err"
    local source_rel
    local reject_rc

    source_rel="$(path_relative_to_root "$source")"

    set +e
    run_native_capture "$ROOT_DIR" "$reject_raw" "$reject_err" \
        "$tool_bin" --source "$source_rel"
    reject_rc="$?"
    set -e
    tr -d '\r' < "$reject_raw" > "$reject_norm"
    if [[ "$reject_rc" -eq 0 ]]; then
        echo "[self-host-parity:codegen] backend=$backend $label: codegen accepted unsupported input" >&2
        cat "$reject_norm" "$reject_err" >&2
        exit 1
    fi

    compare_run_output_with_owner \
        "$backend" "$base" "$expected" "$reject_norm" 2
    if [[ -s "$reject_err" ]]; then
        echo "[self-host-parity:codegen] backend=$backend $label: diagnostic leaked to stderr" >&2
        cat "$reject_err" >&2
        exit 1
    fi
    echo "[self-host-parity:codegen] backend=$backend $label fail-closed"
}
