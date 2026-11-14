#!/usr/bin/env bash
# Owns nested Option<Int> coalesce identity through a local value chain.

pgy_selfhost_nested_coalesce_reject_mutation() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local label="$5" from="$6" to="$7" diagnostic="$8" mutated
    mutated="$BUILD_DIR/${base}_${backend}.${label}.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$mutated" "$from" "$to"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend nested-coalesce mutation accepted: $label" >&2
        exit 1
    fi
    grep -Fq "$diagnostic" "$mutated.err" "$mutated.out" || {
        echo "[self-host-parity:driver-rung2] $backend nested-coalesce diagnostic drifted: $label" >&2
        cat "$mutated.out" "$mutated.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_nested_coalesce() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact target_count
    [[ "$base" == "nested_coalesce_chain" ]] || return 0

    if ! grep -Eq '"name":"fallback","source_syntax_id":[1-9][0-9]*,"type":"Int","carriage":"value"' \
        "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend nested-coalesce parameter row drifted" >&2
        exit 1
    fi
    for fact in \
        '"name":"HalvedIfPositive","kind":"function"' \
        '"return":"Option<Int>"' \
        '"kind":"coalesce","text":"HalvedIfPositive(n) ?? fallback"' \
        '"kind":"coalesce","text":"HalvedIfPositive(first) ?? fallback"' \
        '"name":"first","type":"Int"' \
        '"name":"second","type":"Int"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend nested-coalesce fact drifted: $fact" >&2
            exit 1
        }
    done
    target_count="$(grep -Fo '"call_target_kind":"direct","call_target_name":"HalvedIfPositive"' \
        "$self_mir_json" | wc -l | tr -d ' ')"
    [[ "$target_count" -eq 2 ]] || {
        echo "[self-host-parity:driver-rung2] $backend nested-coalesce target cardinality drifted: $target_count" >&2
        exit 1
    }

    pgy_selfhost_nested_coalesce_reject_mutation "$backend" "$base" \
        "$self_mir_json" "$driver_bin" \
        missing-target \
        '"call_target_kind":"direct","call_target_name":"HalvedIfPositive"' \
        '"call_target_kind":"none","call_target_name":""' \
        "MIR instruction expression graph is missing or invalid"
    pgy_selfhost_nested_coalesce_reject_mutation "$backend" "$base" \
        "$self_mir_json" "$driver_bin" \
        first-coalesce-kind \
        '"kind":"coalesce","text":"HalvedIfPositive(n) ?? fallback"' \
        '"kind":"logical_or","text":"HalvedIfPositive(n) ?? fallback"' \
        "Code: initializer_type_unresolved"
}
