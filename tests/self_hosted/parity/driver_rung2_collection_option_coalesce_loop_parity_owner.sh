#!/usr/bin/env bash
# Owns collection element -> Option call -> coalesce -> scalar loop state as one Pergyra seam.

pgy_selfhost_collection_option_coalesce_reject_mutation() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local label="$5" from="$6" to="$7" diagnostic="$8" mutated
    mutated="$BUILD_DIR/${base}_${backend}.${label}.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$mutated" "$from" "$to"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend collection/Option/coalesce mutation accepted: $label" >&2
        exit 1
    fi
    grep -Fq "$diagnostic" "$mutated.err" "$mutated.out" || {
        echo "[self-host-parity:driver-rung2] $backend collection/Option/coalesce diagnostic drifted: $label" >&2
        cat "$mutated.out" "$mutated.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_collection_option_coalesce_loop() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact row kind target expected actual
    [[ "$base" == "coalesce_accumulate_loop" ]] || return 0

    if ! grep -Eq '"name":"arr","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"' \
        "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend collection/Option/coalesce parameter row drifted" >&2
        exit 1
    fi
    for fact in \
        '"name":"ParityVal","kind":"function"' \
        '"return":"Option<Int>"' \
        '"kind":"index","text":"arr[i]"' \
        '"kind":"coalesce","text":"ParityVal(arr[i]) ?? (-1)"' \
        '"kind":"add","text":"total + (ParityVal(arr[i]) ?? (-1))"' \
        '"kind":"phi","name":"total","result":"total.3"' \
        '"uses":["total.1","total.6"]' \
        '"kind":"phi","name":"i","result":"i.4"' \
        '"uses":["i.1","i.7"]'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend collection/Option/coalesce fact drifted: $fact" >&2
            exit 1
        }
    done

    for row in direct:ParityVal:1 direct:SumEvenWeighted:4; do
        IFS=: read -r kind target expected <<<"$row"
        actual="$(grep -Fo "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
            "$self_mir_json" | wc -l | tr -d ' ')"
        [[ "$actual" -eq "$expected" ]] || {
            echo "[self-host-parity:driver-rung2] $backend collection/Option/coalesce target drifted: $target=$actual/$expected" >&2
            exit 1
        }
    done

    pgy_selfhost_collection_option_coalesce_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        missing-target '"call_target_kind":"direct","call_target_name":"ParityVal"' '"call_target_kind":"none","call_target_name":""' \
        "MIR instruction expression graph is missing or invalid"
    pgy_selfhost_collection_option_coalesce_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        index-kind '"kind":"index","text":"arr[i]"' '"kind":"leaf","text":"arr[i]"' \
        "MIR instruction expression graph is missing or invalid"
    pgy_selfhost_collection_option_coalesce_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        operator-kind '"kind":"coalesce","text":"ParityVal(arr[i]) ?? (-1)"' '"kind":"logical_or","text":"ParityVal(arr[i]) ?? (-1)"' \
        "Code: binop_type_mismatch"
    pgy_selfhost_collection_option_coalesce_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        loop-phi '"uses":["total.1","total.6"]' '"uses":["total.1"]' \
        "MIR phi facts are missing or inconsistent: SumEvenWeighted"
}
