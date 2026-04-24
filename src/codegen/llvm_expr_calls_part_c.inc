                : key_name != NULL && strcmp(key_name, "Long") == 0
                    ? "pgy_map_get_raw_i64_export"
                    : key_name != NULL && strcmp(key_name, "Bool") == 0
                        ? "pgy_map_get_raw_bool_export"
                        : "pgy_map_get_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 4, "");
        }
        return LLVMBuildLoad2(ctx->builder, value_ty, tmp, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "MapHas") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMValueRef key;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        key = llvm_emit_expression(node->data.call.arguments[1], ctx);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_has_raw_i32_export"
                : key_name != NULL && strcmp(key_name, "Long") == 0
                    ? "pgy_map_has_raw_i64_export"
                    : key_name != NULL && strcmp(key_name, "Bool") == 0
                        ? "pgy_map_has_raw_bool_export"
                        : "pgy_map_has_raw_export");
        if (map_var == NULL || key == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "MapRemove") == 0 && node->data.call.arg_count == 2) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        const char *value_name;
        LLVMTypeRef value_ty;
        LLVMValueRef key;
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
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_remove_raw_i32_export"
                : key_name != NULL && strcmp(key_name, "Long") == 0
                    ? "pgy_map_remove_raw_i64_export"
                    : key_name != NULL && strcmp(key_name, "Bool") == 0
                        ? "pgy_map_remove_raw_bool_export"
                        : "pgy_map_remove_raw_export");
        if (key == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                key,
                llvm_sizeof_type_i64(ctx, value_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "MapSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        fn = llvm_lookup_function(ctx, "pgy_map_size_raw_export");
        if (map_var == NULL || fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "MapKeys") == 0 && node->data.call.arg_count == 1) {
        ASTNode *map_arg = node->data.call.arguments[0];
        LLVMVarEntry *map_var;
        const char *key_name;
        LLVMTypeRef array_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (map_arg == NULL || map_arg->type != AST_IDENTIFIER)
            return LLVMConstNull(ctx->array_type_String);
        map_var = llvm_scope_lookup(ctx, map_arg->data.identifier.name);
        key_name = llvm_lookup_map_key(ctx, map_arg->data.identifier.name);
        array_ty = (key_name != NULL && strcmp(key_name, "Int") == 0)
            ? ctx->array_type_Int
            : (key_name != NULL && strcmp(key_name, "Long") == 0)
                ? ctx->array_type_Long
                : (key_name != NULL && strcmp(key_name, "Bool") == 0)
                    ? ctx->array_type_Bool
                    : ctx->array_type_String;
        if (map_var == NULL)
            return LLVMConstNull(array_ty);
        tmp = llvm_create_entry_alloca(ctx, array_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(array_ty), tmp);
        fn = llvm_lookup_function(ctx,
            key_name != NULL && strcmp(key_name, "Int") == 0
                ? "pgy_map_keys_raw_i32_export"
                : key_name != NULL && strcmp(key_name, "Long") == 0
                    ? "pgy_map_keys_raw_i64_export"
                    : key_name != NULL && strcmp(key_name, "Bool") == 0
                        ? "pgy_map_keys_raw_bool_export"
                        : "pgy_map_keys_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, map_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMBuildLoad2(ctx->builder, array_ty, tmp, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "ViewRead") == 0
         || strcmp(callee_name, "ViewWrite") == 0
         || strcmp(callee_name, "Move") == 0)
        && node->data.call.arg_count == 1) {
        return llvm_emit_expression(node->data.call.arguments[0], ctx);
    }

    /* Built-in: StringLength(s) → call strlen */
    if (strcmp(callee_name, "StringLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef s = llvm_emit_expression(node->data.call.arguments[0], ctx);
        /* Declare strlen if not already */
        LLVMFuncEntry *strlen_fn = llvm_lookup_function(ctx, "strlen");
        if (strlen_fn == NULL) {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, "strlen", ft);
            llvm_register_function(ctx, "strlen", fn, ft, ctx->type_i64);
            strlen_fn = llvm_lookup_function(ctx, "strlen");
        }
        LLVMValueRef args[] = { s };
        LLVMValueRef len = LLVMBuildCall2(ctx->builder, strlen_fn->fn_type,
            strlen_fn->fn, args, 1, llvm_tmp_name(ctx));
        return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "Contains") == 0
         || strcmp(callee_name, "StringContains") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringContains");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if ((strcmp(callee_name, "Replace") == 0
         || strcmp(callee_name, "StringReplace") == 0)
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringReplace");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
    }

    if (strcmp(callee_name, "Substring") == 0
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Substring");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
    }

    if ((strcmp(callee_name, "Trim") == 0
         || strcmp(callee_name, "StringTrim") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringTrim");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Upper") == 0
         || strcmp(callee_name, "ToUpper") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToUpper");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Lower") == 0
         || strcmp(callee_name, "ToLower") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToLower");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Concat") == 0
         || strcmp(callee_name, "StringConcat") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "ReadFile") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_read_file");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "ToString") == 0
        && node->data.call.arg_count == 1) {
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return llvm_coerce_value_to_string(value, ctx);
    }

    if (strcmp(callee_name, "ToInt") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToInt");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "ToFloat") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToFloat");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "Random") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Random");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "WriteFile") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_write_file");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "Input") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentLastTrace") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_trace_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastFailure") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_failure_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastName") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_name_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastHandle") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_handle_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastTraceId") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_trace_id_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastStepCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_step_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentLastFailed") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_last_failed_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentHistoryCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentHistoryStepName") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepZone") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepPhase") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_phase_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepParticipant") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_participant_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepSlot") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepFromZone") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_from_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepFromSlot") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_from_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepToZone") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_to_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepToSlot") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_to_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepOk") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_ok_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentHistoryStepFailure") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_history_step_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentActiveName") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveHandle") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_handle_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveParentHandle") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_parent_handle_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveTraceId") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_trace_id_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActivePriority") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_priority_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveSubjectCount") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_subject_count_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveStepCount") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_count_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveConcurrent") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_concurrent_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveFailed") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_failed_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveFailure") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveTrace") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_trace_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentActiveStepName") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepZone") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepPhase") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_phase_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepParticipant") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_participant_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepSlot") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepFromZone") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_from_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepFromSlot") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_from_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepToZone") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_to_zone_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepToSlot") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_to_slot_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepOk") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_ok_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentActiveStepFailure") == 0
        && node->data.call.arg_count == 2) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_active_step_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 2);
    }
    if (strcmp(callee_name, "IntentCurrentHandle") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_current_handle_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "IntentRecentCount") == 0
        && node->data.call.arg_count == 0) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_count_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
    }
    if (strcmp(callee_name, "IntentRecentHandle") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_handle_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }
    if (strcmp(callee_name, "IntentRecentTraceId") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_trace_id_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentName") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_name_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentTrace") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_trace_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentFailure") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_failure_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentStepCount") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_step_count_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "IntentRecentFailed") == 0
        && node->data.call.arg_count == 1) {
        ctx->uses_intent_observability = true;
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_intent_recent_failed_export");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn, node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "SeedRandom") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "SeedRandom");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "FileOpen") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_open");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "FileRead") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_read");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "FileWrite") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_write");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "FileClose") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_close");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    /* Built-in: Print(s) → printf("%s", s) */
    if (strcmp(callee_name, "Print") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef vt = LLVMTypeOf(val);
        LLVMFuncEntry *pf = llvm_lookup_function(ctx, "printf");
        if (pf != NULL) {
            if (vt == ctx->type_i8ptr) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%s", ".fmt_s");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%d", ".fmt_d");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ReadLine") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL) {
            LLVMValueRef empty = LLVMBuildGlobalStringPtr(ctx->builder, "", ".readline_empty");
            LLVMValueRef args[] = { empty };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
    }

    if (strcmp(callee_name, "Now") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_now_ms");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0, "");
    }

    if (strcmp(callee_name, "Sleep") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_sleep_ms");
        if (fn != NULL) {
            LLVMValueRef arg = llvm_emit_expression(node->data.call.arguments[0], ctx);
            LLVMValueRef args[] = { arg };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Ok(value) → Result<T, E>; prefer active expected layout. */
    if (strcmp(callee_name, "Ok") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 3) {
            result_ty = ctx->current_ret_type;
            LLVMGetStructElementTypes(result_ty, fields);
        } else {
            LLVMTypeRef fallback_fields[] = { ctx->type_i32, LLVMTypeOf(val), ctx->type_i8ptr };
            result_ty = LLVMStructTypeInContext(ctx->context, fallback_fields, 3, 0);
            fields[0] = fallback_fields[0];
            fields[1] = fallback_fields[1];
            fields[2] = fallback_fields[2];
        }
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        if (LLVMTypeOf(val) != fields[1]) {
            LLVMTypeRef val_ty = LLVMTypeOf(val);
            if ((fields[1] == ctx->type_i32 || fields[1] == ctx->type_i64)
                && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(fields[1]) > LLVMGetIntTypeWidth(val_ty))
                    ? LLVMBuildSExt(ctx->builder, val, fields[1], llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if ((fields[1] == ctx->type_f32 || fields[1] == ctx->type_f64)
                       && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if ((fields[1] == ctx->type_i32 || fields[1] == ctx->type_i64)
                       && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if (LLVMGetTypeKind(fields[1]) == LLVMPointerTypeKind
                       && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind) {
