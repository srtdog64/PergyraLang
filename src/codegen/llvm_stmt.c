#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

/* =================================================================
 * Statement emission
 * ================================================================= */

static void
llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init     = node->data.let_decl.initializer;

    /* Detect ClaimSlot / ClaimSecureSlot */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        if (strcmp(callee, "ClaimSlot") == 0
            || strcmp(callee, "ClaimSecureSlot") == 0) {
            /* Resolve inner type from type annotation */
            const char *inner = "Int";
            if (type_ann != NULL && type_ann->type == AST_TYPE) {
                /* Check for generic args: Slot<Int> */
                if (type_ann->data.type.generic_args != NULL
                    && type_ann->data.type.generic_args->count > 0)
                    inner = type_ann->data.type.generic_args->params[0]->name;
                else if (type_ann->data.type.name != NULL) {
                    /* Try to extract inner from type name like "Slot_Int" */
                    const char *tn = type_ann->data.type.name;
                    if (strncmp(tn, "Slot", 4) == 0)
                        inner = "Int"; /* default */
                }
            }

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);

            /* Inline ClaimSlot: zero-init the struct and set claimed=true.
             * Avoids struct-return-by-value ABI mismatch between LLVM and C. */
            LLVMValueRef zero = LLVMConstNull(slot_ty);
            LLVMBuildStore(ctx->builder, zero, alloca_val);
            /* Set the 'claimed' field (index 1) to true */
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner);
            return;
        }
    }

    /* Slot sugar: let x: Slot<Int> = 42 → auto Claim + Write */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        const char *ann_name = type_ann->data.type.name;
        bool is_slot_sugar = (strcmp(ann_name, "Slot") == 0
                           || strncmp(ann_name, "Slot<", 5) == 0);
        if (is_slot_sugar) {
            const char *inner = "Int";
            if (type_ann->data.type.generic_args != NULL
                && type_ann->data.type.generic_args->count > 0)
                inner = type_ann->data.type.generic_args->params[0]->name;

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);

            /* Inline Claim: zero-init + set claimed=true */
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner);

            /* Auto Write the initializer value */
            if (init != NULL) {
                LLVMValueRef val = llvm_emit_expression(init, ctx);
                if (val != NULL) {
                    char fn_name[64];
                    snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", inner);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { alloca_val, val };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    }
                }
            }
            return;
        }
    }

    /* Detect class constructor: let v = ClassName(args...) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee);
        if (cls != NULL) {
            /* Allocate struct on stack */
            LLVMValueRef alloca_val = llvm_create_entry_alloca(
                ctx, cls->struct_type, name);

            /* Store each argument into corresponding field */
            size_t argc = init->data.call.arg_count;
            for (size_t i = 0; i < argc && (int)i < cls->field_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    init->data.call.arguments[i], ctx);
                LLVMValueRef gep = LLVMBuildStructGEP2(
                    ctx->builder, cls->struct_type, alloca_val,
                    (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, arg, gep);
            }

            llvm_scope_declare(ctx, name, alloca_val, cls->struct_type);
            llvm_register_var_class(ctx, name, callee);
            return;
        }
    }

    /* Detect Channel constructor: let ch: Channel<Int> = Channel(capacity) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "Channel") == 0) {
        /* Allocate opaque channel as a large-enough byte array.
         * PgyChannel_Int_RT on the runtime side is ~128 bytes;
         * we allocate 256 bytes for safety. */
        LLVMTypeRef ch_type = LLVMArrayType(
            LLVMInt8TypeInContext(ctx->context), 256);
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, ch_type, name);

        /* Call pgy_channel_init_Int(ptr, capacity) */
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
            "pgy_channel_init_Int");
        if (init_fn != NULL) {
            LLVMValueRef cap = LLVMConstInt(ctx->type_i64, 16, 0);
            if (init->data.call.arg_count > 0)
                cap = LLVMBuildZExt(ctx->builder,
                    llvm_emit_expression(init->data.call.arguments[0], ctx),
                    ctx->type_i64, llvm_tmp_name(ctx));
            LLVMValueRef args[] = { alloca_val, cap };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 2, "");
        }
        llvm_scope_declare(ctx, name, alloca_val, ch_type);
        return;
    }

    /* Determine type from annotation or initializer */
    LLVMTypeRef var_type = ctx->type_i32; /* default */
    if (type_ann != NULL)
        var_type = ast_type_to_llvm(ctx, type_ann);

    /* Create alloca at function entry */
    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, var_type, name);

    /* Store initializer if present */
    if (init != NULL) {
        LLVMValueRef val = llvm_emit_expression(init, ctx);
        if (val != NULL) {
            LLVMTypeRef val_type = LLVMTypeOf(val);

            /* Type coercion between numeric types */
            if (var_type != val_type) {
                bool var_is_int = (var_type == ctx->type_i32 || var_type == ctx->type_i64);
                bool var_is_fp  = (var_type == ctx->type_f32 || var_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (var_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(var_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_fp)
                    val = (var_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
            }

            LLVMBuildStore(ctx->builder, val, alloca);
        }
    }

    llvm_scope_declare(ctx, name, alloca, var_type);

    /* Track class type for member access */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx,
            type_ann->data.type.name);
        if (cls != NULL)
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
    }
}

