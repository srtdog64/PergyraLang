        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (fn == NULL || val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "DeviceRead") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM DeviceRead on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "ReleaseDeviceSlot") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM ReleaseDeviceSlot on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_release_device_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        if (slot_arg->type == AST_IDENTIFIER)
            llvm_mark_device_slot_released(ctx, slot_arg->data.identifier.name);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SubmitDeviceRead") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM SubmitDeviceRead on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return NULL;
        }
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_submit_device_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    /* Event invocation: OnHit(42) → OnHit_INVOKE(&OnHit, 42) */
    {
        LLVMEventTypeEntry *evt = llvm_lookup_event(ctx, callee_name);
        if (evt != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMValueRef ev_ptr = LLVMGetNamedGlobal(ctx->module, callee_name);
            if (ev_ptr == NULL) {
                LLVMVarEntry *ev = llvm_scope_lookup(ctx, callee_name);
                if (ev != NULL) ev_ptr = ev->alloca;
            }
            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.call.arg_count;
                LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                    (ac + 1) * sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.call.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    /* Built-in: Abs(x) → select(x < 0, -x, x) */
    if (strcmp(callee_name, "Abs") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef x = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
    }

    /* Built-in: Min(a, b) → select(a < b, a, b) */
    if (strcmp(callee_name, "Min") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: Max(a, b) → select(a > b, a, b) */
    if (strcmp(callee_name, "Max") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: ArrayLength(arr) */
    if (strcmp(callee_name, "ArrayLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef arr = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (arr != NULL && LLVMGetTypeKind(LLVMTypeOf(arr)) == LLVMStructTypeKind) {
            LLVMValueRef len = llvm_array_length_i64(ctx, arr);
            return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArrayPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL || entry == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
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
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArraySet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        LLVMValueRef idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (arr_var == NULL || entry == NULL || idx == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

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
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArrayPop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

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
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

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
        return LLVMBuildLoad2(ctx->builder, list_ty, tmp, llvm_tmp_name(ctx));
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
        return LLVMBuildLoad2(ctx->builder, set_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "SetAdd") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
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
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SetHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
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
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3,
                                  llvm_tmp_name(ctx));
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
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        inner_name = llvm_lookup_set_inner(ctx, set_arg->data.identifier.name);
        if (set_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
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
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SetSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *set_arg = node->data.call.arguments[0];
        LLVMVarEntry *set_var;
        LLVMFuncEntry *fn;
        if (set_arg == NULL || set_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        set_var = llvm_scope_lookup(ctx, set_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_set_size_raw_export");
        if (set_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, set_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
                                  llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "ListPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
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
        fn = llvm_lookup_function(ctx, "pgy_list_push_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ListGet") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        fn = llvm_lookup_function(ctx, "pgy_list_get_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        return LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "ListSet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (idx == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
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
        fn = llvm_lookup_function(ctx, "pgy_list_set_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ListSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_list_size_raw_export");
        if (list_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "ListRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *list_arg = node->data.call.arguments[0];
        LLVMVarEntry *list_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef idx;
        LLVMFuncEntry *fn;
        if (list_arg == NULL || list_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        list_var = llvm_scope_lookup(ctx, list_arg->data.identifier.name);
        inner_name = llvm_lookup_list_inner(ctx, list_arg->data.identifier.name);
        if (list_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(idx) != ctx->type_i32)
            idx = LLVMBuildTruncOrBitCast(ctx->builder, idx, ctx->type_i32, llvm_tmp_name(ctx));
        fn = llvm_lookup_function(ctx, "pgy_list_remove_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                idx,
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "QueuePush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        inner_name = llvm_lookup_queue_inner(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
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
        fn = llvm_lookup_function(ctx, "pgy_queue_push_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "QueuePop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        inner_name = llvm_lookup_queue_inner(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_ty = pergyra_type_to_llvm(ctx, inner_name != NULL ? inner_name : "Int");
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        fn = llvm_lookup_function(ctx, "pgy_queue_pop_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "QueueSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_queue_size_raw_export");
        if (queue_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "QueueEmpty") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i1, 1, 0);
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_queue_empty_raw_export");
        if (queue_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i1, 1, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "MapSet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        value_ty = pergyra_type_to_llvm(ctx, value_name != NULL ? value_name : "Int");
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (key == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != value_ty) {
            if ((value_ty == ctx->type_i32 || value_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
            else if ((value_ty == ctx->type_f32 || value_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, value_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_set_raw_i32_export"
                : key_name != NULL && strcmp(key_name, "Long") == 0
                    ? "pgy_map_set_raw_i64_export"
                    : key_name != NULL && strcmp(key_name, "Bool") == 0
                        ? "pgy_map_set_raw_bool_export"
                        : "pgy_map_set_raw_export");
        if (fn != NULL) {
            if (key_name != NULL
                && (strcmp(key_name, "Int") == 0
                    || strcmp(key_name, "Long") == 0
                    || strcmp(key_name, "Bool") == 0)) {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    key,
                    LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
            } else {
                LLVMValueRef args[] = {
                    LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    key,
                    LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                    llvm_sizeof_type_i64(ctx, value_ty)
                };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "MapGet") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        value_name = llvm_lookup_map_value(ctx, map_arg->data.identifier.name);
        if (map_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        value_ty = pergyra_type_to_llvm(ctx, value_name != NULL ? value_name : "Int");
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (key == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_get_raw_i32_export"
