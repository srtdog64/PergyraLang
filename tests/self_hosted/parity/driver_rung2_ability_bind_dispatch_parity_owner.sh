#!/usr/bin/env bash
# Owns dynamic role-slot ABI dispatch and fail-closed bind validation.
# Registry forbidden-fallback inventory exercised below:
# bind_party_slot_text_reparse, role_implementation_text_reparse,
# direct_ability_call_fallback, missing_role_slot_abi_fact_success.

pgy_selfhost_verify_driver_rung2_ability_bind_dispatch() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    [[ "$base" == "generic_default_ability_bind_dispatch" ]] || return 0

    for fact in \
        '"kind":"ability","nominal_kind":"ability","name":"Bufferable"' \
        '"generic_params":[{"name":"T","constraint":null,"default_type":"Int"}]' \
        '"kind":"party","nominal_kind":"party","name":"StorageParty","fields":[],"methods":[],"role_slots":[{"name":"buffer","dynamic":true,"abilities":[{"base":"Bufferable","actuals":[]}]' \
        '"name":"Put","kind":"method","owner":"IntBuffer"' \
        '"slot_anchor":"buffer"' \
        '"source_type":"AST_BIND_STMT"' \
        '"call_target_kind":"member","call_target_name":"Bufferable_Put"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend ability-bind MIR fact drifted: $fact" >&2
            exit 1
        }
    done

    local source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    local mutated="$BUILD_DIR/${base}_${backend}.bad-argument.pgy"
    local actual="$mutated.out" err="$mutated.err"
    pgy_replace_first_literal "$source" "$mutated" \
        'storage.buffer.Put(7)' 'storage.buffer.Put("bad")'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$actual" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend dynamic ability type mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: call_arg_type_mismatch' "$actual" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend dynamic ability type diagnostic drifted" >&2
        cat "$actual" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_ability_bind_dispatch_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "generic_default_ability_bind_dispatch" ]] || return 0

    grep -Fq 'Die("ability bind is missing its party role-slot ABI fact")' \
        "$ROOT_DIR/src/self_hosted/codegen/emission/ability_bind_emit_owner.pgy" || {
        echo "[self-host-parity:driver-rung2] ability-bind missing-fact boundary drifted" >&2
        exit 1
    }

    for term in \
        'const Bufferable_Int_vtable *buffer_Bufferable_Int_vt;' \
        'StorageParty_bind_buffer(&storage, NULL, &IntBuffer_Bufferable_Bufferable_Int_vtable_instance);' \
        'storage.buffer_Bufferable_Int_vt->Put(storage.buffer, 7)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend ability-bind C ABI fact drifted: $term" >&2
            exit 1
        }
    done
    grep -Fq 'Bufferable_Put(storage.buffer, 7)' "$emitted_c" && {
        echo "[self-host-parity:driver-rung2] $backend dynamic ability call fell back to direct call" >&2
        exit 1
    }
    return 0
}
