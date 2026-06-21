#include "transpiler_parallel_capture.h"

#include <stdbool.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_analysis.h"
#include "../semantic/diag_codes.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_symbols.h"

static const char *
transpiler_current_local_type_name(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || ctx->current_func_decl == NULL)
        return NULL;
    return transpiler_find_local_type_name(ctx, ctx->current_func_decl, name);
}

static bool
transpiler_current_local_callable_capture(
    TranspilerCtx *ctx,
    const char *name,
    const char *type_name,
    TranspilerParallelCallableCapture *capture)
{
    const MIRRoutine *routine;
    const MIRSourceLocalType *fact;

    if (capture != NULL) {
        capture->is_callable = false;
        capture->return_type_name = NULL;
        capture->param_count = 0;
        capture->param_type_names = NULL;
    }
    if (ctx == NULL || name == NULL || type_name == NULL
        || strncmp(type_name, "func(", 5) != 0) {
        return true;
    }
    if (ctx->current_func_decl == NULL)
        return true;

    routine = transpiler_find_mir_function(ctx, ctx->current_func_decl);
    fact = routine != NULL
        ? mir_routine_source_local_type_fact(routine, name)
        : NULL;
    if (fact == NULL || !fact->is_callable) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing parallel capture callable source-local fact for '%s'",
                name);
            return false;
        }
        return true;
    }
    if (fact->callable_return_type_name == NULL
        || (fact->callable_param_count > 0
            && fact->callable_param_type_names == NULL)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing parallel capture callable signature metadata for '%s'",
            name);
        return false;
    }
    for (size_t i = 0; i < fact->callable_param_count; i++) {
        if (fact->callable_param_type_names[i] == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing parallel capture callable parameter type-name metadata for '%s'",
                name);
            return false;
        }
    }
    if (capture != NULL) {
        capture->is_callable = true;
        capture->return_type_name = fact->callable_return_type_name;
        capture->param_count = fact->callable_param_count;
        capture->param_type_names = fact->callable_param_type_names;
    }
    return true;
}

static bool
transpiler_parallel_capture_has_name(char names[MAX_SLOT_VARS][64],
                                     int count,
                                     const char *name)
{
    if (name == NULL)
        return false;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void
transpiler_parallel_add_capture_name(TranspilerCtx *ctx,
                                     const char *name,
                                     char slot_names[MAX_SLOT_VARS][64],
                                     int *slot_count,
                                     char typed_names[MAX_SLOT_VARS][64],
                                     TranspilerParallelCallableCapture typed_callables[MAX_SLOT_VARS],
                                     int *typed_count)
{
    TranspilerParallelCallableCapture callable_capture = { 0 };

    if (ctx == NULL || name == NULL || name[0] == '\0'
        || strcmp(name, "self") == 0) {
        return;
    }
    if (ctx->backend_error != NULL)
        return;

    if (is_slot_var(ctx, name)) {
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && slot_count != NULL) {
            if (*slot_count >= MAX_SLOT_VARS) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    "parallel capture registry exceeded MAX_SLOT_VARS while capturing Slot<T> local '%s'; split the parallel block or pass values through an explicit boundary",
                    name);
                return;
            }
            pergyra_str_copy(slot_names[*slot_count],
                sizeof(slot_names[*slot_count]), name);
            (*slot_count)++;
        }
        return;
    }

    if (lookup_typed_entry(ctx, name) != NULL) {
        const char *type_name = lookup_typed_var(ctx, name);
        if (type_name == NULL || strcmp(type_name, "Unknown") == 0) {
            type_name = transpiler_current_local_type_name(ctx, name);
            if (type_name != NULL && type_name[0] != '\0'
                && strcmp(type_name, "Unknown") != 0) {
                register_typed_var(ctx, name, type_name);
            }
        }
        if (!transpiler_current_local_callable_capture(
                ctx, name, type_name, &callable_capture)) {
            return;
        }
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL) {
            if (*typed_count >= MAX_SLOT_VARS) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    "parallel capture registry exceeded MAX_SLOT_VARS while capturing local '%s'; split the parallel block or pass values through an explicit boundary",
                    name);
                return;
            }
            pergyra_str_copy(typed_names[*typed_count],
                sizeof(typed_names[*typed_count]), name);
            if (typed_callables != NULL)
                typed_callables[*typed_count] = callable_capture;
            (*typed_count)++;
        }
    } else {
        const char *type_name = transpiler_current_local_type_name(ctx, name);
        if (type_name != NULL && type_name[0] != '\0'
            && strcmp(type_name, "Unknown") != 0
            && !transpiler_parallel_capture_has_name(slot_names,
                    slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                    typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL) {
            if (*typed_count >= MAX_SLOT_VARS) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    "parallel capture registry exceeded MAX_SLOT_VARS while capturing inferred local '%s'; split the parallel block or pass values through an explicit boundary",
                    name);
                return;
            }
            register_typed_var(ctx, name, type_name);
            if (!transpiler_current_local_callable_capture(
                    ctx, name, type_name, &callable_capture)) {
                return;
            }
            pergyra_str_copy(typed_names[*typed_count],
                sizeof(typed_names[*typed_count]), name);
            if (typed_callables != NULL)
                typed_callables[*typed_count] = callable_capture;
            (*typed_count)++;
        }
    }
}

void
transpiler_parallel_collect_stmt_captures(ASTNode *node,
                                          TranspilerCtx *ctx,
                                          char slot_names[MAX_SLOT_VARS][64],
                                          int *slot_count,
                                          char typed_names[MAX_SLOT_VARS][64],
                                          TranspilerParallelCallableCapture typed_callables[MAX_SLOT_VARS],
                                          int *typed_count)
{
    if (node == NULL || ctx == NULL)
        return;

    for (int i = 0; i < ctx->slot_var_count; i++) {
        const char *name = ctx->slot_vars[i].name;
        if (ast_contains_free_identifier_ref(node, name)) {
            transpiler_parallel_add_capture_name(ctx, name, slot_names,
                slot_count, typed_names, typed_callables, typed_count);
        }
    }

    for (int i = 0; i < ctx->typed_var_count; i++) {
        const char *name = ctx->typed_vars[i].name;
        if (ast_contains_free_identifier_ref(node, name)) {
            transpiler_parallel_add_capture_name(ctx, name, slot_names,
                slot_count, typed_names, typed_callables, typed_count);
        }
    }
}
