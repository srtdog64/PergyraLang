#ifdef PGY_LLVM_ENABLED
/*
 * MIR-owned generic method specialization for the LLVM backend.
 */

#include "llvm_generic_method_specialization.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_generic_method_specialization.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_internal.h"

static bool
llvm_generic_method_fact_matches_routine(
    const MIRProgram *mir,
    const MIRGenericMethodSpecializationFact *fact,
    const MIRRoutine *routine)
{
    return mir != NULL && fact != NULL && routine != NULL
        && fact->method_routine_index < mir->routine_count
        && &mir->routines[fact->method_routine_index] == routine;
}

static bool
llvm_generic_method_fact_is_first_symbol(
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

static const char *
llvm_generic_method_bound_type_name(
    const MIRGenericMethodSpecializationFact *fact,
    const char *type_name)
{
    if (fact == NULL || type_name == NULL)
        return type_name;
    for (size_t i = 0; i < fact->binding_count; i++) {
        if (strcmp(fact->generic_param_names[i], type_name) == 0)
            return fact->actual_type_names[i];
    }
    return type_name;
}

static bool
llvm_push_generic_method_fact_substitutions(
    LLVMGenCtx *ctx,
    const MIRGenericMethodSpecializationFact *fact)
{
    if (ctx == NULL || fact == NULL
        || fact->binding_count > MAX_TYPE_SUBST
        || ctx->type_subst_count > (int)(MAX_TYPE_SUBST - fact->binding_count)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR generic method specialization substitution capacity exceeded");
        return false;
    }

    for (size_t i = 0; i < fact->binding_count; i++) {
        LLVMTypeRef concrete =
            pergyra_type_to_llvm(ctx, fact->actual_type_names[i]);
        if (concrete == NULL || ctx->has_error)
            return false;
        ctx->type_subst[ctx->type_subst_count].param_name =
            fact->generic_param_names[i];
        ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
        ctx->type_subst[ctx->type_subst_count].type_name =
            pergyra_strdup(fact->actual_type_names[i]);
        if (ctx->type_subst[ctx->type_subst_count].type_name == NULL) {
            llvm_set_mir_memory_exhausted(ctx,
                "LLVM generic method specialization type-name allocation failed");
            return false;
        }
        ctx->type_subst_count++;
    }
    return true;
}

static bool
llvm_register_one_generic_method_specialization(
    LLVMGenCtx *ctx,
    const char *host_name,
    LLVMTypeRef host_type,
    bool pointer_self,
    const MIRDeclMethod *method_meta,
    const MIRRoutine *method_routine,
    const MIRGenericMethodSpecializationFact *fact)
{
    size_t user_param_count = 0;
    size_t param_count = llvm_mir_routine_param_count(method_routine);
    const char *return_type_name =
        llvm_mir_routine_return_type_name(method_routine);
    LLVMTypeRef return_type;
    LLVMTypeRef *param_types;
    size_t output_index = 1;
    int saved_subst = ctx->type_subst_count;

    if (!llvm_push_generic_method_fact_substitutions(ctx, fact)) {
        llvm_type_subst_restore_owned(ctx, saved_subst);
        return false;
    }

    for (size_t i = 0; i < param_count; i++) {
        if (!llvm_param_is_implicit_self(
                llvm_mir_routine_param(method_routine, i)))
            user_param_count++;
    }
    param_types = pgy_arena_calloc(&ctx->scratch,
        (user_param_count + 1) * sizeof(LLVMTypeRef));
    if (param_types == NULL) {
        llvm_type_subst_restore_owned(ctx, saved_subst);
        llvm_set_mir_memory_exhausted(ctx,
            "LLVM generic method specialization parameter allocation failed for '%s'",
            fact->specialized_name);
        return false;
    }
    param_types[0] = pointer_self ? LLVMPointerType(host_type, 0) : host_type;

    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = llvm_mir_routine_param(method_routine, i);
        const char *raw_type_name =
            llvm_mir_routine_param_type_name(method_routine, i);
        const char *bound_type_name =
            llvm_generic_method_bound_type_name(fact, raw_type_name);
        LLVMTypeRef param_type;

        if (llvm_param_is_implicit_self(param))
            continue;
        if (raw_type_name == NULL) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            llvm_set_mir_inventory_missing(ctx,
                "MIR generic method specialization parameter type is missing for '%s'",
                fact->specialized_name);
            return false;
        }
        param_type = pergyra_type_to_llvm(ctx, raw_type_name);
        if (param_type == NULL || ctx->has_error) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
        if (llvm_mir_routine_param_passes_indirect(method_routine, i)
            || llvm_type_name_uses_pointer_self(ctx, bound_type_name))
            param_type = LLVMPointerType(param_type, 0);
        if (llvm_mir_routine_param_carriage(method_routine, i)
            == MIR_PARAM_CARRIAGE_VALUE_RESULT)
            param_type = LLVMPointerType(param_type, 0);
        param_types[output_index++] = param_type;
    }

    return_type = return_type_name != NULL
        ? pergyra_type_to_llvm(ctx, return_type_name)
        : ctx->type_void;
    if (return_type == NULL || ctx->has_error) {
        llvm_type_subst_restore_owned(ctx, saved_subst);
        return false;
    }

    {
        LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types,
            (unsigned)(user_param_count + 1), 0);
        LLVMValueRef function = LLVMAddFunction(ctx->module,
            fact->specialized_name, function_type);
        llvm_register_function(ctx, fact->specialized_name, function,
            function_type, return_type);
        llvm_set_function_flags(ctx, fact->specialized_name,
            llvm_mir_decl_method_is_action_like(method_meta),
            llvm_mir_decl_method_is_action_like(method_meta)
                && user_param_count == 0);
    }

    llvm_type_subst_restore_owned(ctx, saved_subst);
    (void)host_name;
    return !ctx->has_error;
}

