#!/usr/bin/env bash
# Owner line caps and generator drift only.
#
# Every remaining check is a source-size limit (require_max_lines,
# responsibility caps, family LOC totals), a generated-projection drift check,
# or the replacement-frontier build-graph dry run. Executable parity and
# focused negative gates prove behavior. Do not add source-text presence or
# absence checks here.

set -Eeuo pipefail
# Bare commands must not turn a failed structural check into a silent CI exit.
trap 'contract_status=$?; printf "[self-host-component-contract] command failed: %s:%s (status %s)\n" "${BASH_SOURCE[0]}" "$LINENO" "$contract_status" >&2' ERR

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "${SCRIPT_PATH%/*}" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SOURCE_SIZE_COUNTER="$ROOT_DIR/scripts/source_size_count.py"
SELF_HOST_DIR="$ROOT_DIR/src/self_hosted"

bash "$SCRIPT_DIR/self_hosted_component_checker_smoke.sh"

# A generated self-hosted source that no longer matches its native authority
# is a structural inventory fact, and this checker had no CI caller at all:
# the Never primitive reached src/semantic/type_system.c and the projection
# stayed behind for three days without a gate noticing.
python3 "$ROOT_DIR/scripts/render_native_primitive_type_projection.py" --check

fail() {
    echo "[self-host-component-contract] $*" >&2
    exit 1
}

check_replacement_frontier_build_graph() {
    local dry_run_build_dir installed_frontier_dry_run
    local standalone_frontier_dry_run admitted_frontier_dry_run
    local installed_bootstraps standalone_bootstraps admitted_bootstraps

    command -v make >/dev/null 2>&1 || fail "replacement frontier inventory requires make"
    mkdir -p "$ROOT_DIR/.tmp/self_hosted" || fail "could not prepare frontier graph inventory"
    dry_run_build_dir="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/frontier_graph.XXXXXX")" ||
        fail "could not isolate frontier graph inventory"
    # GNU make expands $(file ...) even with -n. Keep response files away from
    # the caller's build and require no prior compiler artifact or build directory.
    installed_frontier_dry_run="$(
        env -u PGY_SELF_HOST_COMPILER_ADMITTED \
        make -C "$ROOT_DIR" --no-print-directory -n \
            BUILD_DIR="$dry_run_build_dir" \
            self-host-replacement-frontier-installed-test-smoke
    )" || fail "installed replacement frontier dry-run failed"
    standalone_frontier_dry_run="$(
        env -u PGY_SELF_HOST_COMPILER_ADMITTED \
        make -C "$ROOT_DIR" --no-print-directory -n \
            BUILD_DIR="$dry_run_build_dir" \
            self-host-replacement-frontier-test-smoke
    )" || fail "standalone replacement frontier dry-run failed"
    admitted_frontier_dry_run="$(
        make -C "$ROOT_DIR" --no-print-directory -n \
            BUILD_DIR="$dry_run_build_dir" PGY_SELF_HOST_COMPILER_ADMITTED=1 \
            self-host-replacement-frontier-test-smoke
    )" || fail "admitted replacement frontier dry-run failed"
    installed_bootstraps="$(grep -F -c \
        'tests/self_hosted/parity/self_host_compiler_build.sh' \
        <<<"$installed_frontier_dry_run" || true)"
    standalone_bootstraps="$(grep -F -c \
        'tests/self_hosted/parity/self_host_compiler_build.sh' \
        <<<"$standalone_frontier_dry_run" || true)"
    admitted_bootstraps="$(grep -F -c \
        'tests/self_hosted/parity/self_host_compiler_build.sh' \
        <<<"$admitted_frontier_dry_run" || true)"
    [[ "$installed_bootstraps" -eq 0 ]] ||
        fail "installed replacement frontier must be bootstrap-free"
    [[ "$standalone_bootstraps" -eq 1 ]] ||
        fail "standalone replacement frontier must bootstrap exactly once"
    [[ "$admitted_bootstraps" -eq 0 ]] ||
        fail "an admitted compiler pair must not be rebuilt"
}

find_stage_owner_sources() {
    local stage="$1"
    local stage_dir="$SELF_HOST_DIR/$stage"
    find "$stage_dir" -type f -name '*.pgy' \
        ! -path "$stage_dir/fixture/*" \
        ! -path "$stage_dir/expected/*" \
        | sort
}

LINE_CAP_REQUESTS=()

require_max_lines() {
    local rel="$1"
    local cap="$2"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing line-count input: $rel"
    [[ "$cap" =~ ^(0|[1-9][0-9]*)$ && "$rel" != *$'\n'* ]] ||
        fail "invalid line-count request: $rel / $cap"
    LINE_CAP_REQUESTS+=("$cap"$'\t'"$rel")
}

require_responsibility_owner_max_lines() {
    local rel="$1" cap
    cap="$(awk -v root="$ROOT_DIR" -v mode=lookup -v owner="$rel" \
        -f "$ROOT_DIR/tests/self_hosted_owner_size_policy.awk")" ||
        fail "missing or invalid responsibility size authority: $rel"
    require_max_lines "$rel" "$cap"
}

run_line_cap_checks() {
    # Call sites still own every individual limit, including duplicate paths.
    # Measure each input once; retain every cap, excluding lexical comments.
    [[ "${#LINE_CAP_REQUESTS[@]}" -gt 0 ]] || fail "no line-count requests"
    [[ -f "$SOURCE_SIZE_COUNTER" ]] || fail "missing source-size counter"
    if ! printf '%s\n' "${LINE_CAP_REQUESTS[@]}" |
        python3 "$SOURCE_SIZE_COUNTER" --root "$ROOT_DIR" --caps; then
        fail "line-count batch failed"
    fi
}

require_entrypoint_only_main() {
    local stage="$1"
    local rel="src/self_hosted/$stage/main.pgy"

    require_max_lines "$rel" 80
}

require_stage_owner_line_cap() {
    local stage="$1"
    local file
    local cap
    while IFS= read -r file; do
        rel="${file#"$ROOT_DIR/"}"
        cap=600
        if [[ "$rel" == "src/self_hosted/codegen/emission/stmt_emit.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_enum_payload_variant_provenance_verdict_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_statement_fact_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_collection_aggregate_release_transition_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_expression_environment_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_expression_verdict_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_signature_fact_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/ast_signature_type_expression_fact_owner.pgy" ||
              "$rel" == "src/self_hosted/semantic/diagnostic_owner.pgy" ]]; then
            require_responsibility_owner_max_lines "$rel"
            continue
        elif [[ "$rel" == "src/self_hosted/codegen/type_facts/type_env.pgy" ]]; then
            cap=640
        fi
        require_max_lines "$rel" "$cap"
    done < <(find_stage_owner_sources "$stage")
}

require_max_lines "src/self_hosted/semantic/callable_receiver_carriage_policy_owner.pgy" 160
require_max_lines \
    "src/self_hosted/mir_lower/program_routine_receiver_identity_owner.pgy" 240
require_max_lines "src/self_hosted/mir/routine_receiver_carriage_owner.pgy" 160
require_max_lines "src/self_hosted/mir/routine_cfg_append_owner.pgy" 320
require_max_lines "src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy" 180
require_max_lines "src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy" 100
# Data-only lifetime inventory: 20 additional concrete Array leaves, no policy.
require_max_lines "src/self_hosted/mir/body_type_bundle_storage_lifetime_owner.pgy" 165
require_max_lines "src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/codegen_callable_receiver_bridge_owner.pgy" 600
for domain_runtime_owner in \
    "src/self_hosted/mir_lower/domain_runtime_participant_role_fact_owner.pgy" \
    "src/self_hosted/mir_lower/domain_runtime_assignment_fact_owner.pgy" \
    "src/self_hosted/mir_lower/domain_runtime_plan_owner.pgy" \
    "src/self_hosted/codegen/input/domain_runtime_codegen_view_owner.pgy" \
    "src/self_hosted/compiler/domain_runtime_c_codegen_bridge_owner.pgy"; do
    require_max_lines "$domain_runtime_owner" 600
done
require_max_lines \
    "tests/self_hosted/parity/domain_runtime_assignment_execution_owner.sh" 360
require_max_lines \
    "tests/self_hosted/parity/domain_runtime_explicit_map_execution_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/domain_runtime_zone_sync_execution_owner.sh" 210
require_max_lines \
    "tests/self_hosted/parity/domain_runtime_zone_value_carriage_admission_owner.sh" 80
require_max_lines \
    "src/self_hosted/semantic/ast_zone_value_carriage_verdict_owner.pgy" 240
require_max_lines \
    "src/self_hosted/semantic/ast_zone_parameter_boundary_verdict_owner.pgy" 180
require_max_lines \
    "src/self_hosted/semantic/ast_zone_spawn_transport_verdict_owner.pgy" 140
require_max_lines "src/self_hosted/semantic/ast_containment_boundary_verdict_owner.pgy" 160
require_max_lines \
    "src/self_hosted/semantic/ast_zone_resource_shape_owner.pgy" 140
require_max_lines \
    "src/self_hosted/semantic/ast_zone_resource_carriage_rows_owner.pgy" 160
require_max_lines "src/self_hosted/codegen/emission/member_call_receiver_carriage_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/emission/role_receiver_binding_owner.pgy" 100
require_max_lines \
    "tests/self_hosted/parity/codegen_role_receiver_admission_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_callable_receiver_carriage_owner.sh" 240
require_max_lines "src/self_hosted/lib/json.pgy" 600
require_max_lines "src/self_hosted/lib/json_emit.pgy" 600
require_max_lines "src/self_hosted/lib/json_scan.pgy" 600
require_max_lines "src/self_hosted/lib/json_fact_table.pgy" 600

for stage in lexer parser semantic codegen; do
    require_entrypoint_only_main "$stage"
    require_stage_owner_line_cap "$stage"
done

for mir_producer_owner in \
    program_fact_owner.pgy \
    instruction_abi_receipt_fact_owner.pgy \
    canonical_instruction_id_owner.pgy \
    parallel_capture_fact_owner.pgy \
    expression_fact_owner.pgy \
    routine_input_owner.pgy \
    routine_local_inventory_owner.pgy \
    routine_iteration_owner.pgy \
    routine_statement_owner.pgy \
    routine_build_owner.pgy \
    routine_build_storage_lifetime_owner.pgy \
    body_type_bundle_storage_lifetime_owner.pgy \
    routine_assignment_owner.pgy \
    routine_local_predecessor_snapshot_owner.pgy \
    routine_control_transfer_owner.pgy \
    routine_if_owner.pgy \
    routine_match_owner.pgy \
    routine_match_pattern_owner.pgy \
    routine_match_merge_owner.pgy \
    routine_while_owner.pgy \
    routine_loop_header_phi_owner.pgy \
    routine_loop_header_backedge_binding_owner.pgy \
    routine_loop_exit_phi_owner.pgy \
    routine_for_owner.pgy \
    routine_tracked_statement_owner.pgy \
    routine_lower_owner.pgy \
    routine_entry_owner.pgy \
    artifact_lower_owner.pgy \
    program_domain_projection_owner.pgy \
    instruction_validation_owner.pgy \
    match_fact_owner.pgy \
    match_json_projection_owner.pgy \
    destructure_type_fact_owner.pgy \
    destructure_type_json_projection_owner.pgy \
    domain_runtime_assignment_fact_owner.pgy \
    domain_runtime_assignment_verify_owner.pgy \
    domain_runtime_assignment_json_owner.pgy \
    program_verify_owner.pgy \
    program_assignment_target_graph_contract_owner.pgy \
    json_projection_owner.pgy \
    instruction_json_artifact_writer_owner.pgy \
    instruction_abi_receipt_json_projection_owner.pgy \
    program_json_artifact_writer_owner.pgy; do
    require_max_lines "src/self_hosted/mir/$mir_producer_owner" 600
done
require_max_lines "tests/self_hosted/fixtures/json_owned_fragment_empty.pgy" 30
require_max_lines "tests/self_hosted/parity/json_owned_fragment_empty_runtime.sh" 95
require_max_lines "tests/self_hosted/fixtures/mir_json_writer_roles_only.pgy" 15
require_max_lines "src/self_hosted/compiler/canonical_mir_source_module_epoch_owner.pgy" 80
require_max_lines "src/self_hosted/mir_lower/program_lower.pgy" 220
require_max_lines \
    "src/self_hosted/mir_lower/program_top_level_routine_order_owner.pgy" 120
require_entrypoint_only_main "mir_lower"
require_stage_owner_line_cap "mir_lower"

for language_word_projection_owner_cap in \
    language_word_identity_projection_owner.pgy:330 \
    language_word_row_projection_owner.pgy:200 \
    language_keyword_compatibility_projection_owner.pgy:250; do
    language_word_projection_owner="${language_word_projection_owner_cap%%:*}"
    language_word_projection_cap="${language_word_projection_owner_cap##*:}"
    require_max_lines \
        "src/self_hosted/lexer/$language_word_projection_owner" \
        "$language_word_projection_cap"
done
require_max_lines \
    "src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy" 80
require_max_lines "tests/self_hosted/parity/generic_default_contracts_parser_parity.sh" 100
require_max_lines "src/self_hosted/parser/intent_default_clause_owner.pgy" 100
require_max_lines "tests/self_hosted/parity/parser_tool_build_leg.sh" 200
require_max_lines "src/self_hosted/semantic/ast_artifact_verdict_owner.pgy" 570
require_max_lines \
    "src/self_hosted/semantic/ast_artifact_verdict_contract_owner.pgy" 70
require_max_lines "src/self_hosted/semantic/ast_intent_signature_fact_owner.pgy" 320
require_max_lines "src/self_hosted/semantic/ast_intent_call_fact_owner.pgy" 140
require_max_lines "tests/self_hosted/parity/intent_callable_execution_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/intent_nested_callable_execution_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/intent_placed_nested_callable_execution_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/intent_execution_graph_target_mutation_owner.py" 100
require_max_lines \
    "tests/self_hosted/parity/intent_execution_zero_compensation_mutation_owner.py" 80
require_max_lines \
    "src/self_hosted/semantic/ast_intent_expression_environment_owner.pgy" 100
require_max_lines \
    "src/self_hosted/semantic/ast_body_expression_environment_owner.pgy" 80
require_max_lines "src/self_hosted/dir/intent_fact_owner.pgy" 590
require_max_lines \
    "src/self_hosted/dir/intent_exact_identity_contract_owner.pgy" 450
require_max_lines "src/self_hosted/dir/intent_participant_contract_owner.pgy" 100
require_max_lines "src/self_hosted/dir/intent_step_fact_owner.pgy" 540
require_max_lines "src/self_hosted/semantic/ast_intent_transition_row_owner.pgy" 240
require_max_lines "src/self_hosted/dir/intent_step_target_contract_owner.pgy" 100
require_max_lines "src/self_hosted/dir/intent_row_owner.pgy" 80
require_max_lines "src/self_hosted/dir/intent_step_clause_fact_owner.pgy" 240
require_max_lines "src/self_hosted/dir/intent_step_carriage_contract_owner.pgy" 120
require_max_lines "src/self_hosted/dir/intent_mode_fact_owner.pgy" 160
require_max_lines "src/self_hosted/dir/intent_priority_fact_owner.pgy" 220
require_max_lines "src/self_hosted/dir/intent_child_policy_owner.pgy" 40
require_max_lines \
    "src/self_hosted/dir/intent_step_provenance_fact_owner.pgy" 190
require_max_lines "src/self_hosted/mir/intent_routine_owner.pgy" 500
require_max_lines "src/self_hosted/mir/intent_mode_carriage_owner.pgy" 80
require_max_lines "src/self_hosted/mir/intent_priority_carriage_owner.pgy" 120
require_max_lines "src/self_hosted/mir/intent_resource_lifetime_owner.pgy" 140
require_max_lines "src/self_hosted/mir/intent_execution_fact_owner.pgy" 720
require_max_lines "src/self_hosted/mir/intent_execution_schema_owner.pgy" 220
require_max_lines "src/self_hosted/mir/intent_execution_digest_owner.pgy" 220
for owner in \
    intent_execution_json_decode_owner.pgy \
    intent_execution_json_rows_owner.pgy \
    intent_execution_identity_index_owner.pgy \
    intent_execution_plan_fact_owner.pgy; do
    require_max_lines "src/self_hosted/mir_lower/$owner" 600
