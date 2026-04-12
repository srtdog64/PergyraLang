/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend pipeline helpers split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static void
llvm_pipeline_debug_stage(const char *stage)
{
    if (stage != NULL && getenv("PGY_DEBUG_LLVM_STAGE") != NULL)
        fprintf(stderr, "[llvm stage] %s\n", stage);
}

static bool
llvm_ast_uses_thread_pool(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
        return true;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.block.statements[i]))
                return true;
        }
        return false;
    case AST_LET_DECL:
        return llvm_ast_uses_thread_pool(node->data.let_decl.type)
            || llvm_ast_uses_thread_pool(node->data.let_decl.initializer);
    case AST_RETURN:
        return llvm_ast_uses_thread_pool(node->data.return_stmt.value);
    case AST_CALL:
        if (llvm_ast_uses_thread_pool(node->data.call.callee))
            return true;
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.call.arguments[i]))
                return true;
        }
        return false;
    case AST_BINARY:
        return llvm_ast_uses_thread_pool(node->data.binary.left)
            || llvm_ast_uses_thread_pool(node->data.binary.right);
    case AST_UNARY:
        return llvm_ast_uses_thread_pool(node->data.unary.operand);
    case AST_ASSIGNMENT:
        return llvm_ast_uses_thread_pool(node->data.assignment.target)
            || llvm_ast_uses_thread_pool(node->data.assignment.value);
    case AST_MEMBER_ACCESS:
        return llvm_ast_uses_thread_pool(node->data.member.object);
    case AST_ARRAY_ACCESS:
        return llvm_ast_uses_thread_pool(node->data.array_access.array)
            || llvm_ast_uses_thread_pool(node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.array_literal.elements[i]))
                return true;
        }
        return false;
    case AST_IF_STMT:
        return llvm_ast_uses_thread_pool(node->data.if_stmt.condition)
            || llvm_ast_uses_thread_pool(node->data.if_stmt.then_branch)
            || llvm_ast_uses_thread_pool(node->data.if_stmt.else_branch);
    case AST_FOR_LOOP:
        return llvm_ast_uses_thread_pool(node->data.for_loop.range_start)
            || llvm_ast_uses_thread_pool(node->data.for_loop.range_end)
            || llvm_ast_uses_thread_pool(node->data.for_loop.iterable)
            || llvm_ast_uses_thread_pool(node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return llvm_ast_uses_thread_pool(node->data.while_loop.condition)
            || llvm_ast_uses_thread_pool(node->data.while_loop.body);
    case AST_MATCH_STMT:
        if (llvm_ast_uses_thread_pool(node->data.match_stmt.subject)
            || llvm_ast_uses_thread_pool(node->data.match_stmt.default_body))
            return true;
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.match_stmt.cases[i]))
                return true;
        }
        return false;
    case AST_MATCH_CASE:
        return llvm_ast_uses_thread_pool(node->data.match_case.pattern)
            || llvm_ast_uses_thread_pool(node->data.match_case.guard)
            || llvm_ast_uses_thread_pool(node->data.match_case.body);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.select_stmt.cases[i]))
                return true;
        }
        return llvm_ast_uses_thread_pool(node->data.select_stmt.default_case);
    case AST_TASK_GROUP:
        for (size_t i = 0; i < node->data.task_group.task_count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.task_group.tasks[i]))
                return true;
        }
        return false;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return llvm_ast_uses_thread_pool(node->data.event_op.event)
            || llvm_ast_uses_thread_pool(node->data.event_op.handler);
    case AST_EVENT_INVOKE:
        if (llvm_ast_uses_thread_pool(node->data.event_invoke.event))
            return true;
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            if (llvm_ast_uses_thread_pool(node->data.event_invoke.arguments[i]))
                return true;
        }
        return false;
    default:
        return false;
    }
}

