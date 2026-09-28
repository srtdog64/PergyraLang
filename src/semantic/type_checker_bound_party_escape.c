/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * Bound party escape owner (docs/206 section 1, PartySlotBinding.v
 * scoped_borrow_live / inner_subject_dangles): a bind stores the address of
 * a local subject in the party's slot, so a party this function binds may
 * not leave the function. Its identifier may not be returned, passed as a
 * call argument (which covers spawn and channel sends), copied into another
 * binding, or assigned. A member use (`team.slot.Method()`, `team.hp`) reads
 * the party in place and stays legal.
 *
 * The check is flow-insensitive: the function records the parties it binds
 * and the party identifiers it uses as values, and crosses the two lists when
 * the function ends, so a use written before the bind is refused too.
 */

#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"

static bool
bound_party_grow(void ***items, size_t *count, size_t *capacity, void *item)
{
    if (*count == *capacity) {
        size_t next = *capacity == 0 ? 8 : *capacity * 2;
        void **grown = realloc(*items, next * sizeof(void *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next;
    }
    (*items)[(*count)++] = item;
    return true;
}

static void
bound_party_reset(SemanticContext *ctx)
{
    free(ctx->bound_party_names);
    free(ctx->party_value_uses);
    ctx->bound_party_names = NULL;
    ctx->bound_party_count = 0;
    ctx->bound_party_capacity = 0;
    ctx->party_value_uses = NULL;
    ctx->party_value_use_count = 0;
    ctx->party_value_use_capacity = 0;
}

void
semantic_bound_party_begin_function(SemanticContext *ctx)
{
    if (ctx != NULL)
        bound_party_reset(ctx);
}

void
semantic_bound_party_note_bind(SemanticContext *ctx, const char *party_var)
{
    if (ctx == NULL || party_var == NULL)
        return;
    if (!bound_party_grow((void ***)&ctx->bound_party_names,
            &ctx->bound_party_count, &ctx->bound_party_capacity,
            (void *)party_var)) {
        semantic_error(ctx, NULL, "bound party table allocation failed");
    }
}

static void
bound_party_note_identifier(SemanticContext *ctx, ASTNode *node)
{
    const char *name;
    Symbol *sym;

    if (node == NULL || node->type != AST_IDENTIFIER)
        return;
    name = ast_identifier_name(node);
    sym = name != NULL ? scope_lookup(ctx->scope, name) : NULL;
    if (sym == NULL || sym->kind != SYMBOL_VARIABLE || sym->type == NULL
        || sym->type->name == NULL
        || semantic_find_party_decl_by_name(ctx, sym->type->name) == NULL)
        return;
    if (!bound_party_grow((void ***)&ctx->party_value_uses,
            &ctx->party_value_use_count, &ctx->party_value_use_capacity,
            node)) {
        semantic_error(ctx, node, "party value-use table allocation failed");
    }
}

/* `node` is a value the caller consumes: a let initializer, an assigned
 * value, a returned value, or a call whose arguments the callee receives. */
void
semantic_bound_party_note_value_use(SemanticContext *ctx, ASTNode *node)
{
    if (ctx == NULL || node == NULL)
        return;
    if (node->type == AST_CALL) {
        for (size_t i = 0; i < ast_call_arg_count(node); i++)
            bound_party_note_identifier(ctx, ast_call_argument(node, i));
        return;
    }
    bound_party_note_identifier(ctx, node);
}

void
semantic_bound_party_end_function(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    for (size_t u = 0; u < ctx->party_value_use_count; u++) {
        ASTNode *use = ctx->party_value_uses[u];
        const char *name = ast_identifier_name(use);
        for (size_t b = 0; name != NULL && b < ctx->bound_party_count; b++) {
            if (strcmp(ctx->bound_party_names[b], name) != 0)
                continue;
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_KEEP_HANDLE_LOCAL_OR_PROJECT,
                use,
                "Party '%s' has a slot bound to a local subject, so it cannot "
                "leave this function.\n"
                "Reason:\n"
                "- a bind stores the subject's address in the slot, and the subject ends with this function\n"
                "- returning, passing, copying or assigning the party would let the slot outlive its subject\n"
                "Fix:\n"
                "- call the slot here (`%s.<slot>.<Method>()`)\n"
                "- or bind the slot in the function that uses the party",
                name, name);
            break;
        }
    }
    bound_party_reset(ctx);
}
