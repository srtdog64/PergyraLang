#include "type_checker_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_module_contract_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

bool
type_check_party_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_party_name(node);
    const char *prev_module_path = ctx->current_module_path;
    ASTNode *saved_party = ctx->current_party;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t role_count;
    size_t shared_count;
    size_t method_count;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;
    ctx->current_party = node;
    role_count = ast_party_role_count(node);
    shared_fields = ast_party_shared_fields(node, &shared_count);
    methods = ast_party_methods(node, &method_count);

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
        ctx->current_party = saved_party;
        ctx->current_module_path = prev_module_path;
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    GenericParams *generic_params = ast_party_generic_params(node);
    if (ast_generic_param_count(generic_params) > 0) {
        validate_generic_param_defaults(generic_params, ctx, node, "party");
    }

    /* Check role slot ability references */
    for (size_t i = 0; i < role_count; i++) {
        ASTNode *rs = ast_party_role(node, i);
        const char *slot_name = ast_role_slot_name(rs);
        size_t ability_count = ast_role_slot_required_ability_count(rs);

        /* dyn slots require at least one ability for vtable dispatch */
        if (ast_role_slot_is_dynamic(rs) && ability_count == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ROLE_CONTRACT_INVALID, PGY_CAUSE_ROLE_CONTRACT, PGY_FIX_ALIGN_ROLE_IMPL_WITH_ABILITY, rs,
                "Dynamic role slot '%s' requires at least one ability type",
                slot_name);
        }

        for (size_t j = 0; j < ability_count; j++) {
            ASTNode *ab_type = ast_role_slot_required_ability(rs, j);
            if (ast_type_name(ab_type) != NULL) {
                const char *ability_name = ast_type_name(ab_type);
                char *required_text = ability_ref_display(ab_type);
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    rs,
                    slot_name != NULL ? slot_name : "<role-slot>",
                    ab_type,
                    "party role slot ability consumer lookup");
                if (ctx->program_root != NULL) {
                    ASTNode *ability_decl = resolve_required_ability_decl(
                        ab_type, rs, ctx, "party role slot",
                        slot_name);
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
                            slot_name,
                            required_text != NULL ? required_text : ability_name,
                            actual_text,
                            slot_name,
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
                            slot_name,
                            required_text != NULL ? required_text : ability_name,
                            slot_name,
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
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (ast_party_shared_type(shared) != NULL)
            domain_lookup_shared_type_metadata(shared, ctx);
        if (ast_party_shared_initializer(shared) != NULL)
            type_check_expression(ast_party_shared_initializer(shared), ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < method_count; i++) {
        type_check_func_decl(methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_party = saved_party;
    ctx->current_module_path = prev_module_path;
    return !ctx->has_error;
}
