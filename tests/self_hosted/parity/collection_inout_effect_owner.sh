#!/usr/bin/env bash
# Current-source native C/LLVM analyzers; supplied programs are never emitted/run.
set -Eeuo pipefail
trap 'status=$?; echo "[collection-inout-effect] failed at line $LINENO (status $status); evidence: ${REL:-not-created}" >&2' ERR
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here collection-inout-effect "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/collection-inout-effect.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
SOURCE_PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
IDENTITY_PROBE=tests/self_hosted/fixtures/collection_inout_effect_identity_probe.pgy
CONSTRUCTOR_PROBE=tests/self_hosted/fixtures/collection_constructor_escape_identity_probe.pgy
EVENT_PROBE=tests/self_hosted/fixtures/collection_lifetime_event_probe.pgy
STORAGE_PROBE=tests/self_hosted/fixtures/collection_storage_producer_probe.pgy
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
INPUTS=(
    allocator_domain_result_positive.pgy allocator_domain_pool_read_positive.pgy
    allocator_domain_pool_negative.pgy allocator_domain_formal_write_negative.pgy
    allocator_domain_inout_negative.pgy allocator_domain_mixed_negative.pgy
    allocator_domain_crossed_formals_negative.pgy
    allocator_domain_string_inout_negative.pgy allocator_domain_string_read_positive.pgy
    numeric_string_result_positive.pgy
    numeric_string_bool_negative.pgy numeric_string_string_negative.pgy
    numeric_string_nested_negative.pgy numeric_string_mixed_negative.pgy
    numeric_string_reassigned_negative.pgy numeric_string_shadow_negative.pgy
    owned_string_literal_transfer_positive.pgy
    owned_string_literal_transfer_exclusive_result_positive.pgy
    owned_string_literal_transfer_aliased_result_negative.pgy
    owned_string_literal_transfer_borrowed_negative.pgy
    owned_string_literal_transfer_deferred_borrow_negative.pgy
    owned_string_literal_transfer_after_use_negative.pgy
    owned_string_literal_transfer_multi_element_negative.pgy
    owned_string_result_reassigned_negative.pgy owned_string_allocator_reassigned_negative.pgy
    owned_string_result_unassigned_positive.pgy
    owned_string_result_alias_reassigned_negative.pgy owned_string_result_branch_reassigned_negative.pgy
    owned_string_result_shadow_assignment_positive.pgy
    generic_default_local_selection_positive.pgy generic_default_borrowed_return_negative.pgy
    owned_string_result_implicit_field_positive.pgy
    owned_string_result_synthetic_binding_positive.pgy
    storage_opaque_return_own_negative.pgy storage_opaque_assign_own_negative.pgy
    storage_opaque_alias_own_negative.pgy storage_opaque_return_read_positive.pgy storage_shadowed_clone_own_negative.pgy
    builtin_completion_nested_other_positive.pgy builtin_completion_nested_same_positive.pgy
    builtin_completion_nested_borrowed_negative.pgy builtin_completion_nested_borrowed_other_positive.pgy
    field_ctor_clone_drop_negative.pgy field_ctor_empty_drop_negative.pgy
    field_ctor_own_argument_negative.pgy field_ctor_fresh_assign_positive.pgy field_ctor_readonly_positive.pgy
    field_ctor_own_formal_drop_negative.pgy field_ctor_own_formal_forward_negative.pgy field_ctor_own_formal_readonly_positive.pgy
    field_ctor_alias_drop_negative.pgy field_ctor_alias_own_negative.pgy
    field_ctor_duplicate_readonly_positive.pgy field_ctor_nested_clone_positive.pgy field_ctor_identity_input.pgy
    field_ctor_indexed_readonly_positive.pgy
    field_ctor_alias_indexed_readonly_positive.pgy field_ctor_inout_mutation_negative.pgy field_ctor_owned_push_negative.pgy
    field_ctor_own_formal_mutation_negative.pgy field_ctor_own_formal_unescaped_mutation_positive.pgy
    field_ctor_local_push_negative.pgy field_ctor_local_pop_negative.pgy field_ctor_local_set_negative.pgy field_ctor_local_index_negative.pgy
    field_ctor_own_formal_push_negative.pgy field_ctor_own_formal_pop_negative.pgy field_ctor_own_formal_set_negative.pgy field_ctor_own_formal_index_negative.pgy
    field_ctor_local_unescaped_statements_positive.pgy field_ctor_own_formal_unescaped_statements_positive.pgy
    field_ctor_fresh_statement_assign_positive.pgy field_ctor_old_alias_mutation_negative.pgy
    field_ctor_own_formal_mutation_before_positive.pgy field_ctor_local_mutation_before_positive.pgy
    field_ctor_own_formal_deferred_mutation_negative.pgy field_ctor_own_formal_deferred_statement_negative.pgy
    field_ctor_local_deferred_statement_negative.pgy field_ctor_own_formal_nested_mutation_negative.pgy
    field_ctor_own_formal_nested_retire_mutation_negative.pgy field_ctor_own_formal_nested_retire_statement_negative.pgy
    field_ctor_own_formal_nested_retire_index_negative.pgy field_ctor_own_formal_deferred_opaque_mutation_negative.pgy
    member_formal_double_move_negative.pgy member_formal_restore_positive.pgy
    member_generic_double_move_negative.pgy member_generic_restore_positive.pgy
    member_formal_index_target_negative.pgy member_local_index_target_negative.pgy member_formal_identity_input.pgy
    borrow_formal_ref_own_negative.pgy borrow_formal_inout_own_negative.pgy borrow_formal_default_own_negative.pgy
    borrow_formal_chain_own_negative.pgy borrow_formal_chain_read_positive.pgy borrow_formal_modes_read_clone_positive.pgy
    borrow_formal_fresh_clone_positive.pgy borrow_formal_fresh_empty_positive.pgy borrow_formal_fresh_own_positive.pgy
    borrow_formal_fresh_binding_move_positive.pgy borrow_formal_old_view_after_fresh_positive.pgy
    borrow_formal_fresh_deferred_negative.pgy borrow_formal_same_name_local_positive.pgy borrow_formal_assign_old_empty_negative.pgy
    borrow_formal_alias_unknown_read_negative.pgy borrow_formal_alias_deferred_read_negative.pgy
    borrow_formal_alias_sibling_read_negative.pgy borrow_formal_alias_read_before_positive.pgy borrow_formal_alias_fresh_isolated_positive.pgy
    borrow_formal_seed_sibling_read_negative.pgy borrow_formal_seed_read_before_positive.pgy borrow_formal_seed_fresh_isolated_positive.pgy
    borrow_formal_assign_read_before_positive.pgy borrow_formal_assign_shared_read_negative.pgy
    borrow_formal_assign_alias_shared_read_negative.pgy borrow_formal_assign_alias_before_positive.pgy
    own_assign_single_positive.pgy own_assign_read_before_positive.pgy
    own_assign_original_read_negative.pgy own_assign_duplicate_redefine_negative.pgy own_assign_self_retired_negative.pgy
    own_assign_distinct_redefine_positive.pgy own_assign_old_drop_positive.pgy own_assign_old_clone_drop_positive.pgy
    own_assign_old_unknown_positive.pgy own_assign_old_new_unknown_negative.pgy
    own_assign_old_drop_new_drop_negative.pgy own_assign_stale_empty_drop_negative.pgy own_assign_stale_clone_drop_negative.pgy
    own_assign_stale_owned_push_negative.pgy own_assign_stale_copy_negative.pgy own_assign_stale_required_owned_negative.pgy
    own_assign_alias_metadata_positive.pgy own_assign_alias_unknown_drop_negative.pgy own_assign_binding_move_redefine_positive.pgy
    own_assign_conditional_empty_negative.pgy own_assign_deferred_old_effect_negative.pgy own_assign_loop_transfer_negative.pgy
    own_event_formal_local_single_positive.pgy
    own_event_formal_local_reuse_negative.pgy
    own_event_formal_local_duplicate_negative.pgy
    own_event_formal_local_sibling_positive.pgy
    own_event_formal_local_distinct_positive.pgy
    own_event_formal_clone_positive.pgy
    own_event_formal_local_unknown_negative.pgy
    own_event_formal_local_read_positive.pgy
    own_event_formal_local_loop_negative.pgy
    own_event_formal_local_deferred_negative.pgy
    own_event_formal_local_deep_drop_negative.pgy
    own_event_read_before_positive.pgy
    own_event_read_after_negative.pgy
    own_event_nested_read_before_positive.pgy
    own_event_nested_read_after_negative.pgy
    own_event_argument_read_before_positive.pgy
    own_event_argument_read_after_negative.pgy
    own_event_local_read_before_positive.pgy
    own_event_local_read_after_negative.pgy
    own_storage_retired_empty_negative.pgy own_storage_retired_clone_negative.pgy
    own_storage_live_empty_positive.pgy own_storage_live_borrowed_positive.pgy
    own_storage_child_retired_negative.pgy own_storage_child_other_binding_positive.pgy
    own_storage_repeated_negative.pgy own_storage_nested_other_binding_positive.pgy
    own_storage_nested_same_binding_negative.pgy own_storage_loop_negative.pgy own_storage_deferred_negative.pgy
    own_storage_formal_retired_negative.pgy own_storage_formal_child_retired_negative.pgy
    own_storage_formal_forward_positive.pgy own_storage_formal_unknown_negative.pgy
    own_storage_return_duplicate_negative.pgy own_storage_return_distinct_positive.pgy
    own_named_clone_positive.pgy own_wrapper_borrowed_negative.pgy
    own_formal_read_after_forward_negative.pgy own_formal_double_forward_negative.pgy
    inout_event_push_before_unknown_positive.pgy inout_event_copy_before_unknown_positive.pgy
    inout_event_copy_after_unknown_negative.pgy inout_event_borrowed_push_after_copy_negative.pgy
    inout_event_nested_other_binding_positive.pgy inout_event_nested_same_binding_negative.pgy
    inout_event_index_write_after_copy_negative.pgy inout_event_index_write_borrowed_positive.pgy
    inout_event_nested_own_push_negative.pgy inout_event_deferred_push_unknown_negative.pgy
    inout_event_owned_push_after_copy_positive.pgy
    inout_event_transferred_child_drop_negative.pgy
    inout_event_unknown_then_owned_transfer_negative.pgy
    inout_event_owned_push_deferred_drop_positive.pgy inout_event_clone_drop_then_push_negative.pgy
    inout_event_deferred_push_after_drop_negative.pgy inout_event_loop_push_drop_negative.pgy
    member_double_move_negative.pgy member_reuse_while_moved_negative.pgy
    member_distinct_moves_positive.pgy member_restore_then_move_positive.pgy
    inout_borrowed_push_drop_negative.pgy inout_borrowed_push_read_positive.pgy
    inout_owned_push_drop_positive.pgy inout_forward_owned_push_drop_positive.pgy
    inout_readonly_drop_positive.pgy inout_shadow_drop_negative.pgy inout_shadow_read_positive.pgy
    inout_cycle_drop_negative.pgy inout_cycle_read_positive.pgy inout_unknown_then_copy_negative.pgy
    inout_borrowed_then_copy_negative.pgy inout_retired_empty_copy_negative.pgy
    inout_retired_clone_copy_negative.pgy inout_moved_unknown_drop_negative.pgy
    inout_branch_borrow_drop_negative.pgy inout_zero_loop_copy_positive.pgy
    inout_alias_borrow_drop_negative.pgy inout_element_escape_drop_negative.pgy
    inout_branch_own_then_copy_negative.pgy inout_own_formal_retired_copy_negative.pgy
    inout_loop_copy_then_own_negative.pgy inout_copy_then_own_positive.pgy
    inout_nested_own_argument_copy_negative.pgy
    inout_deferred_copy_own_negative.pgy inout_deferred_copy_drop_negative.pgy inout_deferred_read_positive.pgy
    inout_index_eq_owned_positive.pgy inout_index_ne_owned_positive.pgy inout_index_borrowed_read_positive.pgy
    inout_index_forward_copy_positive.pgy inout_index_return_drop_negative.pgy inout_index_alias_escape_drop_negative.pgy
    inout_index_append_drop_negative.pgy inout_index_write_drop_negative.pgy inout_index_deferred_own_negative.pgy
    inout_index_branch_own_negative.pgy inout_index_before_own_positive.pgy inout_index_after_unknown_negative.pgy
    inout_index_before_unknown_positive.pgy inout_index_role_eq_drop_negative.pgy inout_index_role_ne_drop_negative.pgy
    inout_index_after_drop_negative.pgy inout_index_own_formal_drop_negative.pgy inout_index_nested_own_negative.pgy
    inout_index_formal_unknown_negative.pgy inout_index_formal_deferred_unknown_negative.pgy
)
for i in "${!INPUTS[@]}"; do INPUTS[i]="$FIXTURES/${INPUTS[i]}"; done
INPUTS+=(
    tests/cases/backend_compare/subject_class_dispatch/main.pgy
    tests/self_hosted/parity/fixture/intent_zone_authority_transition.pgy
    docs/audits/repros/own_formal_local_storage_alias_2026-10-02.pgy
    docs/audits/repros/own_same_expression_read_after_transfer_2026-10-02.pgy
    docs/audits/repros/own_formal_assignment_storage_alias_2026-10-02.pgy
    docs/audits/repros/ref_formal_let_storage_alias_2026-10-02.pgy
)
sha256sum "$PGY" >"$WORK/native.sha256"
sha256sum "$SOURCE_PROBE" "$IDENTITY_PROBE" "$CONSTRUCTOR_PROBE" "$EVENT_PROBE" "$STORAGE_PROBE" \
    tests/self_hosted/fixtures/owned_string_local_reassignment_unit.pgy \
    tests/self_hosted/fixtures/numeric_string_allocation_unit.pgy \
    tests/self_hosted/fixtures/owned_string_allocator_domain_unit.pgy \
    tests/self_hosted/fixtures/generic_default_selection_unit.pgy \
    src/self_hosted/semantic/ast_numeric_string_allocation_call_owner.pgy \
    src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy \
    src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy \
    src/self_hosted/semantic/ast_signature_fact_owner.pgy \
    src/self_hosted/semantic/ast_signature_artifact_match_owner.pgy \
    src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy \
    src/self_hosted/semantic/ast_body_type_bundle_owner.pgy \
    src/self_hosted/semantic/ast_body_type_bundle_schema_owner.pgy \
    src/self_hosted/semantic/ast_body_type_bundle_readiness_owner.pgy \
    src/self_hosted/semantic/ast_collection_formal_effect_identity_owner.pgy \
    src/self_hosted/semantic/ast_collection_formal_effect_owner.pgy \
    src/self_hosted/semantic/ast_collection_call_effect_owner.pgy \
    src/self_hosted/semantic/ast_collection_constructor_storage_escape_owner.pgy \
    src/self_hosted/semantic/ast_collection_constructor_field_input_owner.pgy \
    src/self_hosted/semantic/ast_nominal_constructor_lookup_owner.pgy \
    src/self_hosted/semantic/ast_collection_call_effect_fact_owner.pgy \
    src/self_hosted/semantic/ast_collection_definition_effect_closure_owner.pgy \
    src/self_hosted/semantic/ast_collection_argument_permission_effect_owner.pgy \
    src/self_hosted/semantic/ast_collection_formal_storage_permission_owner.pgy \
    src/self_hosted/semantic/ast_collection_assignment_definition_owner.pgy \
    src/self_hosted/semantic/ast_collection_definition_storage_authority_owner.pgy \
    src/self_hosted/semantic/ast_collection_definition_storage_producer_owner.pgy \
    src/self_hosted/semantic/ast_collection_definition_transition_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_assignment_alias_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_binding_move_use_owner.pgy \
    src/self_hosted/semantic/ast_collection_call_argument_verdict_owner.pgy \
    src/self_hosted/semantic/ast_collection_call_retirement_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_state_owner.pgy \
    src/self_hosted/semantic/ast_collection_owned_argument_admission_owner.pgy \
    src/self_hosted/semantic/ast_collection_owned_parameter_identity_owner.pgy \
    src/self_hosted/semantic/ast_collection_owned_element_parameter_requirement_owner.pgy \
    src/self_hosted/semantic/ast_collection_aggregate_field_entry_requirement_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_argument_transfer_owner.pgy \
    src/self_hosted/semantic/ast_collection_argument_event_order_owner.pgy \
    src/self_hosted/semantic/ast_collection_lifetime_event_order_owner.pgy \
    src/self_hosted/semantic/ast_collection_builtin_transition_owner.pgy \
    src/self_hosted/semantic/ast_local_binding_fact_owner.pgy \
    src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_statement_transition_owner.pgy \
    src/self_hosted/semantic/ast_assignment_fact_owner.pgy \
    src/self_hosted/semantic/ast_expression_graph_call_argument_edge_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_identity_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_member_root_identity_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy \
    src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_result_fact_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_result_domain_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_domain_exposure_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_expression_domain_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_call_result_admission_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_local_reassignment_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_actual_exclusivity_owner.pgy \
    src/self_hosted/semantic/ast_owned_string_literal_transfer_owner.pgy \
    src/self_hosted/semantic/builtin_argument_retention_projection_owner.pgy \
    src/semantic/builtin_argument_retention_registry.def \
    scripts/render_builtin_argument_retention_registry.py \
    tests/builtin_argument_retention_registry_smoke.sh >"$WORK/owners.sha256"