static void
llvm_emit_return_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
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
            LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
        }
    } else {
        if (ctx->current_ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(ctx->current_ret_type, 0, 0));
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

static void
llvm_emit_while_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.body");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef cond = llvm_emit_expression(node->data.while_loop.condition,
                                              ctx);
    if (cond != NULL && LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));
    if (cond == NULL)
        cond = LLVMConstInt(ctx->type_i1, 0, 0);

    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (node->data.while_loop.body != NULL)
        llvm_emit_statement(node->data.while_loop.body, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
}

static void
llvm_emit_for_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *var_name = node->data.for_loop.variable;

    llvm_scope_push(ctx);

    /* Create loop variable */
    LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                        var_name);
    LLVMValueRef start = llvm_emit_expression(node->data.for_loop.range_start,
                                               ctx);
    if (start == NULL)
        start = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, var_name, var_alloca, ctx->type_i32);

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.body");
    LLVMBasicBlockRef incr_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.incr");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition: i < end */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                           var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef end = llvm_emit_expression(node->data.for_loop.range_end, ctx);
    if (end == NULL)
        end = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                                       llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (node->data.for_loop.body != NULL)
        llvm_emit_statement(node->data.for_loop.body, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, incr_bb);

    /* Increment: i = i + 1 */
    LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
    LLVMValueRef cur2 = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                        var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, cur2,
                                      LLVMConstInt(ctx->type_i32, 1, 0),
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, var_alloca);
    LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);

    llvm_scope_pop(ctx);
}

static void
llvm_emit_match_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef subject = llvm_emit_expression(node->data.match_stmt.subject,
                                                 ctx);
    if (subject == NULL)
        return;

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "match.end");

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;

        LLVMValueRef pattern = llvm_emit_expression(mc->data.match_case.pattern,
                                                     ctx);
        if (pattern == NULL)
            continue;

        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                          subject, pattern,
                                          llvm_tmp_name(ctx));

        LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.case");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.next");

        LLVMBuildCondBr(ctx->builder, cmp, case_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
        if (mc->data.match_case.body != NULL)
            llvm_emit_statement(mc->data.match_case.body, ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
    }

    /* Default case */
    if (node->data.match_stmt.default_body != NULL) {
        llvm_emit_statement(node->data.match_stmt.default_body, ctx);
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL
        && node->data.with_stmt.slot_type->type == AST_TYPE
        && node->data.with_stmt.slot_type->data.type.name != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
    LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, alias);

    /* Inline claim: zero-init + set claimed=true (avoids ABI mismatch) */
    char fn_name[64];
    (void)is_secure; /* both secure and normal get same init pattern */
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

    /* Push scope, register slot variable */
    llvm_scope_push(ctx);
    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner);

    /* Emit body */
    if (node->data.with_stmt.body != NULL)
        llvm_emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
        LLVMFuncEntry *release_fn = llvm_lookup_function(ctx, fn_name);
        if (release_fn != NULL) {
            LLVMValueRef args[] = { alloca_val };
            LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                           release_fn->fn, args, 1, "");
        }
    }

    llvm_scope_pop(ctx);
}

