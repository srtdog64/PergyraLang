#!/usr/bin/env bash
# Owns DRV-2 implicit method-field carriage and fail-closed mutation checks.

pgy_selfhost_verify_driver_rung2_owner_field() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local emitted_c="$4"
    local driver_bin="$5"
    local missing_field

    [[ "$base" == "class_method_self_chain" ]] || return 0

    grep -Fq '"name":"Inc","kind":"method","owner":"Builder"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend method owner fact drifted" >&2
        exit 1
    }
    grep -Fq '"fields":[{"name":"val","type":"Int"},{"name":"limit","type":"Int"}]' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend owner field rows drifted" >&2
        exit 1
    }
    grep -Fq 'self.val' "$emitted_c" && grep -Fq 'self.limit' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend owner field bindings were not emitted" >&2
        exit 1
    }

    missing_field="${self_mir_json%.json}.missing-owner-field.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_field" \
        '"name":"val","type":"Int"' \
        '"name":"missing_val","type":"Int"'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_field")" \
        >"$missing_field.out" 2>"$missing_field.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing owner field was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: undefined_symbol" \
        "$missing_field.err" "$missing_field.out" && \
        grep -Fq -- "- name: val" \
            "$missing_field.err" "$missing_field.out" || {
        echo "[self-host-parity:driver-rung2] $backend owner-field diagnostic drifted" >&2
        cat "$missing_field.out" "$missing_field.err" >&2
        exit 1
    }
}
