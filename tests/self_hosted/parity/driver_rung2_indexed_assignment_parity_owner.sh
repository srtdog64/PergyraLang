#!/usr/bin/env bash
# Owns indexed-assignment target graph and no-text-recovery checks.
pgy_selfhost_verify_driver_rung2_indexed_assignment() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_graph
    [[ "$base" == "indexed_assignment" ]] || return 0
    grep -Fq '"expr1":"values[i]"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend indexed target fact was lost" >&2
        exit 1
    }
    grep -Fq '"expr1_graph":{' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend indexed target graph was lost" >&2
        exit 1
    }
    grep -Fq '"kind":"index","text":"values[i]"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend indexed target graph shape drifted" >&2
        exit 1
    }
    grep -Fq '"uses":["values.1","i.1","j.1"]' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend indexed target/RHS use facts drifted" >&2
        exit 1
    }
    missing_graph="$BUILD_DIR/${base}_${backend}.missing-target-graph.mir.json"
    sed 's/"expr1_graph"/"expr1_graph_removed"/g' \
        "$self_mir_json" >"$missing_graph"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
        >"$missing_graph.out" 2>"$missing_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend indexed assignment accepted a missing target graph" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_graph.err" "$missing_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend indexed target graph diagnostic drifted" >&2
        exit 1
    }
    if grep -R -Fq "CodegenSemanticAssignmentIndexExprOrDie" \
        "$ROOT_DIR/src/self_hosted/codegen" || \
        grep -Fq "IntEval(idx_expr" \
        "$ROOT_DIR/src/self_hosted/codegen/emission/stmt_emit.pgy" || \
        grep -Fq 'StringIndexOf(target, "[")' \
        "$ROOT_DIR/src/self_hosted/mir/program_verify_owner.pgy"; then
        echo "[self-host-parity:driver-rung2] indexed assignment reopened text index recovery" >&2
        exit 1
    fi
}
