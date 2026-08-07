#!/usr/bin/env bash
# Owns DRV-2 implicit method-field carriage and fail-closed mutation checks.

pgy_selfhost_verify_driver_rung2_owner_field() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local emitted_c="$4"
    local driver_bin="$5"
    local missing_field
    local field_name
    local field_row
    local missing_row

    if [[ "$base" == "class_method_self_chain" ]]; then
        field_name="val"
        field_row='"name":"val","type":"Int"'
        missing_row='"name":"missing_val","type":"Int"'
        # Decl fields carry field_kind and sealed source ids now; pin the
        # identity columns, not the whole row spelling.
        grep -Eq '"name":"Inc","kind":"method","source_syntax_id":[1-9][0-9]*,"receiver_carriage":"value","owner":"Builder"' \
            "$self_mir_json" && grep -Fq \
            '"name":"val","type":"Int","field_kind":"field"' \
            "$self_mir_json" && grep -Fq \
            '"name":"limit","type":"Int","field_kind":"field"' \
            "$self_mir_json" && grep -Fq 'self.val' "$emitted_c" && \
            grep -Fq 'self.limit' "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend owner field facts drifted" >&2
            exit 1
        }
    elif [[ "$base" == "class_holds_enum_field" ]]; then
        field_name="tier"
        field_row='"name":"tier","type":"Tier"'
        missing_row='"name":"missing_tier","type":"Tier"'
        grep -Eq '"name":"Score","kind":"method","source_syntax_id":[1-9][0-9]*,"receiver_carriage":"value","owner":"Player"' \
            "$self_mir_json" && grep -Fq 'self.tier' "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend match owner field facts drifted" >&2
            exit 1
        }
    elif [[ "$base" == "owner_field_assignment" ]]; then
        field_name="balance"
        field_row='"name":"balance","type":"Int"'
        missing_row='"name":"missing_balance","type":"Int"'
        grep -Eq '"name":"Deposit","kind":"method","source_syntax_id":[1-9][0-9]*,"receiver_carriage":"value","owner":"Account"' \
            "$self_mir_json" && grep -Fq '"result":"balance.1"' \
            "$self_mir_json" && grep -Fq '"uses":["balance.0"]' \
            "$self_mir_json" && grep -Fq '"kind":"leaf","text":"amount"' \
            "$self_mir_json" && grep -Fq 'self.balance = ((self.balance + amount))' \
            "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend owner assignment facts drifted" >&2
            exit 1
        }
    else
        return 0
    fi

    grep -Fq "$field_row" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend owner field bindings were not emitted" >&2
        exit 1
    }

    missing_field="${self_mir_json%.json}.missing-owner-field.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_field" \
        "$field_row" "$missing_row"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_field")" \
        >"$missing_field.out" 2>"$missing_field.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing owner field was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: undefined_symbol" \
        "$missing_field.err" "$missing_field.out" && \
        grep -Fq -- "- name: $field_name" \
            "$missing_field.err" "$missing_field.out" || {
        echo "[self-host-parity:driver-rung2] $backend owner-field diagnostic drifted" >&2
        cat "$missing_field.out" "$missing_field.err" >&2
        exit 1
    }
}
