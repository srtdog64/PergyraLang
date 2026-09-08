/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared C backend generic call binding queries.
 */

#include "transpiler_generic_binding_query.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../compiler/mir_generic_method_specialization.h"
#include "../compiler/mir_type_helpers.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_type_render.h"

/* Binding selection is sealed by MIR. Explicit/default/inferred actuals
 * share this path; no AST parameter/argument inference remains in C. */
bool
transpiler_generic_call_bindings_from_mir(TranspilerCtx *ctx,
                                         ASTNode *decl,
                                         ASTNode *call,
                                         GenericBindingEntry *bindings,
                                         size_t *binding_count)
{
    if (ctx == NULL || decl == NULL || call == NULL
        || bindings == NULL || binding_count == NULL)
        return false;
    const MIRProgram *mir = transpiler_active_mir_identity(ctx);
    const MIRRoutine *routine = transpiler_find_mir_function(ctx, decl);
    const MIRGenericMethodSpecializationFact *fact =
        mir_generic_method_specialization_for_call(mir, ast_node_stable_id(call));
    const char *outer_formals[MAX_GENERIC_BINDINGS];
    const char *outer_actuals[MAX_GENERIC_BINDINGS];

    if (mir == NULL || routine == NULL || fact == NULL
        || fact->method_routine_index >= mir->routine_count
        || fact->caller_routine_index >= mir->routine_count
        || &mir->routines[fact->method_routine_index] != routine
        || ctx->active_mir_routine == NULL
        || transpiler_mir_routine_source_syntax_id(ctx->active_mir_routine) !=
            transpiler_mir_routine_source_syntax_id(
                &mir->routines[fact->caller_routine_index])
        || fact->owner_name == NULL || fact->owner_name[0] != '\0'
        || fact->binding_count == 0
        || fact->binding_count != routine->generic_param_count
        || fact->binding_count > MAX_GENERIC_BINDINGS
        || ctx->generic_binding_count < 0
        || ctx->generic_binding_count > MAX_GENERIC_BINDINGS)
        return false;

    for (int i = 0; i < ctx->generic_binding_count; i++) {
        outer_formals[i] = ctx->generic_bindings[i].name;
        outer_actuals[i] = ctx->generic_bindings[i].concrete_type;
    }
    memset(bindings, 0, sizeof(*bindings) * fact->binding_count);
    for (size_t i = 0; i < fact->binding_count; i++) {
        if (fact->generic_param_names[i] == NULL
            || routine->generic_param_names[i] == NULL
            || strcmp(fact->generic_param_names[i],
                routine->generic_param_names[i]) != 0)
            return false;
        char *actual = mir_substitute_type_name_text(fact->actual_type_names[i],
            outer_formals, outer_actuals, (size_t)ctx->generic_binding_count);
        if (actual == NULL)
            return false;
        bool copied = pergyra_str_copy(bindings[i].name,
                sizeof(bindings[i].name), fact->generic_param_names[i])
            && pergyra_str_copy(bindings[i].concrete_type,
                sizeof(bindings[i].concrete_type), actual);
        free(actual);
        if (!copied)
            return false;
    }
    *binding_count = fact->binding_count;
    return true;
}

char *
transpiler_generic_type_name_with_bindings(const char *type_name,
                                          const GenericBindingEntry *bindings,
                                          size_t binding_count)
{
    const char *formals[MAX_GENERIC_BINDINGS];
    const char *actuals[MAX_GENERIC_BINDINGS];
    if (type_name == NULL || bindings == NULL || binding_count > MAX_GENERIC_BINDINGS)
        return NULL;
    for (size_t i = 0; i < binding_count; i++) {
        formals[i] = bindings[i].name;
        actuals[i] = bindings[i].concrete_type;
    }
    return mir_substitute_type_name_text(type_name, formals, actuals, binding_count);
}

char *
transpiler_generic_call_return_type_from_mir(TranspilerCtx *ctx,
                                           ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t count = 0;
    if (!transpiler_generic_call_bindings_from_mir(ctx, decl, call, bindings, &count))
        return NULL;
    const MIRRoutine *routine = transpiler_find_mir_function(ctx, decl);
    return transpiler_generic_type_name_with_bindings(
        transpiler_mir_routine_return_type_name(routine), bindings, count);
}

TranspilerGenericBindingSnapshot
transpiler_generic_binding_snapshot(TranspilerCtx *ctx)
{
    TranspilerGenericBindingSnapshot snapshot;

    snapshot.binding_count = ctx != NULL ? ctx->generic_binding_count : 0;
    return snapshot;
}

void
transpiler_generic_binding_restore(
    TranspilerCtx *ctx,
    TranspilerGenericBindingSnapshot snapshot)
{
    if (ctx == NULL)
        return;
    ctx->generic_binding_count = snapshot.binding_count;
}

bool
transpiler_generic_binding_push_entries(
    TranspilerCtx *ctx,
    const GenericBindingEntry *bindings,
    size_t binding_count)
{
    if (ctx == NULL || (binding_count > 0 && bindings == NULL)
        || binding_count > MAX_GENERIC_BINDINGS
        || ctx->generic_binding_count
            > (int)(MAX_GENERIC_BINDINGS - binding_count)) {
        return false;
    }

    for (size_t i = 0; i < binding_count; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }
    return true;
}

char *
transpiler_render_type_name_with_bindings(TranspilerCtx *ctx,
                                          ASTNode *type_node,
                                          GenericBindingEntry *bindings,
                                          size_t binding_count)
{
    TranspilerGenericBindingSnapshot snapshot;
    char *result;

    if (ctx == NULL)
        return NULL;

    snapshot = transpiler_generic_binding_snapshot(ctx);
    for (size_t i = 0;
        i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS;
        i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    result = render_type_name_in_ctx(ctx, type_node);
    transpiler_generic_binding_restore(ctx, snapshot);
    return result;
}
