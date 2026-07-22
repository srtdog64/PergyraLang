#!/usr/bin/env bash
# Owns DRV-2 for-range and identifier-foreach expression graph checks.

pgy_selfhost_verify_driver_rung2_iteration_graph() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_graph

    if [[ "$base" == "forloop" ]]; then
        grep -Fq '"iteration_type_fact_count":1' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend range iteration type fact missing" >&2
            exit 1
        }
        grep -Fq '"binding_type":"Int","iterable_type":"Int"' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend range iteration type fact drifted" >&2
            exit 1
        }
        grep -Fq '"kind":"loop-init","name":"loop-init","result":null,"arg0":"i","arg1":null' \
            "$self_mir_json" && \
        grep -Fq '"expr0":"0","expr0_graph":{"root":0,"nodes":[{"kind":"integer_literal","text":"0"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend range-start graph drifted" >&2
            exit 1
        }
        grep -Fq '"kind":"branch","name":"branch","result":null,"arg0":"i","arg1":null' \
            "$self_mir_json" && \
        grep -Fq '"expr0":"0","expr0_graph":{"root":0,"nodes":[{"kind":"integer_literal","text":"3"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend range-stop graph drifted" >&2
            exit 1
        }
        missing_graph="${self_mir_json%.json}.missing-range-stop.mir.json"
        sed 's/\("kind":"branch"[^}]*"expr0":"0","expr0_graph\)/\1_removed/g' \
            "$self_mir_json" >"$missing_graph"
        pgy_selfhost_verify_driver_rung2_integer_literal_kind \
            "$backend" "$self_mir_json" "$driver_bin"
    elif [[ "$base" == "for_each" ]]; then
        grep -Fq '"iteration_type_fact_count":2' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach iteration type fact count drifted" >&2
            exit 1
        }
        for iteration_fact in \
            '"binding_type":"Int","iterable_type":"Array<Int>"' \
            '"binding_type":"String","iterable_type":"Array<String>"'; do
            grep -Fq "$iteration_fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend foreach iteration type fact drifted: $iteration_fact" >&2
                exit 1
            }
        done
        for collection in nums names; do
            grep -Fq "\"expr0\":\"$collection\",\"expr0_graph\":{\"root\":0,\"nodes\":[{\"kind\":\"leaf\",\"text\":\"$collection\"" \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend foreach graph drifted: $collection" >&2
                exit 1
            }
        done
        grep -Fq '"arg0":"n","arg1":null' "$self_mir_json" && \
        grep -Fq '"expr0":"nums","expr0_graph":null,"expr1":"nums"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Int foreach branch drifted" >&2
            exit 1
        }
        grep -Fq '"arg0":"name","arg1":null' "$self_mir_json" && \
        grep -Fq '"expr0":"names","expr0_graph":null,"expr1":"names"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend String foreach branch drifted" >&2
            exit 1
        }
        for fact in '"name":"n","type":"Int"' \
            '"name":"name","type":"String"' \
            '"uses":["nums.1"]' '"uses":["names.1"]'; do
            grep -Fq "$fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend foreach fact drifted: $fact" >&2
                exit 1
            }
        done
        missing_graph="${self_mir_json%.json}.missing-foreach-value.mir.json"
        sed 's/\("kind":"loop-init"[^}]*"arg0":"n","arg1":null[^}]*"expr0":"nums","expr0_graph\)/\1_removed/g' \
            "$self_mir_json" >"$missing_graph"
    else
        return 0
    fi

    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
        >"$missing_graph.out" 2>"$missing_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing iteration graph was accepted: $base" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_graph.err" "$missing_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend iteration graph diagnostic drifted: $base" >&2
        cat "$missing_graph.out" "$missing_graph.err" >&2
        exit 1
    }
}
