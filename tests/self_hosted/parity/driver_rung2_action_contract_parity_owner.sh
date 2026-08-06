#!/usr/bin/env bash
# Owns exact ActionContract carriage and fail-closed wire mutations for DRV-2.
# Gate contract: native/self MIR parity and vocabulary/field mutations fail closed
# CLOSED fallback identities owned by this gate: action_as_function_kind,
# action_clause_skip_to_body, action_clause_text_rescan,
# parser_caps_effects_discard, missing_contract_wire_success,
# callable_kind_default_function, backend_contract_recovery,
# independent_contract_vocabulary, multi_impl_role_declaration_drop.

if grep -Fq 'if impl_count > 1' \
    "$ROOT_DIR/src/self_hosted/mir/declaration_rows_owner.pgy"; then
    echo "[self-host-parity:driver-rung2] multi-ability role declaration fallback returned" >&2
    return 1 2>/dev/null || exit 1
fi
grep -Fq 'SemanticAstAbilityIndexForName(' \
    "$ROOT_DIR/src/self_hosted/mir/declaration_rows_owner.pgy" || {
    echo "[self-host-parity:driver-rung2] role impl method partition lost its ability owner" >&2
    return 1 2>/dev/null || exit 1
}

pgy_selfhost_driver_rung2_action_contract_reject() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local suffix="$5" from="$6" to="$7" diagnostic="$8"
    local mutated="$BUILD_DIR/${base}_${backend}.${suffix}.mir.json"

    if ! pgy_replace_first_literal \
        "$self_mir_json" "$mutated" "$from" "$to"; then
        echo "[self-host-parity:driver-rung2] $backend ActionContract mutation did not apply: $suffix" >&2
        return 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed ActionContract was accepted: $suffix" >&2
        return 1
    fi
    grep -Fq "$diagnostic" "$mutated.out" "$mutated.err" || {
        echo "[self-host-parity:driver-rung2] $backend ActionContract diagnostic drifted: $suffix" >&2
        cat "$mutated.out" "$mutated.err" >&2
        return 1
    }
    if grep -Fq '#include <stdio.h>' "$mutated.out"; then
        echo "[self-host-parity:driver-rung2] $backend malformed ActionContract reached C emission: $suffix" >&2
        return 1
    fi
}

pgy_selfhost_verify_driver_rung2_action_contract() {
    local backend="$1" base="$2" native_mir_json="$3"
    local self_mir_json="$4" driver_bin="$5"
    local contract
    if [[ "$base" != "function_clause_order_minimal" ]]; then
        return 0
    fi

    contract='"name":"Attack","return":"Void","callable_kind":"action","contract":{"requires":[{"base":"Combatable","actuals":[]},{"base":"Movable","actuals":[]}],"within":"BattleZone","causes":"Damage","authorized_by":["self","target"],"caps_present":true,"caps":["io_read","io_write"],"effects_present":true,"effects":["secure","remote"]}'
    grep -Fq "$contract" "$native_mir_json" || {
        echo "[self-host-parity:driver-rung2] native ActionContract carriage drifted" >&2
        return 1
    }
    grep -Fq "$contract" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend self ActionContract carriage drifted" >&2
        return 1
    }

    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "missing-within" '"within":"BattleZone"' \
        '"within_removed":"BattleZone"' \
        "method contract fields are incomplete or malformed"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "unknown-within" '"within":"BattleZone"' \
        '"within":"MissingActionContractZone"' \
        "action contract references an unknown zone"
    # An object decl demands machine-layer projection facts the subject-
    # authored wire never carried, so admission fail-closes before decl
    # lowering can name the action-owner rule. Either way the mutation
    # cannot smuggle an action onto a non-subject.
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "non-subject-owner" '"kind":"subject","nominal_kind":"subject"' \
        '"kind":"object","nominal_kind":"object"' \
        "MIR machine-layer facts are missing or invalid"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "action-as-function" '"callable_kind":"action"' \
        '"callable_kind":"function"' \
        "function method carries action-only contract facts"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "empty-caps" '"caps_present":true,"caps":["io_read","io_write"]' \
        '"caps_present":true,"caps":[]' \
        "method contract clause presence disagrees with its values"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "empty-effects" '"effects_present":true,"effects":["secure","remote"]' \
        '"effects_present":true,"effects":[]' \
        "method contract clause presence disagrees with its values"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "duplicate-cap" '"caps":["io_read","io_write"]' \
        '"caps":["io_read","io_read"]' \
        "method contract fields are incomplete or malformed"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "noncanonical-cap" '"caps":["io_read","io_write"]' \
        '"caps":["io_write","io_read"]' \
        "unknown or noncanonical capability"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "unknown-effect" '"effects":["secure","remote"]' \
        '"effects":["secure","missing_effect"]' \
        "invalid or noncanonical effect"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "local-mixed-first" '"effects":["secure","remote"]' \
        '"effects":["local","secure"]' \
        "invalid or noncanonical effect"
    pgy_selfhost_driver_rung2_action_contract_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "local-mixed-last" '"effects":["secure","remote"]' \
        '"effects":["secure","local"]' \
        "invalid or noncanonical effect"
}

pgy_selfhost_verify_driver_rung2_role_implicit_self_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "function_clause_order_minimal" ]] || return 0

    for signature in \
        'long long HeroCombat_Ping(void *_pgy_raw_self)' \
        'void HeroCombat_Move(void *_pgy_raw_self)'; do
        grep -Fq "$signature" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend implicit role-self ABI drifted: $signature" >&2
            return 1
        }
    done
    if grep -Eq 'HeroCombat_(Ping|Move)\(void\)' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend role method reopened a receiver-free C ABI" >&2
        return 1
    fi
}