if grep -R -Fq --include='*.pgy' 'SemanticAstGenericDefaultTypeForName' src/self_hosted; then
    echo 'legacy borrowed generic-default return path remains' >&2
    exit 1
fi
if grep -R -Fq --include='*.pgy' 'SemanticAstGenericConstraintRowsFromNode' src/self_hosted ||
    grep -R -Fq --include='*.pgy' 'SemanticAstGenericParameterRowsFromNode' src/self_hosted; then
    echo 'legacy split generic-parameter fact path remains' >&2
    exit 1
fi
grep -Fq 'func SemanticAstGenericDefaultRowCountOrDie(' \
    src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy
grep -Fq 'func SemanticAstGenericParameterFactRowsFromOwnerNode(' \
    src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy
for generic_fact_consumer in ast_role_fact_owner.pgy ast_signature_fact_owner.pgy ast_signature_artifact_match_owner.pgy; do
    grep -Fq 'SemanticAstGenericParameterFactRowsFromOwnerNode(' \
        "src/self_hosted/semantic/$generic_fact_consumer"
done
if grep -Fq 'let close: Int = StringIndexOf(row, ">");' \
    src/self_hosted/semantic/ast_generic_parameter_fact_owner.pgy; then
    echo 'nested ability generic defaults use the first closing angle' >&2
    exit 1
