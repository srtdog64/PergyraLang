#!/usr/bin/env bash
# Owns collection element -> enum decision -> match-loop state as one Pergyra seam.

pgy_selfhost_collection_enum_reject_mutation() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local label="$5" from="$6" to="$7" diagnostic="$8" mutated status
    mutated="$BUILD_DIR/${base}_${backend}.${label}.mir.json"
    if [[ "$label" == "loop-phi" ]]; then
        python -B "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_enum_phi_facts.py" \
            "$self_mir_json" drop-backedge "$mutated"
    else
        pgy_replace_first_literal "$self_mir_json" "$mutated" "$from" "$to"
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        status=0
    else
        status=$?
    fi
    if [[ "$status" -ne 1 ]]; then
        echo "[self-host-parity:driver-rung2] $backend collection/enum mutation exit drifted: $label=$status (expected 1)" >&2
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
    local fact row kind target expected actual mutation_from mutation_to
    [[ "$base" == "array_match_action_sim" ]] || return 0

    if ! grep -Eq '"name":"prices","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"' \
        "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend collection/enum parameter row drifted" >&2
        exit 1
    fi
    for fact in \
        '"kind":"enum","nominal_kind":"enum","name":"Action"' \
        '"variants":[{"name":"Buy","param_count":0,"param_types":[]},{"name":"Sell","param_count":0,"param_types":[]},{"name":"Hold","param_count":0,"param_types":[]}' \
        '"name":"DecideOf","kind":"function"' '"return":"Action"' \
        '"kind":"index","text":"prices[i]"' \
        '"match_patterns":["Buy"],"match_variant":null' \
        '"match_patterns":["Sell"],"match_variant":null' \
        '"match_patterns":["Hold"],"match_variant":null'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend collection/enum fact drifted: $fact" >&2
            exit 1
        }
    done
    python -B "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_enum_phi_facts.py" \
        "$self_mir_json" check

    # The producer materializes this one call once; the three arms use its SSA value.
    for row in direct:DecideOf:1 direct:Simulate:3; do
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
        match-variant '"name":"Hold","param_count":0,"param_types":[]}' '"name":"RemovedHold","param_count":0,"param_types":[]}' \
        "match enum variant declaration fact is missing"
    pgy_selfhost_collection_enum_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        loop-phi '' '' \
        "MIR phi facts are missing or inconsistent: Simulate"
    mutation_from="$(grep -Eo '"name":"prices","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"' \
        "$self_mir_json" | head -n 1)"
    mutation_to="${mutation_from/\"type\":\"Array<Int>\"/\"type\":\"Array<String>\"}"
    pgy_selfhost_collection_enum_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
        array-element-type "$mutation_from" "$mutation_to" \
        "Code: let_type_mismatch"
}
