/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend pipeline helpers split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_inventory_host_methods.h"
#include "thread_pool_usage.h"

static void
llvm_pipeline_debug_stage(const char *stage)
{
    if (stage != NULL && getenv("PGY_DEBUG_LLVM_STAGE") != NULL)
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

static bool
llvm_requires_thread_pool(const LLVMGenCtx *ctx)
{
    if (ctx == NULL || ctx->mir == NULL)
        return false;

    return pgy_mir_program_uses_thread_pool(ctx->mir);
}

static bool
llvm_mir_routine_has_instructions(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (routine->blocks[i].instruction_count > 0)
            return true;
    }
    return false;
}

static bool
llvm_register_generic_template_decl(LLVMGenCtx *ctx, ASTNode *func_decl)
{
    if (ctx == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;
    const char *name = ast_declaration_name(func_decl);

    if (name == NULL)
        return false;
    if (llvm_lookup_generic_template(ctx, name) != NULL)
        return true;

    if (ctx->generic_template_count >= ctx->generic_template_capacity) {
        int new_capacity = ctx->generic_template_capacity == 0
            ? 16
            : ctx->generic_template_capacity * 2;
        LLVMGenericTemplate *new_templates =
            realloc(ctx->generic_templates,
                    (size_t)new_capacity * sizeof(LLVMGenericTemplate));
        if (new_templates == NULL) {
            llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_OOM, PGY_CAUSE_LLVM_MEMORY_EXHAUSTED, PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT, "out of memory growing generic_templates");
            return false;
        }
        memset(new_templates + ctx->generic_template_capacity, 0,
               (size_t)(new_capacity - ctx->generic_template_capacity)
                   * sizeof(LLVMGenericTemplate));
        ctx->generic_templates = new_templates;
        ctx->generic_template_capacity = new_capacity;
    }

    ctx->generic_templates[ctx->generic_template_count].name = name;
    ctx->generic_templates[ctx->generic_template_count].ast = func_decl;
    ctx->generic_template_count++;
    return true;
}

void
llvm_emit_main_wrapper(LLVMGenCtx *ctx)
{
    ASTNode *synthetic_executable_func = NULL;
    bool has_top_level_exec = false;
    bool has_main_function = false;
    bool needs_thread_pool = false;

    if (ctx == NULL || ctx->mir == NULL)
        return;

    LLVMFuncEntry *main_user = llvm_lookup_or_declare_function(ctx, "Main", NULL, NULL);
    synthetic_executable_func = mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    has_top_level_exec = ctx->mir->has_top_level_exec;
    has_main_function = ctx->mir->has_main_function;
    needs_thread_pool = llvm_requires_thread_pool(ctx);

    bool has_top_level = has_top_level_exec
        || has_main_function
        || (main_user != NULL);
    if (!has_top_level)
        return;

    LLVMTypeRef main_type = LLVMFunctionType(ctx->type_i32, NULL, 0, 0);
    LLVMFuncEntry *main_entry = llvm_lookup_or_declare_function(ctx, "main", main_type,
                                                                ctx->type_i32);
    LLVMValueRef main_fn = main_entry != NULL ? main_entry->fn : NULL;
    if (main_fn == NULL)
        return;

    LLVMSetLinkage(main_fn, LLVMExternalLinkage);
    ctx->current_function = main_fn;
    ctx->current_ret_type = ctx->type_i32;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        ctx->context, main_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    if (needs_thread_pool) {
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
                                     "pgy_pool_init_export");
        if (init_fn == NULL) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM thread-pool entry requires registered runtime function '%s'",
                "pgy_pool_init_export");
            return;
        }
        LLVMValueRef args[] = { LLVMConstInt(ctx->type_i64, 4, 0) };
        LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                       init_fn->fn, args, 1, "");
    }

    llvm_scope_push(ctx);

    for (int i = 0; i < ctx->event_type_count; i++) {
        LLVMEventTypeEntry *evt = &ctx->event_types[i];
        char fname[256];
        snprintf(fname, sizeof(fname), "%s_INIT", evt->event_name);
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx, fname);
        LLVMValueRef gv = LLVMGetNamedGlobal(ctx->module, evt->event_name);
        if (init_fn == NULL || gv == NULL) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM event initialization requires generated event function '%s' and event storage '%s'",
                fname, evt->event_name);
            llvm_scope_pop(ctx);
            return;
        }
        LLVMValueRef args[] = { gv };
        LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                       init_fn->fn, args, 1, "");
    }

    if (main_user != NULL)
        LLVMBuildCall2(ctx->builder, main_user->fn_type,
                       main_user->fn, NULL, 0, "");

    if (synthetic_executable_func != NULL) {
        LLVMFuncEntry *top_level_entry = llvm_lookup_function(ctx, "__pgy_top_level_exec");
        if (top_level_entry != NULL) {
            LLVMBuildCall2(ctx->builder, top_level_entry->fn_type,
                           top_level_entry->fn, NULL, 0, "");
        } else {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing emitted top-level executable wrapper '__pgy_top_level_exec'");
            llvm_scope_pop(ctx);
            return;
        }
    }

    llvm_scope_pop(ctx);

    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (needs_thread_pool) {
            LLVMFuncEntry *shutdown_fn = llvm_lookup_function(ctx,
                                             "pgy_pool_shutdown_export");
            if (shutdown_fn == NULL) {
                llvm_set_error_at_with_hints(ctx, NULL,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM thread-pool exit requires registered runtime function '%s'",
                    "pgy_pool_shutdown_export");
                return;
            }
            LLVMBuildCall2(ctx->builder, shutdown_fn->fn_type,
                           shutdown_fn->fn, NULL, 0, "");
        }
        LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
    }

    llvm_mark_function_as_used(ctx, "main");
}