fi
ACTUAL_INPUT="$FIXTURES/callable_table_from_artifact_release_probe.pgy"
sha256sum "${INPUTS[@]}" "$FIXTURES/inout_index_identity_input.pgy" \
    "$FIXTURES/own_storage_identity_input.pgy" "$FIXTURES/own_event_order_identity_input.pgy" \
    "$FIXTURES/borrow_formal_identity_input.pgy" "$FIXTURES/storage_producer_identity_input.pgy" "$ACTUAL_INPUT" >"$WORK/inputs.sha256"
find src/self_hosted -name '*.pgy' -type f -print0 | sort -z | xargs -0 sha256sum >"$WORK/imports.sha256"
# Root-coordinated reuse within a frozen source stage, not a compiler install
# or a cache lookup. Refuse a different source/input/native or mutated binary.
PROBE_DIR="${PGY_COLLECTION_EFFECT_PROBE_DIR:-}"
if [[ -n "$PROBE_DIR" ]]; then
    PROBE_DIR="$(cd "$PROBE_DIR" && pwd -P)"
    case "$PROBE_DIR/" in "$ROOT_DIR/.tmp/self_hosted/"*) ;; *) echo 'probe reuse must stay in this checkout artifact directory' >&2; exit 1 ;; esac
    for manifest in native owners inputs imports; do
        cmp "$WORK/$manifest.sha256" "$PROBE_DIR/$manifest.sha256"
        sha256sum --quiet -c "$PROBE_DIR/$manifest.sha256"
    done
    # A valid subset is not a complete proof. Pin exactly the six binaries
    # about to be copied, accepting old relative or new absolute receipt paths.
    sha256sum --binary "$PROBE_DIR/c-source.exe" "$PROBE_DIR/c-identity.exe" "$PROBE_DIR/c-constructor.exe" \
        "$PROBE_DIR/llvm-source.exe" "$PROBE_DIR/llvm-identity.exe" "$PROBE_DIR/llvm-constructor.exe" \
        | LC_ALL=C sort >"$WORK/reuse-expected-binaries.sha256"
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ "${line:0:64}" =~ ^[0-9a-f]{64}$ &&
            ( "${line:64:2}" == ' *' || "${line:64:2}" == '  ' ) ]] || {
            echo 'invalid probe binary hash receipt' >&2; exit 1;
        }
        binary_path="$(realpath -- "${line:66}")"
        printf '%s *%s\n' "${line:0:64}" "$binary_path"
    done <"$PROBE_DIR/probe-binaries.sha256" \
        | LC_ALL=C sort >"$WORK/reuse-declared-binaries.sha256"
    cmp "$WORK/reuse-expected-binaries.sha256" "$WORK/reuse-declared-binaries.sha256"
    sha256sum --quiet -c "$WORK/reuse-declared-binaries.sha256"
    echo "[collection-inout-effect] exact-source probe reuse: ${PROBE_DIR#"$ROOT_DIR/"}"
