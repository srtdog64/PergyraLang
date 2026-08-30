#!/usr/bin/env bash
# Owns class plus growable-array composition as one Pergyra semantic seam.

pgy_selfhost_class_array_reject_mutation() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local label="$5" from="$6" to="$7" diagnostic="$8" mutated
    mutated="$BUILD_DIR/${base}_${backend}.${label}.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$mutated" "$from" "$to"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend class/array mutation accepted: $label" >&2
        exit 1
    fi
    grep -Fq "$diagnostic" "$mutated.err" "$mutated.out" || {
        echo "[self-host-parity:driver-rung2] $backend class/array diagnostic drifted: $label" >&2
        cat "$mutated.out" "$mutated.err" >&2
        exit 1
    }
}

pgy_selfhost_class_array_reject_missing_target() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local kind="$5" target="$6"
    pgy_selfhost_class_array_reject_mutation \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "$target.missing-target" \
        "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
        '"call_target_kind":"none","call_target_name":""' \
        "MIR instruction expression graph is missing or invalid"
}

pgy_selfhost_verify_driver_rung2_class_array_composition() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact row kind target expected actual mutation_from mutation_to
    local -a facts parameter_rows target_rows
    case "$base" in
        class_with_array_param)
            facts=(
                '"name":"FillArr","kind":"function"' '"return":"Array<Int>"'
                '"kind":"phi","name":"arr","result":"arr.3"' '"uses":["arr.1","arr.6"]'
                '"kind":"phi","name":"total","result":"total.3"' '"uses":["total.1","total.6"]'
                '"kind":"member_access","text":"slot.id"' '"kind":"member_access","text":"slot.mark"'
                '"expr1":"arr[i]","expr1_graph":{"root":2')
            parameter_rows=(
                '"name":"slot","source_syntax_id":[1-9][0-9]*,"type":"Slot2","carriage":"value"'
                '"name":"arr","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"')
            target_rows=(direct:FillArr:1 direct:SumWith:4 direct:Slot2:4)
            ;;
        class_param_method_arr)
            facts=(
                '"name":"Worth","kind":"method","source_syntax_id":5,'
                '"receiver_carriage":"value","owner":"Bag2"'
                '"kind":"phi","name":"total","result":"total.4"' '"uses":["total.1","total.7"]'
                '"kind":"member_access","text":"bag.Worth"'
                '"kind":"index","text":"rates[i]"'
                '"kind":"call_argument","text":"bag.Worth(rates[i])"')
            parameter_rows=(
                '"name":"rate","source_syntax_id":[1-9][0-9]*,"type":"Int","carriage":"value"'
                '"name":"rates","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"')
            target_rows=(direct:Bag2:1 member:Bag2_Worth:1 direct:TotalWorth:4)
            ;;
        *) return 0 ;;
    esac

    for fact in "${facts[@]}"; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend class/array fact drifted: $fact" >&2
            exit 1
        }
    done
    for row in "${parameter_rows[@]}"; do
        grep -Eq "$row" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend class/array parameter row drifted: $row" >&2
            exit 1
        }
    done
    for row in "${target_rows[@]}"; do
        IFS=: read -r kind target expected <<<"$row"
        actual="$(grep -Fo "\"call_target_kind\":\"$kind\",\"call_target_name\":\"$target\"" \
            "$self_mir_json" | wc -l | tr -d ' ')"
        [[ "$actual" -eq "$expected" ]] || {
            echo "[self-host-parity:driver-rung2] $backend class/array target count drifted: $target=$actual/$expected" >&2
            exit 1
        }
        pgy_selfhost_class_array_reject_missing_target \
            "$backend" "$base" "$self_mir_json" "$driver_bin" "$kind" "$target"
    done

    if [[ "$base" == "class_with_array_param" ]]; then
        pgy_selfhost_class_array_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
            index-target-graph '"expr1":"arr[i]","expr1_graph":{' '"expr1":"arr[i]","expr1_graph_removed":{' \
            "MIR instruction expression graph is missing or invalid"
        mutation_from="$(grep -Eo '"name":"arr","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"' \
            "$self_mir_json" | head -n 1)"
        mutation_to="${mutation_from/\"type\":\"Array<Int>\"/\"type\":\"Unknown\"}"
        pgy_selfhost_class_array_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
            array-param-type "$mutation_from" "$mutation_to" \
            "Code: assignment_type_unresolved"
    else
        pgy_selfhost_class_array_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
            index-kind '"kind":"index","text":"rates[i]"' '"kind":"leaf","text":"rates[i]"' \
            "MIR instruction expression graph is missing or invalid"
        mutation_from="$(grep -Eo '"name":"rates","source_syntax_id":[1-9][0-9]*,"type":"Array<Int>","carriage":"value"' \
            "$self_mir_json" | head -n 1)"
        mutation_to="${mutation_from/\"type\":\"Array<Int>\"/\"type\":\"Array<String>\"}"
        pgy_selfhost_class_array_reject_mutation "$backend" "$base" "$self_mir_json" "$driver_bin" \
            array-element-type "$mutation_from" "$mutation_to" \
            "Code: call_arg_type_mismatch"
    fi
}
