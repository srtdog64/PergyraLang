#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

bool
type_check_roster_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_roster_name(node);
    ASTNode *saved_roster = ctx->current_roster;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t party_count;
    size_t shared_count;
    size_t method_count;

    party_count = ast_roster_party_count(node);
    shared_fields = ast_roster_shared_fields(node, &shared_count);
    methods = ast_roster_methods(node, &method_count);
    ctx->current_roster = node;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ROSTER;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_ROSTER;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_ROSTER_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of roster '%s'", name);
        symbol_destroy(sym);
        ctx->current_roster = saved_roster;
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    GenericParams *generic_params = ast_roster_generic_params(node);
    if (ast_generic_param_count(generic_params) > 0) {
        validate_generic_param_defaults(generic_params, ctx, node, "roster");
    }

    /* Check party slot references */
    for (size_t i = 0; i < party_count; i++) {
        ASTNode *ps = ast_roster_party(node, i);
        const char *party_type = ast_roster_slot_party_type(ps);
        const char *slot_name = ast_roster_slot_name(ps);
        if (party_type != NULL) {
            Symbol *party = scope_lookup(ctx->scope,
                party_type);
            if (party == NULL || party->kind != SYMBOL_PARTY) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
                    PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_DECLARE_OR_IMPORT_TYPE,
                    ps,
                    "Party type '%s' not found for roster slot '%s'.\n"
                    "Reason:\n"
                    "- roster slot '%s' must reference a visible party declaration\n"
                    "- no party named '%s' is visible at this point\n"
                    "Fix:\n"
                    "- declare party '%s'\n"
                    "- or import/export the module that defines it",
                    party_type,
                    slot_name,
                    slot_name,
                    party_type,
                    party_type);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (ast_party_shared_type(shared) != NULL)
            domain_resolve_shared_type(shared, ctx);
        if (ast_party_shared_initializer(shared) != NULL)
            type_check_expression(ast_party_shared_initializer(shared), ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < method_count; i++) {
        type_check_func_decl(methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_roster = saved_roster;
    return !ctx->has_error;
}