fi
for backend in c llvm; do
    if [[ -n "$PROBE_DIR" ]]; then
        cp "$PROBE_DIR/$backend-source.exe" "$WORK/$backend-source.exe"
        cmp "$PROBE_DIR/$backend-source.exe" "$WORK/$backend-source.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$SOURCE_PROBE" "--backend=$backend" --opt=dev \
            -o "$REL/$backend-source.exe" >"$WORK/$backend-source.compile" 2>&1
    fi
    sha256sum "$WORK/$backend-source.exe" >>"$WORK/probe-binaries.sha256"
    for mode in current missing-owner missing-type foreign-owner crossed-node crossed-lane crossed-ordinal \
        borrowed-type unknown-type family-deleted declared-target runtime-abi callee-binding; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/numeric_string_result_positive.pgy" \
            "numeric-string-$mode" >"$WORK/$backend-numeric-$mode.raw" 2>"$WORK/$backend-numeric-$mode.err"
        tr -d '\r' <"$WORK/$backend-numeric-$mode.raw" >"$WORK/$backend-numeric-$mode.run"
        [[ ! -s "$WORK/$backend-numeric-$mode.err" ]]
        grep -Fxq 'numeric-string-allocation-unit:PASS' "$WORK/$backend-numeric-$mode.run"
    done
    for mode in current missing-domain missing-formal wrong-formal wrong-ordinal wrong-receiver foreign-owner \
        missing-return-column missing-mode-column producer-missing-return producer-missing-mode family-deleted \
        wrong-callee-binding foreign-allocator-binding cross-base cross-callee cross-actual foreign-finish; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/allocator_domain_result_positive.pgy" \
            "allocator-domain-$mode" >"$WORK/$backend-allocator-$mode.raw" 2>"$WORK/$backend-allocator-$mode.err"
        tr -d '\r' <"$WORK/$backend-allocator-$mode.raw" >"$WORK/$backend-allocator-$mode.run"
        [[ ! -s "$WORK/$backend-allocator-$mode.err" ]]
        grep -Fxq 'owned-string-allocator-domain-unit:PASS' "$WORK/$backend-allocator-$mode.run"
    done
    for mode in current missing-leaf foreign-function carried-kind wrong-type untracked-type missing-row crossed-row; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/owned_string_result_reassigned_negative.pgy" \
            "owned-string-reassignment-$mode" >"$WORK/$backend-reassignment-$mode.raw" 2>"$WORK/$backend-reassignment-$mode.err"
        tr -d '\r' <"$WORK/$backend-reassignment-$mode.raw" >"$WORK/$backend-reassignment-$mode.run"
        [[ ! -s "$WORK/$backend-reassignment-$mode.err" ]]
        grep -Fxq 'owned-string-reassignment-unit:PASS' "$WORK/$backend-reassignment-$mode.run"
    done
    for mode in owner-field owner-field-missing owner-field-type owner-field-mode; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/owned_string_result_implicit_field_positive.pgy" \
            "owned-string-reassignment-$mode" >"$WORK/$backend-reassignment-$mode.raw" 2>"$WORK/$backend-reassignment-$mode.err"
        tr -d '\r' <"$WORK/$backend-reassignment-$mode.raw" >"$WORK/$backend-reassignment-$mode.run"
        [[ ! -s "$WORK/$backend-reassignment-$mode.err" ]]
        grep -Fxq 'owned-string-reassignment-unit:PASS' "$WORK/$backend-reassignment-$mode.run"
    done
    timeout 30 "$WORK/$backend-source.exe" \
        "$FIXTURES/generic_default_local_selection_positive.pgy" \
        generic-default-selection >"$WORK/$backend-generic-default.raw" \
        2>"$WORK/$backend-generic-default.err"
    tr -d '\r' <"$WORK/$backend-generic-default.raw" >"$WORK/$backend-generic-default.run"
    [[ ! -s "$WORK/$backend-generic-default.err" ]]
    grep -Fxq 'generic-default-selection-unit:PASS' "$WORK/$backend-generic-default.run"
    for input in "${INPUTS[@]}"; do
        name="${input##*/}"
        timeout 30 "$WORK/$backend-source.exe" "$input" >"$WORK/$backend-$name.raw" 2>"$WORK/$backend-$name.err"
        tr -d '\r' <"$WORK/$backend-$name.raw" >"$WORK/$backend-$name.run"
        [[ ! -s "$WORK/$backend-$name.err" ]]
        case "$name" in
            ref_formal_let_storage_alias_2026-10-02.pgy)
                printf 'body_ok=false\nbody_diagnostic=borrow_boundary_escape\n' >"$WORK/expected" ;;
            own_event_formal_local_unknown_negative.pgy|own_event_formal_local_loop_negative.pgy|own_event_formal_local_deferred_negative.pgy|own_event_formal_local_deep_drop_negative.pgy)
                printf 'body_ok=false\nbody_diagnostic=borrow_boundary_escape\n' >"$WORK/expected" ;;
            own_assign_original_read_negative.pgy|own_assign_duplicate_redefine_negative.pgy|own_assign_self_retired_negative.pgy|own_formal_assignment_storage_alias_2026-10-02.pgy|own_event_*_negative.pgy|own_formal_local_storage_alias_2026-10-02.pgy|own_same_expression_read_after_transfer_2026-10-02.pgy)
                printf 'body_ok=false\nbody_diagnostic=move_from_released\n' >"$WORK/expected" ;;
            member_*_negative.pgy|own_storage_nested_same_binding_negative.pgy|own_storage_formal_retired_negative.pgy|own_storage_formal_child_retired_negative.pgy|own_storage_return_duplicate_negative.pgy|own_formal_*_negative.pgy)
                printf 'body_ok=false\nbody_diagnostic=move_from_released\n' >"$WORK/expected" ;;
            *_negative.pgy) printf 'body_ok=false\nbody_diagnostic=borrow_boundary_escape\n' >"$WORK/expected" ;;
            *_positive.pgy|member_formal_identity_input.pgy|field_ctor_identity_input.pgy|main.pgy|intent_zone_authority_transition.pgy) printf 'body_ok=true\nbody_diagnostic=\n' >"$WORK/expected" ;;
            *) echo "unclassified inout fixture: $name" >&2; exit 1 ;;
        esac
        cmp "$WORK/expected" "$WORK/$backend-$name.run"
    done
    while IFS='|' read -r input boundary atom function value; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-constructor.raw" 2>"$WORK/$backend-$input-constructor.err"
        tr -d '\r' <"$WORK/$backend-$input-constructor.raw" >"$WORK/$backend-$input-constructor.run"
        [[ ! -s "$WORK/$backend-$input-constructor.err" ]]
        grep -Fxq 'body_ok=false' "$WORK/$backend-$input-constructor.run"
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$input-constructor.run"
        grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$input-constructor.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-constructor.run"
        if [[ -n "$value" ]]; then grep -Fxq "body_value=$value" "$WORK/$backend-$input-constructor.run"; fi
        grep -Fxq "body_function=$function" "$WORK/$backend-$input-constructor.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-constructor.run"
        grep -Eq '^body_syntax=[1-9][0-9]*$' "$WORK/$backend-$input-constructor.run"
    done <<'CONSTRUCTOR_DIAGNOSTICS'
