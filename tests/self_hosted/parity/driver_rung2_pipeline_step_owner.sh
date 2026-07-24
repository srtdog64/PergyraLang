#!/usr/bin/env bash
# Owns the generic DRV-2 per-fixture pipeline steps: canonicalize, MIR
# consumption, graph negatives, source emission, and emitted-C compilation.
# Split from driver_rung2_machine_mir_parity_owner.sh by responsibility; that
# owner keeps machine-fixture classification and machine-fact verification.

pgy_selfhost_driver_rung2_canonicalize() {
    local machine_fixture="$1" driver_bin="$2" mode="$3" input_arg="$4"
    local output="$5"
    local -a command=("$driver_bin" "$mode" "$input_arg")
    if [[ "$machine_fixture" -eq 1 ]]; then
        command+=("$DRIVER_RUNG2_MACHINE_MANIFEST_REL")
    fi
    if ! (cd "$ROOT_DIR" && "${command[@]}" | tr -d '\r' >"$output"); then
        echo "[self-host-parity:driver-rung2] MIR canonicalization failed: mode=$mode input=$input_arg" >&2
        cat "$output" >&2
        return 1
    fi
}

pgy_selfhost_driver_rung2_consume_mir() {
    local machine_fixture="$1" driver_bin="$2" input_arg="$3" output="$4"
    local error="$5"
    local -a command=("$driver_bin" --mir-json "$input_arg")
    if [[ "$machine_fixture" -eq 1 ]]; then
        command+=("$DRIVER_RUNG2_MACHINE_MANIFEST_REL")
    fi
    (cd "$ROOT_DIR" && "${command[@]}" >"$output" 2>"$error")
}

pgy_selfhost_driver_rung2_verify_graph_negatives() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local driver_bin="$5"
    if [[ "$machine_fixture" -eq 1 ]]; then
        pgy_selfhost_verify_driver_rung2_mir_graph_negatives \
            "$backend" "$base" "$self_mir_json" "$driver_bin" \
            "$DRIVER_RUNG2_MACHINE_MANIFEST_REL"
    else
        pgy_selfhost_verify_driver_rung2_mir_graph_negatives \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
    fi
    pgy_selfhost_verify_driver_rung2_defer_graph_negative \
        "$backend" "$base" "$self_mir_json" "$driver_bin"
}

pgy_selfhost_driver_rung2_emit_source() {
    local machine_fixture="$1" driver_bin="$2" fixture_rel="$3" output="$4"
    local error="$5"
    local -a command=("$driver_bin" "$fixture_rel" --emit-c-verified)
    if [[ "$machine_fixture" -eq 1 ]]; then
        command=("$driver_bin" "$fixture_rel" --machine-manifest-json \
            "$DRIVER_RUNG2_MACHINE_MANIFEST_REL")
    fi
    (cd "$ROOT_DIR" && "${command[@]}" >"$output" 2>"$error")
}

pgy_selfhost_driver_rung2_compile_emitted() {
    local machine_fixture="$1" actual="$2" output_bin="$3" log="$4"
    local -a command=("$CC" -x c -std=c11)
    local runtime_artifact="$machine_fixture"
    if pgy_selfhost_emitted_c_uses_runtime_headers "$actual"; then
        runtime_artifact=1
    fi
    if [[ "$runtime_artifact" -eq 1 ]]; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=("$actual" -o "$output_bin")
    if [[ "$machine_fixture" -eq 1 ]]; then
        command+=(-lm)
    fi
    "${command[@]}" >"$log" 2>&1
}