bool
llvm_register_generic_method_specializations(
    LLVMGenCtx *ctx,
    const char *host_name,
    LLVMTypeRef host_type,
    bool pointer_self,
    const MIRDeclMethod *method_meta,
    const MIRRoutine *method_routine)
{
    const MIRProgram *mir = llvm_active_mir_identity(ctx);

    if (ctx == NULL || host_name == NULL || host_type == NULL
        || method_meta == NULL || method_routine == NULL || mir == NULL)
        return false;

    for (size_t i = 0; i < mir_generic_method_specialization_count(mir); i++) {
        const MIRGenericMethodSpecializationFact *fact =
            mir_generic_method_specialization_at(mir, i);
        if (!llvm_generic_method_fact_matches_routine(
                mir, fact, method_routine)
            || !llvm_generic_method_fact_is_first_symbol(mir, i, fact))
            continue;
        if (!llvm_register_one_generic_method_specialization(ctx, host_name,
                host_type, pointer_self, method_meta, method_routine, fact))
            return false;
    }
    return true;
}

bool
llvm_emit_generic_method_specialization_bodies(
    LLVMGenCtx *ctx,
    const MIRRoutine *method_routine)
{
    const MIRProgram *mir = llvm_active_mir_identity(ctx);

    if (ctx == NULL || method_routine == NULL || mir == NULL)
        return false;

    for (size_t i = 0; i < mir_generic_method_specialization_count(mir); i++) {
        const MIRGenericMethodSpecializationFact *fact =
            mir_generic_method_specialization_at(mir, i);
        const char *owner_name;
        size_t owner_len;
        MIRRoutine specialized;
        int saved_subst;

        if (!llvm_generic_method_fact_matches_routine(
                mir, fact, method_routine)
            || !llvm_generic_method_fact_is_first_symbol(mir, i, fact))
            continue;

        owner_name = llvm_mir_routine_owner_name(method_routine);
        owner_len = owner_name != NULL ? strlen(owner_name) : 0;
        if (owner_len == 0
            || strncmp(fact->specialized_name, owner_name, owner_len) != 0
            || fact->specialized_name[owner_len] != '_') {
            llvm_set_mir_inventory_missing(ctx,
                "MIR generic method specialization symbol does not match owner '%s'",
                owner_name != NULL ? owner_name : "(anonymous-owner)");
            return false;
        }

        saved_subst = ctx->type_subst_count;
        if (!llvm_push_generic_method_fact_substitutions(ctx, fact)) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
        specialized = *method_routine;
        specialized.name = fact->specialized_name + owner_len + 1;
        if (llvm_emit_func_from_mir(&specialized, ctx) == NULL
            || ctx->has_error) {
            llvm_type_subst_restore_owned(ctx, saved_subst);
            return false;
        }
        llvm_type_subst_restore_owned(ctx, saved_subst);
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