field_ctor_clone_drop_negative.pgy|ArrayDropOwnedStrings|ArrayDropOwnedStrings(values)|Main
field_ctor_empty_drop_negative.pgy|ArrayDropOwnedStrings|ArrayDropOwnedStrings(values)|Main
field_ctor_own_argument_negative.pgy|owned_argument_storage_not_live|Consume(values)|Main
field_ctor_own_formal_drop_negative.pgy|owned_argument_storage_unproved|ArrayDropOwnedStrings(values)|Store
field_ctor_own_formal_forward_negative.pgy|owned_argument_storage_unproved|Consume(values)|Store
field_ctor_alias_drop_negative.pgy|ArrayDropOwnedStrings|ArrayDropOwnedStrings(renamed)|Main
field_ctor_alias_own_negative.pgy|owned_argument_storage_not_live|Consume(renamed)|Main
field_ctor_inout_mutation_negative.pgy|unproved_inout_copy_entry|AppendCopy(values)|Main
field_ctor_owned_push_negative.pgy|ArrayPushOwnedString|ArrayPushOwnedString(values, "next")|Main
field_ctor_own_formal_mutation_negative.pgy|owned_argument_storage_unproved|ArrayPushOwnedString(values, "next")|Store
field_ctor_local_push_negative.pgy|ArrayPush|values|Main|"next"
field_ctor_local_pop_negative.pgy|ArrayPop|values|Main
field_ctor_local_set_negative.pgy|ArraySet|values|Main|0
field_ctor_local_index_negative.pgy|ArraySet|values[0]|Main|"next"
field_ctor_own_formal_push_negative.pgy|owned_argument_storage_unproved|values|Store|"next"
field_ctor_own_formal_pop_negative.pgy|owned_argument_storage_unproved|values|Store
field_ctor_own_formal_set_negative.pgy|owned_argument_storage_unproved|values|Store|0
field_ctor_own_formal_index_negative.pgy|owned_argument_storage_unproved|values[0]|Store|"next"
field_ctor_old_alias_mutation_negative.pgy|ArrayPush|old|Main|"next"
field_ctor_own_formal_deferred_mutation_negative.pgy|owned_argument_storage_unproved|ArrayPushOwnedString(values, "late")|Store
field_ctor_own_formal_deferred_statement_negative.pgy|owned_argument_storage_unproved|values|Store|"late"
field_ctor_local_deferred_statement_negative.pgy|ArrayPush|values|Main|"late"
field_ctor_own_formal_nested_mutation_negative.pgy|owned_argument_storage_unproved|ArrayPushOwnedString(values, Stamp(Carrier(values)))|Store
field_ctor_own_formal_nested_retire_mutation_negative.pgy|owned_argument_storage_unproved|ArrayPushOwnedString(values, Consume(values))|Store
field_ctor_own_formal_nested_retire_statement_negative.pgy|owned_argument_storage_unproved|values|Store|Consume(values)
field_ctor_own_formal_nested_retire_index_negative.pgy|owned_argument_storage_unproved|values[0]|Store|Consume(values)
field_ctor_own_formal_deferred_opaque_mutation_negative.pgy|owned_argument_storage_unproved|ArrayPushOwnedString(values, "deferred")|Store
CONSTRUCTOR_DIAGNOSTICS
    timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/member_double_move_negative.pgy" diagnostic \
        >"$WORK/$backend-diagnostic.raw" 2>"$WORK/$backend-diagnostic.err"
    tr -d '\r' <"$WORK/$backend-diagnostic.raw" >"$WORK/$backend-diagnostic.run"
    [[ ! -s "$WORK/$backend-diagnostic.err" ]]
    grep -Fxq 'body_ok=false' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_atom=second' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_value=bundle.values' "$WORK/$backend-diagnostic.run"
    grep -Fxq 'body_function=Main' "$WORK/$backend-diagnostic.run"
    grep -Fxq "body_module=$FIXTURES/member_double_move_negative.pgy" "$WORK/$backend-diagnostic.run"
    grep -Eq '^body_syntax=[0-9]+$' "$WORK/$backend-diagnostic.run"
    while IFS='|' read -r input atom value function; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-member.raw" 2>"$WORK/$backend-$input-member.err"
        tr -d '\r' <"$WORK/$backend-$input-member.raw" >"$WORK/$backend-$input-member.run"
        [[ ! -s "$WORK/$backend-$input-member.err" ]]
        grep -Fxq 'body_ok=false' "$WORK/$backend-$input-member.run"
        grep -Fxq 'body_diagnostic=move_from_released' "$WORK/$backend-$input-member.run"
        grep -Fxq -- '- boundary: member_use_after_move' "$WORK/$backend-$input-member.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-member.run"
        grep -Fxq "body_value=$value" "$WORK/$backend-$input-member.run"
        grep -Fxq "body_function=$function" "$WORK/$backend-$input-member.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-member.run"
        grep -Eq '^body_syntax=[0-9]+$' "$WORK/$backend-$input-member.run"
    done <<'MEMBER_DIAGNOSTICS'
