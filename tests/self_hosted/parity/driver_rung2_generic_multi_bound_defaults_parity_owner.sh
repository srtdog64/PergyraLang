#!/usr/bin/env bash
# Owns ordered ability-bound/default carriage and its fail-closed falsifier.
# Forbidden regressions: ability_where_source_reparse,
# inline_constraint_generic_param_reparse, missing_bound_success, and
# marker_role_impl_loss.

pgy_selfhost_verify_driver_rung2_generic_multi_bound_defaults() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local source mutated self_out self_err oracle_bin oracle_log fact
    [[ "$base" == "generic_multi_bound_defaults" ]] || return 0

    for fact in \
        '"name":"Packable","source_syntax_id":10,' \
        '"fields":[],"methods":[{"name":"Accept","return":"Void","callable_kind":"function","contract":{"requires":[],"within":null,"causes":null,"authorized_by":[],"caps_present":false,"caps":[],"effects_present":false,"effects":[]},"params":[{"name":"value","type":"T"}]}],"generic_params":[{"name":"T","constraint":"Comparable + Cloneable","default_type":"Item"}]' \
        '"name":"ItemComparable","source_syntax_id":6,' \
        '"fields":[],"methods":[],"for_type":"Item","impls":[{"ability":{"base":"Comparable","actuals":[]},"method_start":0,"method_count":0}]' \
        '"name":"ItemCloneable","source_syntax_id":8,' \
        '"fields":[],"methods":[],"for_type":"Item","impls":[{"ability":{"base":"Cloneable","actuals":[]},"method_start":0,"method_count":0}]'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend generic multi-bound fact drifted: $fact" >&2
            exit 1
        }
    done

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    mutated="$BUILD_DIR/${base}_${backend}.missing-bound.pgy"
    self_out="$mutated.self.out"
    self_err="$mutated.self.err"
    oracle_bin="$mutated.oracle.exe"
    oracle_log="$mutated.oracle.log"
    pgy_replace_first_literal "$source" "$mutated" \
        'where T: Comparable + Cloneable' \
        'where T: Comparable + MissingAbility'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend missing generic bound was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: ast_artifact_invalid' "$self_out" "$self_err" &&
    grep -Fq -- '- owner: SemanticAstAbilityGenericBoundVerdict' \
        "$self_out" "$self_err" &&
    grep -Fq -- '- broken_bound: MissingAbility' "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend generic bound diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted missing generic bound" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_generic_multi_bound_defaults_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3" term
    [[ "$base" == "generic_multi_bound_defaults" ]] || return 0

    for term in \
        'typedef struct Packable_Item_vtable Packable_Item_vtable;' \
        'struct Packable_Item_vtable {' \
        'void (*Accept)(void *self, Item);' \
        'void BagPackable_Accept(void *_pgy_raw_self, Item value)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend generic multi-bound C fact drifted: $term" >&2
            exit 1
        }
    done
}
