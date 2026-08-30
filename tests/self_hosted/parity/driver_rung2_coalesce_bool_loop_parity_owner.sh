#!/usr/bin/env bash
# Owns Option<Bool> coalesce inside an indexed collection loop.

pgy_selfhost_coalesce_bool_loop_reject_mutation() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local label="$5" from="$6" to="$7" diagnostic="$8" mutated
    mutated="$BUILD_DIR/${base}_${backend}.${label}.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$mutated" "$from" "$to"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend coalesce-bool-loop mutation accepted: $label" >&2
        exit 1
    fi
    grep -Fq "$diagnostic" "$mutated.err" "$mutated.out" || {
        echo "[self-host-parity:driver-rung2] $backend coalesce-bool-loop diagnostic drifted: $label" >&2
        cat "$mutated.out" "$mutated.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_coalesce_bool_loop() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact
    [[ "$base" == "coalesce_in_if_condition" ]] || return 0

    if ! grep -Eq '"name":"arr","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"' \
        "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend coalesce-bool-loop parameter row drifted" >&2
        exit 1
    fi
    for fact in \
        '"name":"MaybeFlag","kind":"function"' \
        '"return":"Option<Bool>"' \
        '"kind":"index","text":"arr[i]"' \
        '"kind":"coalesce","text":"MaybeFlag(arr[i]) ?? false"' \
        '"kind":"phi","name":"count","result":"count.3"' \
        '"uses":["count.1","count.8"]' \
        '"kind":"phi","name":"i","result":"i.4"' \
        '"uses":["i.1","i.9"]'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend coalesce-bool-loop fact drifted: $fact" >&2
            exit 1
        }
    done
    local target_count
    target_count="$(grep -Fo '"call_target_kind":"direct","call_target_name":"MaybeFlag"' \
        "$self_mir_json" | wc -l | tr -d ' ')"
    [[ "$target_count" -eq 1 ]] || {
        echo "[self-host-parity:driver-rung2] $backend MaybeFlag target cardinality drifted: $target_count" >&2
        exit 1
    }

    pgy_selfhost_coalesce_bool_loop_reject_mutation "$backend" "$base" \
        "$self_mir_json" "$driver_bin" \
        missing-target \
        '"call_target_kind":"direct","call_target_name":"MaybeFlag"' \
        '"call_target_kind":"none","call_target_name":""' \
        "MIR instruction expression graph is missing or invalid"
    pgy_selfhost_coalesce_bool_loop_reject_mutation "$backend" "$base" \
        "$self_mir_json" "$driver_bin" \
        index-kind '"kind":"index","text":"arr[i]"' \
        '"kind":"leaf","text":"arr[i]"' \
        "MIR instruction expression graph is missing or invalid"
    pgy_selfhost_coalesce_bool_loop_reject_mutation "$backend" "$base" \
        "$self_mir_json" "$driver_bin" \
        operator-kind '"kind":"coalesce","text":"MaybeFlag(arr[i]) ?? false"' \
        '"kind":"logical_or","text":"MaybeFlag(arr[i]) ?? false"' \
        "Code: statement_type_unresolved"
    pgy_selfhost_coalesce_bool_loop_reject_mutation "$backend" "$base" \
        "$self_mir_json" "$driver_bin" \
        loop-phi '"uses":["count.1","count.8"]' '"uses":["count.1"]' \
        "MIR phi facts are missing or inconsistent: CountActive"
}
