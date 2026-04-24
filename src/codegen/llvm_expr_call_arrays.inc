static bool
llvm_emit_array_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                             const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    if (strcmp(callee_name, "ArrayLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef arr = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (arr != NULL && LLVMGetTypeKind(LLVMTypeOf(arr)) == LLVMStructTypeKind) {
            LLVMValueRef len = llvm_array_length_i64(ctx, arr);
            *out = LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
            return true;
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ArrayPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL || entry == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_array_push_%s", suffix);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { arr_var->alloca, value };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ArraySet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        LLVMValueRef idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (arr_var == NULL || entry == NULL || idx == NULL || value == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type, arr_var->alloca,
            llvm_tmp_name(ctx));
        LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type, data_ptr, &idx, 1,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, gep);
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ArrayPop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type, arr_var->alloca,
            llvm_tmp_name(ctx));
        LLVMValueRef len = llvm_array_length_i64(ctx, arr);
        LLVMValueRef has_any = LLVMBuildICmp(ctx->builder, LLVMIntUGT, len,
            LLVMConstInt(ctx->type_i64, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef dec = LLVMBuildSub(ctx->builder, len,
            LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
        LLVMValueRef next_len = LLVMBuildSelect(ctx->builder, has_any, dec, len,
            llvm_tmp_name(ctx));
        arr = LLVMBuildInsertValue(ctx->builder, arr, next_len, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, arr, arr_var->alloca);
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    return false;
}