done

require_max_lines "src/self_hosted/mir/intent_phase_contract_owner.pgy" 80
require_max_lines "src/self_hosted/mir/intent_phase_emission_owner.pgy" 120
require_max_lines "src/self_hosted/mir_lower/intent_lower_owner.pgy" 430
for owner_cap in \
    "intent_routine_tree_projection_owner.pgy:100" \
    "intent_routine_plan_owner.pgy:240" \
    "intent_routine_carrier_projection_owner.pgy:180" \
    "intent_routine_step_projection_owner.pgy:100" \
    "intent_routine_step_plan_owner.pgy:330" \
    "intent_step_placement_contract_owner.pgy:60" \
    "intent_step_placement_binding_owner.pgy:100" \
    "intent_step_subject_binding_owner.pgy:140" \
    "intent_execution_carrier_projection_owner.pgy:180" \
    "intent_execution_structure_owner.pgy:220" \
    "intent_execution_tree_projection_owner.pgy:320" \
    "expression_graph_semantic_occurrence_owner.pgy:160" \
    "intent_execution_graph_mirror_owner.pgy:240" \
    "intent_execution_graph_target_owner.pgy:180"; do
    owner="${owner_cap%%:*}"
    cap="${owner_cap##*:}"
    require_max_lines "src/self_hosted/mir_lower/$owner" "$cap"
done
require_max_lines \
    "src/self_hosted/mir_lower/expression_graph_persisted_shape_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_intent_plan_projection_owner.pgy" 220
require_max_lines \
    "src/self_hosted/codegen/input/intent_execution_codegen_view_owner.pgy" 180
require_max_lines \
    "src/self_hosted/codegen/input/intent_policy_codegen_view_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/intent_policy_c_codegen_bridge_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/intent_execution_c_codegen_bridge_owner.pgy" 100
require_max_lines "src/self_hosted/semantic/intent_subject_slot_policy_owner.pgy" 70
require_max_lines \
    "src/self_hosted/mir_lower/intent_phase_projection_owner.pgy" 260
require_max_lines \
    "src/self_hosted/mir_lower/intent_expression_carrier_contract_owner.pgy" 50
require_max_lines \
    "src/self_hosted/mir_lower/intent_mode_projection_owner.pgy" 100
require_max_lines \
    "src/self_hosted/mir_lower/intent_priority_projection_owner.pgy" 120
require_max_lines "src/self_hosted/mir_lower/intent_phase_tree_owner.pgy" 120
require_max_lines \
    "tests/self_hosted/parity/intent_typed_outcome_compensation_owner.sh" 260
require_max_lines \
    "tests/self_hosted/parity/intent_guard_post_compensation_execution_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/fixture/intent_declaration_rows_negative_probe.pgy" 110
require_max_lines "src/self_hosted/codegen/emission/intent_emit_owner.pgy" 360
require_max_lines \
    "src/self_hosted/codegen/emission/intent_action_step_emit_owner.pgy" 120
require_max_lines \
    "src/self_hosted/codegen/emission/intent_nested_call_emit_owner.pgy" 70
require_max_lines "src/self_hosted/semantic/ast_intent_action_call_fact_owner.pgy" 250
require_max_lines "tests/self_hosted/fixtures/intent_participant_readonly_query.pgy" 90
require_max_lines "tests/self_hosted/parity/intent_participant_readonly_query_owner.sh" 65
require_max_lines "tests/self_hosted/fixtures/compiler_readonly_query.pgy" 90
require_max_lines "tests/self_hosted/parity/compiler_readonly_query_owner.sh" 65
require_max_lines "tests/self_hosted/fixtures/call_spine_root_projection.pgy" 165
require_max_lines "tests/self_hosted/parity/call_spine_root_projection_owner.sh" 65
require_max_lines "tests/self_hosted/fixtures/logical_record_shape_query.pgy" 170
require_max_lines "tests/self_hosted/parity/logical_record_shape_query_owner.sh" 65
require_max_lines "tests/self_hosted/fixtures/logical_record_constructor_query.pgy" 210
require_max_lines "tests/self_hosted/parity/logical_record_constructor_query_owner.sh" 65
require_max_lines \
    "src/self_hosted/semantic/ast_zone_authority_fact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/semantic/ast_zone_authority_validation_owner.pgy" 200
require_max_lines \
    "src/self_hosted/mir/intent_zone_authority_transition_fact_owner.pgy" 150
require_max_lines \
    "src/self_hosted/mir_lower/intent_zone_authority_transition_owner.pgy" 120
require_max_lines \
    "src/self_hosted/mir_lower/program_declaration_zone_authority_index_owner.pgy" 180
require_max_lines \
    "src/self_hosted/codegen/input/intent_zone_authority_transition_codegen_view_owner.pgy" 240
require_max_lines \
    "src/self_hosted/codegen/input/intent_zone_authority_source_seal_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/intent_zone_authority_transition_c_codegen_bridge_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/semantic_intent_zone_authority_transition_codegen_bridge_owner.pgy" 320
require_max_lines \
    "src/self_hosted/codegen/emission/intent_signature_emit_owner.pgy" 180
require_max_lines \
    "src/self_hosted/codegen/emission/intent_control_flow_emit_owner.pgy" 180
require_max_lines \
    "src/self_hosted/codegen/emission/intent_observability_emit_owner.pgy" 220
require_max_lines \
    "src/self_hosted/codegen/emission/intent_mode_emit_owner.pgy" 60
require_max_lines \
    "src/self_hosted/codegen/emission/intent_priority_emit_owner.pgy" 80
require_max_lines \
    "tests/self_hosted/parity/intent_priority_nested_observability_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/intent_mode_nested_observability_owner.sh" 170
for owner_cap in \
    "intent_execution_plan_emit_owner.pgy:180" \
    "intent_execution_plan_index_owner.pgy:180" \
    "intent_execution_plan_local_emit_owner.pgy:120" \
    "intent_execution_plan_control_emit_owner.pgy:260"; do
    owner="${owner_cap%%:*}"
    cap="${owner_cap##*:}"
    require_max_lines "src/self_hosted/codegen/emission/$owner" "$cap"
done
require_max_lines \
    "src/self_hosted/codegen/emission/expr_semantic_machine_call_emit_owner.pgy" 100
require_max_lines \
    "src/self_hosted/codegen/emission/expr_semantic_domain_query_emit_owner.pgy" 100
require_max_lines "src/self_hosted/dir/domain_graph_inventory_owner.pgy" 650
require_max_lines "src/self_hosted/dir/domain_graph_row_owner.pgy" 190
require_max_lines "src/self_hosted/dir/zone_state_row_fact_owner.pgy" 300
require_max_lines "src/self_hosted/compiler/dir_text_artifact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/dir_intent_text_artifact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/dir_text_row_format_owner.pgy" 40
require_max_lines \
    "src/self_hosted/parser/intent_parameter_resolution_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/wrapper_type_owner.pgy" 300
require_max_lines "src/self_hosted/semantic/collection_mutation_policy_owner.pgy" 300
# Exact scratch-retirement callers add generated data rows only; no traversal
# allowance changes. Generator parity owns the complete caller identities.
require_max_lines "src/self_hosted/semantic/compiler_internal_builtin_caller_registry_owner.pgy" 125
require_max_lines "src/self_hosted/semantic/enum_callable_signature_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/source_bundle_owner.pgy" 320
require_max_lines "src/self_hosted/semantic/builtin_signature_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/builtin_capability_projection_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/capability_mask_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/ast_capability_fact_owner.pgy" 500
require_max_lines "src/self_hosted/semantic/ast_capability_call_graph_owner.pgy" 350
require_max_lines "src/self_hosted/semantic/ast_capability_instantiation_owner.pgy" 300
require_max_lines "src/self_hosted/compiler/capability_manifest_owner.pgy" 130
require_max_lines "src/self_hosted/lib/intent_observability_abi_projection_owner.pgy" 150
require_max_lines "src/self_hosted/semantic/builtin_shadow_owner.pgy" 240
require_max_lines "src/self_hosted/lib/source_scan_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/delimited_range_fact_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/expression_normalization_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/diagnostic_contract_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/emission/tagged_enum_match_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/target_capability_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/sandbox_capability_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/sandbox_capability_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/backend_emitter_contract_owner.pgy" 600
require_max_lines "src/self_hosted/tools/backend_emitter_contract_checker/main.pgy" 600
require_max_lines "src/self_hosted/tools/backend_emitter_contract_checker/report_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/backend_air_access_contract_owner.pgy" 600
require_max_lines "src/self_hosted/tools/backend_air_access_checker/main.pgy" 600
require_max_lines "src/self_hosted/tools/backend_air_access_checker/report_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/symbol_table_owner.pgy" 400
require_max_lines \
    "src/self_hosted/codegen/emission/implicit_receiver_c_parameter_owner.pgy" 60
require_max_lines "src/self_hosted/codegen/emission/function_global_env_owner.pgy" 280
require_max_lines "src/self_hosted/codegen/emission/function_emit.pgy" 500
require_max_lines \
    "src/self_hosted/codegen/emission/function_prototype_block_owner.pgy" 270
require_max_lines "src/self_hosted/codegen/emission/program_emit.pgy" 650
require_max_lines "src/self_hosted/codegen/emission/program_function_definition_block_owner.pgy" 180
require_max_lines \
    "src/self_hosted/codegen/type_facts/type_env_global_index_owner.pgy" 400
require_max_lines \
    "src/self_hosted/codegen/type_facts/type_env_local_row_scan_owner.pgy" 80
require_max_lines \
    "src/self_hosted/codegen/type_facts/type_env_local_row_materialization_owner.pgy" 140
require_max_lines \
    "src/self_hosted/codegen/type_facts/type_env_state_lifetime_owner.pgy" 140
require_max_lines "src/self_hosted/codegen/text/owned_string_join_owner.pgy" 20
require_max_lines "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_row_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_abi_paths_owner.pgy" 600
require_max_lines "src/self_hosted/tools/backend_abi_layout_contract_checker/main.pgy" 600
require_max_lines "src/self_hosted/tools/backend_abi_layout_contract_checker/report_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/runtime_call_abi_row_owner.pgy" 620
require_max_lines "src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy" 680
require_max_lines "src/self_hosted/compiler/runtime_call_abi_row_manifest.pgy" 600
require_max_lines "src/self_hosted/codegen/runtime_abi/task_runtime_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_task_builtin_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_task_expression_kind_owner.pgy" 10
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_builtin_argument_prefix_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_external_runtime_expression_readiness_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_external_runtime_declaration_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_external_runtime_declaration_owner.pgy" 70
require_max_lines "src/self_hosted/compiler/test_harness_target_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_backend_contract_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/compatibility_evolution_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_compatibility_paths_owner.pgy" 600
require_max_lines "src/self_hosted/tools/compatibility_evolution_checker/main.pgy" 600
require_max_lines "src/self_hosted/tools/compatibility_evolution_checker/report_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_execution_lane_paths_owner.pgy" 600
require_max_lines "src/self_hosted/sea/lane_executor_contract_owner.pgy" 600
require_max_lines "src/self_hosted/sea/lane_executor_contract.pgy" 600

require_max_lines "src/self_hosted/parallel/chunk_policy_owner.pgy" 600
require_max_lines "src/self_hosted/parallel/chunk_policy_manifest.pgy" 600

require_max_lines "src/self_hosted/parallel/lane_policy_owner.pgy" 600
require_max_lines "src/self_hosted/parallel/lane_policy_manifest.pgy" 600

require_max_lines "src/self_hosted/parallel/spawn_lane_plan_owner.pgy" 600
require_max_lines "src/self_hosted/parallel/spawn_lane_plan_manifest.pgy" 600

require_max_lines "src/self_hosted/compiler/region_plan_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/region_plan_manifest.pgy" 600

require_max_lines "src/self_hosted/compiler/reachability_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/reachability_manifest.pgy" 600
require_max_lines "src/self_hosted/hir/ast_expression_graph_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/emission/program_admitted_semantic_owner.pgy" 100
require_max_lines "src/self_hosted/parser/expression_graph_owner.pgy" 600
require_max_lines "src/self_hosted/parser/expression_fact_owner.pgy" 100
require_max_lines "src/self_hosted/parser/expression_scalar_fact_owner.pgy" 100
require_max_lines "src/self_hosted/hir/ast_source_module_fact_owner.pgy" 126
require_max_lines "src/self_hosted/codegen/emission/program_entry_owner.pgy" 100
require_max_lines "src/self_hosted/semantic/ast_body_analysis_admission_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/ast_body_analysis_shape_owner.pgy" 240
require_max_lines \
    "src/self_hosted/semantic/ast_body_analysis_admission_contract_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/ast_signature_fact_owner.pgy" 619
require_max_lines "src/self_hosted/semantic/ast_signature_type_expression_fact_owner.pgy" 677
require_max_lines "src/self_hosted/semantic/ast_signature_type_binding_owner.pgy" 599
require_max_lines \
    "src/self_hosted/semantic/ast_callable_receiver_source_parameter_owner.pgy" 60
require_max_lines \
    "src/self_hosted/semantic/ast_entrypoint_selection_policy_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/ast_signature_artifact_match_owner.pgy" 599
require_max_lines "src/self_hosted/semantic/ast_nominal_constructor_fact_owner.pgy" 599
require_max_lines "src/self_hosted/semantic/ast_type_name_canonical_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_expression_environment_owner.pgy" 676
require_max_lines "src/self_hosted/codegen/emission/enum_emit_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_initializer_type_fact_owner.pgy" 548
require_max_lines "src/self_hosted/semantic/ast_initializer_type_query_owner.pgy" 80
require_max_lines "src/self_hosted/semantic/ast_initializer_type_contract_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/ast_body_type_bundle_owner.pgy" 60
# Body assembly also seals and carries retained result/formal projections.
require_max_lines "src/self_hosted/semantic/ast_body_type_bundle_assembly_owner.pgy" 325
require_max_lines "src/self_hosted/semantic/ast_body_type_bundle_contract_owner.pgy" 180
require_max_lines "src/self_hosted/semantic/ast_initializer_iteration_refinement_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/ast_generic_specialization_fact_owner.pgy" 615
require_max_lines "src/self_hosted/semantic/ast_generic_specialization_query_owner.pgy" 92
require_max_lines "src/self_hosted/semantic/ast_expression_call_identity_owner.pgy" 160
require_max_lines "src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy" 166
require_max_lines "src/self_hosted/semantic/ast_expression_call_return_type_owner.pgy" 260
require_max_lines "src/self_hosted/semantic/ast_assignment_fact_owner.pgy" 600
require_max_lines \
    "src/self_hosted/semantic/ast_assignment_target_graph_owner.pgy" 80
require_responsibility_owner_max_lines "src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
require_responsibility_owner_max_lines "src/self_hosted/semantic/ast_statement_fact_owner.pgy"
require_max_lines "src/self_hosted/semantic/ast_expression_verdict_fact_owner.pgy" 80
require_max_lines "src/self_hosted/semantic/ast_expression_verdict_owner.pgy" 614
require_max_lines "src/self_hosted/semantic/ast_expression_graph_identifier_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/ast_statement_type_fact_owner.pgy" 540

require_max_lines "src/self_hosted/semantic/ast_statement_type_contract_owner.pgy" 200
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_array_set_graph_negative_owner.sh" 100
require_max_lines "src/self_hosted/semantic/ast_body_verdict_owner.pgy" 600
require_max_lines \
    "src/self_hosted/compiler/driver_rung2_enum_payload_variant_admission_owner.pgy" 45
