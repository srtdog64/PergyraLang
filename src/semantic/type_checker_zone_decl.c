#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

bool
type_check_zone_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_zone = ctx->current_zone;
    const char *prev_module_path = ctx->current_module_path;
    size_t mutation_rule_count;
    ctx->current_zone = node;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.zone_decl.name,
        SYMBOL_ZONE,
        node->data.zone_decl.shared_fields,
        node->data.zone_decl.shared_count,
        node->data.zone_decl.methods,
        node->data.zone_decl.method_count,
        "zone");

    ok = type_check_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "Zone") && ok;
    ok = type_check_domain_slot_initializers(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "zone") && ok;
    mutation_rule_count = type_check_zone_shape_warnings(node, ctx);
    type_check_zone_authorities(node, ctx);

    if (mutation_rule_count > 0 && node->data.zone_decl.authority_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has lifecycle-changing rules but no explicit authority set",
            node->data.zone_decl.name);
    }

    type_check_zone_layer_slots(node, ctx);

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        const char *effect_slot_name = apply->data.zone_apply.effect_slot_name;
        const char *target_slot_name = apply->data.zone_apply.target_slot_name;
        const char *state_name = apply->data.zone_apply.state_name;
        const char *participant_slot_name = apply->data.zone_apply.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, apply, state_name, ctx, "apply",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply,
                "Zone apply references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- apply mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply,
                "Zone apply references unknown target slot '%s'.\n"
                "Reason:\n"
                "- apply must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this apply clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, apply,
            effect_slot_name, target_slot_name, ctx, "apply");
        type_check_zone_lifecycle_authority_presence(node, apply,
            participant_slot_name, ctx, "apply", "effect",
            effect_slot_name, target_slot_name);
        type_check_zone_participant_authority(node, apply, participant_slot_name, ctx, "apply");
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        const char *relation_slot_name = link->data.zone_link.relation_slot_name;
        const char *left_slot_name = link->data.zone_link.left_slot_name;
        const char *right_slot_name = link->data.zone_link.right_slot_name;
        const char *state_name = link->data.zone_link.state_name;
        const char *participant_slot_name = link->data.zone_link.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, link, state_name, ctx, "link",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
                "Zone link references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- link mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
                "Zone link references unknown left slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link,
                "Zone link references unknown right slot '%s'.\n"
                "Reason:\n"
                "- link needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this link clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, link,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "link");
        type_check_zone_lifecycle_authority_presence(node, link,
            participant_slot_name, ctx, "link", "relation",
            relation_slot_name, NULL);
        type_check_zone_participant_authority(node, link, participant_slot_name, ctx, "link");
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        const char *effect_slot_name = detach->data.zone_detach.effect_slot_name;
        const char *target_slot_name = detach->data.zone_detach.target_slot_name;
        const char *state_name = detach->data.zone_detach.state_name;
        const char *participant_slot_name = detach->data.zone_detach.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, detach, state_name, ctx, "detach",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, detach,
                "Zone detach references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- detach mutates an effect lifecycle and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, detach,
                "Zone detach references unknown target slot '%s'.\n"
                "Reason:\n"
                "- detach must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this detach clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, detach,
            effect_slot_name, target_slot_name, ctx, "detach");
        type_check_zone_lifecycle_authority_presence(node, detach,
            participant_slot_name, ctx, "detach", "effect",
            effect_slot_name, target_slot_name);
        type_check_zone_participant_authority(node, detach, participant_slot_name, ctx, "detach");
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        const char *relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
        const char *left_slot_name = unlink->data.zone_unlink.left_slot_name;
        const char *right_slot_name = unlink->data.zone_unlink.right_slot_name;
        const char *state_name = unlink->data.zone_unlink.state_name;
        const char *participant_slot_name = unlink->data.zone_unlink.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, unlink, state_name, ctx, "unlink",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
                "Zone unlink references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- unlink mutates a relation lifecycle and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
                "Zone unlink references unknown left slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, unlink,
                "Zone unlink references unknown right slot '%s'.\n"
                "Reason:\n"
                "- unlink needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this unlink clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, unlink,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "unlink");
        type_check_zone_lifecycle_authority_presence(node, unlink,
            participant_slot_name, ctx, "unlink", "relation",
            relation_slot_name, NULL);
        type_check_zone_participant_authority(node, unlink, participant_slot_name, ctx, "unlink");
    }

    type_check_zone_projection_rules(node, ctx);
    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        const char *effect_slot_name = maintain->data.zone_maintain_effect.effect_slot_name;
        const char *target_slot_name = maintain->data.zone_maintain_effect.target_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_effect.participant_slot_name;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown effect slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps an effect lifecycle active and must target a declared zone effect slot\n"
                "- zone '%s' does not declare effect slot '%s'\n"
                "Fix:\n"
                "- declare effect slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing effect slot",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown target slot '%s'.\n"
                "Reason:\n"
                "- maintain must target a declared zone slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing target slot",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                target_slot_name != NULL ? target_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_effect_contract(node, maintain,
            effect_slot_name, target_slot_name, ctx, "maintain");
        type_check_zone_lifecycle_authority_presence(node, maintain,
            participant_slot_name, ctx, "maintain", "effect",
            effect_slot_name, target_slot_name);
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_effect_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_effects[j];
            if (strcmp(effect_slot_name, other->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(target_slot_name, other->data.zone_maintain_effect.target_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains effect '%s' on '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for effect '%s' on '%s'\n"
                    "- or split the contract if the lifecycle targets are actually different",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
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
                    "Zone '%s' both maintains and detaches effect '%s' on '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while detach removes the same lifecycle path\n"
                    "- the current zone contract points in two different directions for one effect target pair\n"
                    "Fix:\n"
                    "- keep either maintain or detach for effect '%s' on '%s'\n"
                    "- or split the rules so they target different slots/states",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        const char *relation_slot_name = maintain->data.zone_maintain_relation.relation_slot_name;
        const char *left_slot_name = maintain->data.zone_maintain_relation.left_slot_name;
        const char *right_slot_name = maintain->data.zone_maintain_relation.right_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_relation.participant_slot_name;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown relation slot '%s'.\n"
                "Reason:\n"
                "- maintain keeps a relation lifecycle active and must target a declared zone relation slot\n"
                "- zone '%s' does not declare relation slot '%s'\n"
                "Fix:\n"
                "- declare relation slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing relation slot",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown left slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared left endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing left endpoint",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, maintain,
                "Zone maintain references unknown right slot '%s'.\n"
                "Reason:\n"
                "- maintain needs a declared right endpoint slot\n"
                "- zone '%s' does not declare slot '%s'\n"
                "Fix:\n"
                "- declare slot '%s' in zone '%s'\n"
                "- or change this maintain clause to an existing right endpoint",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>");
        }
        type_check_zone_relation_contract(node, maintain,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
        type_check_zone_lifecycle_authority_presence(node, maintain,
            participant_slot_name, ctx, "maintain", "relation",
            relation_slot_name, NULL);
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_relation_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_relations[j];
            if (strcmp(relation_slot_name, other->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(left_slot_name, other->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(right_slot_name, other->data.zone_maintain_relation.right_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains relation '%s' between '%s' and '%s' more than once.\n"
                    "Reason:\n"
                    "- duplicate maintain rules restate the same relation lifecycle contract\n"
                    "- repeated clauses add noise without changing propagation semantics\n"
                    "Fix:\n"
                    "- keep one maintain rule for relation '%s' between '%s' and '%s'\n"
                    "- or split the contract if the relation endpoints are actually different",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
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
                    "Zone '%s' both maintains and unlinks relation '%s' between '%s' and '%s'.\n"
                    "Reason:\n"
                    "- maintain keeps the lifecycle active, while unlink removes the same relation path\n"
                    "- the current zone contract points in two different directions for one relation edge\n"
                    "Fix:\n"
                    "- keep either maintain or unlink for relation '%s' between '%s' and '%s'\n"
                    "- or split the rules so they target different relation states",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
    }

    type_check_zone_state_aliases(node, ctx);
    ctx->current_zone = saved_zone;
    ctx->current_module_path = prev_module_path;
    return ok && !ctx->has_error;
}
