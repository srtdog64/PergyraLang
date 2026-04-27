static LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.call.callee == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Method call: obj.method(args) */
    if (node->data.call.callee->type == AST_MEMBER_ACCESS)
        return llvm_emit_member_call(node, ctx);

    /* Get callee name */
    const char *callee_name = NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        callee_name = node->data.call.callee->data.identifier.name;

    if (callee_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (strcmp(callee_name, "Clone") == 0 && node->data.call.arg_count == 1) {
        return llvm_emit_expression(node->data.call.arguments[0], ctx);
    }

    {
        LLVMValueRef constructor_value =
            llvm_emit_constructor_call(node, ctx, callee_name);
        if (constructor_value != NULL)
            return constructor_value;
    }

    if ((strcmp(callee_name, "ToTObject") == 0 || strcmp(callee_name, "ToObject") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_subject_projection(node, ctx);
    }

    {
        LLVMValueRef domain_query = NULL;
        if (llvm_emit_domain_query_call(node, ctx, callee_name, &domain_query))
            return domain_query;
    }

    {
        LLVMValueRef log_call = NULL;
        if (llvm_emit_log_family_call(node, ctx, callee_name, &log_call))
            return log_call;
    }

    {
        LLVMValueRef rc_call = NULL;
        if (llvm_emit_rc_builtin_call(node, ctx, callee_name, &rc_call))
            return rc_call;
    }

    {
        LLVMValueRef slot_builtin = NULL;
        if (llvm_emit_slot_builtin_call(node, ctx, callee_name, &slot_builtin))
            return slot_builtin;
    }

    {
        LLVMValueRef event_call = NULL;
        if (llvm_emit_event_invocation_call(node, ctx, callee_name, &event_call))
            return event_call;
    }

    {
        LLVMValueRef math_call = NULL;
        if (llvm_emit_scalar_math_call(node, ctx, callee_name, &math_call))
            return math_call;
    }

    {
        LLVMValueRef array_call = NULL;
        if (llvm_emit_array_builtin_call(node, ctx, callee_name, &array_call))
            return array_call;
    }

    {
        LLVMValueRef collection_call = NULL;
        if (llvm_emit_collection_base_call(node, ctx, callee_name, &collection_call))
            return collection_call;
    }

    {
        LLVMValueRef collection_ext = NULL;
        if (llvm_emit_collection_extended_call(node, ctx, callee_name, &collection_ext))
            return collection_ext;
    }

    {
        LLVMValueRef stdlib_call = NULL;
        if (llvm_emit_stdlib_string_file_call(node, ctx, callee_name, &stdlib_call))
            return stdlib_call;
    }

    {
        LLVMValueRef intent_call = NULL;
        if (llvm_emit_intent_observability_call(node, ctx, callee_name, &intent_call))
            return intent_call;
    }
    {
        LLVMValueRef runtime_io_call = NULL;
        if (llvm_emit_stdlib_runtime_io_call(node, ctx, callee_name,
                &runtime_io_call)) {
            return runtime_io_call;
        }
    }

    {
        LLVMValueRef result_option_call = llvm_emit_result_option_call(node, ctx, callee_name);
        if (result_option_call != NULL)
            return result_option_call;
    }

    {
        LLVMValueRef task_channel_call = llvm_emit_task_channel_call(node, ctx, callee_name);
        if (task_channel_call != NULL)
            return task_channel_call;
    }
    if (node->data.call.callee->type == AST_IDENTIFIER) {
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *host_method = llvm_current_host_method_decl(ctx, callee_name);
        if (host_name != NULL && host_method != NULL) {
            char full_name[256];
            snprintf(full_name, sizeof(full_name), "%s_%s", host_name, callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = node->data.call.arg_count;
                LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                    (argc + 1) * sizeof(LLVMValueRef));
                if (args == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                args[0] = llvm_current_self_call_arg(ctx);
                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(node->data.call.arguments[i], ctx);
                    size_t logical_idx = 0;
                    for (size_t pk = 0; pk < host_method->data.func_decl.param_count; pk++) {
                        FuncParam *p = host_method->data.func_decl.params[pk];
                        const char *ptn = NULL;
                        LLVMClassTypeEntry *param_cls = NULL;
                        if (p->type == NULL && strcmp(p->name, "self") == 0)
                            continue;
                        if (logical_idx == i) {
                            if (p->type != NULL && p->type->type == AST_TYPE)
                                ptn = p->type->data.type.name;
                            param_cls = ptn != NULL ? llvm_lookup_class(ctx, ptn) : NULL;
                            if (param_cls != NULL && param_cls->is_pointer_self_host
                                && node->data.call.arguments[i] != NULL
                                && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                const char *arg_name =
                                    node->data.call.arguments[i]->data.identifier.name;
                                LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                if (arg_var != NULL) {
                                    if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                        arg_val = LLVMBuildLoad2(ctx->builder,
                                            arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                    else
                                        arg_val = arg_var->alloca;
                                }
                            }
                            break;
                        }
                        logical_idx++;
                    }
                    args[i + 1] = arg_val;
                }
                {
                    LLVMValueRef result;
                    if (fn->ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), "");
                        result = LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    }
                    return result;
                }
            }
        }
    }

    size_t argc = node->data.call.arg_count;
    ASTNode *decl = llvm_find_function_decl(ctx, callee_name);
    ASTNode *intent_decl = decl == NULL ? llvm_find_intent_decl(ctx, callee_name) : NULL;
    unsigned emitted_argc = 0;
    LLVMValueRef *args = NULL;

    if (decl != NULL)
        args = llvm_build_boundary_call_args(ctx, decl, node->data.call.arguments,
            argc, &emitted_argc);
    if (args == NULL && intent_decl != NULL) {
        args = pgy_arena_calloc(&ctx->scratch,
            (argc > 0 ? argc : 1) * sizeof(LLVMValueRef));
        if (args != NULL) {
            for (size_t i = 0; i < argc; i++) {
                const char *type_name = NULL;
                bool pointer_self = false;
                ASTNode *arg_node = node->data.call.arguments[i];

                {
                    size_t binding_count = intent_decl->data.intent_decl.binding_count > 0
                        ? intent_decl->data.intent_decl.binding_count
                        : (intent_decl->data.intent_decl.involve_count
                            + intent_decl->data.intent_decl.value_count);
                    if (i < binding_count) {
                        ASTNode *binding = intent_decl->data.intent_decl.binding_count > 0
                            ? intent_decl->data.intent_decl.bindings[i]
                            : (i < intent_decl->data.intent_decl.involve_count
                                ? intent_decl->data.intent_decl.involves[i]
                                : intent_decl->data.intent_decl.values[
                                    i - intent_decl->data.intent_decl.involve_count]);
                        if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                            && binding->data.intent_involves.subject_type != NULL
                            && binding->data.intent_involves.subject_type->type == AST_TYPE) {
                            type_name = binding->data.intent_involves.subject_type->data.type.name;
                        } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                            && binding->data.intent_value.value_type != NULL
                            && binding->data.intent_value.value_type->type == AST_TYPE) {
                            type_name = binding->data.intent_value.value_type->data.type.name;
                        }
                    }
                }
                pointer_self = llvm_type_name_uses_pointer_self(ctx, type_name);
                if (pointer_self) {
                    if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                        const char *arg_name = arg_node->data.identifier.name;
                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                        if (arg_var != NULL) {
                            if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                args[i] = LLVMBuildLoad2(ctx->builder, arg_var->type,
                                    arg_var->alloca, llvm_tmp_name(ctx));
                            else
                                args[i] = arg_var->alloca;
                        } else {
                            ASTNode *host_decl = llvm_current_host_decl(ctx);
                            const char *host_name = llvm_decl_node_name(host_decl);
                            LLVMClassTypeEntry *host_cls =
                                host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
                            if (host_cls != NULL) {
                                int field_idx = llvm_class_field_index(host_cls, arg_name);
                                if (field_idx >= 0) {
                                    LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, host_cls);
                                    if (base_ptr != NULL) {
                                        args[i] = LLVMBuildStructGEP2(ctx->builder,
                                            host_cls->struct_type, base_ptr,
                                            (unsigned)field_idx, llvm_tmp_name(ctx));
                                    }
                                }
                            }
                        }
                    } else if (arg_node != NULL && arg_node->type == AST_MEMBER_ACCESS) {
                        args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                    }
                }
                if (args[i] == NULL)
                    args[i] = llvm_emit_expression(arg_node, ctx);
            }
            emitted_argc = (unsigned)argc;
        }
    }
    if (args == NULL) {
        args = pgy_arena_calloc(&ctx->scratch,
            (argc > 0 ? argc : 1) * sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);
        emitted_argc = (unsigned)argc;
    }

    LLVMFuncEntry *func = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    if (func == NULL && decl != NULL && decl->type == AST_FUNC_DECL) {
        llvm_forward_declare_func(decl, ctx);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL && intent_decl != NULL) {
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef fn_type;
        LLVMValueRef fn;

        if (intent_decl->data.intent_decl.binding_count > 0
            || intent_decl->data.intent_decl.involve_count > 0
            || intent_decl->data.intent_decl.value_count > 0) {
            size_t param_count = intent_decl->data.intent_decl.binding_count > 0
                ? intent_decl->data.intent_decl.binding_count
                : (intent_decl->data.intent_decl.involve_count
                    + intent_decl->data.intent_decl.value_count);
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            for (size_t i = 0; i < param_count; i++) {
                LLVMTypeRef pt = ctx->type_i8ptr;
                ASTNode *binding = intent_decl->data.intent_decl.binding_count > 0
                    ? intent_decl->data.intent_decl.bindings[i]
                    : (i < intent_decl->data.intent_decl.involve_count
                        ? intent_decl->data.intent_decl.involves[i]
                        : intent_decl->data.intent_decl.values[
                            i - intent_decl->data.intent_decl.involve_count]);
                const char *type_name = NULL;

                if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                    && binding->data.intent_involves.subject_type != NULL
                    && binding->data.intent_involves.subject_type->type == AST_TYPE) {
                    type_name = binding->data.intent_involves.subject_type->data.type.name;
                    pt = ast_type_to_llvm(ctx, binding->data.intent_involves.subject_type);
                    if (llvm_type_name_uses_pointer_self(ctx, type_name))
                        pt = LLVMPointerType(pt, 0);
                } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                    && binding->data.intent_value.value_type != NULL) {
                    pt = ast_type_to_llvm(ctx, binding->data.intent_value.value_type);
                }
                if (pt == NULL) {
                    pt = ctx->type_i8ptr;
                }
                param_types[i] = pt;
            }
        }

        fn_type = LLVMFunctionType(ctx->type_i1, param_types,
            (unsigned)(intent_decl->data.intent_decl.involve_count
                + intent_decl->data.intent_decl.value_count), 0);
        fn = LLVMAddFunction(ctx->module, callee_name, fn_type);
        llvm_register_function(ctx, callee_name, fn, fn_type, ctx->type_i1);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL) {
        LLVMVarEntry *callee_var = NULL;
        LLVMValueRef fn_ptr = NULL;
        LLVMTypeRef fn_type = NULL;
        LLVMValueRef result;
        LLVMCallableVarEntry *callable_entry = NULL;

        if (node->data.call.callee->type == AST_IDENTIFIER)
            callee_var = llvm_scope_lookup(ctx, callee_name);
        callable_entry = llvm_lookup_callable_entry(ctx, callee_name);
        if (callee_var != NULL) {
            LLVMTypeRef callable_ptr_ty = NULL;
            if (LLVMGetTypeKind(callee_var->type) == LLVMPointerTypeKind)
                fn_type = LLVMGetElementType(callee_var->type);
            if (fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind) {
                if (callable_entry != NULL)
                    fn_type = llvm_function_signature_from_callable_entry(ctx, callable_entry);
            }
            if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
                callable_ptr_ty = LLVMPointerType(fn_type, 0);
            if ((fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind)
                && ctx->current_function != NULL) {
                const char *current_name = LLVMGetValueName(ctx->current_function);
                ASTNode *current_decl = current_name != NULL
                    ? llvm_find_function_decl(ctx, current_name) : NULL;
                if (current_decl != NULL && current_decl->type == AST_FUNC_DECL) {
                    for (size_t i = 0; i < current_decl->data.func_decl.param_count; i++) {
                        FuncParam *p = current_decl->data.func_decl.params[i];
                        if (p == NULL || p->name == NULL || p->type == NULL)
                            continue;
                        if (strcmp(p->name, callee_name) != 0)
                            continue;
                        if (p->type->type == AST_EVENT_HANDLER_TYPE) {
                            size_t pc = p->type->data.event_handler_type.param_count;
                            LLVMTypeRef *pts = NULL;
                            LLVMTypeRef ret = ctx->type_void;
                            if (p->type->data.event_handler_type.return_type != NULL)
                                ret = ast_type_to_llvm(ctx,
                                    p->type->data.event_handler_type.return_type);
                            if (pc > 0) {
                                pts = pgy_arena_calloc(&ctx->scratch,
                                    pc * sizeof(LLVMTypeRef));
                                if (pts != NULL) {
                                    for (size_t pi = 0; pi < pc; pi++) {
                                        pts[pi] = ast_type_to_llvm(ctx,
                                            p->type->data.event_handler_type.param_types[pi]);
                                    }
                                }
                            }
                            fn_type = LLVMFunctionType(ret, pts, (unsigned)pc, 0);
                        } else {
                            LLVMTypeRef declared_ptr_ty = ast_type_to_llvm(ctx, p->type);
                            if (LLVMGetTypeKind(declared_ptr_ty) == LLVMPointerTypeKind)
                                fn_type = LLVMGetElementType(declared_ptr_ty);
                            else
                                fn_type = declared_ptr_ty;
                        }
                        break;
                    }
                }
            }
            if (fn_type != NULL
                && LLVMGetTypeKind(fn_type) == LLVMPointerTypeKind) {
                callable_ptr_ty = fn_type;
                fn_type = LLVMGetElementType(fn_type);
            }
            if (callable_ptr_ty == NULL
                && fn_type != NULL
                && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
                callable_ptr_ty = LLVMPointerType(fn_type, 0);
            }
            fn_ptr = llvm_emit_expression(node->data.call.callee, ctx);
            if (fn_ptr != NULL && callable_ptr_ty != NULL
                && LLVMTypeOf(fn_ptr) != callable_ptr_ty) {
                fn_ptr = LLVMBuildBitCast(ctx->builder, fn_ptr, callable_ptr_ty,
                    llvm_tmp_name(ctx));
            }
            if (fn_type == NULL && LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMFunctionTypeKind)
                fn_type = LLVMTypeOf(fn_ptr);
            if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
                if (LLVMGetReturnType(fn_type) == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                        args, emitted_argc, "");
                    result = LLVMConstInt(ctx->type_i32, 0, 0);
                } else {
                    result = LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                        args, emitted_argc, llvm_tmp_name(ctx));
                }
                return result;
            }
        }

        fprintf(stderr, "[llvm] warning: unknown function '%s'\n", callee_name);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    for (size_t i = 0; decl == NULL && i < argc; i++) {
        ASTNode *arg_node = node->data.call.arguments[i];
        unsigned param_count = LLVMCountParams(func->fn);
        LLVMTypeRef param_ty = (i < param_count)
            ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
            : NULL;
            if (param_ty != NULL
            && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind) {
            if (arg_node->type == AST_IDENTIFIER) {
                LLVMVarEntry *v = llvm_scope_lookup(ctx,
                    arg_node->data.identifier.name);
                LLVMCallableVarEntry *callable_entry =
                    llvm_lookup_callable_entry(ctx, arg_node->data.identifier.name);
                if (v != NULL) {
                    if (callable_entry != NULL) {
                        LLVMTypeRef callable_sig =
                            llvm_function_signature_from_callable_entry(ctx, callable_entry);
                        LLVMTypeRef callable_ptr_ty = callable_sig != NULL
                            ? LLVMPointerType(callable_sig, 0)
                            : v->type;
                        args[i] = LLVMBuildLoad2(ctx->builder, callable_ptr_ty,
                            v->alloca, llvm_tmp_name(ctx));
                    } else if (LLVMGetTypeKind(v->type) == LLVMPointerTypeKind) {
                        args[i] = LLVMBuildLoad2(ctx->builder, v->type,
                            v->alloca, llvm_tmp_name(ctx));
                    } else {
                        args[i] = v->alloca;
                    }
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
            }
            if (args[i] == NULL && arg_node->type == AST_CALL) {
                LLVMValueRef maybe_value = llvm_emit_expression(arg_node, ctx);
                if (maybe_value != NULL
                    && LLVMGetTypeKind(LLVMTypeOf(maybe_value)) == LLVMPointerTypeKind) {
                    args[i] = maybe_value;
                }
            }
        }
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, emitted_argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, emitted_argc, llvm_tmp_name(ctx));
    }

    return result;
}