require_max_lines "src/self_hosted/compiler/driver_rung2_owner.pgy" 525
require_max_lines "src/self_hosted/compiler/canonical_mir_execution_owner.pgy" 140
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_analysis_admission_owner.sh" 130
require_max_lines \
    "src/self_hosted/compiler/driver_rung2_intent_consumer_owner.pgy" 80
require_max_lines "src/self_hosted/compiler/canonical_mir_identity_epoch_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/canonical_mir_field_identity_epoch_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung2_readiness_owner.pgy" 100
require_max_lines "src/self_hosted/compiler/driver_rung2_semantic_fixture_manifest_owner.pgy" 80
require_max_lines "src/self_hosted/parser/expression_graph_contract_owner.pgy" 180
require_max_lines "src/self_hosted/mir/expression_graph_fact_owner.pgy" 600
require_max_lines "src/self_hosted/mir/expression_graph_kind_name_owner.pgy" 300
require_max_lines "src/self_hosted/codegen/emission/function_binding_env_owner.pgy" 140
require_max_lines "src/self_hosted/semantic/ast_expression_owner_field_environment_owner.pgy" 137
require_max_lines "src/self_hosted/semantic/ast_expression_call_target_fact_owner.pgy" 300
require_max_lines "src/self_hosted/semantic/ast_expression_call_target_capture_owner.pgy" 352
require_max_lines "src/self_hosted/semantic/ast_expression_call_target_contract_owner.pgy" 300
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_callable_parameter_identity_owner.sh" 240
require_max_lines \
    "tests/self_hosted/parity/callable_parameter_installed_self_host_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/lowercase_entrypoint_installed_self_host_owner.sh" 180
require_max_lines "src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy" 200
require_max_lines "src/self_hosted/compiler/machine_layer_declaration_consumer.pgy" 260
require_max_lines \
    "src/self_hosted/mir_lower/program_routine_block_fact_owner.pgy" 180
require_max_lines \
    "src/self_hosted/mir_lower/program_instruction_expression_index_owner.pgy" 430
require_max_lines \
    "src/self_hosted/mir_lower/program_instruction_routing_span_owner.pgy" 80
require_max_lines \
    "src/self_hosted/mir_lower/routine_instruction_fact_bundle_owner.pgy" 320
require_max_lines "src/self_hosted/mir/routine_let_owner.pgy" 500
require_max_lines "src/self_hosted/mir_lower/expression_graph_fact_owner.pgy" 600
require_max_lines "src/self_hosted/mir_lower/expression_graph_occurrence_owner.pgy" 200
require_max_lines \
    "src/self_hosted/mir_lower/expression_graph_indexed_instruction_policy_owner.pgy" 100
require_max_lines "tests/self_hosted/fixtures/expression_graph_identity_prefix_owner.pgy" 115
require_max_lines "tests/self_hosted/parity/expression_graph_identity_prefix_owner_smoke.sh" 80
require_max_lines \
    "src/self_hosted/semantic/ast_body_type_bundle_admission_receipt_owner.pgy" 90
require_max_lines \
    "src/self_hosted/semantic/ast_body_type_bundle_admission_receipt_schema_owner.pgy" 40
require_max_lines "src/self_hosted/semantic/ast_collection_execution_context_fact_owner.pgy" 150
require_max_lines "src/self_hosted/semantic/ast_collection_member_read_call_target_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_non_trapping_operand_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_non_trapping_comparison_owner.pgy" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_indexed_assignment_parity_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_index_expression_type_parity_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_iteration_graph_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_foreach_call_type_parity_owner.sh" 100
require_max_lines "src/self_hosted/semantic/ast_iteration_graph_root_owner.pgy" 220
require_max_lines "tests/self_hosted/parity/driver_rung2_array_argument_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_struct_argument_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_struct_value_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_generic_struct_value_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_inferred_generic_value_parity_owner.sh" 140
require_max_lines "src/self_hosted/mir/loop_reachability_fact_owner.pgy" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_loop_phi_parity_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_destructure_parity_owner.sh" 120
require_max_lines "src/self_hosted/mir_lower/destructure_expression_projection_owner.pgy" 120
require_max_lines "src/self_hosted/mir_lower/destructure_temporary_owner.pgy" 160
require_max_lines "src/self_hosted/mir/routine_defer_owner.pgy" 120
require_max_lines "src/self_hosted/codegen/runtime_abi/runtime_header_owner.pgy" 60
require_max_lines "src/self_hosted/codegen/runtime_abi/runtime_header_ownership_owner.pgy" 60
require_max_lines "tests/self_hosted/parity/intent_observability_installed_self_host_owner.sh" 160
require_max_lines "tests/self_hosted/parity/direct_mir_legacy_intent_program_llvm_owner.sh" 190
require_max_lines "src/self_hosted/compiler/direct_mir_intent_completion_fact_owner.pgy" 40
require_max_lines "src/self_hosted/compiler/direct_mir_intent_completion_llvm_emission_owner.pgy" 40
require_max_lines "tests/self_hosted/parity/direct_mir_composite_intent_program_llvm_owner.sh" 200
require_max_lines "tests/self_hosted/parity/direct_mir_nested_intent_program_llvm_owner.sh" 190
require_max_lines "tests/self_hosted/parity/direct_mir_nested_intent_program_c_owner.sh" 220
while IFS='|' read -r composite_intent_owner composite_intent_cap; do
    require_max_lines \
        "src/self_hosted/compiler/$composite_intent_owner" \
        "$composite_intent_cap"
done <<'COMPOSITE_INTENT_OWNERS'
direct_mir_composite_intent_program_route_fact_owner.pgy|170
direct_mir_composite_intent_program_graph_fact_owner.pgy|350
direct_mir_composite_intent_program_projection_owner.pgy|40
COMPOSITE_INTENT_OWNERS
require_responsibility_owner_max_lines \
    "src/self_hosted/compiler/direct_mir_composite_intent_program_plan_owner.pgy"
require_responsibility_owner_max_lines \
    "src/self_hosted/compiler/direct_mir_composite_intent_program_llvm_emission_owner.pgy"
while IFS='|' read -r nested_intent_owner nested_intent_cap; do
    require_max_lines \
        "src/self_hosted/compiler/$nested_intent_owner" \
        "$nested_intent_cap"
done <<'NESTED_INTENT_OWNERS'
direct_mir_nested_intent_program_route_fact_owner.pgy|170
direct_mir_nested_intent_program_graph_fact_owner.pgy|600
direct_mir_nested_intent_program_plan_owner.pgy|700
direct_mir_nested_intent_program_c_emission_owner.pgy|360
direct_mir_nested_intent_program_llvm_emission_owner.pgy|360
direct_mir_nested_intent_program_projection_owner.pgy|40
driver_rung2_nested_intent_c_substitution_owner.pgy|50
NESTED_INTENT_OWNERS
require_max_lines "tests/self_hosted/parity/intent_observability_mir_identity_owner.sh" 145
require_max_lines "tests/self_hosted/parity/intent_observability_mir_identity_mutations.py" 90
require_max_lines "tests/self_hosted/parity/emitted_c_runtime_header_owner.sh" 20
require_max_lines "tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh" 20
require_max_lines "tests/self_hosted/parity/driver_rung2_pipeline_step_owner.sh" 120
require_max_lines "tests/self_hosted/parity/driver_rung2_defer_parity_owner.sh" 60
require_max_lines "tests/self_hosted/parity/driver_rung2_defer_graph_negative_owner.sh" 40
require_max_lines "src/self_hosted/mir/generic_specialization_owner.pgy" 220
require_max_lines "src/self_hosted/mir/generic_specialization_json_projection_owner.pgy" 100
require_max_lines "src/self_hosted/mir_lower/generic_specialization_fact_owner.pgy" 420
require_max_lines \
    "src/self_hosted/mir_lower/generic_specialization_identity_epoch_owner.pgy" 140
require_max_lines "tests/self_hosted/parity/driver_rung2_spawn_await_parity_owner.sh" 120
require_max_lines "tests/self_hosted/parity/driver_rung2_generic_spawn_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_generic_string_spawn_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_generic_spawn_mixed_parity_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_generic_default_contract_parity_owner.sh" 120
require_max_lines "tests/self_hosted/parity/driver_rung2_generic_default_contract_parity.sh" 120
require_max_lines "tests/self_hosted/parity/driver_rung2_nested_generic_containers_parity_owner.sh" 140
require_max_lines "tests/self_hosted/parity/driver_rung2_list_ops_parity_owner.sh" 120
require_max_lines "tests/self_hosted/parity/driver_rung2_list_int_loop_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_for_in_list_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_list_push_scalar_value_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_list_shadow_scope_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_list_literal_context_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_queue_ops_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_set_ops_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_set_index_value_parity_owner.sh" 100
require_max_lines "src/self_hosted/semantic/ast_expression_graph_set_call_owner.pgy" 220
require_max_lines "src/self_hosted/codegen/emission/set_call_emit_owner.pgy" 80
require_max_lines "src/self_hosted/codegen/emission/set_call_type_owner.pgy" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_result_field_parity_owner.sh" 140
require_max_lines "tests/self_hosted/parity/driver_rung2_string_concat_alias_parity_owner.sh" 140
require_max_lines "src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy" 100
require_max_lines "src/self_hosted/semantic/ast_expression_graph_nominal_constructor_argument_owner.pgy" 100
require_max_lines "src/self_hosted/semantic/ast_nominal_constructor_lookup_owner.pgy" 74
require_max_lines "src/self_hosted/semantic/ast_expression_graph_nominal_constructor_call_owner.pgy" 160
require_max_lines "src/self_hosted/semantic/ast_domain_query_protocol_owner.pgy" 80
require_max_lines "src/self_hosted/semantic/ast_expression_graph_domain_query_owner.pgy" 420
require_max_lines "tests/self_hosted/parity/world_tobject_projection_query_owner.sh" 150
require_max_lines "tests/self_hosted/parity/tobject_boundary_execution_owner.sh" 180
require_max_lines "tests/self_hosted/parity/tobject_boundary_self_mir_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_fallible_tobject_outcome_owner.sh" 140
require_max_lines "tests/self_hosted/parity/driver_rung2_fieldless_class_parity_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_recursive_call_target_parity_owner.sh" 80
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_wrapper_match_loop_phi_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_try_parity_owner.sh" 60
require_max_lines "tests/self_hosted/parity/driver_rung2_option_struct_value_parity_owner.sh" 100
require_max_lines "src/self_hosted/semantic/ast_expression_graph_type_owner.pgy" 160
require_max_lines "src/self_hosted/semantic/ast_expression_graph_scalar_type_owner.pgy" 599
require_max_lines "src/self_hosted/semantic/ast_expression_graph_wrapper_value_owner.pgy" 599
require_max_lines "src/self_hosted/tools/wrapper_policy_probe/main.pgy" 300
require_max_lines "tests/self_hosted/parity/wrapper_policy_probe_parity.sh" 300
require_max_lines "src/self_hosted/semantic/ast_expression_graph_collection_mutation_owner.pgy" 300
require_max_lines "src/self_hosted/tools/collection_policy_probe/main.pgy" 300
require_max_lines "tests/self_hosted/parity/collection_policy_probe_parity.sh" 300
require_max_lines "src/self_hosted/semantic/array_type_shape_owner.pgy" 80
require_max_lines "src/self_hosted/semantic/tuple_type_shape_owner.pgy" 100
require_max_lines "src/self_hosted/semantic/ast_expression_graph_struct_view_owner.pgy" 127
require_max_lines "src/self_hosted/semantic/ast_expression_graph_struct_type_verdict_owner.pgy" 194
require_max_lines "src/self_hosted/semantic/ast_expression_graph_field_type_owner.pgy" 180
require_max_lines "src/self_hosted/tools/aggregate_field_policy_probe/main.pgy" 300
require_max_lines "tests/self_hosted/parity/aggregate_field_policy_probe_parity.sh" 300
require_max_lines "src/self_hosted/semantic/ast_expression_graph_view_owner.pgy" 100
require_max_lines "src/self_hosted/parser/expression_set_literal_graph_owner.pgy" 100
require_max_lines "src/self_hosted/parser/expression_set_literal_contract_owner.pgy" 100
require_max_lines "src/self_hosted/mir_lower/assignment_binding_mode_fact_owner.pgy" 100
require_max_lines "src/self_hosted/mir/program_assignment_parameter_use_contract_owner.pgy" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_assignment_binding_mode_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_else_if_graph_parity_owner.sh" 40
require_max_lines "tests/self_hosted/parity/driver_rung2_continue_parity_owner.sh" 60
require_max_lines "src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy" 310
require_max_lines "src/self_hosted/mir_lower/expression_graph_persisted_read_owner.pgy" 360
require_max_lines \
    "src/self_hosted/mir_lower/expression_graph_persisted_node_read_owner.pgy" 300
require_max_lines \
    "src/self_hosted/mir_lower/expression_graph_kind_code_owner.pgy" 100
require_max_lines "src/self_hosted/mir_lower/expression_graph_match_owner.pgy" 220
require_max_lines "src/self_hosted/mir_lower/expression_graph_option_match_owner.pgy" 120
require_max_lines "src/self_hosted/mir_lower/expression_graph_tagged_enum_match_owner.pgy" 220
require_max_lines "src/self_hosted/mir_lower/expression_graph_fact_owner.pgy" 280
require_max_lines "src/self_hosted/mir_lower/expression_graph_instruction_policy_owner.pgy" 120
require_max_lines "src/self_hosted/compiler/driver_rung2_cli_owner.pgy" 40
require_max_lines "src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy" 320
require_max_lines "src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy" 150
require_max_lines \
    "src/self_hosted/compiler/driver_source_c_stdout_execution_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/driver_mir_c_stdout_execution_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/driver_source_mir_stdout_execution_owner.pgy" 60
require_max_lines \
    "src/self_hosted/mir_lower/mir_diagnostic_projection_owner.pgy" 200
require_max_lines "src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy" 160
require_max_lines \
    "src/self_hosted/compiler/driver_rung2_artifact_request_execution_owner.pgy" 130
require_max_lines "src/self_hosted/compiler/driver_rung2_main.pgy" 30
require_max_lines "src/self_hosted/compiler/driver_rung2_fixture_manifest_cli_owner.pgy" 60
require_max_lines "src/self_hosted/compiler/driver_rung2_fixture_manifest_main.pgy" 30
require_max_lines "src/self_hosted/semantic/ast_iteration_type_fact_owner.pgy" 600
require_max_lines "tests/self_hosted/parity/self_host_compiler_build.sh" 220
require_max_lines \
    "tests/self_hosted/parity/self_host_driver_fixed_point_receipt_owner.sh" 285
require_max_lines \
    "tests/self_hosted/parity/self_host_driver_fixed_point_receipt_smoke.sh" 125
require_max_lines \
    "tests/self_hosted/parity/self_host_fixed_point_profile_receipt_owner.sh" 340
require_max_lines \
    "tests/self_hosted/parity/self_host_fixed_point_profile_receipt_smoke.sh" 100
require_max_lines \
    "tests/self_hosted/parity/codegen_bootstrap_seed_receipt_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/codegen_bootstrap_seed_receipt_smoke.sh" 120
require_max_lines \
    "tests/self_hosted/parity/public_logical_operand_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_condition_not_bool_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_not_operand_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_compare_type_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_binop_type_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_let_type_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_assign_type_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_call_arg_type_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_builtin_arg_type_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_value_param_collection_mutation_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_call_arity_mismatch_json_diagnostic_receipt_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/nested_collection_call_arity_semantic_admission_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/vessel_method_argument_type_admission_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/array_index_type_semantic_admission_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/public_parser_callable_contract_json_diagnostic_receipt_owner.sh" 165
require_max_lines "src/self_hosted/lib/public_diagnostic_receipt_owner.pgy" 70
require_max_lines \
    "tests/self_hosted/parity/fixture/fmt_recovery_publication_owner.c" 80