member_formal_double_move_negative.pgy|rejected_second|pair.value|Count
member_generic_double_move_negative.pgy|rejected_second|pair.value|Count
member_formal_index_target_negative.pgy|slots[At(pair.value)]|value|Count
member_local_index_target_negative.pgy|slots[At(pair.value)]|value|Main
MEMBER_DIAGNOSTICS
    # A false verdict alone also passes an unordered implementation. Pin the
    # later event, not the earlier valid Copy, without hardcoding syntax IDs.
    while IFS='|' read -r input boundary atom value; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-diagnostic.raw" 2>"$WORK/$backend-$input-diagnostic.err"
        tr -d '\r' <"$WORK/$backend-$input-diagnostic.raw" >"$WORK/$backend-$input-diagnostic.run"
        [[ ! -s "$WORK/$backend-$input-diagnostic.err" ]]
        grep -Fxq 'body_ok=false' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_value=$value" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq 'body_function=Main' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-diagnostic.run"
        grep -Eq '^body_syntax=[0-9]+$' "$WORK/$backend-$input-diagnostic.run"
    done <<'EVENT_DIAGNOSTICS'
inout_event_copy_after_unknown_negative.pgy|unproved_inout_copy_entry|rejected_copy|Copy(values, 2)
inout_event_borrowed_push_after_copy_negative.pgy|ArrayPush|values|"borrowed"
inout_event_nested_same_binding_negative.pgy|unproved_inout_copy_entry|rejected_nested|Copy(a, Mark(a))
inout_event_index_write_after_copy_negative.pgy|ArraySet|values[0]|"borrowed"
own_storage_retired_empty_negative.pgy|owned_argument_storage_not_live|rejected_own|Metadata(values)
own_storage_retired_clone_negative.pgy|owned_argument_storage_not_live|rejected_clone|Metadata(values)
own_storage_child_retired_negative.pgy|owned_argument_storage_not_live|rejected_child|Metadata(a)
own_storage_repeated_negative.pgy|owned_argument_storage_not_live|rejected_second|Metadata(values)
own_storage_loop_negative.pgy|owned_argument_storage_not_live|rejected_loop|Metadata(values)
EVENT_DIAGNOSTICS
    while IFS='|' read -r input boundary atom value function; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-diagnostic.raw" 2>"$WORK/$backend-$input-diagnostic.err"
        tr -d '\r' <"$WORK/$backend-$input-diagnostic.raw" >"$WORK/$backend-$input-diagnostic.run"
        [[ ! -s "$WORK/$backend-$input-diagnostic.err" ]]
        grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_value=$value" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_function=$function" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-diagnostic.run"
    done <<'OWN_FORMAL_DIAGNOSTICS'
own_storage_formal_retired_negative.pgy|owned_argument_use_after_move|rejected_formal|Metadata(values)|Forward
own_storage_formal_child_retired_negative.pgy|owned_argument_use_after_move|rejected_formal_child|Metadata(values)|Forward
own_storage_formal_unknown_negative.pgy|owned_argument_storage_unproved|rejected_unknown_formal|Metadata(values)|Forward
OWN_FORMAL_DIAGNOSTICS
    while IFS='|' read -r input boundary atom value; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-diagnostic.raw" 2>"$WORK/$backend-$input-diagnostic.err"
        tr -d '\r' <"$WORK/$backend-$input-diagnostic.raw" >"$WORK/$backend-$input-diagnostic.run"
        [[ ! -s "$WORK/$backend-$input-diagnostic.err" ]]
        grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_value=$value" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq 'body_function=Forward' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-diagnostic.run"
    done <<'OWN_EVENT_DIAGNOSTICS'
