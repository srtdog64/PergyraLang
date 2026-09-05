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
        '"kind":"party","nominal_kind":"party","name":"StorageParty","source_syntax_id":20,' \
        '"fields":[],"methods":[],"role_slots":[{"name":"buffer","dynamic":true,"abilities":[{"base":"Bufferable","actuals":[]}]' \
        '"name":"Put","kind":"method","source_syntax_id":12,' \
        '"receiver_carriage":"mutable-identity","owner":"IntBuffer"' \
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
    local actual="$mutated.out" err="$mutated.err" status=0 diagnostic
    local diagnostic_output="$mutated.diagnostic"
    pgy_replace_first_literal "$source" "$mutated" \
        'storage.buffer.Put(7)' 'storage.buffer.Put("bad")'
    (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$actual" 2>"$err") || status=$?
    if [[ "$status" -ne 1 ]]; then
        echo "[self-host-parity:driver-rung2] $backend dynamic ability type mismatch expected exit 1, got $status" >&2
        exit 1
    fi
    # A dynamic ability slot is a resolved member call, not a free function.
    # Preserve the semantic owner's exact code and argument facts on CRLF hosts.
    tr -d '\r' <"$actual" >"$diagnostic_output"
    tr -d '\r' <"$err" >>"$diagnostic_output"
    for diagnostic in 'Diagnostic: pgy.selfhost.semantic.v1' 'Stage: semantic' \
        'Code: member_call_arg_type_mismatch' '- func: storage.buffer.Put' \
        '- expected: Int' '- actual: String'; do
        grep -Fxq -- "$diagnostic" "$diagnostic_output" || {
            echo "[self-host-parity:driver-rung2] $backend dynamic ability type diagnostic drifted: $diagnostic" >&2
            cat "$actual" "$err" >&2
            exit 1
        }
    done
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

# Called after the existing parent has executed and compared the valid input.
pgy_selfhost_verify_driver_rung2_ability_identity_epoch() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    [[ "$base" == generic_default_ability_bind_dispatch ]] || return 0
    local python_bin work="$BUILD_DIR/${base}_${backend}.identity"
    python_bin="$(command -v python3 || command -v python)"
    "$python_bin" "$ROOT_DIR/tests/self_hosted/parity/ability_declaration_identity_mutations.py" \
        "$self_mir_json" "$work"
    local name status actual err expected_diagnostic
    for name in missing-ability-id zero-ability-id negative-ability-id \
        string-ability-id unknown-target duplicate-id crossed-owner-ids \
        missing-runtime-id unknown-runtime-id; do
        status=0
        expected_diagnostic='CODEGEN ERROR: MIR instruction expression graph is missing or invalid'
        case "$name" in
            crossed-owner-ids|missing-runtime-id|unknown-runtime-id)
                expected_diagnostic='MIR-LOWER ERROR: MIR machine-layer facts are missing or invalid' ;;
        esac
        actual="$work/$name.out"; err="$work/$name.err"
        (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$(pgy_selfhost_path_relative_to_root "$work/$name.mir.json")" \
            >"$actual" 2>"$err") || status=$?
        if [[ "$status" -ne 1 ]] || ! grep -Fq "$expected_diagnostic" \
            "$actual" "$err" || grep -Fq '"schema":"pgy.mir.v1"' "$actual"; then
            echo "[self-host-parity:ability-identity] wrong refusal: $name status=$status" >&2
            cat "$actual" "$err" >&2
            exit 1
        fi
    done
    for name in declarations-reordered ability-rekeyed; do
        actual="$work/$name.c"; err="$work/$name.err"
        (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$work/$name.mir.json")" \
            >"$actual" 2>"$err") || {
            cat "$actual" "$err" >&2; exit 1;
        }
        pgy_selfhost_driver_rung2_compile_emitted 0 "$actual" \
            "$work/$name.exe" "$work/$name.cc.log" || {
            cat "$work/$name.cc.log" >&2; exit 1;
        }
        "$work/$name.exe" | tr -d '\r' >"$work/$name.run"
        [[ "$(<"$work/$name.run")" == 12 ]] || {
            echo "[self-host-parity:ability-identity] valid control drifted: $name" >&2
            exit 1
        }
    done
    actual="$work/distinct-owners.c"; err="$work/distinct-owners.err"
    (cd "$ROOT_DIR" && "$driver_bin" \
        tests/self_hosted/parity/fixture/ability_method_identity_distinct_owners.pgy \
        --emit-c-verified >"$actual" 2>"$err") || {
        cat "$actual" "$err" >&2; exit 1;
    }
    pgy_selfhost_driver_rung2_compile_emitted 0 "$actual" \
        "$work/distinct-owners.exe" "$work/distinct-owners.cc.log" || {
        cat "$work/distinct-owners.cc.log" >&2; exit 1;
    }
    "$work/distinct-owners.exe" | tr -d '\r' >"$work/distinct-owners.run"
    [[ "$(<"$work/distinct-owners.run")" == $'12\n16' ]] || {
        echo '[self-host-parity:ability-identity] same-name owners were conflated' >&2
        exit 1
    }
    echo "[self-host-parity:ability-identity] $backend nine refusals and three runtime controls passed"
}
