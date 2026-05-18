#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

void
type_check_zone_lifecycle_maintenance(ASTNode *node, SemanticContext *ctx)
{
    const char *zone_name = ast_zone_name(node);
    ASTNode **detaches;
    ASTNode **unlinks;
    ASTNode **maintained_effects;
    ASTNode **maintained_relations;
    size_t detach_count;
    size_t unlink_count;
    size_t maintained_effect_count;
    size_t maintained_relation_count;

    detaches = ast_zone_detaches(node, &detach_count);
    unlinks = ast_zone_unlinks(node, &unlink_count);
    maintained_effects = ast_zone_maintained_effects(node,
                                                     &maintained_effect_count);
    maintained_relations = ast_zone_maintained_relations(node,
        &maintained_relation_count);

    for (size_t i = 0; i < maintained_effect_count; i++) {
        ASTNode *maintain = maintained_effects[i];
        const char *effect_slot_name = ast_zone_effect_slot_name(maintain);
        const char *target_slot_name =
            ast_zone_effect_target_slot_name(maintain);
        const char *participant_slot_name =
            ast_zone_directive_participant_slot_name(maintain);
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                maintain,
                "Zone maintain references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps an effect lifecycle active and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                maintain,
                "Zone maintain references unknown target slot '%s'.\n"
                "Reason:\n"
                "- maintain must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        type_check_zone_effect_contract(node, maintain,
            effect_slot_name, target_slot_name, ctx, "maintain");
        type_check_zone_lifecycle_authority_presence(node, maintain,
            participant_slot_name, ctx, "maintain", "effect",
            effect_slot_name, target_slot_name);
        type_check_zone_participant_authority(node, maintain,
            participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < maintained_effect_count; j++) {
            ASTNode *other = maintained_effects[j];
            if (strcmp(effect_slot_name, ast_zone_effect_slot_name(other)) == 0
                && strcmp(target_slot_name,
                    ast_zone_effect_target_slot_name(other)) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains effect '%s' on '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for effect '%s' on '%s'\n"
                    "- or split the contract if the lifecycle targets are actually different",
                    zone_name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
        for (size_t j = 0; j < detach_count; j++) {
            ASTNode *detach = detaches[j];
            const char *detach_effect_slot_name =
                ast_zone_effect_slot_name(detach);
            const char *detach_target_slot_name =
                ast_zone_effect_target_slot_name(detach);
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
                    "Zone '%s' both maintains and detaches effect '%s' on '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while detach removes the same lifecycle path\n"
                    "- the current zone contract points in two different directions for one effect target pair\n"
                    "Fix:\n"
                    "- keep either maintain or detach for effect '%s' on '%s'\n"
                    "- or split the rules so they target different slots/states",
                    zone_name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
    }

    for (size_t i = 0; i < maintained_relation_count; i++) {
        ASTNode *maintain = maintained_relations[i];
        const char *relation_slot_name = ast_zone_relation_slot_name(maintain);
        const char *left_slot_name =
            ast_zone_relation_left_slot_name(maintain);
        const char *right_slot_name =
            ast_zone_relation_right_slot_name(maintain);
        const char *participant_slot_name =
            ast_zone_directive_participant_slot_name(maintain);
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                maintain,
                "Zone maintain references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps a relation lifecycle active and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                maintain,
                "Zone maintain references unknown left slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                maintain,
                "Zone maintain references unknown right slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        type_check_zone_relation_contract(node, maintain,
            relation_slot_name, left_slot_name, right_slot_name, ctx,
            "maintain");
        type_check_zone_lifecycle_authority_presence(node, maintain,
            participant_slot_name, ctx, "maintain", "relation",
            relation_slot_name, NULL);
        type_check_zone_participant_authority(node, maintain,
            participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < maintained_relation_count; j++) {
            ASTNode *other = maintained_relations[j];
            if (strcmp(relation_slot_name, ast_zone_relation_slot_name(other)) == 0
                && strcmp(left_slot_name,
                    ast_zone_relation_left_slot_name(other)) == 0
                && strcmp(right_slot_name,
                    ast_zone_relation_right_slot_name(other)) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains relation '%s' between '%s' and '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same relation lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for relation '%s' between '%s' and '%s'\n"
                    "- or split the contract if the relation endpoints are actually different",
                    zone_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
        for (size_t j = 0; j < unlink_count; j++) {
            ASTNode *unlink = unlinks[j];
            const char *unlink_relation_slot_name =
                ast_zone_relation_slot_name(unlink);
            const char *unlink_left_slot_name =
                ast_zone_relation_left_slot_name(unlink);
            const char *unlink_right_slot_name =
                ast_zone_relation_right_slot_name(unlink);
            if (ast_zone_directive_state_name(unlink) != NULL) {
                resolve_zone_relation_state(node, unlink,
                    ast_zone_directive_state_name(unlink), ctx, "unlink",
                    &unlink_relation_slot_name, &unlink_left_slot_name,
                    &unlink_right_slot_name);
            }
            if (unlink_relation_slot_name != NULL
                && unlink_left_slot_name != NULL
                && unlink_right_slot_name != NULL
                && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                && strcmp(left_slot_name, unlink_left_slot_name) == 0
                && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and unlinks relation '%s' between '%s' and '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while unlink removes the same relation path\n"
                    "- the current zone contract points in two different directions for one relation edge\n"
                    "Fix:\n"
                    "- keep either maintain or unlink for relation '%s' between '%s' and '%s'\n"
                    "- or split the rules so they target different relation states",
                    zone_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
    }
}
