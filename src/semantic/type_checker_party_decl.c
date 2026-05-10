#include "type_checker_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_module_contract_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static Type *
party_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx,
                                                                   type_ref);
}

bool
type_check_party_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.party_decl.name;
    const char *prev_module_path = ctx->current_module_path;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_PARTY;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        /* Forward-declared in Pass 1 — update kind */
        existing->kind = SYMBOL_PARTY;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_PARTY_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of party '%s'", name);
        symbol_destroy(sym);
        ctx->current_module_path = prev_module_path;
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    if (node->data.party_decl.generic_params != NULL
        && node->data.party_decl.generic_params->count > 0) {
        validate_generic_param_defaults(node->data.party_decl.generic_params,
            ctx, node, "party");
    }

    /* Check role slot ability references */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];

        /* dyn slots require at least one ability for vtable dispatch */
        if (rs->data.role_slot.is_dynamic &&
            rs->data.role_slot.ability_count == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, rs,
                "Dynamic role slot '%s' requires at least one ability type",
                rs->data.role_slot.slot_name);
        }

        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab_type = rs->data.role_slot.required_abilities[j];
            if (ab_type != NULL && ab_type->data.type.name != NULL) {
                const char *ability_name = ab_type->data.type.name;
                char *required_text = ability_ref_display(ab_type);
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    rs,
                    rs->data.role_slot.slot_name != NULL
                        ? rs->data.role_slot.slot_name : "<role-slot>",
                    ab_type,
                    "party role slot ability consumer lookup");
                if (ctx->program_root != NULL) {
                    ASTNode *ability_decl = resolve_required_ability_decl(
                        ab_type, rs, ctx, "party role slot",
                        rs->data.role_slot.slot_name);
                    if (ability_decl == NULL) {
                        free(required_text);
                        continue;
                    }
                }
                if (ctx->program_root != NULL
                           && !any_subject_role_has_ability(ctx->program_root,
                               ab_type)) {
                    ASTNode *actual_impl = any_subject_role_find_base_ability_impl(
                        ctx->program_root, ability_name);
                    char *actual_text = actual_impl != NULL
                        ? ability_ref_display(actual_impl) : NULL;
                    if (actual_text != NULL) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, rs,
                            "Party role slot '%s' requires ability '%s', but subject-bound roles implement '%s' instead.\n"
                            "Reason:\n"
                            "- consumer path is party role slot '%s'\n"
                            "- no subject-bound role satisfies required ability '%s'\n"
                            "- expected type args are '%s'\n"
                            "- actual implementation is '%s'\n"
                            "- actual type args are '%s'\n"
                            "Fix:\n"
                            "- implement '%s' on a subject-bound role\n"
                            "- or change the role slot contract",
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            actual_text,
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name,
                            actual_text,
                            actual_text,
                            required_text != NULL ? required_text : ability_name);
                    } else {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, rs,
                            "Party role slot '%s' requires ability '%s', but no subject-bound role implements it.\n"
                            "Reason:\n"
                            "- consumer path is party role slot '%s'\n"
                            "- no subject-bound role satisfies required ability '%s'\n"
                            "- expected type args are '%s'\n"
                            "- no matching implementation was found for '%s'\n"
                            "Fix:\n"
                            "- implement '%s' on a subject-bound role\n"
                            "- or change the role slot contract",
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name);
                    }
                    free(actual_text);
                }
                free(required_text);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            party_resolve_type_ref(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        type_check_func_decl(node->data.party_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_module_path = prev_module_path;
    return !ctx->has_error;
}