static bool
llvm_hir_requires_thread_pool(const HIRProgram *hir)
{
    if (hir == NULL)
        return false;

    for (size_t i = 0; i < hir->routine_count; i++) {
        if (llvm_ast_uses_thread_pool(hir->routines[i].body))
            return true;
    }
    for (size_t i = 0; i < hir->executable_count; i++) {
        if (llvm_ast_uses_thread_pool(hir->executables[i]))
            return true;
    }
    return false;
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
    if (func_decl->data.func_decl.name == NULL)
        return false;
    if (llvm_lookup_generic_template(ctx, func_decl->data.func_decl.name) != NULL)
        return true;

    if (ctx->generic_template_count >= ctx->generic_template_capacity) {
        int new_capacity = ctx->generic_template_capacity == 0
            ? 16
            : ctx->generic_template_capacity * 2;
        LLVMGenericTemplate *new_templates =
            realloc(ctx->generic_templates,
                    (size_t)new_capacity * sizeof(LLVMGenericTemplate));
        if (new_templates == NULL) {
            llvm_set_error(ctx, "out of memory growing generic_templates");
            return false;
        }
        memset(new_templates + ctx->generic_template_capacity, 0,
               (size_t)(new_capacity - ctx->generic_template_capacity)
                   * sizeof(LLVMGenericTemplate));
        ctx->generic_templates = new_templates;
        ctx->generic_template_capacity = new_capacity;
    }

    ctx->generic_templates[ctx->generic_template_count].name =
        func_decl->data.func_decl.name;
    ctx->generic_templates[ctx->generic_template_count].ast = func_decl;
    ctx->generic_template_count++;
    return true;
}

static void
llvm_build_hir_view_from_mir(const MIRProgram *mir, HIRProgram *hir_view)
{
    if (hir_view == NULL)
        return;
    memset(hir_view, 0, sizeof(*hir_view));
    if (mir == NULL)
        return;
    hir_view->items = mir->items;
    hir_view->item_count = mir->item_count;
    hir_view->externs = mir->externs;
    hir_view->extern_count = mir->extern_count;
    hir_view->types = mir->types;
    hir_view->type_count = mir->type_count;
    hir_view->abilities = mir->abilities;
    hir_view->ability_count = mir->ability_count;
    hir_view->roles = mir->roles;
    hir_view->role_count = mir->role_count;
    hir_view->parties = mir->parties;
    hir_view->party_count = mir->party_count;
    hir_view->rosters = mir->rosters;
    hir_view->roster_count = mir->roster_count;
    hir_view->worlds = mir->worlds;
    hir_view->world_count = mir->world_count;
    hir_view->relations = mir->relations;
    hir_view->relation_count = mir->relation_count;
    hir_view->effects = mir->effects;
    hir_view->effect_count = mir->effect_count;
    hir_view->zones = mir->zones;
    hir_view->zone_count = mir->zone_count;
    hir_view->events = mir->events;
    hir_view->event_count = mir->event_count;
    hir_view->intents = mir->intents;
    hir_view->intent_count = mir->intent_count;
    hir_view->functions = mir->functions;
    hir_view->function_count = mir->function_count;
    hir_view->executables = mir->executables;
    hir_view->executable_count = mir->executable_count;
    hir_view->synthetic_executable_func = mir->synthetic_executable_func;
    hir_view->has_main_function = mir->has_main_function;
}

static const MIRRoutine *
llvm_find_mir_method_routine(const MIRProgram *mir,
                             const char *owner_name,
                             ASTNode *method)
{
    const char *method_name;

    if (mir == NULL || owner_name == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return NULL;
    method_name = method->data.func_decl.name;
    if (method_name == NULL)
        return NULL;

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->kind != MIR_SCOPE_METHOD)
            continue;
        if (routine->ast == method)
            return routine;
        if (routine->name != NULL
            && routine->owner_name != NULL
            && strcmp(routine->name, method_name) == 0
            && strcmp(routine->owner_name, owner_name) == 0) {
            return routine;
        }
    }
    return NULL;
}