require_max_lines "src/compiler/self_host_driver.c" 270
require_max_lines "src/compiler/self_host_debug_driver.c" 90
require_max_lines "src/compiler/self_host_fmt_driver.c" 100
require_max_lines "src/compiler/fmt.c" 180
require_max_lines "src/self_hosted/fmt/layout_owner.pgy" 200
require_max_lines "src/self_hosted/fmt/session_owner.pgy" 50
require_max_lines "src/compiler/self_host_mir_artifact_owner.c" 90
require_max_lines "src/compiler/self_host_mir_diagnostic_stdout_owner.c" 130
require_max_lines "src/compiler/self_host_artifact_process_owner.c" 100
require_max_lines "src/compiler/compiler_process.c" 620
require_max_lines "src/compiler/self_host_llvm_driver.c" 120
require_max_lines "src/compiler/driver_self_host_selection_owner.c" 140
require_max_lines "src/compiler/self_host_machine_manifest_artifact_owner.c" 70
require_max_lines "tests/self_hosted/parity/public_mir_json_installed_self_host_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/public_mir_diagnostic_installed_self_host_owner.sh" 250
require_max_lines \
    "tests/self_hosted/parity/fixture/silent_self_host_driver.c" 60
require_max_lines "tests/self_hosted/parity/public_machine_manifest_installed_self_host_owner.sh" 140
require_max_lines "tests/self_hosted/parity/public_tokens_installed_self_host_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/public_fmt_installed_self_host_owner.sh" 520
require_max_lines \
    "tests/self_hosted/parity/fixture/counting_self_host_fmt_driver.c" 70
require_max_lines "tests/self_hosted/parity/public_ast_installed_self_host_owner.sh" 140
require_max_lines "tests/self_hosted/parity/public_capability_manifest_installed_self_host_owner.sh" 180
require_max_lines "tests/self_hosted/parity/public_dir_installed_self_host_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/public_source_inspection_opt_profile_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/fixture/counting_self_host_source_stdout_driver.c" 45
require_max_lines \
    "tests/self_hosted/parity/public_mir_opt_profile_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/fixture/counting_self_host_mir_read_driver.c" 45
require_max_lines "src/compiler/driver_self_host_llvm_selection_owner.c" 60
require_max_lines "src/compiler/self_host_llvm_ir_artifact_owner.c" 90
require_max_lines "tests/self_hosted/parity/public_llvm_ir_installed_self_host_owner.sh" 140
require_max_lines "tests/self_hosted/parity/public_llvm_ir_opt_profile_owner.sh" 180
require_max_lines "tests/self_hosted/parity/public_nested_scalar_cfg_llvm_owner.sh" 70
require_max_lines "src/compiler/self_host_llvm_ir_stdout_owner.c" 80
require_max_lines "tests/self_hosted/parity/public_llvm_ir_stdout_installed_self_host_owner.sh" 120
require_max_lines "src/compiler/compiler_self_host_artifact.c" 180
require_max_lines "src/compiler/compiler_transient_artifact_workspace.c" 160
require_max_lines "src/pgy_driver.c" 343
require_max_lines "src/compiler/c_runner.c" 380
require_max_lines "src/compiler/llvm_runner.c" 300
require_max_lines "src/compiler/pkg.c" 300
require_max_lines \
    "tests/self_hosted/parity/package_commands_installed_self_host_owner.sh" 220
require_max_lines \
    "tests/self_hosted/parity/fixture/counting_package_self_host_driver.c" 90
require_max_lines "src/compiler/compiler.c" 560
require_max_lines "src/self_hosted/compiler/driver_bootstrap_main.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/compiler_root_intent_execution_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/driver_source_llvm_intent_protocol_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/driver_source_llvm_intent_execution_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/driver_source_llvm_projection_publication_owner.pgy" 90
require_max_lines \
    "tests/self_hosted/parity/canonical_mir_verified_projection_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/canonical_mir_routine_phase_identity_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/generic_specialization_identity_epoch_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/default_c_emit_installed_self_host_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh" 120
require_max_lines "tests/self_hosted/parity/fixture/counting_self_host_c_driver.c" 40
require_max_lines \
    "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/fixture/counting_self_host_llvm_driver.c" 90
require_max_lines "tests/self_hosted/parity/driver_rung2_mir_producer_parity_owner.sh" 320
require_max_lines "tests/self_hosted/parity/driver_rung2_match_materialization_delta_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_canonical_declaration_order_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh" 180
require_max_lines "tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_call_target_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_enum_argument_parity_owner.sh" 80
require_max_lines "tests/self_hosted/parity/driver_rung2_enum_return_parity_owner.sh" 80
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_class_enum_composition_parity_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_nominal_builtin_collision_parity_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_class_array_composition_parity_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_collection_enum_match_loop_parity_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_collection_option_coalesce_loop_parity_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_coalesce_bool_loop_parity_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/driver_rung2_nested_coalesce_parity_owner.sh" 100
require_max_lines "src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_expression_surface_query_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_expression_surface_contract_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy" 599
require_max_lines "src/self_hosted/semantic/ast_expression_graph_build_owner.pgy" 599
require_max_lines "src/self_hosted/semantic/ast_expression_graph_serialized_parser_contract_owner.pgy" 599
require_max_lines "src/self_hosted/semantic/ast_expression_typed_binding_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/input/ast_expression_usage_owner.pgy" 450
require_max_lines "src/self_hosted/codegen/emission/type_declaration_emit_owner.pgy" 360
require_max_lines "src/self_hosted/codegen/input/value_wrapper_usage_owner.pgy" 190
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_builtin_signature_projection_owner.pgy" 230
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_collection_clone_signature_owner.pgy" 40
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_to_string_expression_readiness_owner.pgy" 30
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_string_literal_fact_owner.pgy" 35
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_expression_fact_owner.pgy" 200
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_literal_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_join_materialization_owner.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_collection_builtin_projection.sh" 140
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_collection_builtin_mutations.py" 150
require_max_lines "src/self_hosted/codegen/emission/assign_emit_owner.pgy" 160
require_max_lines \
    "src/self_hosted/semantic/ast_expression_call_return_type_diagnostic_owner.pgy" 50
require_max_lines "src/self_hosted/parser/stmt_collection_graph_owner.pgy" 100
require_max_lines \
    "src/self_hosted/mir/routine_local_predecessor_snapshot_owner.pgy" 90
require_max_lines "src/self_hosted/mir/routine_loop_header_phi_owner.pgy" 100
require_max_lines \
    "src/self_hosted/mir/routine_loop_header_backedge_binding_owner.pgy" 80
require_max_lines "src/self_hosted/mir/routine_loop_exit_phi_owner.pgy" 90
require_max_lines "src/self_hosted/mir/routine_for_owner.pgy" 180
require_max_lines "src/self_hosted/semantic/ast_statement_type_query_owner.pgy" 150
require_max_lines "src/self_hosted/semantic/ast_expression_graph_lane_policy_owner.pgy" 100
require_max_lines "src/self_hosted/mir_lower/expression_graph_parser_bridge_owner.pgy" 80
require_max_lines "src/self_hosted/mir_lower/structured_expression_emission_order_owner.pgy" 220
require_max_lines \
    "src/self_hosted/codegen/emission/expression_c_text_materialization_owner.pgy" 70
require_max_lines \
    "src/self_hosted/codegen/emission/expr_semantic_struct_call_emit_owner.pgy" 100
require_max_lines "src/self_hosted/codegen/emission/expr_array_storage_release_emit_owner.pgy" 50
require_max_lines "src/self_hosted/codegen/emission/call_argument_order_owner.pgy" 200
require_max_lines \
    "src/self_hosted/codegen/emission/expr_semantic_identity_bound_call_emit_owner.pgy" 100
require_max_lines \
    "src/self_hosted/codegen/emission/expr_semantic_leaf_place_contract_owner.pgy" 80
# The existing call-view owner also owns the bounded, ephemeral whole-arena
# root projection. Keep it cohesive; the shared semantic-owner cap is unchanged.
require_max_lines "src/self_hosted/semantic/ast_expression_graph_call_view_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/ast_expression_graph_member_view_owner.pgy" 500
# 360: the expected-value dispatcher and the Array/List/Queue/Set literal
# emitters are one mutual-recursion cluster; the one-way import DAG forbids
# splitting them across files, so the budget carries all four collection kinds.
require_max_lines "src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy" 374
require_max_lines "src/self_hosted/semantic/ast_expression_graph_array_literal_owner.pgy" 500
require_max_lines "src/self_hosted/codegen/emission/expr_semantic_type_owner.pgy" 250
require_max_lines "tests/self_hosted/parity/expression_surface_closure_probe_parity.sh" 220
require_max_lines "src/self_hosted/codegen/emission/collection_element_emit_owner.pgy" 600
require_max_lines "tests/self_hosted/parity/driver_rung2_collection_mutation_graph_parity_owner.sh" 100
require_max_lines "tests/self_hosted/parity/driver_rung2_array_literal_graph_parity_owner.sh" 100
require_max_lines "src/self_hosted/compiler/abi_layout_nominal_array_owner.pgy" 200
require_max_lines "src/self_hosted/codegen/input/nominal_array_usage_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/ast_expression_surface_query_owner.pgy" 500
require_max_lines "src/self_hosted/codegen/runtime_abi/list_runtime_owner.pgy" 180
require_max_lines "src/self_hosted/codegen/runtime_abi/queue_runtime_owner.pgy" 120
require_max_lines "src/self_hosted/codegen/runtime_abi/set_runtime_owner.pgy" 120
require_max_lines "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" 320
require_max_lines "src/self_hosted/codegen/emission/option_value_emit_owner.pgy" 160
require_max_lines "src/self_hosted/codegen/emission/expr_semantic_option_value_owner.pgy" 120
require_max_lines "src/self_hosted/mir_lower/routine_instruction_view_owner.pgy" 180
require_max_lines "src/self_hosted/mir_lower/phi_fact_owner.pgy" 200
require_max_lines "src/self_hosted/mir_lower/ssa_identity_owner.pgy" 80
require_max_lines \
    "src/self_hosted/mir_lower/routine_result_definition_fact_owner.pgy" 140

require_max_lines "src/self_hosted/tools/air_graph_id_uniqueness/report_owner.pgy" 600
require_max_lines "src/self_hosted/tools/ast_read_surface_checker/report_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_linter_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_doc_link_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_comparator_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_backend_compare_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_inventory_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_size_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_air_graph_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_diagnostic_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_ast_surface_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_lexer_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_runtime_boundary_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_stable_subset_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/completeness_ledger_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/completeness_impact_owner.pgy" 600

require_max_lines "src/self_hosted/lib/json_bounded_fact_read.pgy" 465
require_max_lines "src/self_hosted/mir_lower/program_declaration_index_owner.pgy" 120
require_max_lines "src/self_hosted/mir_lower/program_enum_variant_index_owner.pgy" 240
require_max_lines "src/self_hosted/mir/declaration_callable_rows_owner.pgy" 200
require_max_lines "src/self_hosted/mir/declaration_generic_rows_owner.pgy" 80
require_max_lines "src/self_hosted/mir/declaration_rows_owner.pgy" 600
require_max_lines "src/self_hosted/mir/declaration_zone_authority_rows_owner.pgy" 180
require_max_lines "src/self_hosted/mir_lower/declaration_zone_authority_projection_owner.pgy" 180
require_max_lines "src/self_hosted/mir_lower/routine_instruction_scalar_capture_owner.pgy" 600
require_max_lines "src/self_hosted/mir_lower/abi_layout_fact_owner.pgy" 600
require_max_lines "src/self_hosted/mir_lower/routine_lower.pgy" 600
require_max_lines "src/self_hosted/mir_lower/routine_cfg_projection_owner.pgy" 120
require_max_lines "src/self_hosted/mir/runtime_call_abi_fact_owner.pgy" 400
require_max_lines "src/self_hosted/mir/runtime_value_call_abi_fact_owner.pgy" 180
require_max_lines "src/self_hosted/mir/cfg_instruction_mutation_owner.pgy" 210
require_max_lines "src/self_hosted/mir/runtime_call_abi_json_projection_owner.pgy" 310
require_max_lines "src/self_hosted/mir/expression_runtime_abi_owner.pgy" 270
require_max_lines "src/self_hosted/mir/routine_expression_runtime_abi_owner.pgy" 185

require_max_lines "tests/self_hosted/parity/codegen_reject_parity_leg.sh" 600
require_max_lines "tests/self_hosted/parity/codegen_role_parity_leg.sh" 600
require_max_lines "tests/self_hosted/parity/codegen_tool_build_leg.sh" 600
require_max_lines "src/self_hosted/tools/assignment_projection_probe/main.pgy" 599
require_max_lines "tests/self_hosted/parity/assignment_projection_probe_parity.sh" 599
require_max_lines "tests/selfhost_bootstrap_policy_corpus_smoke.sh" 200
require_responsibility_owner_max_lines "src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
require_max_lines "tests/self_hosted/parity/codegen_bootstrap.sh" 600
require_max_lines "tests/self_hosted/parity/codegen_bootstrap_compile_leg.sh" 120
# Re-armed +5 for the oracle compile's --native-pipeline declaration: the
# oracle is the native reference, and undeclared it delegated to the very
# driver being bootstrapped.
require_max_lines "tests/self_hosted/parity/driver_bootstrap.sh" 305
require_max_lines "tests/self_hosted/parity/one_mir_dual_backend_projection.sh" 300
require_max_lines "tests/self_hosted/parity/one_mir_dual_backend_case_verdict_owner.sh" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy" 335
require_max_lines \
    "src/self_hosted/compiler/direct_mir_pressure_observation_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy" 500
require_max_lines \
    "src/self_hosted/air/mir_option_match_cfg_certificate_fact_owner.pgy" 260
require_max_lines \
    "src/self_hosted/air/mir_option_match_graph_fact_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_abi_fact_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_abi_capture_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_abi_projection_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_cfg_plan_fact_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_cfg_plan_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_cfg_plan_mutation_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_option_match_cfg_emission_owner.pgy" 220
require_max_lines \
    "src/self_hosted/mir_lower/abi_layout_admission_fact_owner.pgy" 80
require_max_lines "src/self_hosted/mir_lower/match_json_fact_owner.pgy" 180
# Ordered binding spans and their validation remain in the same typed-row owner.
require_max_lines \
    "src/self_hosted/mir_lower/routine_instruction_match_fact_owner.pgy" 170
require_max_lines \
    "tests/self_hosted/parity/one_mir_option_match_projection.sh" 200
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_int_graph_fact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_literal_spine_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_int_plan_owner.pgy" 450
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_int_emission_owner.pgy" 320
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_projection.sh" 210
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_return_graph_fact_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_return_program_identity_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy" 380
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_return_emission_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_int_producer_fact_owner.pgy" 200
require_max_lines \
    "src/self_hosted/compiler/direct_mir_returned_array_foreach_program_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_returned_array_foreach_projection_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_int_abi_fact_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_captured_abi_fact_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_string_abi_fact_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_string_abi_projection_owner.pgy" 140
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_return_projection.sh" 220
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_return_mutations.py" 100
require_max_lines "src/self_hosted/mir_lower/json_fact_read.pgy" 454
require_max_lines \
    "src/self_hosted/compiler/direct_mir_routine_param_fact_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_routine_signature_fact_owner.pgy" 195
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_argument_graph_fact_owner.pgy" 225
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_argument_program_identity_owner.pgy" 245
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy" 310
require_max_lines \
    "src/self_hosted/compiler/direct_mir_array_argument_emission_owner.pgy" 265
require_max_lines \
    "src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" 110
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_argument_projection.sh" 220
require_max_lines \
    "tests/self_hosted/parity/direct_mir_mutation_ownership_contract.sh" 75
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_array_argument_mutation_ownership.pgy" 15
require_max_lines \
    "tests/self_hosted/fixtures/borrowed_ref_constructor_store_reject.pgy" 25
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_argument_mutations.py" 130
require_max_lines \
    "src/self_hosted/mir/routine_param_json_projection_owner.pgy" 60
