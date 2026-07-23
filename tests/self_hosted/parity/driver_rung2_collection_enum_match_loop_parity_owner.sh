#!/usr/bin/env bash
# Owns collection element -> enum decision -> match-loop state as one Pergyra seam.

pgy_selfhost_collection_enum_reject_mutation() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local label="$5" from="$6" to="$7" diagnostic="$8" mutated
    mutated="$BUILD_DIR/${base}_${backend}.${label}.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$mutated" "$from" "$to"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend collection/enum mutation accepted: $label" >&2
        exit 1
    fi
    grep -Fq "$diagnostic" "$mutated.err" "$mutated.out" || {
        echo "[self-host-parity:driver-rung2] $backend collection/enum diagnostic drifted: $label" >&2
        cat "$mutated.out" "$mutated.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_collection_enum_match_loop() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact row kind target expected actual
    [[ "$base" == "array_match_action_sim" ]] || return 0

    for fact in \
        '"kind":"enum","nominal_kind":"enum","name":"Action"' \
        '"variants":[{"name":"Buy","param_count":0},{"name":"Sell","param_count":0},{"name":"Hold","param_count":0}]' \
        '"name":"DecideOf","kind":"function"' '"return":"Action"' \
        '"name":"prices","type":"Array<Int>","carriage":"value"' \
        '"kind":"index","text":"prices[i]"' \
        '"match_patterns":["Buy"],"match_variant":null' \
        '"match_patterns":["Sell"],"match_variant":null' \
        '"match_patterns":["Hold"],"match_variant":null' \
        '"kind":"phi","name":"cash","result":"cash.4"' \
        '"uses":["cash.1","cash.23"]' \
        '"kind":"phi","name":"shares","result":"shares.5"' \
        '"uses":["shares.1","shares.24"]' \
        '"kind":"phi","name":"i","result":"i.6"' \
        '"uses":["i.1","i.25"]'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend collection/enum fact drifted: $fact" >&2
            exit 1
        }
    done

    for row in direct:DecideOf:3 direct:Simulate:3; do
        IFS=: read -r kind target expected <<<"$row"
        actual="$(grep -Fo "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
            "$self_mir_json" | wc -l | tr -d ' ')"
        [[ "$actual" -eq "$expected" ]] || {
            echo "[self-host-parity:driver-rung2] $backend collection/enum target drifted: $target=$actual/$expected" >&2
            exit 1
        }
        pgy_selfhost_collection_enum_reject_mutation \
            "$backend" "$base" "$self_mir_json" "$driver_bin" \
            "$target.missing-target" \
            "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
            '"call_target_kind":"none","call_target_name":""' \
            "MIR instruction expression graph is missing or invalid"
    done

    pgy_selfhost_collection_enum_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        index-kind '"kind":"index","text":"prices[i]"' '"kind":"leaf","text":"prices[i]"' \
        "MIR instruction expression graph is missing or invalid"
    pgy_selfhost_collection_enum_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        match-variant '"name":"Hold","param_count":0}' '"name":"RemovedHold","param_count":0}' \
        "match enum variant declaration fact is missing"
    pgy_selfhost_collection_enum_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        loop-phi '"uses":["cash.1","cash.23"]' '"uses":["cash.1"]' \
        "MIR phi facts are missing or inconsistent: Simulate"
    pgy_selfhost_collection_enum_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        array-element-type '"name":"prices","type":"Array<Int>","carriage":"value"' '"name":"prices","type":"Array<String>","carriage":"value"' \
        "Code: let_type_mismatch"
}
