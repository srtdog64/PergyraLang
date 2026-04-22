static LLVMValueRef
llvm_emit_member_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.call.callee->data.member.object;
    const char *method_name = node->data.call.callee->data.member.name;

    /* Dynamic vtable dispatch: party.slot.method()
     * AST: call(member(member(party_var, slot_name), method_name)) */
    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && method_name != NULL) {
        ASTNode *party_node = obj_node->data.member.object;
        const char *slot_name = obj_node->data.member.name;

        if (party_node != NULL && party_node->type == AST_IDENTIFIER
            && slot_name != NULL) {
            const char *party_var = party_node->data.identifier.name;
            const char *party_class = llvm_lookup_var_class(ctx, party_var);
            LLVMClassTypeEntry *cls = party_class
                ? llvm_lookup_class(ctx, party_class) : NULL;

            if (cls != NULL) {
                /* Find vtable pointer field: slot_name + "_vtable" */
                char vt_field[256];
                snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
                int vt_idx = -1;
                for (int fi = 0; fi < cls->field_count; fi++) {
                    if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                        vt_idx = cls->fields[fi].index;
                        break;
                    }
                }

                if (vt_idx >= 0) {
                    /* Find which ability this slot requires, then look up
                     * the method index in that ability's vtable struct */
                    int method_idx = -1;
                    LLVMClassTypeEntry *vt_cls = NULL;

                    /* Search for *_vtable class that has this method */
                    for (int ci = 0; ci < ctx->class_type_count; ci++) {
                        const char *cn = ctx->class_types[ci].class_name;
                        if (cn != NULL && strstr(cn, "_vtable") != NULL) {
                            for (int fi = 0; fi < ctx->class_types[ci].field_count; fi++) {
                                if (strcmp(ctx->class_types[ci].fields[fi].field_name,
                                          method_name) == 0) {
                                    vt_cls = &ctx->class_types[ci];
                                    method_idx = ctx->class_types[ci].fields[fi].index;
                                    break;
                                }
                            }
                            if (method_idx >= 0)
                                break;
                        }
                    }

                    if (vt_cls != NULL && method_idx >= 0) {
                        LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
                        if (pvar != NULL) {
                            /* Load vtable pointer from party struct */
                            LLVMValueRef vt_ptr_field = LLVMBuildStructGEP2(
                                ctx->builder, cls->struct_type, pvar->alloca,
                                (unsigned)vt_idx, llvm_tmp_name(ctx));
                            LLVMValueRef vt_raw = LLVMBuildLoad2(ctx->builder,
                                ctx->type_i8ptr, vt_ptr_field, llvm_tmp_name(ctx));

                            /* Cast to vtable struct pointer */
                            LLVMTypeRef vt_ptr_ty = LLVMPointerType(
                                vt_cls->struct_type, 0);
                            LLVMValueRef vt_typed = LLVMBuildBitCast(
                                ctx->builder, vt_raw, vt_ptr_ty,
                                llvm_tmp_name(ctx));

                            /* GEP to method function pointer */
                            LLVMValueRef fn_ptr_field = LLVMBuildStructGEP2(
                                ctx->builder, vt_cls->struct_type, vt_typed,
                                (unsigned)method_idx, llvm_tmp_name(ctx));

                            size_t argc = node->data.call.arg_count;
                            LLVMTypeRef fn_ptr_ty = vt_cls->fields[method_idx].field_type;
                            LLVMTypeRef fn_type = LLVMGetElementType(fn_ptr_ty);
                            LLVMTypeRef ret_type = LLVMGetReturnType(fn_type);

                            /* Load the function pointer from vtable */
                            LLVMValueRef fn_ptr = LLVMBuildLoad2(ctx->builder,
                                fn_ptr_ty, fn_ptr_field, llvm_tmp_name(ctx));

                            /* Build args: self (party_alloca as i8*) + user args */
                            LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                                (argc + 1) * sizeof(LLVMValueRef));
                            args[0] = LLVMBuildBitCast(ctx->builder,
                                pvar->alloca, ctx->type_i8ptr,
                                llvm_tmp_name(ctx));
                            for (size_t ai = 0; ai < argc; ai++)
                                args[ai + 1] = llvm_emit_expression(
                                    node->data.call.arguments[ai], ctx);

                            LLVMValueRef result;
                            if (ret_type == ctx->type_void) {
                                LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                                    args, (unsigned)(argc + 1), "");
                                result = LLVMConstInt(ctx->type_i32, 0, 0);
                            } else {
                                result = LLVMBuildCall2(ctx->builder, fn_type,
                                    fn_ptr, args, (unsigned)(argc + 1),
                                    llvm_tmp_name(ctx));
                            }
                            return result;
                        }
                    }
                }
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL
        && (strcmp(method_name, "Write") == 0
            || strcmp(method_name, "Read") == 0
            || strcmp(method_name, "Release") == 0)) {
        const char *slot_name = obj_node->data.identifier.name;
        const char *inner = llvm_lookup_slot_inner(ctx, slot_name);
        bool is_secure = llvm_lookup_slot_is_secure(ctx, slot_name);
        LLVMVarEntry *slot_var = inner != NULL ? llvm_scope_lookup(ctx, slot_name) : NULL;
        if (inner != NULL && slot_var != NULL) {
            if (strcmp(method_name, "Write") == 0 && node->data.call.arg_count >= 1) {
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_write_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = { slot_var->alloca, val, token_var->alloca };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                        } else {
                            llvm_direct_secure_slot_write(ctx, slot_var, val);
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { slot_var->alloca, val };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    } else {
                        llvm_direct_slot_write(ctx, slot_var, val);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }

            if (strcmp(method_name, "Read") == 0) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_read_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                args, 2, llvm_tmp_name(ctx));
                        }
                        return llvm_direct_secure_slot_read(ctx, slot_var, inner);
                    }
                }

                {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_read_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { slot_var->alloca };
                        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 1, llvm_tmp_name(ctx));
                    }
                    return llvm_direct_slot_read(ctx, slot_var, inner);
                }
            }

            if (strcmp(method_name, "Release") == 0) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_release_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                        } else {
                            llvm_direct_secure_slot_release(ctx, slot_var);
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = { slot_var->alloca };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                    } else {
                        llvm_direct_slot_release(ctx, slot_var);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    if (obj_node != NULL && method_name != NULL
        && strcmp(method_name, "Slice") == 0
        && node->data.call.arg_count == 2) {
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=begin\n");
        LLVMValueRef receiver = llvm_emit_expression(obj_node, ctx);
        LLVMValueRef start = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef len = llvm_emit_expression(node->data.call.arguments[1], ctx);
        const char *suffix = NULL;
        LLVMTypeRef elem_type = NULL;
        LLVMTypeRef slice_type = NULL;
        LLVMValueRef data_ptr;
        LLVMValueRef start64;
        LLVMValueRef len64;
        LLVMValueRef offset_ptr;
        LLVMValueRef result;
        unsigned field_count = 0;

        if (receiver == NULL || start == NULL || len == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=operands-ready\n");

        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=receiver-type kind=%d\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(receiver)));
        if (LLVMGetTypeKind(LLVMTypeOf(receiver)) == LLVMStructTypeKind)
            field_count = LLVMCountStructElementTypes(LLVMTypeOf(receiver));
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=field-count count=%u\n", field_count);
        data_ptr = llvm_array_data_ptr(ctx, receiver);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=data-ptr\n");
        if (LLVMGetTypeKind(LLVMTypeOf(data_ptr)) != LLVMPointerTypeKind)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_type = llvm_stmt_resolve_array_elem_type(ctx, obj_node, data_ptr);
        if (elem_type == NULL)
            elem_type = ctx->type_i32;
        suffix = llvm_type_to_suffix(ctx, elem_type);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=elem ptr-kind=%d elem-kind=%d suffix=%s\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(data_ptr)),
                elem_type != NULL ? (int)LLVMGetTypeKind(elem_type) : -1,
                suffix != NULL ? suffix : "(null)");
        if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
            slice_type = llvm_slice_struct_type(ctx, suffix);
        else
            slice_type = LLVMStructTypeInContext(ctx->context,
                (LLVMTypeRef[]){ LLVMPointerType(elem_type, 0), ctx->type_i64 }, 2, 0);
        start64 = (LLVMTypeOf(start) == ctx->type_i64)
            ? start
            : LLVMBuildSExt(ctx->builder, start, ctx->type_i64, llvm_tmp_name(ctx));
        len64 = (LLVMTypeOf(len) == ctx->type_i64)
            ? len
            : LLVMBuildSExt(ctx->builder, len, ctx->type_i64, llvm_tmp_name(ctx));

        /* Emit slices directly in LLVM IR instead of calling the runtime
         * aggregate-return helper. This keeps Array<T>.Slice(...) and
         * Slice<T>.Slice(...) on the same lowering path and avoids Windows-
         * specific ABI sensitivity around returning PgySlice_* structs from
         * external C helpers. */
        offset_ptr = LLVMBuildGEP2(ctx->builder, elem_type, data_ptr, &start64, 1,
            llvm_tmp_name(ctx));
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=manual-emit\n");
        result = LLVMGetUndef(slice_type);
        result = LLVMBuildInsertValue(ctx->builder, result, offset_ptr, 0, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result, len64, 1, llvm_tmp_name(ctx));
        return result;
    }

    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL) {
        if (llvm_is_upper_ident(obj_node)) {
            size_t fn_len = strlen(obj_node->data.identifier.name) + 1 + strlen(method_name) + 1;
            char *full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
            if (full_name == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            snprintf(full_name, fn_len, "%s_%s",
                     obj_node->data.identifier.name, method_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                return llvm_emit_function_call_args(ctx, fn,
                    node->data.call.arguments, node->data.call.arg_count);
            }
        }

        const char *var_name = obj_node->data.identifier.name;
        LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
        const char *class_name = llvm_lookup_var_class(ctx, var_name);
        if (class_name == NULL)
            class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *parent_cls = NULL;
        int field_idx = -1;

        if (var == NULL) {
            ASTNode *host_decl = llvm_current_host_decl(ctx);
            const char *host_name = llvm_decl_node_name(host_decl);
            class_name = llvm_current_field_class_name(ctx, var_name);
            if (class_name != NULL && host_name != NULL) {
                parent_cls = llvm_lookup_class(ctx, host_name);
                if (parent_cls != NULL)
                    field_idx = llvm_class_field_index(parent_cls, var_name);
            }
        }

        if (class_name != NULL && (var != NULL || (parent_cls != NULL && field_idx >= 0))) {
            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
            if (cls != NULL) {
                LLVMFuncEntry *fn;
                LLVMValueRef fn_value;
                LLVMTypeRef fn_type;
                LLVMTypeRef ret_type;
                size_t fn_len = strlen(class_name) + 1 + strlen(method_name) + 1;
                char *full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
                if (full_name == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                snprintf(full_name, fn_len, "%s_%s", class_name, method_name);
                fn = llvm_lookup_function(ctx, full_name);
                fn_value = fn != NULL ? fn->fn
                    : LLVMGetNamedFunction(ctx->module, full_name);
                fn_type = fn != NULL
                    ? fn->fn_type
                    : (fn_value != NULL ? LLVMGlobalGetValueType(fn_value) : NULL);
                ret_type = fn != NULL
                    ? fn->ret_type
                    : (fn_type != NULL ? LLVMGetReturnType(fn_type) : NULL);
                ASTNode *method_decl = llvm_find_nominal_host_method_decl(ctx,
                    class_name, method_name);
                if (fn_value != NULL && fn_type != NULL && ret_type != NULL) {
                    /* subject methods receive a self pointer; class methods a self value */
                    size_t argc = node->data.call.arg_count;
                    LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                                                 (argc + 1) * sizeof(LLVMValueRef));
                    if (llvm_type_name_uses_pointer_self(ctx, class_name)) {
                        if (var != NULL && strcmp(var_name, "self") == 0) {
                            if (var->type == LLVMPointerType(cls->struct_type, 0))
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    var->type, var->alloca, llvm_tmp_name(ctx));
                            else
                                args[0] = var->alloca;
                        } else if (var != NULL) {
                            if (var->type == LLVMPointerType(cls->struct_type, 0))
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    var->type, var->alloca, llvm_tmp_name(ctx));
                            else
                                args[0] = var->alloca;
                        } else {
                            LLVMValueRef base_ptr =
                                llvm_current_self_base_ptr(ctx, parent_cls);
                            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                                ctx->builder, parent_cls->struct_type, base_ptr,
                                (unsigned)field_idx, llvm_tmp_name(ctx));
                            args[0] = field_ptr;
                        }
                    } else {
                        if (var != NULL) {
                            args[0] = LLVMBuildLoad2(ctx->builder,
                                var->type, var->alloca, llvm_tmp_name(ctx));
                        } else {
                            LLVMValueRef base_ptr =
                                llvm_current_self_base_ptr(ctx, parent_cls);
                            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                                ctx->builder, parent_cls->struct_type, base_ptr,
                                (unsigned)field_idx, llvm_tmp_name(ctx));
                            args[0] = LLVMBuildLoad2(ctx->builder,
                                cls->struct_type, field_ptr, llvm_tmp_name(ctx));
                        }
                    }
                    for (size_t i = 0; i < argc; i++) {
                        LLVMValueRef arg_val = llvm_emit_expression(
                            node->data.call.arguments[i], ctx);
                        if (method_decl != NULL) {
                            size_t logical_idx = 0;
                            for (size_t pk = 0;
                                 pk < method_decl->data.func_decl.param_count; pk++) {
                                FuncParam *p = method_decl->data.func_decl.params[pk];
                                const char *ptn = NULL;
                                LLVMClassTypeEntry *param_cls = NULL;
                                if (p->type == NULL
                                    && strcmp(p->name, "self") == 0) {
                                    continue;
                                }
                                if (logical_idx == i) {
                                    if (p->type != NULL && p->type->type == AST_TYPE)
                                        ptn = p->type->data.type.name;
                                    param_cls = ptn != NULL
                                        ? llvm_lookup_class(ctx, ptn) : NULL;
                                    if (param_cls != NULL && param_cls->is_pointer_self_host
                                        && node->data.call.arguments[i] != NULL
                                        && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                        const char *arg_name =
                                            node->data.call.arguments[i]->data.identifier.name;
                                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                        if (arg_var != NULL) {
                                            if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
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
                        }
                        args[i + 1] = arg_val;
                    }

                    LLVMValueRef result;
                    if (ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn_type,
                            fn_value, args, (unsigned)(argc + 1), "");
                        result = LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        result = LLVMBuildCall2(ctx->builder,
                            fn_type, fn_value, args,
                            (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    }
                    return result;
                }
            }
            if (cls == NULL) {
                size_t fn_len = strlen(class_name) + 1 + strlen(method_name) + 1;
                char *full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
                if (full_name == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                snprintf(full_name, fn_len, "%s_%s", class_name, method_name);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                LLVMValueRef fn_value = fn != NULL ? fn->fn
                    : LLVMGetNamedFunction(ctx->module, full_name);
                LLVMTypeRef fn_type = fn != NULL
                    ? fn->fn_type
                    : (fn_value != NULL ? LLVMGlobalGetValueType(fn_value) : NULL);
                LLVMTypeRef ret_type = fn != NULL
                    ? fn->ret_type
                    : (fn_type != NULL ? LLVMGetReturnType(fn_type) : NULL);
                if (fn_value != NULL && fn_type != NULL && ret_type != NULL) {
                    size_t argc = node->data.call.arg_count;
                    LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                        (argc + 1) * sizeof(LLVMValueRef));
                    args[0] = llvm_emit_expression(obj_node, ctx);
                    for (size_t i = 0; i < argc; i++)
                        args[i + 1] = llvm_emit_expression(node->data.call.arguments[i], ctx);
                    if (ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn_type, fn_value,
                                       args, (unsigned)(argc + 1), "");
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        LLVMValueRef result = LLVMBuildCall2(
                            ctx->builder, fn_type, fn_value,
                            args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        return result;
                    }
                }
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && method_name != NULL) {
        const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *host_cls = class_name != NULL
            ? llvm_lookup_class(ctx, class_name) : NULL;

        if (host_cls != NULL) {
            LLVMFuncEntry *fn;
            LLVMValueRef fn_value;
            LLVMTypeRef fn_type;
            LLVMTypeRef ret_type;
            ASTNode *method_decl;
            size_t argc = node->data.call.arg_count;
            LLVMValueRef *args;
            LLVMValueRef self_ptr;
            size_t fn_len = strlen(class_name) + 1 + strlen(method_name) + 1;
            char *full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
            if (full_name == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);

            snprintf(full_name, fn_len, "%s_%s", class_name, method_name);
            fn = llvm_lookup_function(ctx, full_name);
            fn_value = fn != NULL ? fn->fn
                : LLVMGetNamedFunction(ctx->module, full_name);
            fn_type = fn != NULL
                ? fn->fn_type
                : (fn_value != NULL ? LLVMGlobalGetValueType(fn_value) : NULL);
            ret_type = fn != NULL
                ? fn->ret_type
                : (fn_type != NULL ? LLVMGetReturnType(fn_type) : NULL);
            method_decl = llvm_find_nominal_host_method_decl(ctx,
                class_name, method_name);
            if (fn_value != NULL && fn_type != NULL && ret_type != NULL) {
                args = pgy_arena_calloc(&ctx->scratch,
                                        (argc + 1) * sizeof(LLVMValueRef));
                self_ptr = llvm_emit_member_lvalue_ptr(obj_node, ctx, NULL);
                if (self_ptr == NULL) {
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }

                if (llvm_type_name_uses_pointer_self(ctx, class_name)) {
                    args[0] = self_ptr;
                } else {
                    args[0] = LLVMBuildLoad2(ctx->builder,
                        host_cls->struct_type, self_ptr, llvm_tmp_name(ctx));
                }

                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(
                        node->data.call.arguments[i], ctx);
                    if (method_decl != NULL) {
                        size_t logical_idx = 0;
                        for (size_t pk = 0;
                             pk < method_decl->data.func_decl.param_count; pk++) {
                            FuncParam *p = method_decl->data.func_decl.params[pk];
                            const char *ptn = NULL;
                            LLVMClassTypeEntry *param_cls = NULL;
                            if (p->type == NULL
                                && strcmp(p->name, "self") == 0) {
                                continue;
                            }
                            if (logical_idx == i) {
                                if (p->type != NULL && p->type->type == AST_TYPE)
                                    ptn = p->type->data.type.name;
                                param_cls = ptn != NULL
                                    ? llvm_lookup_class(ctx, ptn) : NULL;
                                if (param_cls != NULL && param_cls->is_pointer_self_host
                                    && node->data.call.arguments[i] != NULL
                                    && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                    const char *arg_name =
                                        node->data.call.arguments[i]->data.identifier.name;
                                    LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                    if (arg_var != NULL) {
                                        if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
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
                    }
                    args[i + 1] = arg_val;
                }

                if (ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn_type, fn_value,
                        args, (unsigned)(argc + 1), "");
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }

                {
                    LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                        fn_type, fn_value, args,
                        (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    return result;
                }
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && obj_node->data.member.object != NULL
        && obj_node->data.member.object->type == AST_IDENTIFIER
        && obj_node->data.member.object->data.identifier.name != NULL
        && strcmp(obj_node->data.member.object->data.identifier.name, "self") == 0
        && obj_node->data.member.name != NULL
        && method_name != NULL) {
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        const char *field_name = obj_node->data.member.name;
        const char *class_name = llvm_current_field_class_name(ctx, field_name);
        LLVMClassTypeEntry *parent_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        LLVMClassTypeEntry *host_cls = class_name != NULL
            ? llvm_lookup_class(ctx, class_name) : NULL;
        int field_idx = parent_cls != NULL
            ? llvm_class_field_index(parent_cls, field_name) : -1;

        if (parent_cls != NULL && host_cls != NULL && field_idx >= 0) {
            size_t fn_len = strlen(class_name) + 1 + strlen(method_name) + 1;
            char *full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
            if (full_name == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            snprintf(full_name, fn_len, "%s_%s", class_name, method_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = node->data.call.arg_count;
                LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                                                      (argc + 1) * sizeof(LLVMValueRef));
                LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
                LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                    parent_cls->struct_type, base_ptr, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                ASTNode *method_decl = llvm_find_nominal_host_method_decl(ctx,
                    class_name, method_name);

                if (llvm_type_name_uses_pointer_self(ctx, class_name))
                    args[0] = field_ptr;
                else
                    args[0] = LLVMBuildLoad2(ctx->builder,
                        host_cls->struct_type, field_ptr, llvm_tmp_name(ctx));

                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(
                        node->data.call.arguments[i], ctx);
                    if (method_decl != NULL) {
                        size_t logical_idx = 0;
                        for (size_t pk = 0;
                             pk < method_decl->data.func_decl.param_count; pk++) {
                            FuncParam *p = method_decl->data.func_decl.params[pk];
                            const char *ptn = NULL;
                            LLVMClassTypeEntry *param_cls = NULL;
                            if (p->type == NULL
                                && strcmp(p->name, "self") == 0) {
                                continue;
                            }
                            if (logical_idx == i) {
                                if (p->type != NULL && p->type->type == AST_TYPE)
                                    ptn = p->type->data.type.name;
                                param_cls = ptn != NULL
                                    ? llvm_lookup_class(ctx, ptn) : NULL;
                                if (param_cls != NULL && param_cls->is_pointer_self_host
                                    && node->data.call.arguments[i] != NULL
                                    && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                    const char *arg_name =
                                        node->data.call.arguments[i]->data.identifier.name;
                                    LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                    if (arg_var != NULL) {
                                        if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
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
                    }
                    args[i + 1] = arg_val;
                }

                LLVMValueRef result;
                if (fn->ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, (unsigned)(argc + 1), "");
                    result = LLVMConstInt(ctx->type_i32, 0, 0);
                } else {
                    result = LLVMBuildCall2(ctx->builder,
                        fn->fn_type, fn->fn, args,
                        (unsigned)(argc + 1), llvm_tmp_name(ctx));
                }
                return result;
            }
        }
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}
