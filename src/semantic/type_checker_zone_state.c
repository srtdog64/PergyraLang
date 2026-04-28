#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

void
type_check_zone_state_aliases(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        ASTNode *state;
        const char *state_name = maintain->data.zone_maintain_state.state_name;
        const char *participant_slot_name = maintain->data.zone_maintain_state.participant_slot_name;
        state = find_zone_state(node, state_name);
        if (state == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown state '%s'.\n"
                "Reason:\n"
                "- maintain state aliases must reference a declared zone state\n"
                "- zone '%s' does not declare state '%s'\n"
                "Fix:\n"
                "- declare state '%s' before this maintain clause\n"
                "- or change maintain to an existing state alias",
                state_name != NULL ? state_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                state_name != NULL ? state_name : "<unknown>",
                state_name != NULL ? state_name : "<unknown>");
        } else if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            type_check_zone_relation_contract(node, maintain,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        node->data.zone_decl.name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
                ASTNode *unlink = node->data.zone_decl.unlinks[j];
                const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
                const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
                const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
                if (unlink->data.zone_unlink.state_name != NULL) {
                    resolve_zone_relation_state(node, unlink,
                        unlink->data.zone_unlink.state_name, ctx, "unlink",
                        &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
                }
                if (unlink_relation_slot_name != NULL
                    && unlink_left_slot_name != NULL
                    && unlink_right_slot_name != NULL
                    && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                    && strcmp(left_slot_name, unlink_left_slot_name) == 0
                    && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and unlinks state '%s'.\n"
                        "Reason:\n"
                        "- maintain keeps the named relation-state active, while unlink removes the same underlying path\n"
                        "- the current zone contract points in two different directions for state '%s'\n"
                        "Fix:\n"
                        "- keep either maintain or unlink for state '%s'\n"
                        "- or split the rules so they target different states",
                        node->data.zone_decl.name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            type_check_zone_effect_contract(node, maintain,
                effect_slot_name, target_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        node->data.zone_decl.name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
                ASTNode *detach = node->data.zone_decl.detaches[j];
                const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
                const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
                if (detach->data.zone_detach.state_name != NULL) {
                    resolve_zone_effect_state(node, detach,
                        detach->data.zone_detach.state_name, ctx, "detach",
                        &detach_effect_slot_name, &detach_target_slot_name);
                }
                if (detach_effect_slot_name != NULL
                    && detach_target_slot_name != NULL
                    && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                    && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and detaches state '%s'.\n"
                        "Reason:\n"
                        "- maintain keeps the named effect-state active, while detach removes the same underlying path\n"
                        "- the current zone contract points in two different directions for state '%s'\n"
                        "Fix:\n"
                        "- keep either maintain or detach for state '%s'\n"
                        "- or split the rules so they target different states",
                        node->data.zone_decl.name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        }
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and maintain keeps named state '%s' active\n"
                "- persistent named-state lifecycle rules must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this maintain clause\n"
                "- or remove zone authority if this maintenance rule is intentionally authority-free",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                state_name != NULL ? state_name : "<state>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        const char *state_name = state->data.zone_state.state_name;
        if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown relation slot '%s'.\n"
                    "Reason:\n"
                    "- state '%s' is declared as a relation-backed lifecycle alias\n"
                    "- zone '%s' does not declare relation slot '%s'\n"
                    "Fix:\n"
                    "- reference an existing relation slot in state '%s'\n"
                    "- or declare relation slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, left_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown left slot '%s'.\n"
                    "Reason:\n"
                    "- relation state '%s' needs a declared left endpoint slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the left endpoint\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, right_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown right slot '%s'.\n"
                    "Reason:\n"
                    "- relation state '%s' needs a declared right endpoint slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the right endpoint\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>");
            }
            type_check_zone_relation_contract(node, state,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "state");
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown effect slot '%s'.\n"
                    "Reason:\n"
                    "- state '%s' is declared as an effect-backed lifecycle alias\n"
                    "- zone '%s' does not declare effect slot '%s'\n"
                    "Fix:\n"
                    "- reference an existing effect slot in state '%s'\n"
                    "- or declare effect slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, target_slot_name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, state,
                    "Zone state '%s' references unknown target slot '%s'.\n"
                    "Reason:\n"
                    "- effect state '%s' needs a declared target slot\n"
                    "- zone '%s' does not declare slot '%s'\n"
                    "Fix:\n"
                    "- use an existing zone slot as the target\n"
                    "- or declare slot '%s' before this state",
                    state_name != NULL ? state_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    state_name != NULL ? state_name : "<unknown>",
                    node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>");
            }
            type_check_zone_effect_contract(node, state,
                effect_slot_name, target_slot_name, ctx, "state");
        }

        for (size_t j = i + 1; j < node->data.zone_decl.state_count; j++) {
            ASTNode *other = node->data.zone_decl.states[j];
            if (state_name != NULL
                && other != NULL
                && other->data.zone_state.state_name != NULL
                && strcmp(state_name, other->data.zone_state.state_name) == 0) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_REDECLARATION,
                    PGY_CAUSE_ZONE_STATE_DUPLICATE_NAME,
                    PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                    other,
                    "Redeclaration of zone state '%s'",
                    state_name);
            }
        }
    }

}