own_event_formal_local_reuse_negative.pgy|owned_argument_use_after_move|rejected_original_read|ArrayLength(values)
own_event_formal_local_duplicate_negative.pgy|owned_argument_use_after_move|rejected_second_move|values
own_event_formal_local_unknown_negative.pgy|owned_argument_storage_unproved|rejected_unknown_move|values
own_event_read_after_negative.pgy|owned_argument_use_after_move|rejected_after|(Metadata(values) + ArrayLength(values))
own_event_argument_read_after_negative.pgy|owned_argument_use_after_move|rejected_argument_after|OwnFirst(values, ArrayLength(values))
OWN_EVENT_DIAGNOSTICS
    while IFS='|' read -r input code boundary atom value function; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-diagnostic.raw" 2>"$WORK/$backend-$input-diagnostic.err"
        tr -d '\r' <"$WORK/$backend-$input-diagnostic.raw" >"$WORK/$backend-$input-diagnostic.run"
        [[ ! -s "$WORK/$backend-$input-diagnostic.err" ]]
        grep -Fxq "body_diagnostic=$code" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-diagnostic.run"
        if [[ -n "$value" ]]; then grep -Fxq "body_value=$value" "$WORK/$backend-$input-diagnostic.run"; fi
        grep -Fxq "body_function=$function" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-diagnostic.run"
    done <<'ASSIGN_DIAGNOSTICS'
own_assign_original_read_negative.pgy|move_from_released|owned_argument_use_after_move|rejected_original_read|ArrayLength(values)|Forward
own_assign_duplicate_redefine_negative.pgy|move_from_released|owned_argument_use_after_move|moved|first|Forward
own_assign_self_retired_negative.pgy|move_from_released|owned_argument_use_after_move|moved|moved|Forward
own_assign_old_new_unknown_negative.pgy|borrow_boundary_escape|owned_argument_storage_not_live|rejected_new_unknown|Metadata(moved)|Forward
own_assign_stale_empty_drop_negative.pgy|borrow_boundary_escape|owned_string_drop|ArrayDropOwnedStrings(moved)||Forward
own_assign_stale_clone_drop_negative.pgy|borrow_boundary_escape|owned_string_drop|ArrayDropOwnedStrings(moved)||Forward
own_assign_old_drop_new_drop_negative.pgy|borrow_boundary_escape|owned_string_drop|ArrayDropOwnedStrings(moved)||Forward
own_assign_stale_copy_negative.pgy|borrow_boundary_escape|unproved_inout_copy_entry|rejected_copy|Copy(moved)|Forward
own_assign_stale_required_owned_negative.pgy|borrow_boundary_escape|owned_argument_without_owned_provenance:callee=DropElements|DropElements(moved)||Forward
own_assign_conditional_empty_negative.pgy|borrow_boundary_escape|ArrayDropOwnedStrings|ArrayDropOwnedStrings(moved)||Main
ASSIGN_DIAGNOSTICS
    while IFS='|' read -r input boundary atom function; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-diagnostic.raw" 2>"$WORK/$backend-$input-diagnostic.err"
        tr -d '\r' <"$WORK/$backend-$input-diagnostic.raw" >"$WORK/$backend-$input-diagnostic.run"
        [[ ! -s "$WORK/$backend-$input-diagnostic.err" ]]
        grep -Fxq 'body_ok=false' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq -- "- boundary: $boundary" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_function=$function" "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-diagnostic.run"
        grep -Eq '^body_syntax=[0-9]+$' "$WORK/$backend-$input-diagnostic.run"
    done <<'BORROW_FORMAL_DIAGNOSTICS'
