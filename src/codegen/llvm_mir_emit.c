/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR function emission split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_boundary_slot_param.h"
#include "llvm_internal.h"
#include "llvm_intent_internal.h"
#include "llvm_mir_vars.h"
#include "llvm_mir_phi.h"
#include "llvm_mir_signature.h"
#include "llvm_mir_type_helpers.h"

static void
llvm_mir_debug_stage(const char *stage, const MIRRoutine *routine)
{
    if (!llvm_debug_stage_enabled() || stage == NULL)
        return;
    fprintf(stderr, "[llvm stage] %s", stage);
    if (llvm_mir_routine_name(routine) != NULL)
        fprintf(stderr, ":%s", llvm_mir_routine_name(routine));
    fputc('\n', stderr);
}

/* P0 #4: pre-register source-local class facts materialized by MIR lowering
 * so the type-infer dry pass that precedes real emit can resolve receiver
 * classes without rescanning the source AST body. */
static void
llvm_mir_preregister_source_local_classes(LLVMGenCtx *ctx,
                                          const MIRRoutine *routine)
{
    if (ctx == NULL || routine == NULL)
        return;
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        const MIRSourceLocalType *fact = &routine->source_local_types[i];
        if (fact->name != NULL && fact->type_name != NULL
            && llvm_lookup_class(ctx, fact->type_name) != NULL) {
            llvm_register_var_class(ctx, fact->name, fact->type_name);
        }
    }
}

static void
llvm_mir_mark_owner_dirty_for_exit(LLVMGenCtx *ctx,
                                   LLVMClassTypeEntry *owner_cls,
                                   const char *owner_name)
{
    LLVMVarEntry self_entry;
    LLVMValueRef self_ptr;

    if (ctx == NULL || owner_cls == NULL || owner_name == NULL)
        return;

    if (!llvm_scope_lookup_snapshot(ctx, "self", &self_entry))
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(owner_cls->struct_type, 0),
        self_entry.alloca, llvm_tmp_name(ctx));

    if (owner_cls->domain_kind == LLVM_DOMAIN_WORLD) {
        int field_count = llvm_class_field_count(owner_cls);
        for (int i = 0; i < field_count; i++) {
            const char *field_name = llvm_class_field_name_at(owner_cls, i);
            int field_index = llvm_class_field_struct_index_at(owner_cls, i);
            LLVMValueRef dirty_ptr;

            if (field_name == NULL
                || field_index < 0
                || strncmp(field_name, "__zone_dirty_", 13) != 0) {
                continue;
            }

            dirty_ptr = LLVMBuildStructGEP2(ctx->builder,
                owner_cls->struct_type, self_ptr,
                (unsigned)field_index,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                dirty_ptr);
        }

        {
            int derived_idx = llvm_class_field_index(owner_cls, "__world_derived_dirty");
            if (derived_idx >= 0) {
                LLVMValueRef derived_ptr = LLVMBuildStructGEP2(ctx->builder,
                    owner_cls->struct_type, self_ptr, (unsigned)derived_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                    derived_ptr);
            }
        }
    }
}

void
llvm_mir_emit_owner_sync_exit(LLVMGenCtx *ctx,
                              LLVMClassTypeEntry *owner_cls,
                              LLVMFuncEntry *owner_sync,
                              const char *owner_name)
{
    LLVMVarEntry self_entry;
    LLVMValueRef self_ptr;
    LLVMValueRef sync_args[1];

    if (ctx == NULL || owner_cls == NULL || owner_sync == NULL)
        return;

    llvm_mir_mark_owner_dirty_for_exit(ctx, owner_cls, owner_name);

    if (!llvm_scope_lookup_snapshot(ctx, "self", &self_entry))
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(owner_cls->struct_type, 0),
        self_entry.alloca, llvm_tmp_name(ctx));
    sync_args[0] = self_ptr;
    LLVMBuildCall2(ctx->builder, owner_sync->fn_type, owner_sync->fn,
        sync_args, 1, "");
}

#include "llvm_mir_block_emit.h"
#include "llvm_mir_local_emit.h"