for collection_program_owner_cap in \
    direct_mir_collection_local_context_fact_owner.pgy:100 \
    direct_mir_collection_program_route_fact_owner.pgy:100 \
    direct_mir_collection_program_identity_owner.pgy:130 \
    direct_mir_collection_program_graph_fact_owner.pgy:185 \
    direct_mir_collection_program_routine_fact_owner.pgy:190 \
    direct_mir_collection_program_producer_admission_owner.pgy:210 \
    direct_mir_collection_program_consumer_admission_owner.pgy:230 \
    direct_mir_collection_program_entrypoint_admission_owner.pgy:135 \
    direct_mir_collection_program_local_plan_owner.pgy:250 \
    direct_mir_collection_program_local_join_owner.pgy:90 \
    direct_mir_collection_program_edge_fact_owner.pgy:135 \
    direct_mir_collection_program_plan_owner.pgy:220 \
    direct_mir_collection_program_c_emission_owner.pgy:140 \
    direct_mir_collection_program_llvm_emission_owner.pgy:180 \
    direct_mir_collection_program_projection_owner.pgy:45 \
    direct_mir_multi_routine_terminal_projection_owner.pgy:35 \
    direct_mir_array_argument_legacy_route_owner.pgy:25; do
    collection_program_owner="${collection_program_owner_cap%%:*}"
    collection_program_cap="${collection_program_owner_cap##*:}"
    require_max_lines \
        "src/self_hosted/compiler/$collection_program_owner" \
        "$collection_program_cap"
done
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_param_projection.sh" 190
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_param_mutations.py" 150
while IFS='|' read -r scalar_program_owner scalar_program_cap; do
    scalar_program_cap="${scalar_program_cap%$'\r'}"
    [[ -z "$scalar_program_owner" || "$scalar_program_owner" == \#* ]] && continue
    require_max_lines "$scalar_program_owner" "$scalar_program_cap"
done < tests/self_hosted/parity/scalar_program_owner_caps.tsv
require_max_lines \
    "src/self_hosted/compiler/direct_mir_returned_array_program_route_owner.pgy" 100
require_max_lines \
    "tests/self_hosted/parity/one_mir_bool_logic_projection.sh" 170
require_max_lines \
    "tests/self_hosted/parity/one_mir_bool_logic_mutations.py" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_int_owner.sh" 210
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_int_try_let_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_int_try_let_mutations.py" 70
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_sub_equals_short_circuit_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_sub_equals_short_circuit_mutations.py" 50
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_bool_equality_short_circuit_owner.sh" 110
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_bool_equality_short_circuit_mutations.py" 60
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_multiply_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_multiply_mutations.py" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_divide_owner.sh" 125
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_divide_mutations.py" 35
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_long_remainder.pgy" 15
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_remainder_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_remainder_mutations.py" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_remainder_artifact_variants.py" 65
require_max_lines "tests/self_hosted/fixtures/direct_mir_long_addition.pgy" 10
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_addition_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_addition_mutations.py" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_addition_artifact_variants.py" 55
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_long_multiplication.pgy" 10
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_multiplication_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_multiplication_mutations.py" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_multiplication_artifact_variants.py" 55
require_max_lines "tests/self_hosted/fixtures/direct_mir_long_subtraction.pgy" 10
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_subtraction_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_subtraction_mutations.py" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_subtraction_artifact_variants.py" 55
require_max_lines "tests/self_hosted/fixtures/direct_mir_int_to_long_cast.pgy" 15
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_to_long_cast_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_to_long_cast_mutations.py" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_to_long_cast_artifact_variants.py" 60
require_max_lines "tests/self_hosted/fixtures/direct_mir_long_division.pgy" 10
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_division_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_division_mutations.py" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_division_artifact_variants.py" 55
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_long_phi_value.pgy" 15
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_phi_value_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_phi_value_mutations.py" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_phi_value_artifact_variants.py" 45
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_long_greater.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_greater_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_greater_mutations.py" 45
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_greater_artifact_variants.py" 75
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_failure_owner.pgy" 45
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_program_routine_admission_diagnostic_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_program_routine_admission_diagnostic_mutations.py" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_int_logical_record_mutations.py" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_string_owner.sh" 200
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_option_bool_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_two_int_nominal_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_option_int_field_owner.sh" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_option_int_abi_owner.pgy" 90
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_record_abi_fact_owner.pgy" 180
require_max_lines "tests/self_hosted/parity/direct_mir_physical_record_return_owner.sh" 100
require_max_lines "tests/self_hosted/parity/direct_mir_physical_record_return_mutations.py" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_owner.sh" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_int_runtime_requirement_owner.pgy" 50
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_collection_phi_value_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_collection_phi_value_mutations.py" 55
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_mutations.py" 70
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_int_value_parameter_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_int_return_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_literal_return_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_long_return_mutations.py" 50
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_array_string_value_result_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_array_string_value_result_mutations.py" 50
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_value_result_owner.sh" 260
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_value_result_mutations.py" 160
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_readonly_logical_record_single_value_result.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_single_value_result_owner.sh" 90
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_value_parameter_owner.sh" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_kind_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_comparison_expression_readiness_owner.pgy" 45
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_recursive_logical_record_phi_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_collection_fields_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_logical_record_collection_index_mutations.py" 80
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_nested_logical_record_array_bool_return_owner.sh" 200
require_max_lines \
    "tests/self_hosted/parity/direct_mir_multi_routine_mutations.py" 792
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_payload_free_enum_join_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_option_expression_kind_owner.pgy" 10
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_option_builtin_signature_owner.pgy" 65
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_option_expression_readiness_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_option_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_option_owner.pgy" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_option_return_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_logical_record_option_return_mutations.py" 60
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_zero_parameter_callable_owner.sh" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_readonly_ref_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_readonly_ref_owner.pgy" 105
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_set_string_value_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_set_string_value_parameter_owner.sh" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_set_string_expression_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_set_string_expression_owner.pgy" 80
for readonly_array_owner in \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_readonly_ref_policy_owner.pgy" \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_readonly_ref_target_owner.pgy" \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_array_readonly_ref_owner.pgy" \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_readonly_ref_owner.pgy"; do
    require_max_lines "$readonly_array_owner" 80
done
require_max_lines "tests/self_hosted/fixtures/direct_mir_array_int_readonly_ref.pgy" 50
require_max_lines "tests/self_hosted/parity/direct_mir_array_int_readonly_ref_owner.sh" 110
require_max_lines "tests/self_hosted/parity/direct_mir_array_int_readonly_ref_mutations.py" 90
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_value_parameter_storage_owner.pgy" 60
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_operand_admission_owner.pgy" 170
require_max_lines "tests/self_hosted/parity/direct_mir_cfg_identity_digest_reference.py" 60

require_max_lines "src/self_hosted/semantic/collection_mutation_policy_owner.pgy" 300
require_max_lines "src/self_hosted/semantic/ast_named_value_boundary_verdict_owner.pgy" 286
require_max_lines \
    "src/self_hosted/semantic/ast_body_type_bundle_schema_owner.pgy" 80
require_max_lines "src/self_hosted/semantic/ast_body_type_bundle_owner.pgy" 60
for executable_gate in \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_readonly_ref_owner.sh" \
    "tests/self_hosted/parity/direct_mir_scalar_entrypoint_early_return_owner.sh" \
    "tests/self_hosted/parity/direct_mir_scalar_namespace_internal_call_owner.sh"; do
    require_max_lines "$executable_gate" 160
done
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_value_result_void_owner.sh" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_fact_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_coverage_fact_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_coverage_admission_owner.pgy" 170
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_flow_owner.pgy" 330
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_query_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_plan_readiness_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_use_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_move_admission_owner.pgy" 160
require_max_lines "tests/self_hosted/parity/fixture/owned_array_string_move_sealed_flow_probe.pgy" 110
require_max_lines "tests/self_hosted/parity/fixture/owned_array_string_move_receipt_rewrite_probe.pgy" 90
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_owned_array_string_parameter_use_after_move.pgy" 20
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_alternative_owned_array_string_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_one_sided_owned_array_string_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_alternative_owned_array_string_parameter_use_after_move.pgy" 20
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_duplicate_arm_owned_array_string_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_alternative_owned_array_string_parameter_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_owned_array_string_parameter_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_non_entrypoint_owned_array_string_parameter_owner.sh" 120
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_nested_caller_owned_array_string_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_nested_caller_owned_array_string_parameter_use_after_move.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_multiple_owned_array_string_parameters_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_single_value_result_mutations.py" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_result_policy_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_result_target_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_target_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_array_value_result_owner.pgy" 160
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_array_value_result_owner.pgy" 170
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_empty_literal_admission_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_array_empty_literal_expression_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_array_empty_literal_expression_owner.pgy" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_value_result_owner.sh" 190
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_value_result_mutations.py" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_value_parameter_policy_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_index_expression_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_logical_value_expression_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_value_expression_owner.pgy" 75
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_index_owner.sh" 180
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_index_mutations.py" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_mutation_owner.sh" 200
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_mutation_mutations.py" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_value_result_pop_mutations.py" 60
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_bool_push_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_indexed_assignment_fact_owner.pgy" 220
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_indexed_assignment_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_int_value_result_indexed_assignment_mutations.py" 65
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_array_string_value_result_dynamic_indexed_assignment.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_dynamic_indexed_assignment_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_dynamic_indexed_assignment_mutations.py" 65
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_nested_logical_record_array_string_indexed_assignment.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_nested_logical_record_array_string_indexed_assignment_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_nested_logical_record_array_string_indexed_assignment_mutations.py" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_value_result_logical_record_array_int_indexed_assignment.pgy" 50
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_value_result_logical_record_array_int_indexed_assignment_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_value_result_logical_record_array_int_indexed_assignment_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_indexed_assignment_route_owner.pgy" 260
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_indexed_assignment_value_use_owner.pgy" 25
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_local_array_int_indexed_direct_call.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_local_array_int_indexed_direct_call_owner.sh" 90
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_local_array_int_indexed_direct_call_mutations.py" 55
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_value_parameter_owner.sh" 165
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_value_parameter_mutations.py" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_fact_owner.pgy" 300
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_expression_owner.pgy" 150
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_expression_readiness_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_match_condition_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_payload_free_enum_exhaustive_match_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_enum_exhaustive_match_owner.pgy" 220
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_payload_free_enum_parameter_owner.sh" 175
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_payload_free_enum_parameter_mutations.py" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_payload_enum_match_binding_owner.sh" 110
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_payload_enum_match_binding_mutations.py" 50
require_max_lines \
    "tests/self_hosted/parity/one_mir_tagged_enum_payload_projection.sh" 190
require_max_lines \
    "tests/self_hosted/parity/one_mir_tagged_enum_payload_mutations.py" 75
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_payload_enum_member_plan_readiness_owner.pgy" 115
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_leaf_operand_latest_value_readiness_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_operand_admission_owner.pgy" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_formal_literal_owner.sh" 150
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_formal_literal_mutations.py" 75
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_local_literal_owner.sh" 150
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_local_literal_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_nested_array_literal_seed_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_string_nested_literal_owner.pgy" 70
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_nested_expression_literal_owner.sh" 110
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_array_string_nested_expression_literal_mutations.py" 80
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_logical_record_array_record_input.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_record_input_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_record_input_mutations.py" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_logical_record_array_element_input.pgy" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_element_input_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_array_element_input_mutations.py" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_mixed_collection_value_result_policy_owner.pgy" 80
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_logical_record_mixed_collection_value_result.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_mixed_collection_value_result_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_mixed_collection_value_result_mutations.py" 70
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_bool_mixed_collection_value_result.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_mixed_collection_value_result_owner.sh" 190
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_mixed_collection_value_result_mutations.py" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_void_logical_record_array_int_value_result_policy_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_void_logical_record_array_int_value_result.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_array_int_value_result_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_array_int_value_result_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_inputs_value_result_policy_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_logical_record_inputs_value_result.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_inputs_value_result_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_inputs_value_result_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_array_string_value_result_policy_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_readonly_logical_record_two_array_string_value_result.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_two_array_string_value_result_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_two_array_string_value_result_mutations.py" 80
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_int_two_array_string_value_result.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_two_array_string_value_result_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_two_array_string_value_result_mutations.py" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_bool_two_array_string_two_array_int_value_result_policy_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_bool_two_array_string_two_array_int_value_result.pgy" 40
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_two_array_string_two_array_int_value_result_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_bool_two_array_string_two_array_int_value_result_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_array_bool_return_policy_owner.pgy" 50
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_readonly_logical_record_array_bool_return.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_array_bool_return_owner.sh" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_array_bool_return_mutations.py" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_logical_record_return_policy_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_direct_parameter_type_policy_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_readonly_ref_member_argument_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_readonly_ref_parameter_argument_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_owned_logical_record_return.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_owned_logical_record_return_owner.sh" 110
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_owned_logical_record_return_mutations.py" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_array_string_parameter_policy_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_string_parameter_mutation_policy_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_owned_array_string_parameter_binding_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_collection_parameter_binding_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_owned_string_parameter.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_owned_string_parameter_owner.sh" 150
require_max_lines "src/self_hosted/semantic/ast_numeric_string_allocation_call_owner.pgy" 120
require_max_lines "src/self_hosted/semantic/ast_builtin_argument_retention_call_fact_owner.pgy" 207
require_max_lines "src/self_hosted/semantic/ast_builtin_argument_retention_call_fact_validation_owner.pgy" 100
require_max_lines "src/self_hosted/semantic/ast_owned_string_local_reassignment_owner.pgy" 140
require_max_lines "src/self_hosted/semantic/ast_owned_string_result_domain_owner.pgy" 170
require_max_lines "src/self_hosted/semantic/ast_owned_string_domain_exposure_owner.pgy" 160
require_max_lines "src/self_hosted/semantic/ast_owned_string_expression_domain_owner.pgy" 200
require_max_lines "src/self_hosted/semantic/ast_owned_string_call_result_admission_owner.pgy" 30
require_max_lines "src/self_hosted/semantic/ast_owned_string_result_fact_owner.pgy" 100
require_max_lines "tests/self_hosted/fixtures/owned_string_allocator_domain_unit.pgy" 175
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_readonly_logical_record_string_array_string_value_result_policy_owner.pgy" 55
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_readonly_logical_record_string_array_string_value_result.pgy" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_string_array_string_value_result_owner.sh" 110
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_string_array_string_value_result_mutations.py" 70
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_readonly_logical_record_two_logical_record_value_result.pgy" 45
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_two_logical_record_value_result_owner.sh" 110
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_readonly_logical_record_two_logical_record_value_result_mutations.py" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_return_array_string_value_result_policy_owner.pgy" 55
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_logical_record_return_array_string_value_result.pgy" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_return_array_string_value_result_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_logical_record_return_array_string_value_result_mutations.py" 65
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_composable_logical_record_return.pgy" 60
require_max_lines \
    "tests/self_hosted/parity/direct_mir_composable_logical_record_return_owner.sh" 60
require_max_lines \
    "tests/self_hosted/parity/direct_mir_composable_logical_record_return_mutations.py" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_void_logical_record_array_string_value_result_policy_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_void_logical_record_array_string_value_result.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_array_string_value_result_owner.sh" 90
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_array_string_value_result_mutations.py" 65
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_void_logical_record_three_string_array_string_value_result.pgy" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_three_string_array_string_value_result_owner.sh" 90
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_three_string_array_string_value_result_mutations.py" 70
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_void_logical_record_four_string_array_string_value_result.pgy" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_four_string_array_string_value_result_owner.sh" 90
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_logical_record_four_string_array_string_value_result_mutations.py" 70
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_direct_scalar_string_return.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_direct_scalar_callable_owner.sh" 125
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_string_literal_mutations.py" 35
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_direct_scalar_mutations.py" 90
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_int_comparison.pgy" 20
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_comparison_owner.sh" 135
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_int_comparison_mutations.py" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_compiler_artifact_builtin_signature_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_compiler_artifact_expression_readiness_owner.pgy" 65
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_compiler_artifact_expression_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_compiler_artifact_expression_owner.pgy" 125
require_max_lines \
    "tests/self_hosted/fixtures/direct_mir_void_scalar_process_exit.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_process_exit_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_void_process_exit_mutations.py" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_admission_owner.pgy" 210
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_operand_admission_owner.pgy" 170
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_int_populated_literal_readiness_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_array_int_populated_literal_expression_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_c_array_int_populated_literal_materialization_owner.pgy" 85
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_int_populated_literal_expression_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_array_bool_nested_literal_owner.pgy" 50
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_owned_array_string_return_owner.sh" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_callable_runtime_value_parameter_policy_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_program_runtime_value_parameter_readiness_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_program_owned_string_parameter_policy_owner.pgy" 25
require_max_lines "src/self_hosted/mir/nominal_abi_layout_fact_owner.pgy" 245
require_max_lines "src/self_hosted/mir/nominal_abi_layout_identity_owner.pgy" 70
for option_abi_owner_cap in \
    option_abi_layout_contract_owner.pgy:40 \
    option_nominal_abi_layout_identity_owner.pgy:70 \
    option_nominal_abi_layout_fact_owner.pgy:160 \
    option_nominal_abi_layout_verify_owner.pgy:70 \
    option_nominal_abi_layout_json_projection_owner.pgy:70; do
    option_abi_owner="${option_abi_owner_cap%%:*}"
    option_abi_cap="${option_abi_owner_cap##*:}"
    require_max_lines "src/self_hosted/mir/$option_abi_owner" "$option_abi_cap"
done
require_max_lines \
    "src/self_hosted/mir/nominal_abi_layout_json_projection_owner.pgy" 70
require_max_lines \
    "src/self_hosted/mir/instruction_abi_receipt_fact_owner.pgy" 100
require_max_lines \
    "src/self_hosted/mir/instruction_abi_receipt_verify_owner.pgy" 70
require_max_lines \
    "src/self_hosted/mir/instruction_abi_receipt_json_projection_owner.pgy" 70
require_max_lines "src/self_hosted/mir/canonical_instruction_id_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_nominal_declaration_abi_fact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_struct_argument_graph_fact_owner.pgy" 260
require_max_lines \
    "src/self_hosted/compiler/direct_mir_struct_argument_program_admission_owner.pgy" 95
require_max_lines \
    "src/self_hosted/compiler/direct_mir_struct_argument_program_identity_owner.pgy" 230
require_max_lines \
    "src/self_hosted/compiler/direct_mir_struct_argument_plan_owner.pgy" 215
require_max_lines \
    "src/self_hosted/compiler/direct_mir_struct_argument_c_emission_owner.pgy" 165
require_max_lines \
    "src/self_hosted/compiler/direct_mir_struct_argument_llvm_emission_owner.pgy" 110
require_max_lines \
    "tests/self_hosted/parity/one_mir_struct_argument_projection.sh" 150
require_max_lines \
    "tests/self_hosted/parity/one_mir_struct_argument_mutations.py" 190
for value_flow_owner_cap in \
    direct_mir_nominal_abi_row_equality_owner.pgy:40 \
    direct_mir_struct_value_flow_program_admission_owner.pgy:95 \
    direct_mir_struct_value_flow_program_identity_owner.pgy:230 \
    direct_mir_struct_value_flow_graph_fact_owner.pgy:260 \
    direct_mir_struct_value_flow_instruction_admission_owner.pgy:200 \
    direct_mir_struct_value_flow_abi_fact_owner.pgy:180 \
    direct_mir_struct_value_flow_plan_owner.pgy:230 \
    direct_mir_struct_value_flow_c_emission_owner.pgy:180 \
    direct_mir_struct_value_flow_llvm_emission_owner.pgy:140 \
    direct_mir_struct_value_flow_projection_owner.pgy:40; do
    value_flow_owner="${value_flow_owner_cap%%:*}"
    value_flow_cap="${value_flow_owner_cap##*:}"
    require_max_lines "src/self_hosted/compiler/$value_flow_owner" "$value_flow_cap"
done
require_max_lines \
    "tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh" 200
require_max_lines \
    "tests/self_hosted/parity/one_mir_struct_value_flow_mutations.py" 240
for option_value_flow_owner_cap in \
    direct_mir_two_routine_nominal_classification_owner.pgy:150 \
    direct_mir_two_routine_nominal_program_admission_owner.pgy:80 \
    direct_mir_two_routine_nominal_projection_owner.pgy:55 \
    direct_mir_option_struct_value_flow_program_admission_owner.pgy:30 \
    direct_mir_option_struct_value_flow_program_identity_owner.pgy:230 \
    direct_mir_option_struct_value_flow_graph_shape_owner.pgy:140 \
    direct_mir_option_struct_value_flow_graph_fact_owner.pgy:180 \
    direct_mir_option_struct_value_flow_instruction_admission_owner.pgy:280 \
    direct_mir_option_struct_value_flow_abi_fact_owner.pgy:220 \
    direct_mir_option_struct_value_flow_plan_owner.pgy:180 \
    direct_mir_option_struct_value_flow_c_emission_owner.pgy:190 \
    direct_mir_option_struct_value_flow_llvm_emission_owner.pgy:220 \
    direct_mir_option_struct_value_flow_projection_owner.pgy:40; do
    option_value_flow_owner="${option_value_flow_owner_cap%%:*}"
    option_value_flow_cap="${option_value_flow_owner_cap##*:}"
    require_max_lines \
        "src/self_hosted/compiler/$option_value_flow_owner" \
        "$option_value_flow_cap"
done
require_max_lines \
    "src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" 110
require_max_lines \
    "tests/self_hosted/parity/one_mir_option_struct_value_flow_projection.sh" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_option_struct_value_flow_mutations.py" 260
require_max_lines \
    "tests/self_hosted/parity/one_mir_generic_struct_value_flow_projection.sh" 190
require_max_lines \
    "tests/self_hosted/parity/one_mir_generic_struct_value_flow_mutations.py" 270
for inferred_scalar_owner_cap in \
    direct_mir_three_routine_shape_owner.pgy:40 \
    direct_mir_three_routine_classification_owner.pgy:110 \
    direct_mir_three_routine_projection_owner.pgy:80 \
    direct_mir_mixed_lane_generic_specialization_fact_owner.pgy:180 \
    direct_mir_inferred_generic_scalar_program_admission_owner.pgy:80 \
    direct_mir_inferred_generic_scalar_graph_fact_owner.pgy:170 \
    direct_mir_inferred_generic_scalar_program_identity_owner.pgy:220 \
    direct_mir_inferred_generic_scalar_instruction_admission_owner.pgy:230 \
    direct_mir_inferred_generic_scalar_abi_fact_owner.pgy:80 \
    direct_mir_inferred_generic_scalar_plan_owner.pgy:140 \
    direct_mir_inferred_generic_scalar_c_emission_owner.pgy:100 \
    direct_mir_inferred_generic_scalar_llvm_emission_owner.pgy:90 \
    direct_mir_inferred_generic_scalar_projection_owner.pgy:40; do
    inferred_scalar_owner="${inferred_scalar_owner_cap%%:*}"
    inferred_scalar_cap="${inferred_scalar_owner_cap##*:}"
    require_max_lines \
        "src/self_hosted/compiler/$inferred_scalar_owner" "$inferred_scalar_cap"
done
require_max_lines \
    "tests/self_hosted/parity/one_mir_inferred_generic_scalar_projection.sh" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_inferred_generic_scalar_mutations.py" 280
for inferred_member_owner_cap in \
    direct_mir_two_routine_shape_owner.pgy:80 \
    direct_mir_two_routine_classification_owner.pgy:120 \
    direct_mir_inferred_generic_member_routine_array_shape_owner.pgy:50 \
    direct_mir_inferred_generic_member_program_admission_owner.pgy:70 \
    direct_mir_inferred_generic_member_specialization_fact_owner.pgy:160 \
    direct_mir_generic_member_signature_fact_owner.pgy:190 \
    direct_mir_inferred_generic_member_declaration_fact_owner.pgy:170 \
    direct_mir_inferred_generic_member_host_kind_fact_owner.pgy:60 \
    direct_mir_inferred_generic_member_graph_fact_owner.pgy:180 \
    direct_mir_inferred_generic_member_program_identity_owner.pgy:190 \
    direct_mir_inferred_generic_member_instruction_admission_owner.pgy:220 \
    direct_mir_inferred_generic_member_representation_fact_owner.pgy:100 \
    direct_mir_inferred_generic_member_plan_owner.pgy:120 \
    direct_mir_member_receiver_target_projection_owner.pgy:90 \
    direct_mir_inferred_generic_member_c_emission_owner.pgy:100 \
    direct_mir_inferred_generic_member_llvm_emission_owner.pgy:100 \
    direct_mir_inferred_generic_member_projection_owner.pgy:40; do
    inferred_member_owner="${inferred_member_owner_cap%%:*}"
    inferred_member_cap="${inferred_member_owner_cap##*:}"
    require_max_lines \
        "src/self_hosted/compiler/$inferred_member_owner" "$inferred_member_cap"
done
require_max_lines "tests/self_hosted/parity/generic_member_parameter_identity_cases.py" 80
require_max_lines \
    "tests/self_hosted/parity/one_mir_inferred_generic_member_projection.sh" 200
require_max_lines \
    "tests/self_hosted/parity/one_mir_inferred_generic_member_mutations.py" 360
require_max_lines \
    "tests/self_hosted/parity/one_mir_passive_nominal_literal_projection.sh" 200
require_max_lines \
    "tests/self_hosted/parity/one_mir_mutable_nominal_identity_projection.sh" 200
require_max_lines \
    "tests/self_hosted/parity/one_mir_nominal_literal_mutations.py" 260
require_max_lines \
    "src/self_hosted/compiler/direct_mir_exact_json_array_cardinality_owner.pgy" 90
shared_nominal_total=0
while IFS='|' read -r owner cap; do
    require_max_lines \
        "src/self_hosted/compiler/$owner" "$cap"
    source_size_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "src/self_hosted/compiler/$owner")" || fail "source-size measurement failed: $owner"
    shared_nominal_total=$((shared_nominal_total + source_size_lines))
done <<'SHARED_NOMINAL_OWNER_CAPS'
direct_mir_nominal_literal_declaration_fact_owner.pgy|180
direct_mir_nominal_literal_graph_fact_owner.pgy|150
direct_mir_nominal_literal_program_identity_owner.pgy|200
direct_mir_nominal_literal_route_fact_owner.pgy|110
direct_mir_nominal_literal_instruction_envelope_owner.pgy|100
direct_mir_nominal_literal_abi_absence_owner.pgy|120
direct_mir_nominal_literal_program_admission_owner.pgy|220
direct_mir_nominal_literal_projection_owner.pgy|110
SHARED_NOMINAL_OWNER_CAPS
[[ "$shared_nominal_total" -le 900 ]] || \
    fail "shared nominal literal owner cap exceeded: $shared_nominal_total/900"
mutable_identity_total=0
while IFS='|' read -r owner cap; do
    require_max_lines "src/self_hosted/compiler/$owner" "$cap"
    source_size_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "src/self_hosted/compiler/$owner")" || fail "source-size measurement failed: $owner"
    mutable_identity_total=$((mutable_identity_total + source_size_lines))
