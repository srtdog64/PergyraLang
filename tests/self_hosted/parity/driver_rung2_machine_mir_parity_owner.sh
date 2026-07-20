#!/usr/bin/env bash
# Owns DRV-2 machine-fixture target input and command shaping.

pgy_selfhost_driver_rung2_fixture_base() {
    case "$1" in
        tests/cases/backend_compare/device_slot_machine_layer/main.pgy)
            printf '%s\n' "device_slot_machine_layer"
            ;;
        tests/cases/backend_compare/device_slot_remote/main.pgy)
            printf '%s\n' "device_slot_remote"
            ;;
        tests/cases/backend_compare/device_slot_routine/main.pgy)
            printf '%s\n' "device_slot_routine"
            ;;
        tests/cases/backend_compare/class_method_self_return/main.pgy)
            printf '%s\n' "class_method_self_return"
            ;;
        *)
            basename "$1" .pgy
            ;;
    esac
}

pgy_selfhost_driver_rung2_is_machine_fixture() {
    case "$1" in
        tests/cases/backend_compare/device_slot_machine_layer/main.pgy | \
            tests/cases/backend_compare/device_slot_remote/main.pgy | \
            tests/cases/backend_compare/device_slot_routine/main.pgy)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

pgy_selfhost_driver_rung2_machine_manifest_init() {
    DRIVER_RUNG2_MACHINE_MANIFEST="$ROOT_DIR/tests/self_hosted/fixtures/machine_layer_declaration.json"
    [[ -f "$DRIVER_RUNG2_MACHINE_MANIFEST" ]] || {
        echo "[self-host-parity:driver-rung2] missing target-owned machine declaration fixture" >&2
        exit 1
    }
    DRIVER_RUNG2_MACHINE_MANIFEST_REL="$(
        pgy_selfhost_path_relative_to_root "$DRIVER_RUNG2_MACHINE_MANIFEST"
    )"
}

pgy_selfhost_driver_rung2_produce_self_mir() {
    local machine_fixture="$1" backend="$2" base="$3" driver_bin="$4"
    local fixture_rel="$5" self_mir_json="$6"
    local -a command=("$driver_bin" --emit-mir-json-verified "$fixture_rel")
    if [[ "$machine_fixture" -eq 1 ]]; then
        if (cd "$ROOT_DIR" && "${command[@]}" \
            >"$self_mir_json.missing-manifest.out" \
            2>"$self_mir_json.missing-manifest.err"); then
            echo "[self-host-parity:driver-rung2] $backend machine MIR producer accepted missing declaration: $base" >&2
            return 1
        fi
        command+=("$DRIVER_RUNG2_MACHINE_MANIFEST_REL")
    fi
    (cd "$ROOT_DIR" && "${command[@]}" \
        >"$self_mir_json.raw" 2>"$self_mir_json.err")
}

pgy_selfhost_verify_driver_rung2_machine_facts() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local machine_fact
    [[ "$machine_fixture" -eq 1 ]] || return 0
    for machine_fact in \
        '"physical_base":268435456' \
        '"physical_size":4096' \
        '"physical_mode":"volatile"' \
        '"machine_contact_kind":"claim"' \
        '"machine_contact_kind":"write"' \
        '"machine_contact_kind":"release"'; do
        grep -Fq "$machine_fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend machine MIR ABI fact drifted: $base/$machine_fact" >&2
            return 1
        }
    done
    if [[ "$base" == "device_slot_machine_layer" \
        || "$base" == "device_slot_routine" ]]; then
        machine_fact='"machine_contact_kind":"read"'
    else
        machine_fact='"machine_contact_kind":"submit-read"'
    fi
    grep -Fq "$machine_fact" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend machine contact was lost: $base/$machine_fact" >&2
        return 1
    }
}

pgy_selfhost_driver_rung2_canonicalize() {
    local machine_fixture="$1" driver_bin="$2" mode="$3" input_arg="$4"
    local output="$5"
    local -a command=("$driver_bin" "$mode" "$input_arg")
    if [[ "$machine_fixture" -eq 1 ]]; then
        command+=("$DRIVER_RUNG2_MACHINE_MANIFEST_REL")
    fi
    (cd "$ROOT_DIR" && "${command[@]}" | tr -d '\r' >"$output")
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
    if [[ "$machine_fixture" -eq 1 ]]; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=("$actual" -o "$output_bin")
    if [[ "$machine_fixture" -eq 1 ]]; then
        command+=(-lm)
    fi
    "${command[@]}" >"$log" 2>&1
}
