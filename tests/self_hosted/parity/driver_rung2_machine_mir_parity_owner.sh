#!/usr/bin/env bash
# Owns DRV-2 machine-fixture target input and command shaping.
# Claim result-local and resource receiver graph owner

pgy_selfhost_driver_rung2_fixture_base() {
    local path="$1"
    local output_var="${2:-}"
    local base
    case "$1" in
        tests/cases/backend_compare/*/main.pgy)
            path="${path%/main.pgy}"
            base="${path##*/}"
            ;;
        *)
            path="${path##*/}"
            base="${path%.pgy}"
            ;;
    esac
    if [[ -n "$output_var" ]]; then
        printf -v "$output_var" '%s' "$base"
    else
        printf '%s\n' "$base"
    fi
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
    pgy_selfhost_verify_driver_rung2_machine_claim_type_owner || exit 1
    DRIVER_RUNG2_MACHINE_MANIFEST="$ROOT_DIR/tests/self_hosted/fixtures/machine_layer_declaration.json"
    [[ -f "$DRIVER_RUNG2_MACHINE_MANIFEST" ]] || {
        echo "[self-host-parity:driver-rung2] missing target-owned machine declaration fixture" >&2
        exit 1
    }
    DRIVER_RUNG2_MACHINE_MANIFEST_REL="$(
        pgy_selfhost_path_relative_to_root "$DRIVER_RUNG2_MACHINE_MANIFEST"
    )"
}

pgy_selfhost_verify_driver_rung2_machine_claim_type_owner() {
    local owner="$ROOT_DIR/src/self_hosted/mir/routine_build_owner.pgy"
    local layout_owner="$ROOT_DIR/src/self_hosted/mir/abi_layout_json_projection_owner.pgy"
    grep -Fq \
        'SelfMirSsaBaseName(cfg.instructions.results[instruction_index])' \
        "$owner" || {
        echo "[self-host-parity:driver-rung2] Claim ABI type lost result-local owner" >&2
        return 1
    }
    if grep -Fq 'return cfg.instructions.expr1s[instruction_index];' "$owner"; then
        echo "[self-host-parity:driver-rung2] Claim ABI type reopened expr1 text fallback" >&2
        return 1
    fi
    for owner_fact in \
        'cfg.instructions.expr0_graphs' \
        'AstExpressionNodeCallArgument()' \
        'graphs.left_children[receiver_wrapper]' \
        'graphs.right_children[receiver_wrapper]'; do
        grep -Fq "$owner_fact" "$owner" || {
            echo "[self-host-parity:driver-rung2] resource receiver graph owner missing: $owner_fact" >&2
            return 1
        }
    done
    for forbidden in \
        'cfg.instructions.uses[cfg.instructions.use_starts[instruction_index]]' \
        'SelfMirTextContainsIdentifier('; do
        if grep -Fq "$forbidden" "$owner"; then
            echo "[self-host-parity:driver-rung2] resource receiver fallback reopened: $forbidden" >&2
            return 1
        fi
    done
    for layout_fact in \
        'rows.source_types[instruction_index] == "AST_LET_DECL"' \
        'rows.source_types[instruction_index] == "AST_CALL"'; do
        grep -Fq "$layout_fact" "$layout_owner" || {
            echo "[self-host-parity:driver-rung2] ABI layout projection owner missing: $layout_fact" >&2
            return 1
        }
    done
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
    if [[ "$base" == "device_slot_machine_layer" ]]; then
        for machine_fact in \
            '"abi_type_name":"Int","abi_layout_id":0,"abi_layout_required":false,"abi_layout":null,"expr0":"DeviceRead(dev)"' \
            '"abi_type_name":null,"abi_layout_id":0,"abi_layout_required":false,"abi_layout":null,"expr0":"DeviceWrite(dev, current)"' \
            '"type":"DeviceSlot<Int>","operation":"Claim"' \
            '"type":"DeviceSlot<Int>","operation":"Read"' \
            '"type":"DeviceSlot<Int>","operation":"Write"' \
            '"type":"DeviceSlot<Int>","operation":"Release"'; do
            grep -Fq "$machine_fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend resource ABI projection drifted: $base/$machine_fact" >&2
                return 1
            }
        done
    fi
}