void
llvm_emit_mir_main_wrapper(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL || ctx == NULL)
        return;

    LLVMFuncEntry *main_user = llvm_lookup_or_create_function(ctx, "Main", NULL, NULL);
    bool has_top_level = (hir->executable_count > 0)
        || hir->has_main_function
        || (main_user != NULL);
    if (!has_top_level)
        return;

    LLVMTypeRef main_type = LLVMFunctionType(ctx->type_i32, NULL, 0, 0);
    LLVMFuncEntry *main_entry = llvm_lookup_or_create_function(ctx, "main", main_type,
                                                               ctx->type_i32);
    LLVMValueRef main_fn = main_entry != NULL ? main_entry->fn : NULL;
    bool needs_thread_pool = llvm_hir_requires_thread_pool(hir);
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
        if (init_fn != NULL) {
            LLVMValueRef args[] = { LLVMConstInt(ctx->type_i64, 4, 0) };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 1, "");
        }
    }

    llvm_scope_push(ctx);

    for (int i = 0; i < ctx->event_type_count; i++) {
        LLVMEventTypeEntry *evt = &ctx->event_types[i];
        char fname[256];
        snprintf(fname, sizeof(fname), "%s_INIT", evt->event_name);
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx, fname);
        LLVMValueRef gv = LLVMGetNamedGlobal(ctx->module, evt->event_name);
        if (init_fn != NULL && gv != NULL) {
            LLVMValueRef args[] = { gv };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 1, "");
        }
    }

    if (main_user != NULL)
        LLVMBuildCall2(ctx->builder, main_user->fn_type,
                       main_user->fn, NULL, 0, "");

    if (hir->executable_count > 0) {
        LLVMFuncEntry *top_level_entry = llvm_lookup_function(ctx, "__pgy_top_level_exec");
        if (top_level_entry != NULL) {
            LLVMBuildCall2(ctx->builder, top_level_entry->fn_type,
                           top_level_entry->fn, NULL, 0, "");
        } else {
            for (size_t i = 0; i < hir->executable_count; i++) {
                ASTNode *stmt = hir->executables[i];
                if (stmt != NULL) {
                    llvm_emit_statement(stmt, ctx);
                    if (LLVMGetBasicBlockTerminator(
                            LLVMGetInsertBlock(ctx->builder)) != NULL)
                        break;
                }
            }
        }
    }

    llvm_scope_pop(ctx);

    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (needs_thread_pool) {
            LLVMFuncEntry *shutdown_fn = llvm_lookup_function(ctx,
                                             "pgy_pool_shutdown_export");
            if (shutdown_fn != NULL)
                LLVMBuildCall2(ctx->builder, shutdown_fn->fn_type,
                               shutdown_fn->fn, NULL, 0, "");
        }
        LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
    }

    llvm_mark_function_as_used(ctx, "main");
}

bool
llvm_func_requires_hir_fallback(ASTNode *func_decl)
{
    (void)func_decl;
    return false;
}

bool
llvm_validate_mir_for_codegen(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is NULL");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        char *topology_error = NULL;

        if (routine->name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("MIR routine is missing name");
            return false;
        }

        if (!mir_validate_emission_topology(routine, false, false, &topology_error)) {
            if (error_message != NULL) {
                if (topology_error != NULL) {
                    size_t msg_len = strlen(topology_error) + 128;
                    *error_message = calloc(1, msg_len);
                    if (*error_message != NULL) {
                        snprintf(*error_message, msg_len,
                                 "MIR routine '%s' emission topology invalid: %s",
                                 routine->name != NULL ? routine->name : "(anonymous)",
                                 topology_error);
                    }
                } else {
                    *error_message = pergyra_strdup(
                        "MIR emission topology validation failed");
                }
            }
            free(topology_error);
            return false;
        }
        free(topology_error);
    }
    return true;
}