LLVMGenResult *
llvm_validate_mir_for_codegen(const MIRProgram *mir)
{
    LLVMMIRRoutineInventory routine_inventory;

    if (mir == NULL) {
        return llvm_result_error_with_hints("MIR program is NULL",
            PGY_CODE_MIR_TOPOLOGY_INVALID,
            PGY_CAUSE_MIR_TOPOLOGY_INVALID,
            PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
    }

    llvm_mir_routine_inventory_from_program(mir, &routine_inventory);
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine = &routine_inventory.routines[i];
        char *topology_error = NULL;

        if (routine->name == NULL) {
            return llvm_result_error_with_hints("MIR routine is missing name",
                PGY_CODE_MIR_TOPOLOGY_INVALID,
                PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
        }

        if (!mir_validate_emission_topology(routine, false, false, &topology_error)) {
            LLVMGenResult *res = topology_error != NULL
                ? llvm_result_error_fmt_with_hints(
                    PGY_CODE_MIR_TOPOLOGY_INVALID,
                    PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                    PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
                    "MIR routine '%s' emission topology invalid: %s",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    topology_error)
                : llvm_result_error_with_hints(
                    "MIR emission topology validation failed",
                    PGY_CODE_MIR_TOPOLOGY_INVALID,
                    PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                    PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING);
            free(topology_error);
            return res;
        }
        free(topology_error);
    }
    return NULL;
}

bool
llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx)
{
    LLVMMIRRoutineInventory routine_inventory;

    if (mir == NULL || ctx == NULL)
        return false;
    llvm_active_routine_inventory(ctx, &routine_inventory);

    /* MIR-only backend entry:
     *
     * function/intent/method bodies lower from MIR routines,
     * and declaration / top-level orchestration state is read from
     * MIR-carried inventory rather than the original HIR program.
     *
     * Remaining debt is not "original HIR dependency" anymore; it is
     * that declaration inventory is still AST-carried inside MIRProgram
     * instead of a dedicated declaration IR.
     */
    llvm_pipeline_debug_stage("emit_program_from_mir:declare_runtime");
    llvm_declare_runtime(ctx);

    llvm_pipeline_debug_stage("emit_program_from_mir:register_decl_items");
    {
        ASTNode **nominal_nodes = NULL;
        size_t nominal_count = 0;

        llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count);
        for (size_t i = 0; i < nominal_count; i++) {
            ASTNode *stmt = nominal_nodes != NULL ? nominal_nodes[i] : NULL;
            if (stmt == NULL)
                continue;
            if (stmt->type != AST_CLASS_DECL
                && stmt->type != AST_ENUM_DECL) {
                continue;
            }
            llvm_register_nominal_decl(ctx, stmt);
        }
    }
    llvm_pipeline_debug_stage("emit_program_from_mir:emit_domain_passes");
    llvm_emit_domain_passes(ctx);

    llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_funcs");
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine = &routine_inventory.routines[i];
        ASTNode *stmt = routine->ast;
        if (routine->kind != MIR_SCOPE_FUNCTION
            || stmt == NULL
            || stmt->type != AST_FUNC_DECL)
            continue;
        GenericParams *generic_params = ast_func_generic_params(stmt);
        if (ast_generic_param_count(generic_params) > 0) {
            if (!llvm_register_generic_template_decl(ctx, stmt))
                return false;
            continue;
        }
        if (llvm_lookup_function(ctx, ast_declaration_name(stmt)) == NULL)
            llvm_forward_declare_func(stmt, ctx);
    }

    llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_intents");
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine = &routine_inventory.routines[i];
        ASTNode *stmt = routine->ast;
        if (routine->kind != MIR_SCOPE_INTENT
            || stmt == NULL
            || stmt->type != AST_INTENT_DECL) {
            continue;
        }
        llvm_forward_declare_intent(stmt, ctx);
    }

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_function_routines");
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine = &routine_inventory.routines[i];
        if (routine->kind != MIR_SCOPE_FUNCTION)
            continue;

        ASTNode *func_decl = routine->ast;
        GenericParams *generic_params = ast_func_generic_params(func_decl);
        if (ast_generic_param_count(generic_params) > 0) {
            continue;
        }
        if (llvm_mir_routine_has_instructions(routine))
            llvm_emit_func_from_mir(routine, ctx);
    }

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_residual_decls");
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine = &routine_inventory.routines[i];
        ASTNode *stmt = routine->ast;
        if (routine->kind == MIR_SCOPE_FUNCTION
            && stmt != NULL
            && stmt->type == AST_FUNC_DECL) {
            GenericParams *generic_params = ast_func_generic_params(stmt);
            if (ast_generic_param_count(generic_params) > 0) {
                continue;
            }
            if (!llvm_mir_routine_has_instructions(routine)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing routine for function '%s'",
                    ast_declaration_name(stmt) != NULL
                        ? ast_declaration_name(stmt)
                        : "(anonymous)");
                return false;
            }
        } else if (routine->kind == MIR_SCOPE_INTENT
                   && stmt != NULL
                   && stmt->type == AST_INTENT_DECL) {
            llvm_emit_intent_decl(stmt, ctx);
        }
    }

    {
        ASTNode **nominal_nodes = NULL;
        size_t nominal_count = 0;

        llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count);
        for (size_t i = 0; i < nominal_count; i++) {
            ASTNode *decl = nominal_nodes != NULL ? nominal_nodes[i] : NULL;
            const char *cls_name;
            const MIRDeclHeader *decl_header;
            const MIRDeclMethod *method_metadata;
            size_t method_metadata_count;

            if (decl == NULL || decl->type != AST_CLASS_DECL)
                continue;

            cls_name = llvm_decl_node_name(decl);
            LLVMHostedMethodView method_view =
                llvm_hosted_method_view_from_decl(ctx, cls_name, decl);
            decl_header = method_view.uses_mir_metadata
                ? llvm_find_host_decl_header_in_context(ctx, cls_name)
                : NULL;
            method_metadata = method_view.uses_mir_metadata
                ? method_view.metadata
                : NULL;
            method_metadata_count = method_view.uses_mir_metadata
                ? method_view.count
                : 0;
            if (decl_header == NULL && method_view.count > 0) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing declaration metadata for class method '%s.%s'",
                    cls_name != NULL ? cls_name : "(anonymous-class)",
                    "(metadata)");
                return false;
            }
            for (size_t j = 0; j < method_metadata_count; j++) {
                const MIRDeclMethod *method_meta = &method_metadata[j];
                const char *method_name;
                const MIRRoutine *mir_method;

                method_name = llvm_mir_decl_method_name(method_meta);
                mir_method = llvm_hosted_method_view_routine(
                    ctx, &method_view, j);
                if (mir_method != NULL) {
                    llvm_emit_func_from_mir(mir_method, ctx);
                    continue;
                }
                {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing routine for class method '%s.%s'",
                        cls_name != NULL ? cls_name : "(anonymous-class)",
                        method_name != NULL ? method_name : "(anonymous)");
                    return false;
                }
            }
        }
    }

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_main_wrapper");
    llvm_emit_main_wrapper(ctx);
    llvm_pipeline_debug_stage("emit_program_from_mir:end");
    return !ctx->has_error;
}

#endif
