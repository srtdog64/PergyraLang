/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR-owned generic method specialization emission for the C backend.
 */

#include "transpiler_generic_method_specialization_emit.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_generic_method_specialization.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_func_emit.h"

static bool
transpiler_generic_method_fact_matches_routine(
    const MIRProgram *mir,
    const MIRGenericMethodSpecializationFact *fact,
    const MIRRoutine *routine)
{
    return mir != NULL && fact != NULL && routine != NULL
        && fact->method_routine_index < mir->routine_count
        && &mir->routines[fact->method_routine_index] == routine;
}

static bool
transpiler_generic_method_fact_is_first_symbol(
    const MIRProgram *mir,
    size_t fact_index,
    const MIRGenericMethodSpecializationFact *fact)
{
    if (mir == NULL || fact == NULL || fact->specialized_name == NULL)
        return false;
    for (size_t i = 0; i < fact_index; i++) {
        const MIRGenericMethodSpecializationFact *prior =
            mir_generic_method_specialization_at(mir, i);
        if (prior != NULL && prior->specialized_name != NULL
            && strcmp(prior->specialized_name, fact->specialized_name) == 0)
            return false;
    }
    return true;
}

static bool
transpiler_push_generic_method_fact_bindings(
    TranspilerCtx *ctx,
    const MIRGenericMethodSpecializationFact *fact)
{
    if (ctx == NULL || fact == NULL
        || fact->binding_count > MAX_GENERIC_BINDINGS
        || ctx->generic_binding_count
            > (int)(MAX_GENERIC_BINDINGS - fact->binding_count)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C backend generic method specialization binding capacity exceeded");
        return false;
    }

    for (size_t i = 0; i < fact->binding_count; i++) {
        GenericBindingEntry *binding =
            &ctx->generic_bindings[ctx->generic_binding_count];
        if (!pergyra_str_copy(binding->name, sizeof(binding->name),
                fact->generic_param_names[i])
            || !pergyra_str_copy(binding->concrete_type,
                sizeof(binding->concrete_type), fact->actual_type_names[i])) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend generic method specialization binding is too long for '%s'",
                fact->specialized_name != NULL
                    ? fact->specialized_name : "(anonymous-specialization)");
            return false;
        }
        ctx->generic_binding_count++;
    }
    return true;
}

bool
transpiler_emit_generic_method_specialization_forwards(
    TranspilerCtx *ctx,
    const char *host_name,
    const MIRDeclMethod *method_meta,
    const MIRRoutine *method_routine,
    bool pointer_self)
{
    const MIRProgram *mir = transpiler_active_mir_identity(ctx);

    if (ctx == NULL || host_name == NULL || method_meta == NULL
        || method_routine == NULL || mir == NULL)
        return false;

    for (size_t i = 0; i < mir_generic_method_specialization_count(mir); i++) {
        const MIRGenericMethodSpecializationFact *fact =
            mir_generic_method_specialization_at(mir, i);
        TranspilerGenericBindingSnapshot snapshot;

        if (!transpiler_generic_method_fact_matches_routine(
                mir, fact, method_routine)
            || !transpiler_generic_method_fact_is_first_symbol(mir, i, fact))
            continue;

        snapshot = transpiler_generic_binding_snapshot(ctx);
        if (!transpiler_push_generic_method_fact_bindings(ctx, fact)) {
            transpiler_generic_binding_restore(ctx, snapshot);
            return false;
        }
        emit_hosted_method_forward_decl_from_metadata_named(
            host_name, fact->specialized_name, method_meta, NULL,
            pointer_self, ctx->out, ctx);
        transpiler_generic_binding_restore(ctx, snapshot);
        if (ctx->backend_error != NULL)
            return false;
    }
    return true;
}

bool
transpiler_emit_generic_method_specialization_bodies(
    TranspilerCtx *ctx,
    const MIRRoutine *method_routine)
{
    const MIRProgram *mir = transpiler_active_mir_identity(ctx);

    if (ctx == NULL || method_routine == NULL || mir == NULL)
        return false;

    for (size_t i = 0; i < mir_generic_method_specialization_count(mir); i++) {
        const MIRGenericMethodSpecializationFact *fact =
            mir_generic_method_specialization_at(mir, i);
        TranspilerGenericBindingSnapshot snapshot;

        if (!transpiler_generic_method_fact_matches_routine(
                mir, fact, method_routine)
            || !transpiler_generic_method_fact_is_first_symbol(mir, i, fact))
            continue;

        snapshot = transpiler_generic_binding_snapshot(ctx);
        if (!transpiler_push_generic_method_fact_bindings(ctx, fact)) {
            transpiler_generic_binding_restore(ctx, snapshot);
            return false;
        }
        emit_func_decl_from_mir_named(NULL, method_routine,
            fact->specialized_name, ctx->out, ctx);
        transpiler_generic_binding_restore(ctx, snapshot);
        if (ctx->backend_error != NULL)
            return false;
    }
    return true;
}
