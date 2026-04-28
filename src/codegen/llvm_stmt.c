#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

/* =================================================================
 * Statement emission
 * ================================================================= */

ASTNode *
llvm_stmt_find_function_decl_by_name(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_FUNC_DECL, name);
}

bool
llvm_mir_base_name_from_versioned(const char *mir_name, char *base_out, size_t base_out_size)
{
    const char *last_dot;
    size_t len;

    if (base_out != NULL && base_out_size > 0)
        base_out[0] = '\0';
    if (mir_name == NULL || base_out == NULL || base_out_size == 0)
        return false;

    last_dot = strrchr(mir_name, '.');
    len = last_dot != NULL ? (size_t)(last_dot - mir_name) : strlen(mir_name);
    if (len == 0 || len >= base_out_size)
        return false;

    memcpy(base_out, mir_name, len);
    base_out[len] = '\0';
    return true;
}

static int
llvm_stmt_find_labeled_loop_depth(LLVMGenCtx *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

void
llvm_defer_scope_push(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth >= MAX_SCOPE_DEPTH)
        return;
    ctx->defer_body_counts[ctx->defer_scope_depth++] = 0;
}

void
llvm_defer_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->defer_scope_depth <= 0)
        return;
    ctx->defer_scope_depth--;
    ctx->defer_body_counts[ctx->defer_scope_depth] = 0;
}

static void
llvm_register_defer(ASTNode *body, LLVMGenCtx *ctx)
{
    if (body == NULL || ctx->defer_scope_depth <= 0)
        return;
    int scope = ctx->defer_scope_depth - 1;
    int count = ctx->defer_body_counts[scope];
    if (count >= MAX_DEFER_PER_SCOPE)
        return;
    ctx->defer_bodies[scope][count] = body;
    ctx->defer_body_counts[scope]++;
}

void
llvm_emit_defers_from(LLVMGenCtx *ctx, int start_depth)
{
    if (start_depth < 0)
        start_depth = 0;
    for (int depth = ctx->defer_scope_depth - 1; depth >= start_depth; depth--) {
        for (int i = ctx->defer_body_counts[depth] - 1; i >= 0; i--) {
            ASTNode *body = ctx->defer_bodies[depth][i];
            if (body != NULL)
                llvm_emit_statement(body, ctx);
        }
    }
}

static void
llvm_emit_return_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    llvm_emit_defers_from(ctx, 0);

    if (node->data.return_stmt.value != NULL) {
        LLVMValueRef val = llvm_emit_expression(node->data.return_stmt.value,
                                                 ctx);
        if (val != NULL) {
            /* Coerce to expected return type */
            LLVMTypeRef val_type = LLVMTypeOf(val);
            LLVMTypeRef ret_type = ctx->current_ret_type;
            if (ret_type != val_type && ret_type != ctx->type_void) {
                bool ret_is_int = (ret_type == ctx->type_i32 || ret_type == ctx->type_i64);
                bool ret_is_fp  = (ret_type == ctx->type_f32 || ret_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (ret_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(ret_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_fp)
                    val = (ret_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
            }
            LLVMBuildRet(ctx->builder, val);
        } else {
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->current_ret_type));
        }
    } else {
        if (ctx->current_ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                         LLVMConstNull(ctx->current_ret_type));
    }
}

