#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

grep -Fq "inherited_who_from_intent" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "inherited_where_from_intent" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "derived_authorized_by_from_zone" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "derived_who_from_single_participant" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "derived_who_from_on_receiver" \
    "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "inherited_who_from_intent = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "inherited_where_from_intent = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "derived_authorized_by_from_zone = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "derived_who_from_single_participant = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "derived_who_from_on_receiver = false" \
    "$ROOT_DIR/src/parser/ast_domain_constructors.c"
grep -Fq "inherited_who_from_intent = copied_any" \
    "$ROOT_DIR/src/parser/parser_intent.c"
grep -Fq "inherited_where_from_intent =" \
    "$ROOT_DIR/src/parser/parser_intent.c"
grep -Fq "reused who from intent-level default" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "derived who from single subject participant" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "derived who from on-call receiver" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "reused zone from intent-level default" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "derived authorized by from zone authority" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "reused who from intent-level default" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "derived who from single subject participant" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "derived who from on-call receiver" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "reused zone from intent-level default" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "approval owner stays on the zone/resource layer" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
grep -Fq "intent_step_derive_authorized_by_from_zone" \
    "$ROOT_DIR/src/semantic/type_checker_intent_authority.c"
grep -Fq "intent_step_derive_who_from_single_participant" \
    "$ROOT_DIR/src/semantic/type_checker_intent_action_contract.c"
grep -Fq "intent_step_derive_who_from_on_receiver" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "intent_step_derive_where_from_on_receiver" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "inherited_where_from_action = true" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "intent_step_derive_where_from_on_receiver" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "intent_step_inherit_contract_from_on_receiver" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "intent_step_authorized_by_alias_from_action" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "intent_on_call_arg_for_action_param" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "intent_step_append_required_ability_clone" \
    "$ROOT_DIR/src/semantic/type_checker_intent_helpers.c"
grep -Fq "inherited_authorized_by_from_action = true" \
    "$ROOT_DIR/src/semantic/type_checker_intent_on_inference.c"
grep -Fq "intent_step_can_derive_zone_authority" \
    "$ROOT_DIR/src/semantic/type_checker_intent_authority.c"
grep -Fq "authorized_by_derived_from_zone" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "derived_authorized_by_from_zone" \
    "$ROOT_DIR/src/compiler/dir_collect.c"
grep -Fq "authority_from_zone" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "authority_from_action" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "authority_provenance=%s" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "zone-derived" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "action-inherited" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "inherited from the intent-level who default" \
    "$ROOT_DIR/src/semantic/type_checker_intent_participants.c"
grep -Fq "intent_step_where_source_label" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "this where value came from %s" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "compressed using derivation can only bind a named intent participant or value" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "using binding points to a different zone than the current where contract" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "compressed intent orchestration needs a stable declared intent target" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "non-intent callees do not carry intent step provenance into AIR" \
    "$ROOT_DIR/src/semantic/type_checker_intent_decl.c"
grep -Fq "transfer target derivation and explicit using binding choose different zones" \
    "$ROOT_DIR/src/semantic/type_checker_intent_transfer.c"
grep -Fq "explicit or inherited step zone conflicts with the transfer destination" \
    "$ROOT_DIR/src/semantic/type_checker_intent_transfer.c"
grep -Fq "who_inherited_from_intent" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "who_derived_from_single_participant" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "who_derived_from_on_receiver" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "where_inherited_from_intent" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "where_inherited_from_action" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "where_derived_from_using" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "where_derived_from_transfer" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "using_derived_from_transfer" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "requires_inherited_from_action" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "causes_inherited_from_action" \
    "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "who-default=intent" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "who-derived=single-participant" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "where-default=intent" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "where-default=action" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "where-derived=using" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "where-derived=transfer" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "using-derived=transfer" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "requires-default=action" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "causes-default=action" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "authorized_by_provenance action-inherited" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "who_from_intent_default" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "who_from_single_participant" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "who_from_on_receiver" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "requires_from_action" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "causes_from_action" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "source_from_intent_default" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "source_from_action" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "source_from_transfer" \
    "$ROOT_DIR/src/compiler/air.h"
grep -Fq "step->where_derived_from_transfer" \
    "$ROOT_DIR/src/compiler/air.c"
grep -Fq "source_provenance=" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "who_provenance=" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "intent-default+transfer" \
    "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "AIR world boundary node %zu has no transfer provenance" \
    "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "test_air_verify_rejects_world_boundary_without_transfer_provenance" \
    "$ROOT_DIR/src/tests/air/test_air_strict_part_f.cases.h"
