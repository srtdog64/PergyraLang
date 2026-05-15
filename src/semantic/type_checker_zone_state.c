#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

void
type_check_zone_state_aliases(ASTNode *node, SemanticContext *ctx)
{
    const char *zone_name = ast_zone_name(node);
    ASTNode **states;
    ASTNode **detaches;
    ASTNode **unlinks;
    ASTNode **maintained_states;
    size_t state_count;
    size_t detach_count;
    size_t unlink_count;
    size_t maintained_state_count;
    size_t authority_count;

    states = ast_zone_states(node, &state_count);
    detaches = ast_zone_detaches(node, &detach_count);
    unlinks = ast_zone_unlinks(node, &unlink_count);
    maintained_states = ast_zone_maintained_states(node,
                                                   &maintained_state_count);
    ast_zone_authorities(node, &authority_count);

    for (size_t i = 0; i < maintained_state_count; i++) {
        ASTNode *maintain = maintained_states[i];
        ASTNode *state;
        const char *state_name = ast_zone_directive_state_name(maintain);
        const char *participant_slot_name = ast_zone_directive_participant_slot_name(maintain);
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
                zone_name != NULL ? zone_name : "<zone>",
                state_name != NULL ? state_name : "<unknown>",
                state_name != NULL ? state_name : "<unknown>");
        } else if (ast_zone_state_is_relation(state)) {
            const char *relation_slot_name = ast_zone_state_layer_slot_name(state);
            const char *left_slot_name = ast_zone_state_left_or_target_slot_name(state);
            const char *right_slot_name = ast_zone_state_right_slot_name(state);
            type_check_zone_relation_contract(node, maintain,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < maintained_state_count; j++) {
                ASTNode *other = maintained_states[j];
                if (other != NULL
                    && ast_zone_directive_state_name(other) != NULL
                    && strcmp(state_name, ast_zone_directive_state_name(other)) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        zone_name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < unlink_count; j++) {
                ASTNode *unlink = unlinks[j];
                const char *unlink_relation_slot_name = ast_zone_relation_slot_name(unlink);
                const char *unlink_left_slot_name = ast_zone_relation_left_slot_name(unlink);
                const char *unlink_right_slot_name = ast_zone_relation_right_slot_name(unlink);
                if (ast_zone_directive_state_name(unlink) != NULL) {
                    resolve_zone_relation_state(node, unlink,
                        ast_zone_directive_state_name(unlink), ctx, "unlink",
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
                        zone_name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        } else {
            const char *effect_slot_name = ast_zone_state_layer_slot_name(state);
            const char *target_slot_name = ast_zone_state_left_or_target_slot_name(state);
            type_check_zone_effect_contract(node, maintain,
                effect_slot_name, target_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < maintained_state_count; j++) {
                ASTNode *other = maintained_states[j];
                if (other != NULL
                    && ast_zone_directive_state_name(other) != NULL
                    && strcmp(state_name, ast_zone_directive_state_name(other)) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once.\n"
                        "Reason:\n"
                        "- duplicate maintain rules restate the same named lifecycle contract\n"
                        "- repeated clauses add noise without changing propagation semantics\n"
                        "Fix:\n"
                        "- keep one maintain rule for state '%s'\n"
                        "- or split the contract if the state names should differ",
                        zone_name,
                        state_name,
                        state_name);
                }
            }
            for (size_t j = 0; j < detach_count; j++) {
                ASTNode *detach = detaches[j];
                const char *detach_effect_slot_name = ast_zone_effect_slot_name(detach);
                const char *detach_target_slot_name = ast_zone_effect_target_slot_name(detach);
                if (ast_zone_directive_state_name(detach) != NULL) {
                    resolve_zone_effect_state(node, detach,
                        ast_zone_directive_state_name(detach), ctx, "detach",
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
                        zone_name,
                        state_name,
                        state_name,
                        state_name);
                }
            }
        }
        if (authority_count > 0 && participant_slot_name == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain must specify 'by <subjectSlot>' when authority is declared.\n"
                "Reason:\n"
                "- zone '%s' declares authority and maintain keeps named state '%s' active\n"
                "- persistent named-state lifecycle rules must record the approving subject slot\n"
                "Fix:\n"
                "- add 'by <subjectSlot>' to this maintain clause\n"
                "- or remove zone authority if this maintenance rule is intentionally authority-free",
                zone_name != NULL ? zone_name : "<zone>",
                state_name != NULL ? state_name : "<state>");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        const char *state_name = ast_zone_state_name(state);
        if (ast_zone_state_is_relation(state)) {
            const char *relation_slot_name = ast_zone_state_layer_slot_name(state);
            const char *left_slot_name = ast_zone_state_left_or_target_slot_name(state);
            const char *right_slot_name = ast_zone_state_right_slot_name(state);
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
                    zone_name != NULL ? zone_name : "<zone>",
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
                    zone_name != NULL ? zone_name : "<zone>",
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
                    zone_name != NULL ? zone_name : "<zone>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>");
            }
            type_check_zone_relation_contract(node, state,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "state");
        } else {
            const char *effect_slot_name = ast_zone_state_layer_slot_name(state);
            const char *target_slot_name = ast_zone_state_left_or_target_slot_name(state);
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
                    zone_name != NULL ? zone_name : "<zone>",
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
                    zone_name != NULL ? zone_name : "<zone>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>");
            }
            type_check_zone_effect_contract(node, state,
                effect_slot_name, target_slot_name, ctx, "state");
        }

        for (size_t j = i + 1; j < state_count; j++) {
            ASTNode *other = states[j];
            if (state_name != NULL
                && other != NULL
                && ast_zone_state_name(other) != NULL
                && strcmp(state_name, ast_zone_state_name(other)) == 0) {
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