done <<'MUTABLE_IDENTITY_OWNER_CAPS'
direct_mir_mutable_nominal_identity_plan_owner.pgy|180
direct_mir_mutable_nominal_identity_target_projection_owner.pgy|100
direct_mir_mutable_nominal_identity_c_emission_owner.pgy|90
direct_mir_mutable_nominal_identity_llvm_emission_owner.pgy|90
MUTABLE_IDENTITY_OWNER_CAPS
[[ "$mutable_identity_total" -le 384 ]] || \
    fail "mutable nominal identity owner cap exceeded: $mutable_identity_total/384"
for constructed_member_owner in \
    direct_mir_three_routine_classification_owner.pgy \
    direct_mir_three_routine_classification_fact_owner.pgy \
    direct_mir_three_routine_constructed_member_shape_owner.pgy \
    direct_mir_three_routine_projection_owner.pgy \
    direct_mir_constructed_generic_member_program_admission_owner.pgy \
    direct_mir_constructed_generic_member_declaration_fact_owner.pgy \
    direct_mir_constructed_generic_member_signature_fact_owner.pgy \
    direct_mir_constructed_generic_member_specialization_fact_owner.pgy \
    direct_mir_constructed_generic_member_substitution_owner.pgy \
    direct_mir_constructed_generic_member_method_graph_fact_owner.pgy \
    direct_mir_constructed_generic_member_main_graph_fact_owner.pgy \
    direct_mir_constructed_generic_member_program_identity_owner.pgy \
    direct_mir_constructed_generic_member_method_instruction_owner.pgy \
    direct_mir_constructed_generic_member_main_instruction_owner.pgy \
    direct_mir_constructed_generic_member_option_abi_admission_owner.pgy \
    direct_mir_constructed_generic_member_representation_owner.pgy \
    direct_mir_constructed_generic_member_plan_owner.pgy \
    direct_mir_constructed_generic_member_c_emission_owner.pgy \
    direct_mir_constructed_generic_member_llvm_emission_owner.pgy \
    direct_mir_constructed_generic_member_projection_owner.pgy; do
    require_max_lines \
        "src/self_hosted/compiler/$constructed_member_owner" 220
done
require_max_lines \
    "tests/self_hosted/parity/one_mir_constructed_generic_member_projection.sh" 170
require_max_lines \
    "tests/self_hosted/parity/one_mir_constructed_generic_member_mutations.py" 350
for constructed_array_member_owner in \
    direct_mir_constructed_member_specialization_pair_owner.pgy \
    direct_mir_constructed_member_variant_owner.pgy \
    direct_mir_constructed_array_member_specialization_fact_owner.pgy \
    direct_mir_constructed_array_member_substitution_owner.pgy \
    direct_mir_constructed_array_member_signature_fact_owner.pgy \
    direct_mir_constructed_array_member_method_graph_fact_owner.pgy \
    direct_mir_constructed_array_member_main_graph_fact_owner.pgy \
    direct_mir_constructed_array_member_program_identity_owner.pgy \
    direct_mir_constructed_array_member_method_instruction_owner.pgy \
    direct_mir_constructed_array_member_main_instruction_owner.pgy \
    direct_mir_constructed_array_member_abi_admission_owner.pgy \
    direct_mir_array_storage_layout_contract_owner.pgy \
    direct_mir_array_storage_abi_projection_owner.pgy \
    direct_mir_array_storage_symbol_owner.pgy \
    direct_mir_array_storage_c_assertion_owner.pgy \
    direct_mir_aggregate_value_flow_fact_owner.pgy \
    direct_mir_aggregate_value_flow_target_projection_owner.pgy \
    direct_mir_constructed_array_member_representation_owner.pgy \
    direct_mir_constructed_array_member_plan_owner.pgy \
    direct_mir_constructed_array_member_c_emission_owner.pgy \
    direct_mir_constructed_array_member_llvm_emission_owner.pgy \
    direct_mir_constructed_array_member_projection_owner.pgy; do
    require_max_lines \
        "src/self_hosted/compiler/$constructed_array_member_owner" 220
done
require_max_lines \
    "src/self_hosted/mir/routine_tracked_statement_owner.pgy" 220
require_max_lines \
    "tests/self_hosted/parity/one_mir_constructed_array_member_projection.sh" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_constructed_array_member_mutations.py" 300