void
llvm_emit_program(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL) {
        llvm_set_error(ctx, "Expected lowered HIR program");
        return;
    }

    ctx->hir = hir;
    llvm_set_type_render_ctx(ctx);

    llvm_declare_runtime(ctx);
    llvm_register_hir_nominal_types(hir, ctx);
    llvm_register_hir_extern_prototypes(hir, ctx);

    for (size_t i = 0; i < hir->function_count; i++) {
        ASTNode *stmt = hir->functions[i];
        if (llvm_can_forward_declare_func_early(ctx, stmt))
            llvm_forward_declare_func(stmt, ctx);
    }
    for (size_t i = 0; i < hir->function_count; i++) {
        ASTNode *stmt = hir->functions[i];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;
        if (stmt->data.func_decl.generic_params != NULL
            && stmt->data.func_decl.generic_params->count > 0)
            continue;
        if (llvm_lookup_function(ctx, stmt->data.func_decl.name) == NULL)
            llvm_forward_declare_func(stmt, ctx);
    }

    llvm_emit_domain_passes(hir, ctx);

    for (size_t i = 0; i < hir->function_count; i++) {
        ASTNode *stmt = hir->functions[i];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;
        if (stmt->data.func_decl.generic_params != NULL
            && stmt->data.func_decl.generic_params->count > 0) {
            if (!llvm_register_generic_template_decl(ctx, stmt))
                return;
        } else if (llvm_lookup_function(ctx, stmt->data.func_decl.name) == NULL) {
            llvm_forward_declare_func(stmt, ctx);
        }
    }
    for (size_t i = 0; i < hir->intent_count; i++)
        llvm_forward_declare_intent(hir->intents[i], ctx);

    for (size_t i = 0; i < hir->function_count; i++) {
        ASTNode *stmt = hir->functions[i];
        if (stmt != NULL
            && llvm_lookup_generic_template(ctx, stmt->data.func_decl.name) == NULL) {
            llvm_emit_func_decl(stmt, ctx);
        }
    }
    for (size_t i = 0; i < hir->intent_count; i++) {
        ASTNode *stmt = hir->intents[i];
        if (stmt != NULL)
            llvm_emit_intent_decl(stmt, ctx);
    }
    for (size_t i = 0; i < hir->type_count; i++) {
        ASTNode *stmt = hir->types[i];
        if (stmt != NULL && stmt->type == AST_CLASS_DECL) {
            const char *cls_name = stmt->data.class_decl.name;
            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, cls_name);

            for (size_t j = 0; j < stmt->data.class_decl.method_count; j++) {
                ASTNode *method = stmt->data.class_decl.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                         cls_name, method->data.func_decl.name);

                LLVMFuncEntry *entry = llvm_lookup_function(ctx, full_name);
                if (entry == NULL)
                    continue;

                LLVMValueRef fn = entry->fn;
                LLVMTypeRef ret_type = entry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                const char *saved_class_name = ctx->current_class_name;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;
                ctx->current_class_name = cls_name;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);

                llvm_scope_push(ctx);

                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                if (cls != NULL && cls->is_pointer_self_host) {
                    LLVMTypeRef self_ptr_type = LLVMPointerType(cls->struct_type, 0);
                    LLVMValueRef self_alloca = llvm_create_entry_alloca(
                        ctx, self_ptr_type, "self.addr");
                    LLVMBuildStore(ctx->builder, self_val, self_alloca);
                    llvm_scope_declare(ctx, "self", self_alloca, self_ptr_type);
                } else {
                    LLVMTypeRef self_type = cls != NULL ? cls->struct_type : LLVMTypeOf(self_val);
                    LLVMValueRef self_alloca = llvm_create_entry_alloca(ctx, self_type, "self");
                    LLVMBuildStore(ctx->builder, self_val, self_alloca);
                    llvm_scope_declare(ctx, "self", self_alloca, self_type);
                }
                llvm_register_var_class(ctx, "self", cls_name);

                size_t pc = method->data.func_decl.param_count;
                unsigned llvm_pidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    LLVMTypeRef pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder, LLVMGetParam(fn, llvm_pidx++), alloca);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    llvm_register_typed_var(ctx, p->name, p->type);
                }

                if (method->data.func_decl.body != NULL)
                    llvm_emit_block(method->data.func_decl.body, ctx);

                if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder, LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;
                ctx->current_class_name = saved_class_name;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }
        } else if (stmt != NULL && stmt->type == AST_ENUM_DECL) {
            const char *enum_name = stmt->data.enum_decl.name;
            for (size_t j = 0; j < stmt->data.enum_decl.method_count; j++) {
                ASTNode *method = stmt->data.enum_decl.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;
                {
                    char *orig_name = method->data.func_decl.name;
                    char prefixed[256];
                    snprintf(prefixed, sizeof(prefixed), "%s_%s", enum_name, orig_name);
                    method->data.func_decl.name = prefixed;
                    ctx->current_class_name = enum_name;
                    llvm_emit_func_decl(method, ctx);
                    ctx->current_class_name = NULL;
                    method->data.func_decl.name = orig_name;
                }
            }
        }
    }

    llvm_emit_mir_main_wrapper(hir, ctx);
    llvm_set_type_render_ctx(NULL);
}

