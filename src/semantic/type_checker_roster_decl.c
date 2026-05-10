#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static Type *
roster_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx,
                                                                   type_ref);
}

bool
type_check_roster_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.roster_decl.name;

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
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    if (node->data.roster_decl.generic_params != NULL
        && node->data.roster_decl.generic_params->count > 0) {
        validate_generic_param_defaults(node->data.roster_decl.generic_params,
            ctx, node, "roster");
    }

    /* Check party slot references */
    for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
        ASTNode *ps = node->data.roster_decl.party_slots[i];
        if (ps->data.roster_slot.party_type != NULL) {
            Symbol *party = scope_lookup(ctx->scope,
                ps->data.roster_slot.party_type);
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
                    ps->data.roster_slot.party_type,
                    ps->data.roster_slot.slot_name,
                    ps->data.roster_slot.slot_name,
                    ps->data.roster_slot.party_type,
                    ps->data.roster_slot.party_type);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.roster_decl.shared_count; i++) {
        ASTNode *shared = node->data.roster_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            roster_resolve_type_ref(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.roster_decl.method_count; i++) {
        type_check_func_decl(node->data.roster_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}