grep -Fq "AIR verify rejects world boundary without transfer provenance" \
    "$ROOT_DIR/src/test_air.c"
grep -Fq "who_from_intent_default" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "who_from_single_participant" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "who_from_on_receiver" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "requires_from_action" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "causes_from_action" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "RIR_OP_ATTACH_EFFECT" \
    "$ROOT_DIR/src/compiler/rir_builder_intent.c"
grep -Fq "RIR_RESOURCE_EFFECT_INSTANCE" \
    "$ROOT_DIR/src/compiler/rir_builder_intent.c"
grep -Fq "unique_effect_slot_for_type" \
    "$ROOT_DIR/src/compiler/rir_builder_intent.c"
grep -Fq "effect_anchor" \
    "$ROOT_DIR/src/compiler/rir_builder_intent.c"
grep -Fq "authority_from_action" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "source_from_intent_default" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "source_from_action" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "source_from_transfer" \
    "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "intent step reuses intent-level who and where defaults" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "intent authority-bearing zone derives authorized by for secure helper" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_c.cases.h"
grep -Fq "intent transfer derives zone using and authorized by from target authority" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_c.cases.h"
grep -Fq "records derived authorized by before action diagnostics" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_c.cases.h"
grep -Fq "intent step effect in authority-bearing zone derives authorized by" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_a.cases.h"
grep -Fq "intent step maps action parameter authority from on-call argument" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_intent_compression_part_a.cases.h"
grep -Fq "test_air_parsed_on_receiver_action_contract_provenance" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "rir_effect_propagation_required_count == 1" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "AIR_EVIDENCE_RIR_EFFECT_PROPAGATION" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "AIR_EVIDENCE_RIR_AUTHORITY" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "has_rir_authority_evidence" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "rir_authority_evidence_name" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "AIR_EVIDENCE_DAG_ABILITY" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "dag_ability_evidence_count == 1" \
    "$ROOT_DIR/src/tests/air/test_air_parsed_part_e.cases.h"
grep -Fq "air_collect_dag_evidence(air, sem" \
    "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR parsed on-receiver action contract provenance" \
    "$ROOT_DIR/src/test_air.c"
grep -Fq "intent step keeps parameter authority explicit across multiple on calls" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_intent_compression_part_a.cases.h"
grep -Fq "intent step keeps parameter authority explicit for expression on argument" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_intent_compression_part_a.cases.h"
grep -Fq "intent-level who default failure reports provenance" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "intent-level where default failure reports provenance" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "this where value came from the intent-level where default" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "intent using mismatch reports compressed derivation reason" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "using binding points to a different zone than the current where contract" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_g.cases.h"
grep -Fq "derived_using_from_where" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_intent_compression_part_a.cases.h"
grep -Fq "derived using from zone type" \
    "$ROOT_DIR/src/parser/ast_print_intent.c"
grep -Fq "using_derived_from_where" \
    "$ROOT_DIR/src/compiler/dir_collect.c"
grep -Fq "using-derived=where" \
    "$ROOT_DIR/src/compiler/dir_validate.c"
grep -Fq "DIR captures intent transfer and zone parameter participants" \
    "$ROOT_DIR/src/tests/dir/test_dir_lowering.cases.h"
grep -Fq "step->where_derived_from_transfer" \
    "$ROOT_DIR/src/tests/dir/test_dir_lowering.cases.h"
grep -Fq "step->using_derived_from_transfer" \
    "$ROOT_DIR/src/tests/dir/test_dir_lowering.cases.h"
grep -Fq "non-intent callees do not carry intent step provenance into AIR" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "declare the participant with 'who ghost: <Subject>;'" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "inherited_who_from_intent" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq "inherited_where_from_intent" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_misc_b2_part_d.cases.h"
grep -Fq ".who_inherited_from_intent = true" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq ".where_inherited_from_intent = true" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "air->intents[0].who_from_intent_default" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "air->boundaries[0].source_from_intent_default" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "air->boundaries[0].source_from_transfer" \
    "$ROOT_DIR/src/tests/air/test_air_cleanup_transfer_part_c.cases.h"

if grep -F "locally declared who on step" \
        "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c" \
    | grep -Fq "inherited_who_from_intent"; then
    :
else
    grep -Fq "&& !step->data.intent_step.inherited_who_from_intent" \
        "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"
fi

grep -Fq "&& !step->data.intent_step.inherited_where_from_intent" \
    "$ROOT_DIR/src/semantic/type_checker_intent_contract_summary.c"

echo "[intent-compression-contract] intent compression provenance is source-gated through DIR/AIR/RIR"
