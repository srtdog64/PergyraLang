static bool
llvm_emit_collection_base_call(ASTNode *node, LLVMGenCtx *ctx,
                               const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    if (strcmp(callee_name, "ListNew") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef list_ty = ctx->type_i32;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_list_new_raw_export");
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind) {
            list_ty = ctx->current_ret_type;
        }
        tmp = llvm_create_entry_alloca(ctx, list_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(list_ty), tmp);
        if (fn != NULL) {
            LLVMTypeRef elem_ty = LLVMInt32TypeInContext(ctx->context);
            if (LLVMCountStructElementTypes(list_ty) > 0) {
                LLVMTypeRef fields[3];
                LLVMGetStructElementTypes(list_ty, fields);
                elem_ty = LLVMGetElementType(fields[0]);
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMBuildLoad2(ctx->builder, list_ty, tmp, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "SetNew") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef set_ty = ctx->type_i32;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_set_new_raw_export");
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind) {
            set_ty = ctx->current_ret_type;
        }
        tmp = llvm_create_entry_alloca(ctx, set_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(set_ty), tmp);
        if (fn != NULL) {
            LLVMTypeRef elem_ty = LLVMInt32TypeInContext(ctx->context);
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMBuildLoad2(ctx->builder, set_ty, tmp, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "SetAdd") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_set_add_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "SetHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_set_has_raw_export");
        if (fn == NULL) {
            *out = LLVMConstInt(ctx->type_i1, 0, 0);
            return true;
        }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3,
                                  llvm_tmp_name(ctx));
            return true;
        }
    }

    if (strcmp(callee_name, "SetRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx, "pgy_set_remove_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "SetSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_set_size_raw_export");
        if (set_var == NULL || fn == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                                  llvm_tmp_name(ctx));
            return true;
        }
    }

    return false;
}