static void
llvm_emit_if_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef cond = llvm_emit_expression(node->data.if_stmt.condition, ctx);
    if (cond == NULL)
        return;

    /* Ensure cond is i1 */
    if (LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef then_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "then");
    LLVMBasicBlockRef else_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "else");
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "ifcont");

    LLVMBuildCondBr(ctx->builder, cond, then_bb, else_bb);

    /* Then block */
    LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
    if (node->data.if_stmt.then_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.then_branch, ctx);
    /* Only branch to merge if no terminator (return) was emitted */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Else block */
    LLVMPositionBuilderAtEnd(ctx->builder, else_bb);
    if (node->data.if_stmt.else_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.else_branch, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Merge */
    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    int saved_slot_count = ctx->slot_var_count;
    llvm_defer_scope_push(ctx);
    llvm_scope_push(ctx);
    for (size_t i = 0; i < node->data.block.count; i++) {
        llvm_emit_statement(node->data.block.statements[i], ctx);
        /* Stop emitting after a terminator (return) */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) != NULL)
            break;
    }

    /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
     * Skip slots already explicitly released by the user. */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        llvm_emit_defers_from(ctx, ctx->defer_scope_depth - 1);
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            if (ctx->slot_vars[i].released) continue;
            const char *inner = ctx->slot_vars[i].inner_type;
            const char *vname = ctx->slot_vars[i].var_name;
            char fn_name[64];
            bool is_secure = ctx->slot_vars[i].is_secure;
            snprintf(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, vname);
            if (fn != NULL && var != NULL) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, vname);
                    if (token_var != NULL) {
                        LLVMValueRef args[] = { var->alloca, token_var->alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    }
                } else {
                    LLVMValueRef args[] = { var->alloca };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                }
            } else if (is_secure && var != NULL) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 1, llvm_tmp_name(ctx));
                LLVMValueRef token_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 2, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
            } else if (!is_secure && var != NULL) {
                LLVMValueRef occ_ptr = LLVMBuildStructGEP2(ctx->builder,
                    var->type, var->alloca, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
                    occ_ptr);
            }
        }
    }

    ctx->slot_var_count = saved_slot_count;
    llvm_scope_pop(ctx);
    llvm_defer_scope_pop(ctx);
}

/* Parallel / async / select statement owners live in llvm_stmt_parallel_async.c. */

