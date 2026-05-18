#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

void
type_check_zone_lifecycle_mutations(ASTNode *node, SemanticContext *ctx)
{
    const char *zone_name = ast_zone_name(node);
    ASTNode **applies;
    ASTNode **links;
    ASTNode **detaches;
    ASTNode **unlinks;
    size_t apply_count;
    size_t link_count;
    size_t detach_count;
    size_t unlink_count;

    applies = ast_zone_applies(node, &apply_count);
    links = ast_zone_links(node, &link_count);
    detaches = ast_zone_detaches(node, &detach_count);
    unlinks = ast_zone_unlinks(node, &unlink_count);

    for (size_t i = 0; i < apply_count; i++) {
        ASTNode *apply = applies[i];
        const char *effect_slot_name = ast_zone_effect_slot_name(apply);
        const char *target_slot_name = ast_zone_effect_target_slot_name(apply);
        const char *state_name = ast_zone_directive_state_name(apply);
        const char *participant_slot_name =
            ast_zone_directive_participant_slot_name(apply);
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, apply, state_name, ctx,
                "apply", &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                apply,
                "Zone apply references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- apply mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                apply,
                "Zone apply references unknown target slot '%s'.\n"
                "Reason:\n"
                "- apply must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        type_check_zone_effect_contract(node, apply,
            effect_slot_name, target_slot_name, ctx, "apply");
        type_check_zone_lifecycle_authority_presence(node, apply,
            participant_slot_name, ctx, "apply", "effect",
            effect_slot_name, target_slot_name);
        type_check_zone_participant_authority(node, apply,
            participant_slot_name, ctx, "apply");
    }

    for (size_t i = 0; i < link_count; i++) {
        ASTNode *link = links[i];
        const char *relation_slot_name = ast_zone_relation_slot_name(link);
        const char *left_slot_name = ast_zone_relation_left_slot_name(link);
        const char *right_slot_name = ast_zone_relation_right_slot_name(link);
        const char *state_name = ast_zone_directive_state_name(link);
        const char *participant_slot_name =
            ast_zone_directive_participant_slot_name(link);
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, link, state_name, ctx,
                "link", &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                link,
                "Zone link references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- link mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                link,
                "Zone link references unknown left slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                link,
                "Zone link references unknown right slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        type_check_zone_relation_contract(node, link,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "link");
        type_check_zone_lifecycle_authority_presence(node, link,
            participant_slot_name, ctx, "link", "relation",
            relation_slot_name, NULL);
        type_check_zone_participant_authority(node, link,
            participant_slot_name, ctx, "link");
    }

    for (size_t i = 0; i < detach_count; i++) {
        ASTNode *detach = detaches[i];
        const char *effect_slot_name = ast_zone_effect_slot_name(detach);
        const char *target_slot_name = ast_zone_effect_target_slot_name(detach);
        const char *state_name = ast_zone_directive_state_name(detach);
        const char *participant_slot_name =
            ast_zone_directive_participant_slot_name(detach);
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, detach, state_name, ctx,
                "detach", &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                detach,
                "Zone detach references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- detach mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                detach,
                "Zone detach references unknown target slot '%s'.\n"
                "Reason:\n"
                "- detach must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        type_check_zone_effect_contract(node, detach,
            effect_slot_name, target_slot_name, ctx, "detach");
        type_check_zone_lifecycle_authority_presence(node, detach,
            participant_slot_name, ctx, "detach", "effect",
            effect_slot_name, target_slot_name);
        type_check_zone_participant_authority(node, detach,
            participant_slot_name, ctx, "detach");
    }

    for (size_t i = 0; i < unlink_count; i++) {
        ASTNode *unlink = unlinks[i];
        const char *relation_slot_name = ast_zone_relation_slot_name(unlink);
        const char *left_slot_name = ast_zone_relation_left_slot_name(unlink);
        const char *right_slot_name = ast_zone_relation_right_slot_name(unlink);
        const char *state_name = ast_zone_directive_state_name(unlink);
        const char *participant_slot_name =
            ast_zone_directive_participant_slot_name(unlink);
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, unlink, state_name, ctx,
                "unlink", &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                unlink,
                "Zone unlink references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- unlink mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                unlink,
                "Zone unlink references unknown left slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
                PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING,
                unlink,
                "Zone unlink references unknown right slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                zone_name != NULL ? zone_name : "<zone>");
        }
        type_check_zone_relation_contract(node, unlink,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "unlink");
        type_check_zone_lifecycle_authority_presence(node, unlink,
            participant_slot_name, ctx, "unlink", "relation",
            relation_slot_name, NULL);
        type_check_zone_participant_authority(node, unlink,
            participant_slot_name, ctx, "unlink");
    }
}
