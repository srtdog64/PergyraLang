/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Local name rule owner (docs/206 section 3, BindingIdentityScope.v): a let,
 * destructure, for or match binding, or a lambda parameter, may not reuse a
 * name that a binding in an enclosing local scope already holds, up to and
 * including the parameters of the named function. A lambda body sees the
 * enclosing function's locals, so the walk crosses a lambda's scope and stops
 * at a named function's. Sibling scopes and a block that has closed
 * may reuse a name because no point sees both bindings. The same-scope
 * redeclaration check lives with each declaration.
 *
 * The check records an error and returns. The caller still declares the new
 * binding, so later uses resolve to it and not to the outer binding, whose
 * type may differ; the program is already refused.
 */

#include "diag_codes.h"
#include "type_checker_internal.h"

/* A host field (a `shared` or owner field reached by its bare name) is not a
 * local binding, so a local may hide it, as a C# local hides a field. */
static bool
local_name_symbol_is_binding(const Symbol *sym)
{
    return sym != NULL && !sym->is_host_field
        && (sym->kind == SYMBOL_VARIABLE || sym->kind == SYMBOL_SLOT
            || sym->kind == SYMBOL_TOKEN);
}

void
semantic_local_name_rule_check(SemanticContext *ctx, ASTNode *site,
                               const char *name)
{
    if (ctx == NULL || name == NULL || ctx->scope == NULL)
        return;
    for (Scope *s = ctx->scope->parent; s != NULL; s = s->parent) {
        /* Globals and class members are not local bindings. */
        if (s->kind == SCOPE_GLOBAL || s->kind == SCOPE_CLASS)
            return;
        const Symbol *prior = scope_lookup_current(s, name);
        if (local_name_symbol_is_binding(prior)) {
            /* The position is diagnostic provenance only. */
            semantic_error_with_hints(ctx, PGY_CODE_SEM_REDECLARATION,
                PGY_CAUSE_SCOPE_DUPLICATE_SYMBOL,
                PGY_FIX_RENAME_OR_REMOVE_DUPLICATE, site,
                "Local '%s' reuses the name of the binding at line %u in an "
                "enclosing scope; a local name binds once in a function, so "
                "rename one of them",
                name, (unsigned) prior->decl_line);
            return;
        }
        if (s->kind == SCOPE_FUNCTION && !s->is_lambda)
            return;
    }
}