LLVMValueRef
llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx)
{
    size_t param_count = 0;
    bool is_intent = false;
    bool is_method = false;
    bool owner_is_role = false;
    const char *owner_name = NULL;
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count = 0;
    LLVMClassTypeEntry *owner_cls = NULL;
    LLVMFuncEntry *owner_sync = NULL;
    ASTNode *func_decl = NULL;
    const char *fn_name = NULL;
    char qualified_name[256];
    const char *routine_name = NULL;
    if (routine == NULL || ctx == NULL)
        return NULL;
    routine_name = llvm_mir_routine_name(routine);
    func_decl = llvm_mir_routine_source_ast(routine);
    if (func_decl == NULL)
        return NULL;
    llvm_mir_debug_stage("emit_func_from_mir:begin", routine);

    if (func_decl == NULL
        || (func_decl->type != AST_FUNC_DECL && func_decl->type != AST_INTENT_DECL))
        return NULL;
    if (!llvm_mir_validate_cleanup_contract(routine, ctx))
        return NULL;

    is_intent = (func_decl->type == AST_INTENT_DECL);
    if (is_intent) {
        mir_binding_count = llvm_collect_mir_intent_bindings(
            routine, ctx, &binding_metadata);
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    &binding_metadata, i)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete ordered intent binding metadata for '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return NULL;
            }
            if (!intent_binding_metadata_kind_is_supported(
                    intent_binding_metadata_view_kind_at(
                        &binding_metadata, i))) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has invalid ordered intent binding metadata for '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return NULL;
            }
        }
    }
    is_method = (!is_intent
        && llvm_mir_routine_kind(routine) == MIR_SCOPE_METHOD);
    owner_is_role = is_method
        && llvm_mir_routine_owner_ast_type(routine) == AST_ROLE_DECL;
    owner_name = llvm_mir_routine_owner_name(routine);
    if (!is_intent
        && !llvm_mir_routine_signature_metadata_complete(ctx,
            routine,
            func_decl,
            "MIR-only LLVM path missing function body signature metadata for '%s'",
            "MIR-only LLVM path missing function body return type-name metadata for '%s'",
            "MIR-only LLVM path missing function body parameter type-name metadata for '%s'")) {
        return NULL;
    }
    if (is_method && owner_name == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "MIR-only LLVM path missing owner metadata for method '%s'",
            routine_name != NULL ? routine_name : "(anonymous)");
        return NULL;
    }
    owner_cls = (is_method && owner_name != NULL)
        ? llvm_lookup_class(ctx, owner_name)
        : NULL;
    if (is_method && owner_cls != NULL
        && owner_cls->sync_function_name != NULL
        && owner_cls->domain_kind != LLVM_DOMAIN_NONE
        && owner_cls->domain_kind != LLVM_DOMAIN_SYSTEMIC) {
        owner_sync = llvm_lookup_function(ctx, owner_cls->sync_function_name);
    }
    param_count = is_intent
        ? mir_binding_count
        : (is_method ? 1 : 0);
    if (!is_intent) {
        size_t func_param_count = llvm_mir_routine_param_count(routine);
        for (size_t i = 0; i < func_param_count; i++) {
            FuncParam *p = llvm_mir_routine_param(routine, i);
            if (is_method && llvm_param_is_implicit_self_local(p)) {
                continue;
            }
            bool is_secure_slot = false;
            if (llvm_mir_boundary_slot_inner_name(ctx, p, &is_secure_slot) != NULL)
                param_count += is_secure_slot ? 2 : 1;
            else
                param_count++;
        }
    }
    LLVMTypeRef *param_types = pgy_arena_calloc(&ctx->scratch,
        (param_count > 0 ? param_count : 1) * sizeof(LLVMTypeRef));
    if (param_types == NULL) {
        llvm_set_mir_memory_exhausted(ctx,
            "LLVM MIR routine '%s' parameter type allocation failed",
            routine_name != NULL ? routine_name : "(anonymous)");
        return NULL;
    }
    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            const char *kind =
                intent_binding_metadata_view_kind_at(&binding_metadata, i);
            const char *type_name =
                intent_binding_metadata_view_type_at(&binding_metadata, i);

            if (kind != NULL && strcmp(kind, "participant") == 0) {
                if (type_name != NULL) {
                    param_types[i] = pergyra_type_to_llvm(ctx, type_name);
                    if (ctx->has_error || param_types[i] == NULL)
                        return NULL;
                    if (llvm_type_name_uses_pointer_self(ctx, type_name))
                        param_types[i] = LLVMPointerType(param_types[i], 0);
                } else {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing intent participant type metadata for '%s'",
                        routine_name != NULL ? routine_name : "(anonymous)");
                    return NULL;
                }
            } else if (kind != NULL && strcmp(kind, "value") == 0) {
                if (type_name != NULL) {
                    param_types[i] = pergyra_type_to_llvm(ctx, type_name);
                    if (ctx->has_error || param_types[i] == NULL)
                        return NULL;
                } else {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing intent value type metadata for '%s'",
                        routine_name != NULL ? routine_name : "(anonymous)");
                    return NULL;
                }
            } else {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing intent parameter metadata for '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return NULL;
            }
        } else if (is_method && i == 0) {
            if (owner_is_role) {
                param_types[i] = ctx->type_i8ptr;
            } else if (owner_cls != NULL) {
                param_types[i] = owner_cls->is_pointer_self_host
                    ? LLVMPointerType(owner_cls->struct_type, 0)
                    : owner_cls->struct_type;
            } else {
                param_types[i] = ctx->type_i8ptr;
            }
        } else {
            size_t logical_index = is_method ? (i - 1) : i;
            size_t seen = 0;
            FuncParam *p = NULL;
            size_t source_param_index = (size_t)-1;
            size_t func_param_count = llvm_mir_routine_param_count(routine);
            for (size_t param_index = 0;
                param_index < func_param_count;
                 param_index++) {
                FuncParam *candidate =
                    llvm_mir_routine_param(routine, param_index);
                if (llvm_param_is_implicit_self_local(candidate)) {
                    continue;
                }
                if (seen == logical_index) {
                    p = candidate;
                    source_param_index = param_index;
                    break;
                }
                seen++;
            }
            bool is_secure_slot = false;
            const char *param_type_name =
                source_param_index != (size_t)-1
                    ? llvm_mir_routine_param_type_name(routine,
                        source_param_index)
                    : NULL;
            const char *slot_inner = param_type_name != NULL
                ? llvm_boundary_slot_inner_name_from_type_name(ctx,
                    p,
                    param_type_name,
                    &is_secure_slot)
                : llvm_mir_boundary_slot_inner_name(ctx, p, &is_secure_slot);
            if (slot_inner != NULL) {
                LLVMTypeRef slot_ty = param_type_name != NULL
                    ? pergyra_type_to_llvm(ctx, param_type_name)
                    : llvm_mir_type_from_ast(ctx, p->type);
                if (ctx->has_error || slot_ty == NULL)
                    return NULL;
                param_types[i] = LLVMPointerType(slot_ty, 0);
                if (is_secure_slot && i + 1 < param_count) {
                    param_types[++i] = llvm_secure_token_type(ctx, slot_inner);
                }
            } else {
                if (param_type_name != NULL)
                    param_types[i] = pergyra_type_to_llvm(ctx,
                        param_type_name);
                else if (p != NULL && p->type != NULL)
                    param_types[i] = llvm_mir_required_type_from_ast(
                        ctx, func_decl, p->type, "function parameter");
                else
                    param_types[i] = llvm_mir_required_type_from_ast(
                        ctx, func_decl, NULL, "function parameter");
                if (ctx->has_error || param_types[i] == NULL)
                    return NULL;
                if (param_type_name != NULL
                    ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
                    : (p != NULL && p->type != NULL
                        && llvm_mir_param_uses_pointer_self(ctx, p->type))) {
                    param_types[i] = LLVMPointerType(param_types[i], 0);
                }
            }
        }
    }
    const char *return_type_name = NULL;
    ASTNode *return_type = NULL;
    LLVMTypeRef ret_type = is_intent ? ctx->type_i1 : ctx->type_i32;
    if (!is_intent) {
        return_type_name = llvm_mir_routine_return_type_name(routine);
        return_type = llvm_mir_routine_return_type(routine);
        if (return_type_name != NULL)
            ret_type = pergyra_type_to_llvm(ctx, return_type_name);
        else if (return_type != NULL)
            ret_type = llvm_mir_type_from_ast(ctx, return_type);
        else if (is_method)
            /* A method/action with no return-type metadata is Void, matching
             * the registered forward signature (which normalizes it to Void).
             * Defaulting to i32 here drifts from the registration. */
            ret_type = ctx->type_void;
    }
    if (ctx->has_error || ret_type == NULL)
        return NULL;

    LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    fn_name = llvm_mir_routine_name(routine);
    if (is_method && owner_name != NULL && fn_name != NULL) {
        snprintf(qualified_name, sizeof(qualified_name), "%s_%s", owner_name, fn_name);
        fn_name = qualified_name;
    }
    LLVMFuncEntry *entry = llvm_lookup_function(ctx, fn_name);
    if (entry == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing registered function for MIR routine '%s'",
            fn_name != NULL ? fn_name : "(anonymous)");
        return NULL;
    }
    if (entry->fn_type != func_type) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path registered function type drift for MIR routine '%s'",
            fn_name != NULL ? fn_name : "(anonymous)");
        return NULL;
    }
    LLVMValueRef fn = entry != NULL ? entry->fn : NULL;
    if (fn == NULL)
        return NULL;
    llvm_mir_debug_stage("emit_func_from_mir:fn_ready", routine);
    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
    const char *saved_return_type_name = ctx->current_return_type_name;
    ASTNode *saved_return_callable_type = ctx->current_return_callable_type;
    const char *saved_within_zone_name = ctx->current_within_zone_name;
    ASTNode *saved_func_decl = ctx->current_func_decl;
    const MIRRoutine *saved_mir_routine = ctx->current_mir_routine;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
    LLVMLexicalRegistrySnapshot lexical_snapshot =
        llvm_lexical_registry_snapshot(ctx);
    ASTNode *saved_host_decl = NULL;
    bool scope_pushed = false;
    bool defer_scope_pushed = false;

    size_t var_capacity = 64;
    LLVMMirVar *vars = pgy_arena_calloc(&ctx->scratch,
        var_capacity * sizeof(LLVMMirVar));
    if (vars == NULL) {
        llvm_set_mir_memory_exhausted(ctx,
            "LLVM MIR routine '%s' local registry allocation failed",
            routine_name != NULL ? routine_name : "(anonymous)");
        return NULL;
    }
    size_t var_count = 0;

    size_t bb_alloc =
        (routine->block_count > 0 ? routine->block_count : 1);
    LLVMBasicBlockRef *llvm_blocks = pgy_arena_calloc(&ctx->scratch,
        bb_alloc * sizeof(LLVMBasicBlockRef));
    LLVMBasicBlockRef *llvm_block_heads = pgy_arena_calloc(&ctx->scratch,
        bb_alloc * sizeof(LLVMBasicBlockRef));
    LLVMBasicBlockRef *llvm_block_tails = pgy_arena_calloc(&ctx->scratch,
        bb_alloc * sizeof(LLVMBasicBlockRef));
    if (llvm_blocks == NULL || llvm_block_heads == NULL
        || llvm_block_tails == NULL) {
        llvm_set_mir_memory_exhausted(ctx,
            "LLVM MIR routine '%s' block registry allocation failed",
            routine_name != NULL ? routine_name : "(anonymous)");
        return NULL;
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        char bb_name[64];
        snprintf(bb_name, sizeof(bb_name), "bb_%zu", i);
        llvm_blocks[i] = LLVMAppendBasicBlockInContext(ctx->context, fn, bb_name);
        llvm_block_heads[i] = llvm_blocks[i];
        llvm_block_tails[i] = llvm_blocks[i];
    }
    llvm_mir_debug_stage("emit_func_from_mir:blocks_ready", routine);

    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;
    ctx->current_function_ret_type = ret_type;
    ctx->current_return_type_name = return_type_name != NULL
        ? return_type_name
        : (return_type != NULL
            ? llvm_stmt_render_type_annotation_copy(ctx, return_type)
            : NULL);
    ctx->current_return_callable_type =
        return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE
            ? return_type
            : NULL;
    ctx->current_within_zone_name = llvm_mir_routine_within_zone(routine);
    ctx->current_func_decl = func_decl;
    ctx->current_mir_routine = routine;
    if (is_method)
        saved_host_decl = llvm_bind_current_host_decl(
            ctx, llvm_find_host_decl_in_active_inventory(ctx, owner_name));

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    llvm_scope_push(ctx);
    if (ctx->has_error)
        goto restore_state;
    scope_pushed = true;
    llvm_defer_scope_push(ctx);
    defer_scope_pushed = true;
    llvm_emit_mir_param_allocas(routine, func_decl, fn, ctx, is_intent, is_method,
                                owner_cls, owner_name, param_count);
    llvm_emit_mir_local_allocas(routine, ctx, &vars, &var_capacity, &var_count);
    llvm_mir_debug_stage("emit_func_from_mir:locals_ready", routine);

    /* P0 #4 eager var-class registration: consume MIR source-local type
     * facts and pre-register `let name: ClassName = ...` bindings into
     * the var-class registry. Otherwise the first call to
     * `llvm_stmt_infer_expr_type` for a member-access expression like
     * `weapon.BossBurn(...)` happens BEFORE
     * `llvm_mir_copy_source_def_to_versioned_local` has had a chance
     * to register `weapon -> WeaponCard` (the type-infer pass runs as
     * a dry pre-pass before the real emit). The lookup misses, the
     * receiver class fallback chain runs out of options under LLVM
     * opaque pointers, and the LLVM compile fails on calls that the
     * C backend handled cleanly. */
    llvm_mir_preregister_source_local_classes(ctx, routine);

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    if (owner_sync != NULL) {
        LLVMVarEntry self_entry;
        if (llvm_scope_lookup_snapshot(ctx, "self", &self_entry)) {
            LLVMValueRef self_ptr = LLVMBuildLoad2(ctx->builder,
                LLVMPointerType(owner_cls->struct_type, 0),
                self_entry.alloca, llvm_tmp_name(ctx));
            LLVMValueRef sync_args[] = { self_ptr };
            LLVMBuildCall2(ctx->builder, owner_sync->fn_type, owner_sync->fn,
                sync_args, 1, "");
        }
    }

    if (routine->entry_block < routine->block_count) {
        llvm_emit_mir_block_with_exprs(&routine->blocks[routine->entry_block], routine, ctx,
                                       llvm_block_heads, llvm_block_tails,
                                       vars, var_count, owner_cls, owner_sync,
                                       owner_name);
    }
    llvm_mir_debug_stage("emit_func_from_mir:entry_emitted", routine);
    for (size_t i = 0; i < routine->block_count; i++) {
        if (i == routine->entry_block)
            continue;
        const MIRBasicBlock *mir_block = &routine->blocks[i];
        if (mir_block->is_reachable && !mir_block->is_cleanup) {
            llvm_emit_mir_block_with_exprs(mir_block, routine, ctx,
                                           llvm_block_heads, llvm_block_tails,
                                           vars, var_count, owner_cls,
                                           owner_sync, owner_name);
        } else if (!mir_block->is_cleanup) {
            LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[i]);
            LLVMBuildUnreachable(ctx->builder);
        }
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *mir_block = &routine->blocks[i];
        if (!mir_block->is_reachable || mir_block->is_cleanup)
            continue;
        LLVMBasicBlockRef tail = llvm_block_tails[i] != NULL
            ? llvm_block_tails[i]
            : llvm_block_heads[i];
        LLVMPositionBuilderAtEnd(ctx->builder, tail);
        if (LLVMGetBasicBlockTerminator(tail) != NULL)
            continue;
        if (mir_block->has_succ_true) {
            LLVMBuildBr(ctx->builder, llvm_block_heads[mir_block->succ_true]);
        } else if (ret_type == ctx->type_void) {
            LLVMBuildRetVoid(ctx->builder);
        } else {
            if (!ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, func_decl,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_CFG_MISSING_RETURN,
                    PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                    "LLVM MIR routine '%s' reached backend without a terminal return value",
                    routine_name != NULL ? routine_name : "<anonymous>");
            }
            LLVMBuildUnreachable(ctx->builder);
        }
    }
    llvm_mir_emit_true_phi_nodes(routine, ctx, llvm_block_heads,
                                 llvm_block_tails,
                                 vars, var_count);
    llvm_mir_debug_stage("emit_func_from_mir:blocks_emitted", routine);

    if (routine->has_cleanup_block) {
        for (size_t i = 0; i < routine->block_count; i++) {
            const MIRBasicBlock *mir_block = &routine->blocks[i];
            if (mir_block->is_cleanup && mir_block->is_reachable) {
                llvm_emit_mir_block_with_exprs(mir_block, routine, ctx,
                                               llvm_block_heads,
                                               llvm_block_tails,
                                               vars, var_count, owner_cls,
                                               owner_sync, owner_name);
            }
        }
    }
    llvm_mir_debug_stage("emit_func_from_mir:cleanup_emitted", routine);

restore_state:
    if (defer_scope_pushed)
        llvm_defer_scope_pop(ctx);
    if (scope_pushed)
        llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    ctx->current_within_zone_name = saved_within_zone_name;
    ctx->current_func_decl = saved_func_decl;
    ctx->current_mir_routine = saved_mir_routine;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    if (is_method)
        llvm_restore_current_host_decl(ctx, saved_host_decl);
    llvm_mir_debug_stage("emit_func_from_mir:return", routine);
    return fn;
}

#endif
