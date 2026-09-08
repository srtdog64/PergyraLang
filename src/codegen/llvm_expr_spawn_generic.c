/*
 * LLVM generic calls consume MIR-owned actual/formal bindings.
 */
#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"
#include <stdlib.h>
#include <string.h>

#include "llvm_boundary_slot_param.h"
#include "llvm_backend_generic.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_mir_signature.h"
#include "../compiler/mir_generic_method_specialization.h"
#include "../compiler/mir_type_helpers.h"
#include "../parser/ast_api.h"

LLVMFuncEntry *
llvm_resolve_callee_entry(LLVMGenCtx *ctx, ASTNode *call,
                          const char *callee_name,
                          const MIRRoutine **bound_routine)
{
    if (bound_routine != NULL) *bound_routine = NULL;
    const LLVMGenericTemplate *generic_template =
        llvm_lookup_generic_template_entry(ctx, callee_name);
    if (generic_template == NULL)
        return llvm_lookup_function(ctx, callee_name);

    const MIRProgram *mir = llvm_active_mir_identity(ctx);
    const MIRRoutine *routine = generic_template->routine;
    const MIRGenericMethodSpecializationFact *fact =
        mir_generic_method_specialization_for_call(mir, ast_node_stable_id(call));
    if (routine == NULL || fact == NULL || mir == NULL
        || fact->method_routine_index >= mir->routine_count
        || fact->caller_routine_index >= mir->routine_count
        || &mir->routines[fact->method_routine_index] != routine
        || ctx->current_mir_routine == NULL
        || mir_routine_source_syntax_id(ctx->current_mir_routine) !=
            mir_routine_source_syntax_id(&mir->routines[fact->caller_routine_index])
        || fact->owner_name == NULL || fact->owner_name[0] != '\0'
        || fact->binding_count != routine->generic_param_count
        || fact->binding_count > MAX_TYPE_SUBST) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM generic call '%s' missing generic call specialization fact",
            callee_name != NULL ? callee_name : "(anonymous)");
        return NULL;
    }

    /* A nested specialization may shadow its caller's formal names. Resolve
     * the actuals in the caller, then install an isolated callee substitution
     * frame and restore the complete frame, not just its length. */
    LLVMTypeSubst saved[MAX_TYPE_SUBST];
    int saved_count = ctx->type_subst_count;
    memcpy(saved, ctx->type_subst, sizeof(saved));
    const char *outer_formals[MAX_TYPE_SUBST];
    const char *outer_actuals[MAX_TYPE_SUBST];
    char *actuals[MAX_TYPE_SUBST] = {0};
    char *symbol = NULL;
    char **param_names = NULL;
    char *return_name = NULL;
    LLVMFuncEntry *result = NULL;
    bool frame_installed = false;
    for (int i = 0; i < saved_count; i++) {
        outer_formals[i] = saved[i].param_name;
        outer_actuals[i] = saved[i].type_name;
    }
    for (size_t i = 0; i < fact->binding_count; i++) {
        actuals[i] = mir_substitute_type_name_text(fact->actual_type_names[i],
            outer_formals, outer_actuals, (size_t)saved_count);
        if (actuals[i] == NULL) goto memory_error;
    }
    symbol = mir_generic_specialization_symbol(
        fact->owner_name, fact->method_name, actuals, fact->binding_count);
    if (symbol == NULL) goto memory_error;

    ctx->type_subst_count = 0;
    frame_installed = true;
    for (size_t i = 0; i < fact->binding_count; i++) {
        LLVMTypeRef concrete = pergyra_type_to_llvm(ctx, actuals[i]);
        if (concrete == NULL || ctx->has_error) goto cleanup;
        ctx->type_subst[i].param_name = fact->generic_param_names[i];
        ctx->type_subst[i].type_name = actuals[i];
        ctx->type_subst[i].llvm_type = concrete;
        ctx->type_subst_count++;
    }
    if (!llvm_mir_routine_signature_metadata_complete(ctx, routine, call,
            "MIR-only LLVM path missing generic function signature metadata for '%s'",
            "MIR-only LLVM path missing generic function return type-name metadata for '%s'",
            "MIR-only LLVM path missing generic function parameter type-name metadata for '%s'"))
        goto cleanup;

    MIRRoutine specialized = *routine;
    specialized.name = symbol;
    size_t pc = llvm_mir_routine_param_count(routine);
    param_names = calloc(pc > 0 ? pc : 1, sizeof(char *));
    if (param_names == NULL) goto memory_error;
    specialized.param_type_names = param_names;
    const char *const *formals = (const char *const *)fact->generic_param_names;
    const char *const *concretes = (const char *const *)actuals;
    for (size_t i = 0; i < pc; i++) {
        param_names[i] = mir_substitute_type_name_text(
            llvm_mir_routine_param_type_name(routine, i),
            formals, concretes, fact->binding_count);
        if (param_names[i] == NULL) goto memory_error;
    }
    const char *raw_return = llvm_mir_routine_return_type_name(routine);
    if (raw_return != NULL) {
        return_name = mir_substitute_type_name_text(
            raw_return, formals, concretes, fact->binding_count);
        if (return_name == NULL) goto memory_error;
        specialized.return_type_name = return_name;
    }
    /* Call-boundary consumers need the same nominal/carriage signature as
     * the emitted body. Its context-owned view outlives nested argument
     * emission; no LLVM storage-type reconstruction is involved. */
    if (bound_routine != NULL) {
        MIRRoutine *view = pgy_arena_alloc(&ctx->scratch, sizeof(*view));
        if (view == NULL) goto memory_error;
        *view = specialized;
        view->name = pgy_arena_strdup(&ctx->scratch, symbol);
        view->param_type_names = pgy_arena_calloc(&ctx->scratch,
            (pc > 0 ? pc : 1) * sizeof(char *));
        if (view->name == NULL || view->param_type_names == NULL) goto memory_error;
        for (size_t i = 0; i < pc; i++) {
            view->param_type_names[i] = pgy_arena_strdup(&ctx->scratch, param_names[i]);
            if (view->param_type_names[i] == NULL) goto memory_error;
        }
        if (return_name != NULL) {
            view->return_type_name = pgy_arena_strdup(&ctx->scratch, return_name);
            if (view->return_type_name == NULL) goto memory_error;
        }
        *bound_routine = view;
    }
    if (llvm_mono_already_emitted(ctx, symbol)) {
        result = llvm_lookup_function(ctx, symbol);
        goto cleanup;
    }
    const MIRCallableSig *return_callable =
        llvm_mir_routine_return_callable_sig(routine);
    LLVMTypeRef ret = return_callable != NULL
        ? llvm_mir_callable_sig_to_llvm(ctx, return_callable)
        : return_name != NULL ? pergyra_type_to_llvm(ctx, return_name)
        : ctx->type_void;
    if (ret == NULL || ctx->has_error) goto cleanup;
    LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
        (pc > 0 ? pc * 2 : 1) * sizeof(LLVMTypeRef));
    if (ptypes == NULL) goto memory_error;
    size_t emitted_count = 0;
    for (size_t i = 0; i < pc; i++) {
        const MIRCallableSig *callable =
            llvm_mir_routine_param_callable_sig(routine, i);
        MIRParamResourceKind resource = MIR_PARAM_RESOURCE_NONE;
        const char *inner = llvm_mir_boundary_resource_inner_name(
            ctx, &specialized, i, &resource);
        LLVMTypeRef pt = callable != NULL
            ? llvm_mir_callable_sig_to_llvm(ctx, callable)
            : pergyra_type_to_llvm(ctx, param_names[i]);
        if (pt == NULL || ctx->has_error) goto cleanup;
        if (inner != NULL || llvm_mir_routine_param_passes_indirect(routine, i)
            || (callable == NULL
                && llvm_type_name_uses_pointer_self(ctx, param_names[i])))
            pt = LLVMPointerType(pt, 0);
        if (llvm_mir_routine_param_carriage(routine, i) == MIR_PARAM_CARRIAGE_VALUE_RESULT)
            pt = LLVMPointerType(pt, 0);
        ptypes[emitted_count++] = pt;
        if (inner != NULL && resource == MIR_PARAM_RESOURCE_SECURE_SLOT)
            ptypes[emitted_count++] = llvm_secure_token_type(ctx, inner);
    }
    LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)emitted_count, 0);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, symbol, ft);
    llvm_register_function(ctx, symbol, fn, ft, ret);
    llvm_register_mono(ctx, symbol);
    if (llvm_emit_func_from_mir(&specialized, ctx) != NULL && !ctx->has_error)
        result = llvm_lookup_function(ctx, symbol);
    goto cleanup;

memory_error:
    llvm_set_mir_memory_exhausted(ctx,
        "LLVM generic specialization binding allocation failed for '%s'", callee_name);
cleanup:
    if (frame_installed) {
        memcpy(ctx->type_subst, saved, sizeof(saved));
        ctx->type_subst_count = saved_count;
    }
    if (param_names != NULL)
        for (size_t i = 0; i < routine->param_count; i++) free(param_names[i]);
    free(param_names);
    free(return_name);
    free(symbol);
    for (size_t i = 0; i < fact->binding_count; i++) free(actuals[i]);
    return result;
}

#endif