for constructed_record_array_member_owner in \
    direct_mir_three_routine_mixed_constructed_member_shape_owner.pgy \
    direct_mir_constructed_record_array_member_program_admission_owner.pgy \
    direct_mir_constructed_record_array_member_specialization_owner.pgy \
    direct_mir_constructed_record_array_member_main_graph_owner.pgy \
    direct_mir_constructed_record_array_member_source_identity_owner.pgy \
    direct_mir_constructed_record_array_member_identity_owner.pgy \
    direct_mir_constructed_record_array_member_instruction_envelope_owner.pgy \
    direct_mir_constructed_record_array_member_main_admission_owner.pgy \
    direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy \
    direct_mir_constructed_record_array_member_point_abi_owner.pgy \
    direct_mir_constructed_record_array_member_representation_owner.pgy \
    direct_mir_constructed_record_array_member_plan_join_owner.pgy \
    direct_mir_closed_module_call_abi_owner.pgy \
    direct_mir_constructed_record_array_member_plan_owner.pgy \
    direct_mir_array_storage_layout_contract_owner.pgy \
    direct_mir_array_storage_abi_projection_owner.pgy \
    direct_mir_array_storage_symbol_owner.pgy \
    direct_mir_array_storage_c_assertion_owner.pgy \
    direct_mir_constructed_record_array_member_abi_projection_owner.pgy \
    direct_mir_constructed_record_array_member_c_emission_owner.pgy \
    direct_mir_constructed_record_array_member_llvm_emission_owner.pgy \
    direct_mir_constructed_record_array_member_projection_owner.pgy; do
    require_max_lines \
        "src/self_hosted/compiler/$constructed_record_array_member_owner" 220
done
require_max_lines \
    "tests/self_hosted/parity/one_mir_constructed_record_array_member_projection.sh" 181
require_max_lines \
    "tests/self_hosted/parity/one_mir_constructed_record_array_member_mutations.py" 360
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_plan_fact_owner.pgy" 260
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_plan_value_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_plan_mutation_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_shape_fact_owner.pgy" 580
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_entry_fact_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_nested_cfg_shape_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_nested_cfg_emission_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_llvm_text_format_owner.pgy" 60
require_max_lines "src/self_hosted/air/mir_cfg_certificate_owner.pgy" 520
require_max_lines \
    "src/self_hosted/air/mir_cfg_certificate_readiness_owner.pgy" 200
require_max_lines \
    "src/self_hosted/air/mir_cfg_certificate_fact_owner.pgy" 230
require_max_lines \
    "src/self_hosted/air/mir_cfg_certificate_value_owner.pgy" 190
require_max_lines \
    "src/self_hosted/air/mir_cfg_certificate_mutation_owner.pgy" 180
require_max_lines "src/self_hosted/air/mir_cfg_identity_owner.pgy" 120
require_max_lines "tests/self_hosted/fixtures/direct_mir_cfg_identity_digest.pgy" 30
require_max_lines "tests/self_hosted/parity/direct_mir_cfg_identity_digest_owner.sh" 90
require_max_lines \
    "src/self_hosted/air/mir_nested_cfg_certificate_fact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/air/mir_loop_cfg_certificate_fact_owner.pgy" 360
require_max_lines \
    "src/self_hosted/compiler/direct_mir_loop_cfg_shape_owner.pgy" 280
require_max_lines \
    "src/self_hosted/compiler/direct_mir_loop_cfg_plan_fact_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_loop_cfg_emission_owner.pgy" 130
require_max_lines \
    "src/self_hosted/air/mir_range_cfg_certificate_fact_owner.pgy" 400
require_max_lines \
    "src/self_hosted/compiler/direct_mir_cfg_log_shape_owner.pgy" 100
require_max_lines \
    "src/self_hosted/air/mir_break_cfg_certificate_fact_owner.pgy" 650
require_max_lines \
    "tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh" 300
require_max_lines "tests/self_hosted/parity/one_mir_cfg_nested_case.sh" 80
require_max_lines "tests/self_hosted/parity/one_mir_cfg_loop_case.sh" 150
require_max_lines "tests/self_hosted/parity/one_mir_cfg_range_case.sh" 180
require_max_lines "tests/self_hosted/parity/one_mir_cfg_break_case.sh" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_graph_admission_owner.pgy" 600
require_max_lines \
    "src/self_hosted/compiler/direct_mir_compile_time_declaration_erasure_owner.pgy" 280
require_max_lines \
    "src/self_hosted/compiler/direct_mir_literal_log_plan_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_literal_log_emission_owner.pgy" 100
require_max_lines \
    "tests/self_hosted/parity/one_mir_compile_time_declaration_literal_projection.sh" 130
require_max_lines \
    "tests/self_hosted/parity/one_mir_compile_time_declaration_literal_mutations.py" 110
literal_log_family_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" --total \
    src/self_hosted/compiler/direct_mir_compile_time_declaration_erasure_owner.pgy \
    src/self_hosted/compiler/direct_mir_literal_log_plan_owner.pgy \
    src/self_hosted/compiler/direct_mir_literal_log_emission_owner.pgy)" || fail "literal Log family measurement failed"
[[ "$literal_log_family_lines" -le 560 ]] || \
    fail "compile-time declaration/literal Log family exceeds 560 LOC ($literal_log_family_lines)"
require_max_lines \
    "src/self_hosted/compiler/direct_mir_enum_value_match_route_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_payload_free_enum_abi_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_enum_value_match_plan_fact_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy" 420
require_max_lines \
    "src/self_hosted/compiler/direct_mir_enum_value_match_plan_mutation_owner.pgy" 70
require_max_lines \
    "src/self_hosted/air/mir_identity_match_cfg_certificate_fact_owner.pgy" 220
require_max_lines \
    "src/self_hosted/air/mir_identity_match_cfg_certificate_mutation_owner.pgy" 50
require_max_lines \
    "tests/self_hosted/parity/one_mir_enum_value_match_projection.sh" 160
require_max_lines \
    "tests/self_hosted/parity/one_mir_enum_value_match_mutations.py" 160
enum_value_match_family_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" --total \
    src/self_hosted/compiler/direct_mir_enum_value_match_route_owner.pgy \
    src/self_hosted/compiler/direct_mir_payload_free_enum_abi_owner.pgy \
    src/self_hosted/compiler/direct_mir_enum_value_match_plan_fact_owner.pgy \
    src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy \
    src/self_hosted/compiler/direct_mir_enum_value_match_plan_mutation_owner.pgy)" || fail "enum value-match family measurement failed"
[[ "$enum_value_match_family_lines" -le 770 ]] || \
    fail "enum value-match family exceeds 770 LOC ($enum_value_match_family_lines)"
require_max_lines \
    "src/self_hosted/semantic/role_operator_vocabulary_owner.pgy" 220
require_max_lines \
    "src/self_hosted/semantic/role_operator_resolution_owner.pgy" 340
require_max_lines \
    "src/self_hosted/compiler/direct_mir_role_operator_declaration_fact_owner.pgy" 310
require_max_lines \
    "src/self_hosted/compiler/direct_mir_role_operator_plan_owner.pgy" 560
require_max_lines \
    "src/self_hosted/compiler/direct_mir_role_operator_abi_projection_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_role_operator_emission_owner.pgy" 150
require_max_lines \
    "tests/self_hosted/parity/one_mir_role_operator_projection.sh" 220
require_max_lines \
    "tests/self_hosted/parity/one_mir_role_operator_mutations.py" 220
role_operator_family_lines=$((
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_operator_declaration_fact_owner.pgy) +
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_operator_plan_owner.pgy) +
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_operator_abi_projection_owner.pgy) +
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_operator_emission_owner.pgy)
))
[[ "$role_operator_family_lines" -le 1200 ]] || \
    fail "role operator direct family exceeds 1200 LOC ($role_operator_family_lines)"
require_responsibility_owner_max_lines \
    "src/self_hosted/compiler/direct_mir_role_override_program_identity_owner.pgy"
for role_override_owner_cap in \
    direct_mir_role_override_plan_owner.pgy:120 \
    direct_mir_role_override_target_projection_owner.pgy:110 \
    direct_mir_role_override_emission_owner.pgy:220; do
    role_override_owner="${role_override_owner_cap%%:*}"
    role_override_cap="${role_override_owner_cap##*:}"
    require_max_lines \
        "src/self_hosted/compiler/$role_override_owner" "$role_override_cap"
done
require_max_lines \
    "tests/self_hosted/parity/role_override_mir_replacement.sh" 380
require_max_lines \
    "tests/self_hosted/parity/role_override_mir_mutations.py" 100
role_override_family_lines=$((
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_override_program_identity_owner.pgy) +
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_override_plan_owner.pgy) +
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_override_target_projection_owner.pgy) +
    $(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_role_override_emission_owner.pgy)
))
[[ "$role_override_family_lines" -le 1250 ]] || \
    fail "role override direct family exceeds 1250 LOC ($role_override_family_lines)"
# Inspect both build-mode graphs independently of an admitted CI environment,
# then the admitted graph. Compiler generation remains forbidden in this gate.
check_replacement_frontier_build_graph
require_max_lines \
    "src/self_hosted/mir_lower/routine_instruction_use_fact_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/driver_rung2_execution_owner.pgy" 190
require_max_lines \
    "src/self_hosted/compiler/driver_rung2_execution_protocol_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/driver_mir_c_protocol_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/driver_mir_c_payload_execution_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/driver_source_mir_execution_owner.pgy" 260
require_max_lines \
    "src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy" 260
require_max_lines \
    "src/self_hosted/compiler/driver_source_c_protocol_owner.pgy" 230
require_max_lines \
    "src/self_hosted/compiler/driver_source_c_execution_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/driver_source_c_request_owner.pgy" 80
require_max_lines \
    "tests/self_hosted/parity/driver_source_c_execution_action_gate.sh" 130
require_max_lines \
    "tests/self_hosted/parity/driver_source_c_stdout_execution_action_gate.sh" 115
require_max_lines \
    "tests/self_hosted/parity/driver_mir_c_stdout_execution_action_gate.sh" 175
require_max_lines \
    "tests/self_hosted/parity/public_device_slot_machine_manifest_installed_self_host_owner.sh" 140
require_max_lines \
    "src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy" 100
require_max_lines \
    "src/self_hosted/mir_lower/phi_predecessor_binding_fact_owner.pgy" 180
require_max_lines \
    "src/self_hosted/mir_lower/routine_definition_dominance_fact_owner.pgy" 70
require_max_lines \
    "src/self_hosted/mir_lower/latest_local_value_fact_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_type_family_owner.pgy" 65
# Exact match-origin/type identity replaces the shorter routine-wide name read.
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_value_type_owner.pgy" 165
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy" 200
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_typed_readiness_owner.pgy" 190
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_identity_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_operand_shape_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy" 243
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_phi_operation_admission_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_mutation_owner.pgy" 15
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_expression_base_owner.pgy" 85
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_expression_owner.pgy" 300
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_leaf_operand_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_direct_local_operand_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy" 300
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_wire_local_ref_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_assignment_target_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy" 450
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_branch_admission_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_control_transfer_admission_owner.pgy" 40
# Re-armed at the file's true size. This owner is a single function, so a few
# lines of growth is not a decomposition failure and there is no seam to split
# on. The 600-line owner cap is the SRP standard and is not relaxed here.
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy" 113
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_fact_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_identity_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_value_readiness_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_operation_readiness_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_observation_readiness_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_readiness_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_binding_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_expression_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_typed_readiness_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_array_int_absence_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_index_assignment_route_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_index_assignment_graph_owner.pgy" 160
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_index_assignment_admission_owner.pgy" 460
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_index_assignment_source_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_selection_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_c_operation_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_storage_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_operation_owner.pgy" 140
# Re-armed at the file's true size. This owner is a single function, so a few
# lines of growth is not a decomposition failure and there is no seam to split
# on. The 600-line owner cap is the SRP standard and is not relaxed here.
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy" 73
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_expression_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_instruction_graph_owner.pgy" 15
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_instruction_position_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_operation_position_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_while_induction_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_guard_dominance_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_sealed_guard_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_route_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_operation_plan_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_loop_flow_admission_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_iteration_owner.pgy" 280
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_bound_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_fact_owner.pgy" 200
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_set_owner.pgy" 320
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_append_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_admission_owner.pgy" 280
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_owner.pgy" 160
require_max_lines \
    "src/self_hosted/compiler/direct_mir_returned_array_foreach_call_abi_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_definition_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_string_collection_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_string_collection_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_length_fact_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_length_log_graph_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_collection_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_collection_admission_owner.pgy" 65
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_fact_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_fact_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_mutating_fact_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_fact_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_fact_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_storage_state_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_source_identity_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_graph_owner.pgy" 75
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_graph_shape_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_admission_owner.pgy" 280
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_dynamic_push_admission_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_graph_owner.pgy" 95
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_admission_owner.pgy" 170
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_route_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_transform_route_fact_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_mode_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_graph_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_source_admission_owner.pgy" 175
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_phi_topology_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_admission_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_admission_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_binding_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_binding_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_operation_binding_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_operation_binding_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_nonmutating_operation_binding_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_binding_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_operation_seed_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_mutating_operation_binding_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_lookup_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_mutating_lookup_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_lookup_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_identity_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_identity_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_variant_identity_owner.pgy" 15
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pre_reverse_identity_owner.pgy" 15
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_identity_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_identity_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_expression_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_mutating_expression_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_expression_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_expression_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_condition_expression_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_consumer_readiness_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_readiness_owner.pgy" 155
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_operation_absence_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_absence_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_absence_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_minimum_plan_shape_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_readiness_owner.pgy" 65
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_log_operation_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_condition_role_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_operation_readiness_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_phi_readiness_owner.pgy" 55
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_readiness_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_mutation_readiness_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_c_emission_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_llvm_emission_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_c_program_operation_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_program_operation_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_nonmutating_c_operation_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_c_operation_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_c_storage_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_nonmutating_llvm_operation_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_llvm_operation_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_reverse_llvm_storage_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_llvm_storage_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_pop_route_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_fact_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_foreach_view_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_foreach_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_admission_owner.pgy" 160
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_binding_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_expression_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_lookup_owner.pgy" 35
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_source_lookup_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_identity_owner.pgy" 20
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_readiness_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_c_operation_owner.pgy" 50
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_llvm_operation_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_llvm_storage_owner.pgy" 55
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_program_fact_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_program_readiness_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_pop_typed_readiness_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_graph_receipt_identity_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_llvm_condition_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_c_collection_mutation_emission_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_collection_mutation_emission_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_collection_operation_shape_owner.pgy" 15
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_hash_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_local_inventory_owner.pgy" 130
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_value_parameter_rebind_owner.sh" 100
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_value_parameter_rebind_mutations.py" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_fact_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_lookup_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_identity_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_readiness_owner.pgy" 200
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_access_readiness_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_append_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_binding_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_dominance_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_capacity_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_mutation_length_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_index_readiness_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_length_log_graph_owner.pgy" 25
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_push_dominance_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_entry_execution_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_push_graph_readiness_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_source_fact_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_statement_source_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_index_safety_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_shape_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy" 420
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_expression_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_condition_expression_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_readiness_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_mutation_graph_readiness_owner.pgy" 95
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_admission_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_element_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_local_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_graph_readiness_owner.pgy" 100
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_set_owner.pgy" 300
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_identity_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_local_ref_identity_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_iteration_local_owner.pgy" 150
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_wire_range_scope_admission_owner.pgy" 150
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_graph_readiness_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_transfer_admission_owner.pgy" 90
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_block_effect_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_fact_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_range_emission_owner.pgy" 110
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_c_operand_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_c_emission_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_c_operation_emission_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_c_collection_operation_emission_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_c_collection_operation_fact_owner.pgy" 10
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_c_emission_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_c_storage_owner.pgy" 53
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_c_emission_owner.pgy" 45
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_c_operation_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_c_materialization_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_c_emission_owner.pgy" 180
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy" 240
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_c_materialization_owner.pgy" 65
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_emission_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy" 80
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_mutation_emission_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_pop_c_operation_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_emission_owner.pgy" 260
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_operation_emission_owner.pgy" 190
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_collection_operation_emission_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_collection_operation_fact_owner.pgy" 10
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_storage_owner.pgy" 73
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_initialization_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_emission_owner.pgy" 130
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_llvm_emission_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_static_llvm_operation_owner.pgy" 30
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_llvm_materialization_owner.pgy" 160
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_array_string_llvm_value_owner.pgy" 60
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy" 70
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_pop_llvm_operation_owner.pgy" 40
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_llvm_emission_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy" 220
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_local_emission_owner.pgy" 140
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_local_emission_owner.pgy" 215
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_terminator_emission_owner.pgy" 120
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_operand_owner.pgy" 40
# Re-armed at the file's true size. This owner is a single function, so a few
# lines of growth is not a decomposition failure and there is no seam to split
# on. The 600-line owner cap is the SRP standard and is not relaxed here.
require_max_lines \
    "src/self_hosted/compiler/direct_mir_scalar_cfg_projection_owner.pgy" 60