void
llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    /* If current block already has a terminator, skip */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        llvm_emit_let_decl(node, ctx);
        break;

    case AST_LET_DESTRUCTURE: {
        /* let (a, b, c) = expr;
         * Two shapes supported:
         *   1) Tuple: struct { T0, T1, ... } ??ExtractValue per field.
         *   2) Array-like: struct { T* data, i64 size, i64 cap } ??GEP + load. */
        ASTNode *init = node->data.let_destructure.initializer;
        if (init == NULL)
            break;
        LLVMValueRef rhs_val = llvm_emit_expression(init, ctx);
        if (rhs_val == NULL)
            break;
        LLVMTypeRef rhs_ty = LLVMTypeOf(rhs_val);
        if (LLVMGetTypeKind(rhs_ty) != LLVMStructTypeKind) {
            llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "destructuring requires an Array-like or tuple struct initializer");
            break;
        }

        /* Heuristic: tuple if struct field count equals the binding count
         * AND the first field is not a pointer (array-like has pointer as
         * the first field for `data`). */
        unsigned field_count = LLVMCountStructElementTypes(rhs_ty);
        bool is_tuple = false;
        if (field_count == (unsigned)node->data.let_destructure.name_count) {
            LLVMTypeRef f0 = LLVMStructGetTypeAtIndex(rhs_ty, 0);
            if (f0 != NULL && LLVMGetTypeKind(f0) != LLVMPointerTypeKind)
                is_tuple = true;
        }

        if (is_tuple) {
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
                const char *bname = node->data.let_destructure.names[i];
                if (bname == NULL) continue;
                LLVMTypeRef ft = LLVMStructGetTypeAtIndex(rhs_ty, (unsigned)i);
                LLVMValueRef v = LLVMBuildExtractValue(ctx->builder, rhs_val,
                    (unsigned)i, llvm_tmp_name(ctx));
                LLVMValueRef alloca = llvm_create_entry_alloca(ctx, ft, bname);
                LLVMBuildStore(ctx->builder, v, alloca);
                llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, ft);
            }
            break;
        }

        /* Array-like path (unchanged) */
        LLVMValueRef data_ptr = LLVMBuildExtractValue(ctx->builder,
            rhs_val, 0, llvm_tmp_name(ctx));
        LLVMTypeRef elem_type = llvm_stmt_resolve_array_elem_type(
            ctx, init, data_ptr);
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            const char *bname = node->data.let_destructure.names[i];
            if (bname == NULL)
                continue;
            LLVMValueRef idx = LLVMConstInt(ctx->type_i64,
                (unsigned long long)i, 0);
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_type, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            LLVMValueRef val = LLVMBuildLoad2(ctx->builder, elem_type,
                gep, llvm_tmp_name(ctx));
            LLVMValueRef alloca = llvm_create_entry_alloca(
                ctx, elem_type, bname);
            LLVMBuildStore(ctx->builder, val, alloca);
            llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, elem_type);
        }
        break;
    }

    case AST_RETURN:
        llvm_emit_return_stmt(node, ctx);
        break;

    case AST_BREAK:
        if (ctx->loop_depth > 0) {
            int target_depth = ctx->loop_depth - 1;
            if (node->data.break_stmt.label != NULL) {
                int found = llvm_stmt_find_labeled_loop_depth(
                    ctx, node->data.break_stmt.label);
                if (found >= 0)
                    target_depth = found;
            }
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[target_depth]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_break_blocks[target_depth]);
        }
        break;
    case AST_ENUM_DECL:
        /* Enums are compile-time only ??no IR needed */
        break;
    case AST_CONTINUE:
        if (ctx->loop_depth > 0) {
            int target_depth = ctx->loop_depth - 1;
            if (node->data.continue_stmt.label != NULL) {
                int found = llvm_stmt_find_labeled_loop_depth(
                    ctx, node->data.continue_stmt.label);
                if (found >= 0)
                    target_depth = found;
            }
            llvm_emit_defers_from(ctx,
                ctx->loop_defer_base_depth[target_depth]);
            LLVMBuildBr(ctx->builder,
                ctx->loop_continue_blocks[target_depth]);
        }
        break;

    case AST_IF_STMT:
        llvm_emit_if_stmt(node, ctx);
        break;

    case AST_WHILE_LOOP:
        llvm_emit_while_loop(node, ctx);
        break;

    case AST_FOR_LOOP:
        llvm_emit_for_loop(node, ctx);
        break;

    case AST_MATCH_STMT:
        llvm_emit_match_stmt(node, ctx);
        break;

    case AST_WITH_STMT:
        llvm_emit_with_stmt(node, ctx);
        break;

    case AST_BLOCK:
        llvm_emit_block(node, ctx);
        break;

    case AST_ASYNC_BLOCK:
        llvm_emit_async_block(node, ctx);
        break;

    case AST_PARALLEL_BLOCK:
        llvm_emit_parallel_block(node, ctx);
        break;

    case AST_SELECT_STMT:
        llvm_emit_select_stmt(node, ctx);
        break;

    case AST_FUNC_DECL:
    case AST_CLASS_DECL:
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
    case AST_EVENT_DECL:
    case AST_INTENT_DECL:
    case AST_IMPORT_DECL:
    case AST_NAMESPACE_DECL:
    case AST_TYPE_ALIAS:
    case AST_USE_DECL:
    case AST_INCLUDE_STMT:
    case AST_IMPL_ABILITY:
        /* Category 3 (top-level declarations) skip list.
         * See docs/95_ast_dispatch_partition.md for the partition model.
         *
         * These are consumed by the program pass (MIR-backed emitters in
         * llvm_pipeline.c / llvm_domain.c / llvm_intent.c / llvm_register.c).
         * Under current MIR-based emission they never reach this switch, but
         * the list absorbs any future dispatcher change that accidentally
         * routes a top-level decl into function-body statement context. */
        break;

    case AST_EXTERN_BLOCK:
        /* extern "C" { func ...; } ??handled in program pass (Pass 0) */
        break;

    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } ??emit body directly, no safety wrappers */
        if (node->data.unsafe_block.body != NULL)
            llvm_emit_block(node->data.unsafe_block.body, ctx);
        break;

    case AST_DEFER_STMT:
        if (node->data.defer_stmt.body != NULL)
            llvm_register_defer(node->data.defer_stmt.body, ctx);
        break;

    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * ??party_var.slot_vtable = &Role_Ability_vtable_instance */
        const char *party_var  = node->data.bind_stmt.party_var;
        const char *slot_name  = node->data.bind_stmt.slot_name;
        const char *role_name  = node->data.bind_stmt.role_name;

        if (party_var == NULL || slot_name == NULL || role_name == NULL)
            break;

        /* Look up the party variable */
        LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
        if (pvar == NULL) break;

        const char *party_class_name = llvm_lookup_var_class(ctx, party_var);
        LLVMClassTypeEntry *cls = party_class_name
            ? llvm_lookup_class(ctx, party_class_name) : NULL;
        if (cls == NULL) break;

        char vt_field[256];
        snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
        int field_idx = -1;
        for (int fi = 0; fi < cls->field_count; fi++) {
            if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                field_idx = cls->fields[fi].index;
                break;
            }
        }
        if (field_idx < 0) break;

        /* Find the Role's vtable global.
         * Convention: RoleName_AbilityName_vtable_instance */
        char global_prefix[256];
        snprintf(global_prefix, sizeof(global_prefix), "%s_", role_name);
        LLVMValueRef vt_global = NULL;
        LLVMValueRef g = LLVMGetFirstGlobal(ctx->module);
        while (g != NULL) {
            const char *gname = LLVMGetValueName(g);
            if (gname != NULL
                && strncmp(gname, global_prefix, strlen(global_prefix)) == 0
                && strstr(gname, "_vtable_instance") != NULL) {
                vt_global = g;
                break;
            }
            g = LLVMGetNextGlobal(g);
        }
        if (vt_global == NULL) break;

        /* GEP to vtable pointer field + store */
        LLVMValueRef party_alloca = pvar->alloca;
        LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
            cls->struct_type, party_alloca, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMValueRef vt_ptr = LLVMBuildBitCast(ctx->builder,
            vt_global, ctx->type_i8ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, vt_ptr, field_ptr);
        break;
    }

    /* Expression statements */
    case AST_CALL:
    case AST_ASSIGNMENT:
    case AST_BINARY:
    case AST_UNARY:
    case AST_IDENTIFIER:
    case AST_MEMBER_ACCESS:
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_ARRAY_ACCESS:
    case AST_PARTY_INSTANCE:
    case AST_CONTEXT_ACCESS:
    case AST_TASK_GROUP:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
    case AST_WORLD_ACTIVATE:
    case AST_WORLD_DEACTIVATE:
    case AST_WORLD_MAINTAIN:
    case AST_WORLD_STATE:
    case AST_ZONE_APPLY:
    case AST_ZONE_LINK:
    case AST_ZONE_DETACH:
    case AST_ZONE_UNLINK:
    case AST_ZONE_REFRESH:
    case AST_ZONE_AUTHORITY:
    case AST_ZONE_STATE:
        /* Category 2 (decl sub-metadata) safety-net forward.
         * See docs/95_ast_dispatch_partition.md for the partition model.
         *
         * These domain runtime verbs are created only as sub-nodes of
         * world_decl / zone_decl in parser_domain.c, and llvm_domain.c
         * consumes those arrays directly with dedicated handlers.  Under
         * the current parser they never reach this switch, but the
         * verbs' identifier semantics ("activate", "apply", "link", ??
         * make future function-body use plausible, so the forward routes
         * any accidental arrival into llvm_emit_expression ??where
         * PGY_CODE_LLVM_TYPE_UNSUPPORTED surfaces the breakage with an
         * actionable diagnostic instead of a silent no-op. */
        llvm_emit_expression(node, ctx);
        if (node->type == AST_CALL)
            llvm_stmt_emit_zone_action_effect_runtime(node, ctx);
        break;

    default:
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM statement emitter has no lowering for AST node type %d; add an explicit lowering or keep this node in declaration/domain metadata",
            (int)node->type);
        break;
    }
}

#endif /* PGY_LLVM_ENABLED */
