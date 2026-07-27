#!/usr/bin/env bash
# Owns exact Effect declaration identity carriage for the active DRV-2 rung.
# Gate evidence: effect declaration and subject/effect slot mutations.
# CLOSED fallback identities: effect_as_class, missing_effect_declaration,
# unknown_nominal_as_struct.

PYTHON_BIN="${PYTHON_BIN:-python3}"
"$PYTHON_BIN" "$ROOT_DIR/scripts/render_mir_decl_field_kind_vocabulary.py" \
    "$ROOT_DIR/src/compiler/mir_decl_field_kind_vocabulary.def" \
    "$ROOT_DIR/src/self_hosted/lib/mir_decl_field_kind_vocabulary_projection_owner.pgy" \
    --check
grep -Fq '#include "mir_decl_field_kind_vocabulary.def"' \
    "$ROOT_DIR/src/compiler/mir_json_dump_decl.c" || {
    echo "[self-host-parity:driver-rung2] native field-kind registry projection detached" >&2
    return 1 2>/dev/null || exit 1
}

pgy_selfhost_driver_rung2_effect_declaration_reject() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local suffix="$5" from="$6" to="$7" diagnostic="$8"
    local mutated="$BUILD_DIR/${base}_${backend}.${suffix}.mir.json"

    if ! pgy_replace_first_literal \
        "$self_mir_json" "$mutated" "$from" "$to"; then
        echo "[self-host-parity:driver-rung2] $backend Effect declaration mutation did not apply: $suffix" >&2
        return 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        >"$mutated.out" 2>"$mutated.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed Effect declaration was accepted: $suffix" >&2
        return 1
    fi
    grep -Fq "$diagnostic" "$mutated.out" "$mutated.err" || {
        echo "[self-host-parity:driver-rung2] $backend Effect declaration diagnostic drifted: $suffix" >&2
        cat "$mutated.out" "$mutated.err" >&2
        return 1
    }
    if grep -Fq '#include <stdio.h>' "$mutated.out"; then
        echo "[self-host-parity:driver-rung2] $backend malformed Effect declaration reached C emission: $suffix" >&2
        return 1
    fi
}

pgy_selfhost_verify_driver_rung2_effect_declaration() {
    local backend="$1" base="$2" native_mir_json="$3"
    local self_mir_json="$4" driver_bin="$5"
    local effect_row effect_prefix effect_field zone_effect_field
    if [[ "$base" != "function_clause_order_minimal" ]]; then
        return 0
    fi

    effect_row='{"kind":"effect","nominal_kind":"effect","name":"Damage","fields":[{"name":"bearer","type":"Hero","field_kind":"subject_slot"}],"methods":[]}'
    effect_prefix='"kind":"effect","nominal_kind":"effect","name":"Damage"'
    effect_field='"name":"bearer","type":"Hero","field_kind":"subject_slot"'
    zone_effect_field='"name":"damage","type":"Damage","field_kind":"effect_slot"'
    grep -Fq "$effect_row" "$native_mir_json" || {
        echo "[self-host-parity:driver-rung2] native Effect declaration carriage drifted" >&2
        return 1
    }
    grep -Fq "$effect_row" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend self Effect declaration carriage drifted" >&2
        return 1
    }
    for mir_json in "$native_mir_json" "$self_mir_json"; do
        grep -Fq "$zone_effect_field" "$mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend zone EffectSlot carriage drifted" >&2
            return 1
        }
    done

    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "effect-as-class" "$effect_prefix" \
        '"kind":"class","nominal_kind":"effect","name":"Damage"' \
        "nominal declaration identity drifted: Damage"
    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "effect-kind-drift" "$effect_prefix" \
        '"kind":"effect","nominal_kind":"class","name":"Damage"' \
        "nominal declaration identity drifted: Damage"
    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "effect-as-generic-class" "$effect_prefix" \
        '"kind":"class","nominal_kind":"class","name":"Damage"' \
        "action contract references an unknown effect: Damage"
    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "effect-subject-slot-flattened" "$effect_field" \
        '"name":"bearer","type":"Hero","field_kind":"field"' \
        "nominal declaration participant shape is invalid: Damage"
    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "effect-field-kind-missing" "$effect_field" \
        '"name":"bearer","type":"Hero"' \
        "field kind is invalid for nominal declaration: Damage.bearer"
    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "zone-effect-slot-flattened" "$zone_effect_field" \
        '"name":"damage","type":"Damage","field_kind":"field"' \
        "field kind is invalid for nominal declaration: BattleZone.damage"
    pgy_selfhost_driver_rung2_effect_declaration_reject \
        "$backend" "$base" "$self_mir_json" "$driver_bin" \
        "missing-effect" "$effect_row," "" \
        "action contract references an unknown effect: Damage"
}
