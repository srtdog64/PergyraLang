/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Intent step authority / authorized-by contract validation.
 */

#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "diag_codes.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

#include <stdbool.h>

static const char *
intent_step_suggest_authorizer_from_zone(ASTNode *intent_decl,
                                         ASTNode *step,
                                         ASTNode *zone_decl,
                                         SemanticContext *ctx)
{
    char **who_names;
    size_t who_count = 0;
    size_t authority_count;

    if (intent_decl == NULL || step == NULL || zone_decl == NULL
        || ctx == NULL
        || zone_decl->type != AST_ZONE_DECL
        || step->type != AST_INTENT_STEP) {
        return NULL;
    }
    ast_zone_authorities(zone_decl, &authority_count);
    if (authority_count == 0)
        return NULL;

    who_names = ast_intent_step_who_names(step, &who_count);
    for (size_t i = 0; i < who_count; i++) {
        const char *alias = who_names != NULL ? who_names[i] : NULL;
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *participant_type_name = intent_involves_type_name(involves);
        bool ambiguous = false;
        ASTNode *authority_slot;
        const char *authority_slot_name;

        if (alias == NULL || participant_type_name == NULL
            || !intent_involves_is_subject_host(ctx->program_root, involves)) {
            continue;
        }

        authority_slot = resolve_zone_subject_slot_for_participant(
            zone_decl, ctx, alias, participant_type_name, &ambiguous);
        authority_slot_name = ast_domain_slot_name(authority_slot);
        if (!ambiguous && authority_slot_name != NULL
            && find_zone_authority(zone_decl, authority_slot_name) != NULL) {
            return alias;
        }
    }

    return NULL;
}

