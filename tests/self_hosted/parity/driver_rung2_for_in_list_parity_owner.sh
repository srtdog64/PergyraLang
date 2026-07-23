#!/usr/bin/env bash
# Owns List<T> foreach iteration facts, graph shape, and List ABI use.
# source_local_type_as_iteration_authority and mir_foreach_collection_type_guess
# are forbidden; the MIR iteration row is required and mutation-gated below.

pgy_selfhost_verify_driver_rung2_for_in_list() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_fact out err
    [[ "$base" == "for_in_list_int" ]] || return 0

    for fact in \
        '"source_syntax_id":' \
        '"iteration_type_facts":[{"function_syntax_id":' \
        '"binding_type":"Int","iterable_type":"List<Int>"' \
        '"kind":"branch","name":"branch"' \
        '"expr0":"values","expr0_graph":{"root":0'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend List foreach fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_fact="$BUILD_DIR/${base}_${backend}.missing-iteration-fact.mir.json"
    out="$missing_fact.out"
    err="$missing_fact.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_fact" \
        '"iteration_type_facts":[' '"iteration_type_facts_removed":['
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_fact")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing List foreach fact was accepted" >&2
        exit 1
    fi
    grep -Eq 'routine MIR fact index is incomplete: Main \[iteration_type_facts\]|MIR instruction expression graph is missing or invalid' \
        "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend List foreach missing-fact diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_for_in_list_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "for_in_list_int" ]] || return 0
    for symbol in pgy_list_size_int pgy_list_get_int; do
        grep -Fq "$symbol" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend List foreach runtime symbol missing: $symbol" >&2
            exit 1
        }
    done
    grep -Fq 'total + value' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend List foreach binding was not carried into the body" >&2
        exit 1
    }
}