bool
llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx)
{
    HIRProgram mir_hir_view;
    const HIRProgram *saved_hir;

    if (mir == NULL || ctx == NULL)
        return false;

    llvm_build_hir_view_from_mir(mir, &mir_hir_view);
    saved_hir = ctx->hir;
    ctx->hir = &mir_hir_view;

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

    if (ctx->hir != NULL) {
        llvm_pipeline_debug_stage("emit_program_from_mir:register_hir_items");
        for (size_t i = 0; i < mir->type_count; i++) {
            ASTNode *stmt = mir->types[i];
            if (stmt == NULL)
                continue;
            if (stmt->type == AST_CLASS_DECL) {
                const char *cls_name = stmt->data.class_decl.name;
                if (cls_name == NULL || llvm_lookup_class(ctx, cls_name) != NULL)
                    continue;
                size_t fc = stmt->data.class_decl.field_count;
                LLVMTypeRef *field_types = calloc(fc > 0 ? fc : 1, sizeof(LLVMTypeRef));
                for (size_t j = 0; j < fc; j++) {
                    ClassField *f = stmt->data.class_decl.fields[j];
                    field_types[j] = (f->type != NULL)
                        ? ast_type_to_llvm(ctx, f->type)
                        : ctx->type_i32;
                }
                LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context, cls_name);
                LLVMStructSetBody(struct_ty, field_types, (unsigned)fc, 0);
                NominalDeclKind nominal_kind = stmt->data.class_decl.nominal_kind;
                bool is_subject = nominal_kind == NOMINAL_DECL_SUBJECT;
                bool is_immutable =
                    llvm_nominal_uses_immutable_projection_storage(nominal_kind);
                bool is_boundary_transfer =
                    llvm_nominal_is_boundary_transfer_contract(nominal_kind);
                bool is_pointer_self_host = is_subject
                    || nominal_kind == NOMINAL_DECL_VESSEL;
                LLVMClassTypeEntry *entry = llvm_register_class(
                    ctx, cls_name, struct_ty, is_subject, is_pointer_self_host);
                if (entry != NULL) {
                    entry->is_immutable = is_immutable;
                    entry->is_boundary_transfer_contract = is_boundary_transfer;
                    for (size_t j = 0; j < fc; j++) {
                        ClassField *f = stmt->data.class_decl.fields[j];
                        llvm_class_add_field(entry, f->name, field_types[j], (int)j);
                    }
                }
                for (size_t j = 0; j < stmt->data.class_decl.method_count; j++) {
                    ASTNode *method = stmt->data.class_decl.methods[j];
                    if (method == NULL || method->type != AST_FUNC_DECL)
                        continue;
                    const char *mname = method->data.func_decl.name;
                    size_t mpc = method->data.func_decl.param_count;
                    LLVMTypeRef mret = ctx->type_void;
                    if (method->data.func_decl.return_type != NULL)
                        mret = ast_type_to_llvm(ctx, method->data.func_decl.return_type);
                    size_t user_pc = 0;
                    for (size_t k = 0; k < mpc; k++) {
                        FuncParam *p = method->data.func_decl.params[k];
                        if (p->type == NULL && strcmp(p->name, "self") == 0)
                            continue;
                        user_pc++;
                    }
                    LLVMTypeRef *mpt = calloc(user_pc + 1, sizeof(LLVMTypeRef));
                    mpt[0] = is_pointer_self_host ? LLVMPointerType(struct_ty, 0) : struct_ty;
                    size_t pidx = 1;
                    for (size_t k = 0; k < mpc; k++) {
                        FuncParam *p = method->data.func_decl.params[k];
                        if (p->type == NULL && strcmp(p->name, "self") == 0)
                            continue;
                        mpt[pidx++] = (p->type != NULL)
                            ? ast_type_to_llvm(ctx, p->type)
                            : ctx->type_i32;
                    }
                    LLVMTypeRef mft = LLVMFunctionType(mret, mpt, (unsigned)(user_pc + 1), 0);
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s", cls_name, mname);
                    LLVMValueRef fn = LLVMAddFunction(ctx->module, full_name, mft);
                    llvm_register_function(ctx, LLVMGetValueName(fn), fn, mft, mret);
                    free(mpt);
                }
                free(field_types);
            } else if (stmt->type == AST_ENUM_DECL) {
                llvm_register_enum_decl(ctx, stmt);
            }
        }
        llvm_pipeline_debug_stage("emit_program_from_mir:emit_domain_passes");
        llvm_emit_domain_passes(&mir_hir_view, ctx);

        llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_funcs");
        for (size_t i = 0; i < mir->routine_count; i++) {
            const MIRRoutine *routine = &mir->routines[i];
            ASTNode *stmt = routine->ast;
            if (routine->kind != MIR_SCOPE_FUNCTION
                || stmt == NULL
                || stmt->type != AST_FUNC_DECL)
                continue;
            if (stmt->data.func_decl.generic_params != NULL
                && stmt->data.func_decl.generic_params->count > 0) {
                if (!llvm_register_generic_template_decl(ctx, stmt))
                    return false;
                continue;
            }
            if (llvm_lookup_function(ctx, stmt->data.func_decl.name) == NULL)
                llvm_forward_declare_func(stmt, ctx);
        }

        llvm_pipeline_debug_stage("emit_program_from_mir:forward_declare_intents");
        for (size_t i = 0; i < mir->routine_count; i++) {
            const MIRRoutine *routine = &mir->routines[i];
            ASTNode *stmt = routine->ast;
            if (routine->kind != MIR_SCOPE_INTENT
                || stmt == NULL
                || stmt->type != AST_INTENT_DECL) {
                continue;
            }
            llvm_forward_declare_intent(stmt, ctx);
        }
    }

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_function_routines");
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->kind != MIR_SCOPE_FUNCTION)
            continue;

        ASTNode *func_decl = routine->ast;
        if (func_decl != NULL
            && func_decl->type == AST_FUNC_DECL
            && func_decl->data.func_decl.generic_params != NULL
            && func_decl->data.func_decl.generic_params->count > 0) {
            continue;
        }
        if (func_decl != NULL && llvm_func_requires_hir_fallback(func_decl))
            continue;

        bool mir_has_instructions = false;
        for (size_t bi = 0; bi < routine->block_count; bi++) {
            if (routine->blocks[bi].instruction_count > 0) {
                mir_has_instructions = true;
                break;
            }
        }
        if (mir_has_instructions)
            llvm_emit_func_from_mir(routine, ctx);
    }

    if (ctx->hir != NULL) {
        llvm_pipeline_debug_stage("emit_program_from_mir:emit_hir_residuals");
        for (size_t i = 0; i < mir->routine_count; i++) {
            const MIRRoutine *routine = &mir->routines[i];
            ASTNode *stmt = routine->ast;
            if (routine->kind == MIR_SCOPE_FUNCTION
                && stmt != NULL
                && stmt->type == AST_FUNC_DECL) {
                if (stmt->data.func_decl.generic_params != NULL
                    && stmt->data.func_decl.generic_params->count > 0) {
                    continue;
                }
                if (llvm_func_requires_hir_fallback(stmt)) {
                    llvm_emit_func_decl(stmt, ctx);
                    continue;
                }
                if (!llvm_mir_routine_has_instructions(routine)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg),
                             "MIR-only LLVM path missing routine for function '%s'",
                             stmt->data.func_decl.name != NULL
                                 ? stmt->data.func_decl.name
                                 : "(anonymous)");
                    llvm_set_error(ctx, msg);
                    return false;
                }
            } else if (routine->kind == MIR_SCOPE_INTENT
                       && stmt != NULL
                       && stmt->type == AST_INTENT_DECL) {
                llvm_emit_intent_decl(stmt, ctx);
            }
        }

        for (size_t i = 0; i < mir->type_count; i++) {
            ASTNode *stmt = mir->types[i];
            if (stmt != NULL && stmt->type == AST_CLASS_DECL) {
                const char *cls_name = stmt->data.class_decl.name;
                bool require_mir_methods =
                    stmt->data.class_decl.nominal_kind == NOMINAL_DECL_CLASS
                    || stmt->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
                for (size_t j = 0; j < stmt->data.class_decl.method_count; j++) {
                    ASTNode *method = stmt->data.class_decl.methods[j];
                    const MIRRoutine *mir_method;
                    if (method == NULL || method->type != AST_FUNC_DECL)
                        continue;
                    mir_method = llvm_find_mir_method_routine(mir, cls_name, method);
                    if (mir_method != NULL && llvm_mir_routine_has_instructions(mir_method)) {
                        const char *saved_class_name = ctx->current_class_name;
                        ctx->current_class_name = cls_name;
                        llvm_emit_func_from_mir(mir_method, ctx);
                        ctx->current_class_name = saved_class_name;
                        continue;
                    }
                    if (require_mir_methods) {
                        char msg[384];
                        snprintf(msg, sizeof(msg),
                                 "MIR-only LLVM path missing routine for class method '%s.%s'",
                                 cls_name != NULL ? cls_name : "(anonymous-class)",
                                 method->data.func_decl.name != NULL
                                     ? method->data.func_decl.name
                                     : "(anonymous)");
                        llvm_set_error(ctx, msg);
                        return false;
                    }
                    {
                        char *orig_name = method->data.func_decl.name;
                        char prefixed[256];
                        snprintf(prefixed, sizeof(prefixed), "%s_%s", cls_name, orig_name);
                        method->data.func_decl.name = prefixed;
                        ctx->current_class_name = cls_name;
                        llvm_emit_func_decl(method, ctx);
                        ctx->current_class_name = NULL;
                        method->data.func_decl.name = orig_name;
                    }
                }
            }
        }
    }

    llvm_pipeline_debug_stage("emit_program_from_mir:emit_main_wrapper");
    llvm_emit_mir_main_wrapper(&mir_hir_view, ctx);
    llvm_pipeline_debug_stage("emit_program_from_mir:end");
    ctx->hir = saved_hir;
    return !ctx->has_error;
}

#endif