borrow_formal_ref_own_negative.pgy|owned_argument_storage_not_live|Metadata(alias)|Observe
borrow_formal_inout_own_negative.pgy|owned_argument_storage_not_live|Metadata(alias)|Observe
borrow_formal_default_own_negative.pgy|owned_argument_storage_not_live|Metadata(alias)|Observe
borrow_formal_chain_own_negative.pgy|owned_argument_storage_not_live|Metadata(third)|Observe
borrow_formal_fresh_deferred_negative.pgy|owned_argument_storage_not_live|Metadata(alias)|Observe
borrow_formal_assign_old_empty_negative.pgy|owned_argument_storage_not_live|Metadata(alias)|Observe
borrow_formal_alias_unknown_read_negative.pgy|unproved_indexed_read_entry|Matches(first)|Observe
borrow_formal_alias_deferred_read_negative.pgy|unproved_indexed_read_entry|Matches(first)|Observe
borrow_formal_alias_sibling_read_negative.pgy|unproved_indexed_read_entry|Matches(right)|Observe
borrow_formal_seed_sibling_read_negative.pgy|unproved_indexed_read_entry|Matches(first)|Observe
borrow_formal_assign_shared_read_negative.pgy|unproved_indexed_read_entry|Matches(first)|Observe
borrow_formal_assign_alias_shared_read_negative.pgy|unproved_indexed_read_entry|Matches(first)|Observe
generic_default_borrowed_return_negative.pgy|unproved_indexed_read_entry|GenericDefaultMatches(first)|Observe
BORROW_FORMAL_DIAGNOSTICS
    for input in storage_opaque_return_own_negative.pgy storage_opaque_assign_own_negative.pgy storage_opaque_alias_own_negative.pgy storage_shadowed_clone_own_negative.pgy; do
        timeout 30 "$WORK/$backend-source.exe" "$FIXTURES/$input" diagnostic \
            >"$WORK/$backend-$input-diagnostic.raw" 2>"$WORK/$backend-$input-diagnostic.err"
        tr -d '\r' <"$WORK/$backend-$input-diagnostic.raw" >"$WORK/$backend-$input-diagnostic.run"
        [[ ! -s "$WORK/$backend-$input-diagnostic.err" ]]
        grep -Fxq 'body_diagnostic=borrow_boundary_escape' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq -- '- boundary: owned_argument_storage_not_live' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq 'body_function=Main' "$WORK/$backend-$input-diagnostic.run"
        grep -Fxq "body_module=$FIXTURES/$input" "$WORK/$backend-$input-diagnostic.run"
        atom='Metadata(values)'
        if [[ "$input" == storage_opaque_alias_own_negative.pgy ]]; then atom='Metadata(alias)'; fi
        grep -Fxq "body_atom=$atom" "$WORK/$backend-$input-diagnostic.run"
    done
    for mode_case in missing extra unknown; do
        args=()
        case "$mode_case" in
            missing) printf 'expected source path and optional diagnostic mode\n' >"$WORK/expected" ;;
            extra) args=("$FIXTURES/member_double_move_negative.pgy" diagnostic extra)
                printf 'expected source path and optional diagnostic mode\n' >"$WORK/expected" ;;
            unknown) args=("$FIXTURES/member_double_move_negative.pgy" unknown)
                printf 'unknown source observation mode\n' >"$WORK/expected" ;;
        esac
        if timeout 30 "$WORK/$backend-source.exe" ${args[@]+"${args[@]}"} >"$WORK/$backend-mode-$mode_case.raw" 2>"$WORK/$backend-mode-$mode_case.err"; then
            echo "source observer accepted $mode_case arguments" >&2; exit 1
        else
            [[ "$?" -eq 2 ]]
        fi
        [[ ! -s "$WORK/$backend-mode-$mode_case.err" ]]
        tr -d '\r' <"$WORK/$backend-mode-$mode_case.raw" >"$WORK/$backend-mode-$mode_case.run"
        cmp "$WORK/expected" "$WORK/$backend-mode-$mode_case.run"
    done
    if [[ -n "$PROBE_DIR" ]]; then
        cp "$PROBE_DIR/$backend-identity.exe" "$WORK/$backend-identity.exe"
        cmp "$PROBE_DIR/$backend-identity.exe" "$WORK/$backend-identity.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$IDENTITY_PROBE" "--backend=$backend" --opt=dev \
            -o "$REL/$backend-identity.exe" >"$WORK/$backend-identity.compile" 2>&1
    fi
    sha256sum "$WORK/$backend-identity.exe" >>"$WORK/probe-binaries.sha256"
    if [[ -n "$PROBE_DIR" ]]; then
        cp "$PROBE_DIR/$backend-constructor.exe" "$WORK/$backend-constructor.exe"
        cmp "$PROBE_DIR/$backend-constructor.exe" "$WORK/$backend-constructor.exe"
    else
        timeout 120 "$PGY" --native-pipeline "$CONSTRUCTOR_PROBE" "--backend=$backend" --opt=dev \
            -o "$REL/$backend-constructor.exe" >"$WORK/$backend-constructor.compile" 2>&1
    fi
    sha256sum "$WORK/$backend-constructor.exe" >>"$WORK/probe-binaries.sha256"
    for ((mutation=0; mutation<=18; mutation++)); do
        timeout 30 "$WORK/$backend-constructor.exe" "$FIXTURES/field_ctor_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-constructor-$mutation.raw" 2>"$WORK/$backend-constructor-$mutation.err"
        tr -d '\r' <"$WORK/$backend-constructor-$mutation.raw" >"$WORK/$backend-constructor-$mutation.run"
        [[ ! -s "$WORK/$backend-constructor-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-constructor-$mutation.run"
    done
    for mode in garbage 19 -1 01; do
        if "$WORK/$backend-constructor.exe" "$FIXTURES/field_ctor_identity_input.pgy" "$mode" \
            >"$WORK/$backend-constructor-mode-$mode.raw" 2>"$WORK/$backend-constructor-mode-$mode.err"; then
            echo "constructor observer accepted invalid mode $mode" >&2; exit 1
        else
            [[ $? == 2 && ! -s "$WORK/$backend-constructor-mode-$mode.err" ]]
        fi
        tr -d '\r' <"$WORK/$backend-constructor-mode-$mode.raw" >"$WORK/$backend-constructor-mode-$mode.run"
        grep -Fxq 'invalid mutation' "$WORK/$backend-constructor-mode-$mode.run"
    done
    for ((mutation=0; mutation<=12; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/inout_forward_owned_push_drop_positive.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=13; mutation<=15; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/inout_index_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=16; mutation<=21; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/own_storage_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=22; mutation<=25; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/own_event_order_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=26; mutation<=36; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/own_assign_single_positive.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=37; mutation<=50; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/borrow_formal_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=51; mutation<=78; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/own_event_order_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=81; mutation<=84; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/builtin_completion_nested_other_positive.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=85; mutation<=94; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/storage_producer_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    for ((mutation=95; mutation<=101; mutation++)); do
        timeout 30 "$WORK/$backend-identity.exe" "$FIXTURES/own_event_order_identity_input.pgy" "$mutation" \
            >"$WORK/$backend-mutation-$mutation.raw" 2>"$WORK/$backend-mutation-$mutation.err"
        tr -d '\r' <"$WORK/$backend-mutation-$mutation.raw" >"$WORK/$backend-mutation-$mutation.run"
        [[ ! -s "$WORK/$backend-mutation-$mutation.err" ]]
        printf 'true\n' >"$WORK/expected"
        cmp "$WORK/expected" "$WORK/$backend-mutation-$mutation.run"
    done
    timeout 30 "$WORK/$backend-identity.exe" "$ACTUAL_INPUT" -1 \
        >"$WORK/$backend-actual-producer.raw" 2>"$WORK/$backend-actual-producer.err"
    tr -d '\r' <"$WORK/$backend-actual-producer.raw" | LC_ALL=C sort >"$WORK/$backend-actual-producer.run"
    [[ ! -s "$WORK/$backend-actual-producer.err" ]]
    printf 'row_index:0=3\nseed:1=2\nseed:2=2\nseed:3=2\ntables:3=2\ntables:4=2\ntables:5=2\n' >"$WORK/expected"
    cmp "$WORK/expected" "$WORK/$backend-actual-producer.run"
    echo "[collection-inout-effect] native-$backend: ${#INPUTS[@]} source admissions, one generic-default selection, eighteen allocator-domain, thirteen numeric-allocation and twelve result-witness units, seventy-six diagnostic locations, three observer mode, fifty-one identity/boundary, thirty-five occurrence/root/element-step, four completion/receipt and ten storage-producer units, nineteen constructor units, four constructor CLI refusals and seven actual formal checks PASS"
done
sha256sum -c "$WORK/native.sha256"
sha256sum -c "$WORK/owners.sha256"
sha256sum -c "$WORK/inputs.sha256"
sha256sum --quiet -c "$WORK/imports.sha256"
sha256sum --quiet -c "$WORK/probe-binaries.sha256"
if [[ -n "$PROBE_DIR" ]]; then sha256sum --quiet -c "$PROBE_DIR/probe-binaries.sha256"; fi
echo "[collection-inout-effect] evidence: $REL (analyze-only inputs)"
