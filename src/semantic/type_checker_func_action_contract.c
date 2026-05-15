#include <stdbool.h>
#include <string.h>

#include "../common/string_compat.h"
#include "parser/ast_api.h"
#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "type_checker_module_contract_internal.h"

static Type *
action_contract_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    return domain_resolve_slot_type(slot, ctx);
}

static Type *
action_contract_resolve_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return domain_resolve_type_ref(param->type, ctx);
}

static bool
callable_contract_is_externally_visible(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *host = current_host_decl(ctx);

    if (node == NULL || ctx == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_exported)
        return true;
    if (host == NULL || !host->is_exported)
        return false;
    if (!node->data.func_decl.has_explicit_access)
        return true;
    return node->data.func_decl.access == ACCESS_PUBLIC
        || node->data.func_decl.access == ACCESS_PROTECTED;
}

void
semantic_validate_action_func_contract(ASTNode *node,
                                       SemanticContext *ctx,
                                       ASTNode *enclosing_nominal,
                                       const char *name,
                                       bool is_action)
{
    if (is_action) {
        const char *subject_name = NULL;

        if (enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && ast_class_nominal_kind(enclosing_nominal) == NOMINAL_DECL_SUBJECT) {
            subject_name = ast_class_name(enclosing_nominal);
        }
        validate_action_required_abilities(node, enclosing_nominal, ctx);

        /* Derive 'within' from the surrounding lexical zone */
        if (node->data.func_decl.within_zone == NULL
            && ctx->current_zone != NULL
            && ctx->current_zone->type == AST_ZONE_DECL
            && ast_zone_name(ctx->current_zone) != NULL) {
            node->data.func_decl.within_zone =
                pergyra_strdup(ast_zone_name(ctx->current_zone));
        }

        if (node->data.func_decl.within_zone != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' references unknown zone '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.within_zone);
        }
        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            if (zone_decl != NULL
                && !explicit_type_reference_allowed(zone_decl, node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported zone '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
            if (zone_decl != NULL
                && !zone_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported zone '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
        }

        if (node->data.func_decl.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' references unknown effect '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.causes_effect);
        }
        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL
                && !explicit_type_reference_allowed(effect_decl, node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported effect '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
            if (effect_decl != NULL
                && !effect_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported effect '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
        }

        for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
            const char *auth_name = node->data.func_decl.authorized_by[i];
            bool found = auth_name != NULL && strcmp(auth_name, "self") == 0;
            const char *auth_type_name = NULL;

            for (size_t j = 0; !found && j < ast_func_param_count(node); j++) {
                FuncParam *param = ast_func_param(node, j);
                if (param != NULL && param->name != NULL
                    && strcmp(param->name, auth_name) == 0) {
                    found = true;
                }
            }

            if (!found) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' authorized subject '%s' must be 'self' or one of the action parameters",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
                continue;
            }

            auth_type_name = find_action_binding_type_name(
                node, enclosing_nominal, ctx, auth_name);
            if (auth_type_name == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' authorized subject '%s' must be a subject host",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
            }
        }

        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            ASTNode **zone_slots = NULL;
            size_t zone_slot_count = 0;
            if (zone_decl != NULL)
                zone_slots = ast_zone_slots(zone_decl, &zone_slot_count);
            if (zone_decl != NULL && subject_name != NULL
                && !domain_has_subject_slot_type(zone_slots,
                    zone_slot_count, ctx, subject_name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' references zone '%s', but that zone has no subject slot for '%s'",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone,
                    subject_name);
            }

            if (zone_decl != NULL) {
                for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
                    const char *auth_name = node->data.func_decl.authorized_by[i];
                    const char *auth_type_name = find_action_binding_type_name(
                        node, enclosing_nominal, ctx, auth_name);
                    if (auth_type_name == NULL) {
                        continue;
                    }
                    if (!domain_has_subject_slot_type(zone_slots,
                            zone_slot_count, ctx, auth_type_name)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                            "action '%s' authorized subject '%s' has type '%s', but zone '%s' has no matching subject slot.\n"
                            "Reason:\n"
                            "- action contract derives authority provenance from binding '%s'\n"
                            "- binding '%s' has subject type '%s'\n"
                            "- zone '%s' exposes no subject slot for that type\n"
                            "Fix:\n"
                            "- add a subject slot for '%s' to zone '%s'\n"
                            "- or authorize this action by a subject already declared in zone '%s'",
                            name != NULL ? name : "<anonymous>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            node->data.func_decl.within_zone);
                    } else if (!zone_has_authority_for_subject_type(zone_decl, ctx, auth_type_name)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                            "action '%s' authorized subject '%s' has type '%s', but zone '%s' declares no matching authority.\n"
                            "Reason:\n"
                            "- within-zone contract comes from action clause 'within %s'\n"
                            "- action contract derives authority provenance from binding '%s'\n"
                            "- binding '%s' has subject type '%s'\n"
                            "- authority check edge is action '%s' -> zone '%s' -> binding '%s'\n"
                            "- zone '%s' has a subject slot for that type but no authority contract\n"
                            "Fix:\n"
                            "- declare authority for '%s' in zone '%s'\n"
                            "- or change/remove 'authorized by %s' on action '%s'",
                            name != NULL ? name : "<anonymous>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            name != NULL ? name : "<anonymous>",
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            node->data.func_decl.within_zone,
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            name != NULL ? name : "<anonymous>");
                    }
                }
            }
        }

        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL) {
                ASTNode **effect_slots;
                size_t effect_slot_count;
                effect_slots = ast_effect_slots(effect_decl, &effect_slot_count);
                for (size_t i = 0; i < effect_slot_count; i++) {
                    ASTNode *slot = effect_slots[i];
                    Type *slot_type;
                    bool matched = false;

                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || !ast_domain_slot_is_binding(slot)
                        || ast_domain_slot_type(slot) == NULL) {
                        continue;
                    }
                    slot_type = action_contract_resolve_domain_slot_type(slot, ctx);
                    if (slot_type == NULL || slot_type->name == NULL)
                        continue;

                    if (subject_name != NULL && strcmp(subject_name, slot_type->name) == 0) {
                        matched = true;
                    } else {
                        for (size_t j = 0; j < ast_func_param_count(node); j++) {
                            FuncParam *param = ast_func_param(node, j);
                            Type *param_type;
                            if (param == NULL || param->type == NULL)
                                continue;
                            param_type = action_contract_resolve_param_type(param, ctx);
                            if (param_type != NULL
                                && param_type->name != NULL
                                && strcmp(param_type->name, slot_type->name) == 0) {
                                matched = true;
                                break;
                            }
                        }
                    }

                    if (!matched) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                            "action '%s' causes effect '%s', but no self/parameter matches effect target type '%s'",
                            name != NULL ? name : "<anonymous>",
                            node->data.func_decl.causes_effect,
                            slot_type->name);
                    }
                }
            }

            if (node->data.func_decl.within_zone != NULL) {
                ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                    node->data.func_decl.within_zone);
                if (zone_decl != NULL
                    && !zone_has_effect_layer_type(zone_decl, node->data.func_decl.causes_effect)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                        "action '%s' causes effect '%s', but zone '%s' has no matching effect slot.\n"
                        "Reason:\n"
                        "- action contract declares causes '%s'\n"
                        "- zone '%s' does not materialize any matching effect slot for that contract\n"
                        "Fix:\n"
                        "- add an effect slot of type '%s' to zone '%s'\n"
                        "- or remove/change the causes clause on action '%s'",
                        name != NULL ? name : "<anonymous>",
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        name != NULL ? name : "<anonymous>");
                }
            }
        }
    }
}