void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    int saved_slot_count = ctx->slot_var_count;
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
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            if (ctx->slot_vars[i].released) continue;
            const char *inner = ctx->slot_vars[i].inner_type;
            const char *vname = ctx->slot_vars[i].var_name;
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, vname);
            if (fn != NULL && var != NULL) {
                LLVMValueRef args[] = { var->alloca };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
            }
        }
    }

    ctx->slot_var_count = saved_slot_count;
    llvm_scope_pop(ctx);
}

/* =================================================================
 * Parallel block — real concurrency via thread pool
 *
 * For each task, generate an LLVM function `_pgy_par_N(i8*) -> i8*`
 * that contains the task body, then spawn all + await all.
 * ================================================================= */

static void
llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    /* -----------------------------------------------------------
     * 1) Collect all variables from the current scope stack.
     *    These will be captured into a context struct so that
     *    wrapper functions can access them.
     * ----------------------------------------------------------- */
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }

    /* -----------------------------------------------------------
     * 2) Build a context struct type: { ptr, ptr, ... }
     *    Each field is a pointer to the captured variable's alloca.
     *    In the wrapper, we GEP to get the pointer, then load/store
     *    through it — exactly like the C transpiler's approach.
     * ----------------------------------------------------------- */
    LLVMTypeRef *ctx_fields = calloc((size_t)n_captured, sizeof(LLVMTypeRef));
    for (int i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;   /* all fields are opaque ptr */

    char ctx_name[64];
    snprintf(ctx_name, sizeof(ctx_name), "_pgy_par_ctx_%d", ctx->parallel_counter);
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_captured, 0);
    free(ctx_fields);

    /* -----------------------------------------------------------
     * 3) In the OUTER function: allocate + fill the context struct.
     * ----------------------------------------------------------- */
    LLVMValueRef ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type,
                                               "_pctx");
    for (int i = 0; i < n_captured; i++) {
        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                                                 ctx_alloca, (unsigned)i,
                                                 llvm_tmp_name(ctx));
        /* Store the alloca address (pointer to the variable) */
        LLVMBuildStore(ctx->builder, captured[i].alloca, gep);
    }

    /* Cast context struct pointer to i8* for spawn argument */
    LLVMValueRef ctx_i8ptr = LLVMBuildBitCast(ctx->builder, ctx_alloca,
                                               ctx->type_i8ptr,
                                               llvm_tmp_name(ctx));

    /* -----------------------------------------------------------
     * 4) Generate wrapper functions for each parallel task.
     *    Each wrapper receives the context struct as i8* arg,
     *    casts it back, and GEPs to access captured variable pointers.
     * ----------------------------------------------------------- */
    LLVMValueRef    saved_fn  = ctx->current_function;
    LLVMTypeRef     saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                 wrapper_params, 1, 0);

    LLVMValueRef *wrapper_fns = calloc(count, sizeof(LLVMValueRef));

    for (size_t i = 0; i < count; i++) {
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "_pgy_par_%d_%zu",
                 ctx->parallel_counter, i);

        LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
        wrapper_fns[i] = fn;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = fn;
        ctx->current_ret_type = ctx->type_i8ptr;

        llvm_scope_push(ctx);

        /* Cast arg (i8*) back to context struct pointer */
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        /* For each captured variable: GEP → load pointer → declare in scope.
         * The loaded pointer points to the original alloca, so
         * load/store through it accesses the outer variable. */
        for (int c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            /* Declare in wrapper scope — the "alloca" is actually the
             * loaded pointer to the outer function's alloca.  Since
             * llvm_emit_identifier does Load2(type, alloca, ...) and
             * store operations do Store(val, alloca), this transparent
             * pointer indirection works correctly. */
            llvm_scope_declare(ctx, captured[c].name, var_ptr, captured[c].type);
        }

        /* Emit the task body */
        llvm_emit_statement(node->data.parallel.tasks[i], ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
    }

    ctx->parallel_counter++;

    /* Restore insertion point */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    /* -----------------------------------------------------------
     * 5) Spawn all tasks, await all.
     * ----------------------------------------------------------- */
    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_spawn_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");

    if (spawn_fn == NULL || await_fn == NULL) {
        /* Fallback: emit sequentially */
        for (size_t i = 0; i < count; i++)
            llvm_emit_statement(node->data.parallel.tasks[i], ctx);
        free(wrapper_fns);
        return;
    }

    LLVMValueRef *handles = calloc(count, sizeof(LLVMValueRef));
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef fn_ptr = LLVMBuildBitCast(
            ctx->builder, wrapper_fns[i], ctx->type_i8ptr,
            llvm_tmp_name(ctx));

        LLVMValueRef args[] = { fn_ptr, ctx_i8ptr };
        handles[i] = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type,
                                     spawn_fn->fn, args, 2,
                                     llvm_tmp_name(ctx));
    }

    for (size_t i = 0; i < count; i++) {
        LLVMValueRef args[] = { handles[i] };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type,
                       await_fn->fn, args, 1, "");
    }

    free(handles);
    free(wrapper_fns);
}

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

    case AST_RETURN:
        llvm_emit_return_stmt(node, ctx);
        break;

    case AST_BREAK:
        /* TODO: LLVM break requires loop exit block tracking */
        break;
    case AST_ENUM_DECL:
        /* Enums are compile-time only — no IR needed */
        break;
    case AST_CONTINUE:
        /* TODO: LLVM continue requires loop header block tracking */
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
        /* MVP: emit contained statements sequentially */
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            llvm_emit_statement(node->data.async_block.statements[i], ctx);
        break;

    case AST_PARALLEL_BLOCK:
        llvm_emit_parallel_block(node, ctx);
        break;

    case AST_SELECT_STMT:
        /* MVP: emit cases sequentially (first match wins) */
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            if (node->data.select_stmt.cases[i] != NULL)
                llvm_emit_statement(node->data.select_stmt.cases[i], ctx);
        }
        if (node->data.select_stmt.default_case != NULL)
            llvm_emit_statement(node->data.select_stmt.default_case, ctx);
        break;

    case AST_FUNC_DECL:
    case AST_CLASS_DECL:
    case AST_ACTOR_DECL:
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
    case AST_PARTY_DECL:
    case AST_SYSTEMIC_DECL:
    case AST_WORLD_DECL:
    case AST_EVENT_DECL:
    case AST_IMPORT_DECL:
        /* Handled in program pass or declaration-only — skip here */
        break;

    case AST_EXTERN_BLOCK:
        /* extern "C" { func ...; } — handled in program pass (Pass 0) */
        break;

    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } — emit body directly, no safety wrappers */
        if (node->data.unsafe_block.body != NULL)
            llvm_emit_block(node->data.unsafe_block.body, ctx);
        break;

    case AST_DEFER_STMT:
        /* defer { ... } — emit at end of current scope
         * For now: emit inline (proper scope-exit requires goto-cleanup) */
        if (node->data.defer_stmt.body != NULL)
            llvm_emit_statement(node->data.defer_stmt.body, ctx);
        break;

    case AST_BIND_STMT:
        /* bind party.slot = Role; — runtime vtable swap
         * Minimal stub: emits nothing (vtable binding is compile-time) */
        break;

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
        llvm_emit_expression(node, ctx);
        break;

    default:
        fprintf(stderr, "[llvm] warning: unhandled statement AST type %d\n",
                (int)node->type);
        break;
    }
}

#endif /* PGY_LLVM_ENABLED */