void
type_check_intent_step_authority_contract(ASTNode *intent_decl,
                                          ASTNode *step,
                                          ASTNode *zone_decl,
                                          bool has_subintent,
                                          bool step_requires_authority_flow,
                                          SemanticContext *ctx)
{
    const char *zone_name;
    const char *step_name;
    const char *step_causes;
    const char *transfer_from;
    const char *transfer_to;
    ASTNode **zone_slots = NULL;
    size_t zone_slot_count = 0;
    size_t zone_authority_count = 0;
    char **authorized_by;
    size_t authorized_by_count = 0;

    if (intent_decl == NULL || step == NULL || step->type != AST_INTENT_STEP
        || ctx == NULL) {
        return;
    }
    zone_name = ast_zone_name(zone_decl);
    step_name = ast_intent_step_name(step);
    step_causes = ast_intent_step_causes_effect(step);
    transfer_from = ast_intent_step_transfer_from_alias(step);
    transfer_to = ast_intent_step_transfer_to_alias(step);
    if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL) {
        zone_slots = ast_zone_slots(zone_decl, &zone_slot_count);
        ast_zone_authorities(zone_decl, &zone_authority_count);
    }

    if (zone_decl != NULL
        && zone_authority_count > 0
        && ast_intent_step_authorized_by_count(step) == 0
        && !has_subintent
        && (((ast_intent_step_where_type(step) != NULL
              && !ast_intent_step_derived_where_from_using(step))
             || ast_intent_step_inherited_where_from_action(step)
             || ast_intent_step_derived_where_from_transfer(step))
            || step_causes != NULL
            || transfer_from != NULL
            || transfer_to != NULL
            || step_requires_authority_flow)) {
        char inherited_summary[512];
        const char *suggested_authorizer =
            intent_step_suggest_authorizer_from_zone(
                intent_decl, step, zone_decl, ctx);
        const char *authority_reason =
            (step_causes != NULL
             || transfer_from != NULL
             || transfer_to != NULL
             || step_requires_authority_flow)
                ? "- the step causes effects, transfers zone state, or invokes authority-sensitive helpers"
                : "- zone authority requires explicit approval even for declarative steps in this zone";
        intent_step_format_contract_source_summary(
            intent_decl, step, ctx, inherited_summary, sizeof(inherited_summary));
        if (ast_intent_step_inherited_where_from_action(step)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' cannot run in authority-bearing zone '%s' without 'authorized by'.\n"
                "Reason:\n"
                "%s\n"
                "- zone '%s' was reused from the matching action contract\n"
                "Contract source:\n"
                "- the inherited/derived zone provenance listed below\n"
                "%s- approval boundary provenance is:\n"
                "%s%s"
                "Fix:\n"
                "- add 'authorized by: %s;' to the step\n"
                "- or add 'authorized by self' to the matching action contract when the receiver is the approver\n"
                "- or move the step to a non-authority zone\n"
                "- or change the action contract that this step reuses",
                step_name != NULL ? step_name : "<step>",
                zone_name != NULL ? zone_name : "<zone>",
                authority_reason,
                zone_name != NULL ? zone_name : "<zone>",
                inherited_summary[0] != '\0' ? "" : "- no inherited or derived zone contract provenance was recorded\n",
                inherited_summary[0] != '\0' ? inherited_summary : "",
                inherited_summary[0] != '\0' ? "\n" : "",
                suggested_authorizer != NULL ? suggested_authorizer : "<participant>");
        } else {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' cannot run in authority-bearing zone '%s' without 'authorized by'.\n"
                "Reason:\n"
                "%s\n"
                "- zone '%s' declares authority, so explicit approval is required\n"
                "Contract source:\n"
                "- the inherited/derived zone provenance listed below\n"
                "%s- approval boundary provenance is:\n"
                "%s%s"
                "Fix:\n"
                "- add 'authorized by: %s;' to the step\n"
                "- or move the step to a non-authority zone\n"
                "- or remove the authority-sensitive operation from this step",
                step_name != NULL ? step_name : "<step>",
                zone_name != NULL ? zone_name : "<zone>",
                authority_reason,
                zone_name != NULL ? zone_name : "<zone>",
                inherited_summary[0] != '\0' ? "" : "- no inherited or derived zone contract provenance was recorded\n",
                inherited_summary[0] != '\0' ? inherited_summary : "",
                inherited_summary[0] != '\0' ? "\n" : "",
                suggested_authorizer != NULL ? suggested_authorizer : "<participant>");
        }
    }

    authorized_by = ast_intent_step_authorized_by(step, &authorized_by_count);
    for (size_t j = 0; j < authorized_by_count; j++) {
        const char *alias = authorized_by[j];
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *participant_type_name = intent_involves_type_name(involves);
        char contract_summary[512];
        intent_step_format_contract_source_summary(
            intent_decl, step, ctx, contract_summary, sizeof(contract_summary));
        if (involves == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' authorizes unknown participant '%s'.\n"
                "Reason:\n"
                "- the authorized-by clause references a participant alias that is not declared on the intent\n"
                "Contract source:\n"
                "- the authority-bearing zone/step approval requirement summarized below\n"
                "%s- approval boundary provenance is:\n"
                "%s%s"
                "Fix:\n"
                "- add 'involves %s: <Subject>' to the intent\n"
                "- or change 'authorized by' to one of the declared participants",
                step_name != NULL ? step_name : "<step>",
                alias != NULL ? alias : "<participant>",
                contract_summary[0] != '\0' ? "" : "- no inherited/derived authority provenance was recorded\n",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "",
                alias != NULL ? alias : "<participant>");
            continue;
        }

        if (zone_decl != NULL && participant_type_name != NULL) {
            if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' authorized participant '%s' must bind to a subject type.\n"
                    "Reason:\n"
                    "- only subject participants can satisfy zone authority contracts\n"
                    "- participant '%s' is not a subject host\n"
                    "Contract source:\n"
                    "- the authority-bearing zone/step approval requirement summarized below\n"
                    "%s- approval boundary provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- authorize a subject participant instead\n"
                    "- or change the intent binding so '%s' is a subject type",
                    step_name != NULL ? step_name : "<step>",
                    alias != NULL ? alias : "<participant>",
                    alias != NULL ? alias : "<participant>",
                    contract_summary[0] != '\0' ? "" : "- no inherited/derived authority provenance was recorded\n",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    alias != NULL ? alias : "<participant>");
                continue;
            }
            if (!domain_has_subject_slot_type(zone_slots,
                    zone_slot_count, ctx, participant_type_name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                    "Intent step '%s' authorized participant '%s' has type '%s', but zone '%s' has no matching subject slot.\n"
                    "Reason:\n"
                    "- authorized participants must map to a subject slot inside the current zone\n"
                    "Contract source:\n"
                    "- authority-bearing zone '%s' with authorized participant '%s'\n"
                    "- declared authorized-by edge points to participant '%s' of type '%s'\n"
                    "- zone '%s' does not declare a subject slot of type '%s'\n"
                    "%s- approval boundary provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- authorize a participant whose subject type exists in zone '%s'\n"
                    "- or add a matching subject slot to zone '%s'",
                    step_name != NULL ? step_name : "<step>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    zone_name != NULL ? zone_name : "<zone>",
                    zone_name != NULL ? zone_name : "<zone>",
                    alias != NULL ? alias : "<participant>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    zone_name != NULL ? zone_name : "<zone>",
                    participant_type_name,
                    contract_summary[0] != '\0' ? "" : "- no inherited/derived authority provenance was recorded\n",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    zone_name != NULL ? zone_name : "<zone>",
                    zone_name != NULL ? zone_name : "<zone>");
            } else if (zone_authority_count > 0) {
                bool authority_slot_ambiguous = false;
                ASTNode *authority_slot = resolve_zone_subject_slot_for_participant(
                    zone_decl, ctx, alias, participant_type_name,
                    &authority_slot_ambiguous);
                const char *authority_slot_name = ast_domain_slot_name(authority_slot);
                if (authority_slot_ambiguous) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                        "Intent step '%s' authorized participant '%s' of type '%s' is ambiguous in zone '%s'.\n"
                        "Reason:\n"
                        "- authorized participants must resolve to one concrete authority subject slot\n"
                        "- zone '%s' has more than one subject slot of type '%s'\n"
                        "- participant alias '%s' does not name one of those slots directly\n"
                        "%s- approval boundary provenance is:\n"
                        "%s%s"
                        "Fix:\n"
                        "- rename the participant alias to the intended authority slot name\n"
                        "- or authorize a participant whose alias directly names the authority slot\n"
                        "- or split the zone contract so authority is not ambiguous",
                        step_name != NULL ? step_name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        zone_name != NULL ? zone_name : "<zone>",
                        zone_name != NULL ? zone_name : "<zone>",
                        participant_type_name,
                        alias != NULL ? alias : "<participant>",
                        contract_summary[0] != '\0' ? "" : "- no inherited/derived authority provenance was recorded\n",
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "");
                } else if (authority_slot == NULL || authority_slot_name == NULL
                    || find_zone_authority(zone_decl, authority_slot_name) == NULL) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                        "Intent step '%s' authorized participant '%s' resolves to non-authority slot '%s' in zone '%s'.\n"
                        "Reason:\n"
                        "Contract source:\n"
                        "- authority-bearing zone '%s' with authorized participant '%s'\n"
                        "- declared authorized-by edge points to participant '%s' of type '%s'\n"
                        "- zone '%s' has authority rules, but resolved slot '%s' is not an authority slot\n"
                        "%s- approval boundary provenance is:\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize the participant mapped to an authority slot in zone '%s'\n"
                        "- or add an authority declaration for slot '%s' in zone '%s'",
                        step_name != NULL ? step_name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        authority_slot_name != NULL ? authority_slot_name : "<unbound>",
                        zone_name != NULL ? zone_name : "<zone>",
                        zone_name != NULL ? zone_name : "<zone>",
                        alias != NULL ? alias : "<participant>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        zone_name != NULL ? zone_name : "<zone>",
                        authority_slot_name != NULL ? authority_slot_name : "<unbound>",
                        contract_summary[0] != '\0' ? "" : "- no inherited/derived authority provenance was recorded\n",
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        zone_name != NULL ? zone_name : "<zone>",
                        authority_slot_name != NULL ? authority_slot_name : "<slot>",
                        zone_name != NULL ? zone_name : "<zone>");
                }
            }
        }
    }
}