require_max_lines \
    "tests/self_hosted/parity/one_mir_scalar_cfg_graph_projection.sh" 160
require_max_lines \
    "tests/self_hosted/parity/one_mir_scalar_cfg_break_exit_projection.sh" 160
require_max_lines \
    "tests/self_hosted/parity/one_mir_scalar_cfg_for_break_exit_projection.sh" 160
require_max_lines \
    "tests/self_hosted/parity/one_mir_scalar_cfg_continue_backedge_projection.sh" 160
require_max_lines \
    "tests/self_hosted/parity/direct_mir_scalar_program_control_transfer_owner.sh" 140
require_max_lines \
    "tests/self_hosted/parity/one_mir_scalar_cfg_foreach_array_int_projection.sh" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_returned_array_foreach_projection.sh" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_mixed_collection_foreach_projection.sh" 100
require_max_lines \
    "tests/self_hosted/parity/one_mir_mixed_collection_foreach_mutations.py" 120
require_max_lines \
    "tests/self_hosted/parity/one_mir_indexed_string_array_projection.sh" 150
require_max_lines \
    "tests/self_hosted/parity/one_mir_indexed_string_array_mutations.py" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_array_mutation_projection.sh" 130
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_array_mutations.py" 180
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_array_push_projection.sh" 140
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_array_push_artifact_contract.sh" 60
require_max_lines \
    "tests/self_hosted/parity/one_mir_string_array_push_mutations.py" 175
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_loop_push_projection.sh" 140
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_loop_push_artifact_contract.sh" 60
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_loop_push_mutations.py" 175
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_sum_projection.sh" 140
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_sum_artifact_contract.sh" 60
require_max_lines \
    "tests/self_hosted/parity/one_mir_array_int_sum_mutations.py" 160
require_max_lines \
    "tests/self_hosted/parity/one_mir_iteration_binding_scope_owner.sh" 160
require_max_lines \
    "tests/self_hosted/parity/one_mir_nested_iteration_binding_scope_owner.sh" 120
require_max_lines \
    "tests/self_hosted/parity/one_mir_nested_iteration_continue_scope_owner.sh" 160
require_max_lines "src/self_hosted/mir/local_ref_fact_owner.pgy" 180
require_max_lines "src/self_hosted/mir/local_ref_validation_owner.pgy" 70
require_max_lines "src/self_hosted/mir/cfg_view_owner.pgy" 20
require_max_lines "src/self_hosted/mir/local_ref_identity_owner.pgy" 55
require_max_lines "src/self_hosted/mir/local_ref_json_projection_owner.pgy" 115
require_max_lines "src/self_hosted/mir/routine_local_ref_attachment_owner.pgy" 40
require_max_lines \
    "src/self_hosted/mir/routine_source_statement_attachment_owner.pgy" 60
require_max_lines \
    "src/self_hosted/mir_lower/routine_fact_index_schema_owner.pgy" 600
require_max_lines "tests/self_hosted/parity/driver_rung2_body_parity.sh" 310
require_max_lines "src/self_hosted/lsp/diagnostics_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/document_store_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/feature_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/request_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/response_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/session_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/session_state_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/hover_content_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/hover_content_projection_owner.pgy" 300
require_max_lines "src/self_hosted/lsp/live_session_owner.pgy" 500
require_max_lines "src/self_hosted/lsp/document_feature_index_owner.pgy" 500
require_max_lines "src/self_hosted/lsp/squiggle_owner.pgy" 600
require_max_lines "src/self_hosted/lsp/transport_owner.pgy" 600
require_max_lines "src/self_hosted/fuzz/backend_parity_generator/manifest_owner.pgy" 600

require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_void_readonly_string_target_owner.pgy" 40
require_max_lines "src/self_hosted/codegen/emission/expr_semantic_call_argument_owner.pgy" 150
require_max_lines "tests/self_hosted/parity/region_user_callee_installed_self_host_owner.sh" 220
require_max_lines "tests/self_hosted/parity/region_user_callee_mutations.py" 70

require_max_lines "src/self_hosted/codegen/emission/expression_c_text_epoch_owner.pgy" 40
require_max_lines "src/self_hosted/mir/intent_execution_canonical_instruction_owner.pgy" 75
require_max_lines "src/self_hosted/semantic/ast_generic_specialization_constraint_owner.pgy" 75
require_max_lines "src/self_hosted/compiler/direct_mir_intent_parameter_binding_admission_owner.pgy" 85
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_logical_record_physical_target_owner.pgy" 60
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_expression_owner.pgy" 75
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_scalar_value_result_storage_owner.pgy" 35
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_payload_enum_exhaustive_condition_owner.pgy" 125
require_max_lines "src/self_hosted/semantic/oracle_json_diagnostic_owner.pgy" 55
require_max_lines "src/self_hosted/compiler/direct_mir_nominal_abi_row_identity_owner.pgy" 35
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_string_core_expression_kind_owner.pgy" 20
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_call_callee_syntax_owner.pgy" 90
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_program_array_int_abi_projection_contract_owner.pgy" 20

# Collection ownership transition receipts are one reached semantic -> MIR ->
# direct-backend seam. Pin every new owner and keep the large verdict split by
# responsibility instead of raising its cap.
for collection_owner_cap in \
    src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy:600 \
    src/self_hosted/semantic/ast_collection_ownership_scan_owner.pgy:410 \
    src/self_hosted/semantic/ast_collection_indexed_string_borrow_owner.pgy:240 \
    src/self_hosted/semantic/ast_collection_indexed_string_borrow_source_policy_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_indexed_string_borrow_storage_lifetime_owner.pgy:20 \
    src/self_hosted/semantic/ast_collection_ownership_state_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_ownership_verdict_carrier_owner.pgy:90 \
    src/self_hosted/semantic/ast_collection_ownership_receipt_owner.pgy:120 \
    src/self_hosted/semantic/ast_collection_ownership_identity_owner.pgy:242 \
    src/self_hosted/semantic/ast_collection_formal_effect_identity_owner.pgy:165 \
    src/self_hosted/semantic/ast_collection_formal_execution_context_owner.pgy:150 \
    src/self_hosted/semantic/ast_collection_formal_use_owner.pgy:465 \
    src/self_hosted/semantic/ast_string_formal_text_containment_owner.pgy:180 \
    src/self_hosted/semantic/ast_collection_formal_effect_fact_owner.pgy:40 \
    src/self_hosted/semantic/ast_collection_formal_statement_effect_owner.pgy:180 \
    src/self_hosted/semantic/ast_collection_formal_effect_admission_owner.pgy:40 \
    src/self_hosted/semantic/ast_collection_formal_effect_fixed_point_owner.pgy:60 \
    src/self_hosted/semantic/ast_collection_formal_effect_graph_owner.pgy:142 \
    src/self_hosted/semantic/ast_collection_formal_effect_closure_owner.pgy:160 \
    src/self_hosted/semantic/ast_collection_formal_effect_readiness_owner.pgy:143 \
    src/self_hosted/semantic/ast_collection_formal_effect_owner.pgy:40 \
    src/self_hosted/semantic/ast_collection_call_effect_owner.pgy:200 \
    src/self_hosted/semantic/ast_collection_formal_descriptor_retention_owner.pgy:170 \
    src/self_hosted/semantic/ast_collection_descriptor_retention_entry_owner.pgy:45 \
    src/self_hosted/semantic/ast_collection_terminal_storage_effect_owner.pgy:140 \
    src/self_hosted/semantic/ast_collection_constructor_storage_escape_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_constructor_field_input_owner.pgy:200 \
    src/self_hosted/semantic/ast_collection_definition_effect_closure_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_argument_permission_effect_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_formal_storage_permission_owner.pgy:104 \
    src/self_hosted/semantic/ast_collection_call_effect_fact_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_definition_fact_owner.pgy:40 \
    src/self_hosted/semantic/ast_collection_definition_query_owner.pgy:60 \
    src/self_hosted/semantic/ast_collection_assignment_definition_owner.pgy:170 \
    src/self_hosted/semantic/ast_collection_definition_storage_authority_owner.pgy:91 \
    src/self_hosted/semantic/ast_collection_definition_storage_producer_owner.pgy:60 \
    src/self_hosted/semantic/ast_collection_definition_transition_owner.pgy:84 \
    src/self_hosted/semantic/ast_collection_call_argument_verdict_owner.pgy:115 \
    src/self_hosted/semantic/ast_collection_call_argument_effect_verdict_owner.pgy:222 \
    src/self_hosted/semantic/ast_collection_ownership_statement_transition_owner.pgy:145 \
    src/self_hosted/semantic/ast_collection_call_retirement_owner.pgy:96 \
    src/self_hosted/semantic/ast_collection_repeated_local_generation_owner.pgy:40 \
    src/self_hosted/semantic/ast_expression_graph_call_argument_edge_owner.pgy:60 \
    src/self_hosted/semantic/ast_collection_ownership_assignment_alias_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_ownership_binding_move_use_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_ownership_member_move_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_ownership_member_root_identity_owner.pgy:90 \
    src/self_hosted/semantic/ast_collection_ownership_member_transition_owner.pgy:180 \
    src/self_hosted/semantic/ast_collection_ownership_member_result_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_ownership_member_restoration_owner.pgy:30 \
    src/self_hosted/semantic/ast_collection_member_capture_obligation_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_member_capture_source_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_member_capture_provenance_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_member_capture_site_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_member_read_permission_owner.pgy:120 \
    src/self_hosted/semantic/ast_collection_member_read_local_root_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_member_place_owner.pgy:64 \
    src/self_hosted/semantic/ast_collection_member_place_use_owner.pgy:200 \
    src/self_hosted/semantic/ast_string_formal_borrow_owner.pgy:235 \
    src/self_hosted/semantic/ast_collection_aggregate_release_element_borrow_owner.pgy:32 \
    src/self_hosted/semantic/ast_collection_ownership_argument_transfer_owner.pgy:180 \
    src/self_hosted/semantic/ast_collection_argument_event_order_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_lifetime_event_order_owner.pgy:80 \
    src/self_hosted/semantic/ast_collection_builtin_transition_owner.pgy:300 \
    src/self_hosted/semantic/ast_collection_owned_parameter_identity_owner.pgy:100 \
    src/self_hosted/semantic/ast_collection_owned_argument_admission_owner.pgy:110 \
    src/self_hosted/semantic/ast_collection_owned_element_parameter_requirement_owner.pgy:160 \
    src/self_hosted/semantic/ast_collection_aggregate_field_entry_requirement_owner.pgy:162 \
    src/self_hosted/semantic/ast_collection_aggregate_release_plan_schema_owner.pgy:316 \
    src/self_hosted/semantic/ast_collection_aggregate_value_exclusivity_owner.pgy:262 \
    src/self_hosted/semantic/ast_collection_aggregate_value_lineage_owner.pgy:590 \
    src/self_hosted/semantic/ast_collection_aggregate_sink_consumption_owner.pgy:385 \
    src/self_hosted/semantic/ast_collection_element_escape_entry_owner.pgy:335 \
    src/self_hosted/semantic/ast_collection_exclusive_value_owner.pgy:565 \
    src/self_hosted/semantic/ast_collection_exclusive_value_class_owner.pgy:280 \
    src/self_hosted/semantic/ast_collection_exclusive_value_growth_owner.pgy:235 \
    src/self_hosted/semantic/ast_collection_exclusive_value_copy_landing_owner.pgy:145 \
    src/self_hosted/semantic/ast_collection_exclusive_value_dead_definition_owner.pgy:135 \
    src/self_hosted/semantic/ast_collection_exclusive_value_consumption_owner.pgy:440 \
    src/self_hosted/semantic/ast_collection_exclusive_value_field_read_owner.pgy:370 \
    src/self_hosted/semantic/ast_collection_exclusive_value_copy_owner.pgy:340 \
    src/self_hosted/semantic/ast_collection_exclusive_value_flow_owner.pgy:395 \
    src/self_hosted/semantic/ast_collection_exclusive_value_mutation_owner.pgy:300 \
    src/self_hosted/semantic/ast_collection_read_literal_argument_owner.pgy:155 \
    src/self_hosted/semantic/ast_collection_aggregate_release_plan_owner.pgy:160 \
    src/self_hosted/semantic/ast_collection_aggregate_release_uniqueness_owner.pgy:225 \
    src/self_hosted/semantic/ast_collection_aggregate_release_transition_owner.pgy:710 \
    src/self_hosted/mir/collection_ownership_receipt_fact_owner.pgy:180 \
    src/self_hosted/mir/collection_ownership_receipt_projection_owner.pgy:180 \
    src/self_hosted/mir/collection_ownership_receipt_json_projection_owner.pgy:100 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_transition_fact_owner.pgy:180 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_transition_policy_owner.pgy:120 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_cfg_flow_owner.pgy:220 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_receipt_source_owner.pgy:80 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_binding_local_owner.pgy:100 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_transition_admission_owner.pgy:380 \
    src/self_hosted/compiler/direct_mir_scalar_program_collection_ownership_transition_plan_readiness_owner.pgy:100 \
    src/self_hosted/compiler/driver_rung2_scalar_c_substitution_owner.pgy:100 \
    src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy:180 \
    src/self_hosted/compiler/direct_mir_scalar_program_owned_string_argument_source_owner.pgy:90; do
    collection_owner="${collection_owner_cap%%:*}"
    collection_cap="${collection_owner_cap##*:}"
    require_max_lines "$collection_owner" "$collection_cap"
done
require_max_lines "src/self_hosted/semantic/ast_collection_builtin_release_identity_owner.pgy" 16
require_max_lines "tests/self_hosted/fixtures/collection_storage_producer_probe.pgy" 160
require_max_lines "src/self_hosted/mir_lower/collection_ownership_binding_move_owner.pgy" 140
require_max_lines "tests/self_hosted/fixtures/collection_aggregate_entry_requirement_probe.pgy" 180
require_max_lines "tests/self_hosted/parity/collection_aggregate_entry_requirement_owner.sh" 140
require_max_lines "tests/self_hosted/parity/collection_aggregate_release_source_owner.sh" 129
require_max_lines "tests/self_hosted/fixtures/collection_lifetime_event_probe.pgy" 220
require_max_lines \
    "tests/self_hosted/parity/fixture/collection_ownership_binding_move_direct_c_probe.pgy" 30
require_max_lines \
    "tests/self_hosted/parity/collection_ownership_receipt_mutations.py" 100

require_max_lines "src/self_hosted/compiler/direct_mir_scalar_cfg_borrowed_collection_admission_owner.pgy" 50
require_max_lines "src/self_hosted/compiler/direct_mir_scalar_cfg_void_return_admission_owner.pgy" 50

require_max_lines "tests/self_hosted/parity/one_mir_string_array_emitted_contract.py" 110
require_max_lines "tests/self_hosted/parity/one_mir_string_array_execution_contract.sh" 50
require_max_lines "tests/self_hosted/parity/array_mutation_receiver_use_contract.sh" 230
require_max_lines "tests/self_hosted/parity/array_mutation_receiver_use_mutations.py" 220

echo "[self-host-component-contract] checkpoint: checking line caps"
run_line_cap_checks
echo "[self-host-component-contract] line-cap requests=${#LINE_CAP_REQUESTS[@]}"
echo "[self-host-component-contract] owner line caps and generator drift ok; executable parity owns behavior"
