static LLVMValueRef
llvm_current_self_base_ptr(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL || cls == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    if (self_var->type == LLVMPointerType(cls->struct_type, 0))
        return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
            llvm_tmp_name(ctx));
    return self_var->alloca;
}

static LLVMValueRef
llvm_identifier_base_ptr(LLVMGenCtx *ctx, const char *name, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *var;

    if (ctx == NULL || name == NULL || cls == NULL)
        return NULL;

    var = llvm_scope_lookup(ctx, name);
    if (var != NULL) {
        LLVMValueRef base_ptr = var->alloca;
        if (var->type == LLVMPointerType(cls->struct_type, 0)) {
            base_ptr = LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                llvm_tmp_name(ctx));
        }
        return base_ptr;
    }

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *parent_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        int parent_field_idx;
        LLVMValueRef self_ptr;
        if (parent_cls == NULL)
            return NULL;
        parent_field_idx = llvm_class_field_index(parent_cls, name);
        if (parent_field_idx < 0)
            return NULL;
        self_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
        if (self_ptr == NULL)
            return NULL;
        return LLVMBuildStructGEP2(ctx->builder, parent_cls->struct_type, self_ptr,
            (unsigned)parent_field_idx, llvm_tmp_name(ctx));
    }

    return NULL;
}

static LLVMValueRef
llvm_emit_projection_from_binding(LLVMGenCtx *ctx,
                                  const char *target_class_name,
                                  const char *source_name)
{
    const char *source_class_name;
    LLVMClassTypeEntry *target_cls;
    LLVMClassTypeEntry *source_cls;
    ASTNode *source_decl;
    LLVMVarEntry *source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (ctx == NULL || target_class_name == NULL || source_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    target_cls = llvm_lookup_class(ctx, target_class_name);
    source_var = llvm_scope_lookup(ctx, source_name);
    source_class_name = llvm_lookup_var_class(ctx, source_name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    source_decl = source_class_name != NULL
        ? llvm_find_projection_nominal_decl(ctx, source_class_name) : NULL;
    if (target_cls == NULL || source_var == NULL || source_cls == NULL || source_decl == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    source_base = source_var->alloca;
    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
            source_var->alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMGetUndef(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *field = &target_cls->fields[i];
        LLVMValueRef field_val = llvm_load_projection_path_value(ctx, source_decl,
            source_cls, source_base, field->field_name);
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_val,
            (unsigned)field->index, llvm_tmp_name(ctx));
    }
    return projected;
}

static ASTNode *
llvm_current_host_method_decl(LLVMGenCtx *ctx, const char *method_name)
{
    const char *host_name;

    host_name = llvm_current_host_class_name(ctx);

    if (ctx == NULL || method_name == NULL || host_name == NULL)
        return NULL;
    return llvm_find_nominal_host_method_decl(ctx, host_name,
                                              method_name);
}

static LLVMValueRef
llvm_current_self_call_arg(LLVMGenCtx *ctx)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
        llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_await_task_handle(LLVMGenCtx *ctx, LLVMValueRef task, const char *inner,
                       bool is_remote)
{
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");
    if (await_fn == NULL || task == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMValueRef args[] = { task };
    LLVMValueRef raw = LLVMBuildCall2(ctx->builder, await_fn->fn_type,
        await_fn->fn, args, 1, llvm_tmp_name(ctx));

    if (inner == NULL || strcmp(inner, "Void") == 0) {
        if (!is_remote)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
    LLVMValueRef value;
    if (strcmp(inner, "String") == 0) {
        LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
            LLVMPointerType(ctx->type_i8ptr, 0), llvm_tmp_name(ctx));
        value = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
            typed_ptr, llvm_tmp_name(ctx));
    } else {
        LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
            LLVMPointerType(inner_ty, 0), llvm_tmp_name(ctx));
        value = LLVMBuildLoad2(ctx->builder, inner_ty, typed_ptr, llvm_tmp_name(ctx));
    }

    if (!is_remote)
        return value;

    LLVMTypeRef result_fields[] = { ctx->type_i32, inner_ty, ctx->type_i8ptr };
    LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context, result_fields, 3, 0);
    LLVMValueRef result = LLVMGetUndef(result_ty);
    result = LLVMBuildInsertValue(ctx->builder, result,
        LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
    result = LLVMBuildInsertValue(ctx->builder, result,
        value, 1, llvm_tmp_name(ctx));
    result = LLVMBuildInsertValue(ctx->builder, result,
        LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
    return result;
}

static LLVMValueRef
llvm_emit_function_call_args(LLVMGenCtx *ctx, LLVMFuncEntry *func,
                             ASTNode **arg_nodes, size_t argc)
{
    LLVMValueRef *args = NULL;

    if (argc > 0) {
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(arg_nodes[i], ctx);
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, (unsigned)argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, (unsigned)argc, llvm_tmp_name(ctx));
    }

    return result;
}

static LLVMFuncEntry *
llvm_resolve_callee_entry(LLVMGenCtx *ctx, const char *callee_name,
                          LLVMValueRef *args, size_t argc)
{
    ASTNode *generic_ast = llvm_lookup_generic_template(ctx, callee_name);
    if (generic_ast == NULL)
        return llvm_lookup_function(ctx, callee_name);

    char mangled[256];
    snprintf(mangled, sizeof(mangled), "%s", callee_name);
    for (size_t i = 0; i < argc; i++) {
        LLVMTypeRef at = (args != NULL && args[i] != NULL)
            ? LLVMTypeOf(args[i]) : ctx->type_i32;
        const char *suf = llvm_type_to_suffix(ctx, at);
        llvm_append_mangled_suffix(mangled, sizeof(mangled), suf);
    }

    if (!llvm_mono_already_emitted(ctx, mangled)) {
        llvm_register_mono(ctx, mangled);

        GenericParams *gp = generic_ast->data.func_decl.generic_params;
        int saved_subst = ctx->type_subst_count;
        ctx->type_subst_count = 0;
        for (size_t gi = 0; gi < gp->count && gi < 8; gi++) {
            LLVMTypeRef concrete = (gi < argc && args != NULL && args[gi] != NULL)
                ? LLVMTypeOf(args[gi]) : ctx->type_i32;
            ctx->type_subst[ctx->type_subst_count].param_name = gp->params[gi]->name;
            ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
            ctx->type_subst[ctx->type_subst_count].type_name = llvm_type_to_suffix(ctx, concrete);
            ctx->type_subst_count++;
        }

        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;

        LLVMTypeRef ret = ctx->type_void;
        if (generic_ast->data.func_decl.return_type != NULL)
            ret = ast_type_to_llvm(ctx, generic_ast->data.func_decl.return_type);

        size_t pc = generic_ast->data.func_decl.param_count;
        LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
            ((pc * 2) > 0 ? (pc * 2) : 1) * sizeof(LLVMTypeRef));
        size_t real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (llvm_param_is_implicit_self(p))
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
                if (inner != NULL) {
                    LLVMTypeRef slot_ty = ast_type_to_llvm(ctx, p->type);
                    ptypes[real_pc++] = LLVMPointerType(slot_ty, 0);
                    if (is_secure)
                        ptypes[real_pc++] = llvm_secure_token_type(ctx, inner);
                } else {
                    ptypes[real_pc++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
                }
            }
        }
        LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
        LLVMValueRef mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
        llvm_register_function(ctx, mangled, mono_fn, ft, ret);
        ctx->current_function = mono_fn;
        ctx->current_ret_type = ret;
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, mono_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (llvm_param_is_implicit_self(p))
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
                LLVMTypeRef pt = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
                if (inner != NULL) {
                    LLVMValueRef slot_ptr = LLVMGetParam(mono_fn, (unsigned)real_pc++);
                    llvm_scope_declare(ctx, p->name, slot_ptr, pt);
                    llvm_register_slot_var(ctx, p->name, inner, is_secure);
                    if (is_secure) {
                        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                        char token_name[256];
                        LLVMValueRef token_alloca;
                        snprintf(token_name, sizeof(token_name), "%s_token", p->name);
                        token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
                        LLVMBuildStore(ctx->builder, LLVMGetParam(mono_fn, (unsigned)real_pc++), token_alloca);
                        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
                    }
                } else {
                    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder, LLVMGetParam(mono_fn, (unsigned)real_pc), alloca);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    if (p->type != NULL && p->type->type == AST_TYPE
                        && p->type->data.type.name != NULL
                        && llvm_lookup_class(ctx, p->type->data.type.name) != NULL) {
                        llvm_register_var_class(ctx, p->name, p->type->data.type.name);
                    }
                    real_pc++;
                }
            }
        }

        if (generic_ast->data.func_decl.body != NULL)
            llvm_emit_block(generic_ast->data.func_decl.body, ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder, LLVMConstInt(ret, 0, 0));
        }

        llvm_scope_pop(ctx);
        ctx->type_subst_count = saved_subst;
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    }

    return llvm_lookup_function(ctx, mangled);
}

static LLVMValueRef
llvm_emit_spawn_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *callee_name = NULL;
    size_t argc = 0;
    LLVMValueRef *args = NULL;
    LLVMFuncEntry *callee_entry = NULL;
    LLVMFuncEntry *spawn_fn = NULL;
    LLVMFuncEntry *malloc_fn = NULL;
    LLVMFuncEntry *free_fn = NULL;
    int wrapper_id = ++ctx->tmp_counter;

    if (target == NULL)
        return LLVMConstNull(ctx->type_task_handle);

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        argc = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL)
        return LLVMConstNull(ctx->type_task_handle);

    if (argc > 0) {
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(call->data.call.arguments[i], ctx);
    }

    callee_entry = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    spawn_fn = llvm_lookup_function(ctx,
        node->data.spawn_expr.is_blocking
            ? "pgy_spawn_blocking_export"
            : "pgy_async_spawn_export");
    malloc_fn = llvm_lookup_function(ctx, "malloc");
    free_fn = llvm_lookup_function(ctx, "free");
    if (spawn_fn == NULL || malloc_fn == NULL || free_fn == NULL) {
        return LLVMConstNull(ctx->type_task_handle);
    }
    if (callee_entry == NULL) {
        return LLVMConstNull(ctx->type_task_handle);
    }

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    char wrapper_name[96];
    snprintf(wrapper_name, sizeof(wrapper_name), "_pgy_spawn_expr_%d", wrapper_id);
    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
    LLVMValueRef wrapper_fn = LLVMAddFunction(ctx->module, wrapper_name, wrapper_type);

    LLVMTypeRef arg_struct_type = NULL;
    if (argc > 0) {
        LLVMTypeRef *field_types = pgy_arena_calloc(&ctx->scratch,
            argc * sizeof(LLVMTypeRef));
        for (size_t i = 0; i < argc; i++)
            field_types[i] = LLVMTypeOf(args[i]);
        arg_struct_type = LLVMStructTypeInContext(ctx->context, field_types, (unsigned)argc, 0);
    }

    ctx->current_function = wrapper_fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    llvm_scope_push(ctx);

    LLVMValueRef loaded_args_storage[16];
    LLVMValueRef *loaded_args = loaded_args_storage;
    if (argc > 16)
        loaded_args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));

    LLVMValueRef raw_arg = LLVMGetParam(wrapper_fn, 0);
    if (argc > 0) {
        LLVMValueRef typed_ctx = LLVMBuildBitCast(ctx->builder, raw_arg,
            LLVMPointerType(arg_struct_type, 0), llvm_tmp_name(ctx));
        for (size_t i = 0; i < argc; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, arg_struct_type,
                typed_ctx, (unsigned)i, llvm_tmp_name(ctx));
            loaded_args[i] = LLVMBuildLoad2(ctx->builder, LLVMTypeOf(args[i]), gep, llvm_tmp_name(ctx));
        }
        LLVMValueRef free_args[] = { raw_arg };
        LLVMBuildCall2(ctx->builder, free_fn->fn_type, free_fn->fn, free_args, 1, "");
    }

    LLVMValueRef call_result = NULL;
    if (callee_entry->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, callee_entry->fn_type, callee_entry->fn,
            loaded_args, (unsigned)argc, "");
        LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    } else {
        call_result = LLVMBuildCall2(ctx->builder, callee_entry->fn_type, callee_entry->fn,
            loaded_args, (unsigned)argc, llvm_tmp_name(ctx));
        LLVMTargetDataRef td = LLVMGetModuleDataLayout(ctx->module);
        uint64_t size = LLVMABISizeOfType(td, callee_entry->ret_type);
        LLVMValueRef malloc_args[] = { LLVMConstInt(ctx->type_i64, size, 0) };
        LLVMValueRef raw_result = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
            malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef typed_result = LLVMBuildBitCast(ctx->builder, raw_result,
            LLVMPointerType(callee_entry->ret_type, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, call_result, typed_result);
        LLVMBuildRet(ctx->builder, raw_result);
    }

    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    LLVMValueRef raw_spawn_arg = LLVMConstNull(ctx->type_i8ptr);
    if (argc > 0) {
        LLVMTargetDataRef td = LLVMGetModuleDataLayout(ctx->module);
        uint64_t size = LLVMABISizeOfType(td, arg_struct_type);
        LLVMValueRef malloc_args[] = { LLVMConstInt(ctx->type_i64, size, 0) };
        raw_spawn_arg = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
            malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef typed_arg = LLVMBuildBitCast(ctx->builder, raw_spawn_arg,
            LLVMPointerType(arg_struct_type, 0), llvm_tmp_name(ctx));
        for (size_t i = 0; i < argc; i++) {
            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, arg_struct_type,
                typed_arg, (unsigned)i, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, args[i], gep);
        }
    }

    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, wrapper_fn, ctx->type_i8ptr,
        llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = { fn_ptr, raw_spawn_arg };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
        spawn_args, 2, llvm_tmp_name(ctx));

    return handle;
}

static const char *
llvm_operator_overload_suffix(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "sub";
    case TOKEN_STAR: return "mul";
    case TOKEN_SLASH: return "div";
    case TOKEN_PERCENT: return "mod";
    case TOKEN_EQUAL: return "eq";
    case TOKEN_NOT_EQUAL: return "ne";
    case TOKEN_LESS: return "lt";
    case TOKEN_LESS_EQUAL: return "le";
    case TOKEN_GREATER: return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default: return NULL;
    }
}

static const char *
llvm_expr_custom_type_name(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_IDENTIFIER: {
        const char *name = node->data.identifier.name;
        const char *class_name = llvm_lookup_var_class(ctx, name);
        if (class_name != NULL)
            return class_name;
        if (strcmp(name, "self") != 0) {
            const char *host_name = llvm_current_host_class_name(ctx);
            LLVMClassTypeEntry *host_cls = host_name != NULL
                ? llvm_lookup_class(ctx, host_name) : NULL;
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty = host_cls->fields[field_idx].field_type;
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == field_ty)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        {
            LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
            if (variant != NULL)
                return variant->enum_name;
        }
        return NULL;
    }
    case AST_MEMBER_ACCESS:
        if (llvm_is_upper_ident(node->data.member.object)) {
            LLVMEnumVariantEntry *variant =
                llvm_lookup_enum_variant_qualified(ctx,
                    node->data.member.object->data.identifier.name,
                    node->data.member.name);
            if (variant != NULL)
                return variant->enum_name;
        }
        {
            const char *base_name =
                llvm_expr_custom_type_name(node->data.member.object, ctx);
            LLVMClassTypeEntry *base_cls = NULL;
            if (base_name != NULL)
                base_cls = llvm_lookup_class(ctx, base_name);
            if (base_cls != NULL) {
                int field_idx = llvm_class_field_index(base_cls,
                    node->data.member.name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty = base_cls->fields[field_idx].field_type;
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == field_ty)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        return NULL;
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee = node->data.call.callee->data.identifier.name;
            if (llvm_lookup_class(ctx, callee) != NULL)
                return callee;
            {
                LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee);
                if (variant != NULL)
                    return variant->enum_name;
            }
            {
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, callee);
                ASTNode *host_decl = llvm_current_host_decl(ctx);
                const char *host_name = llvm_decl_node_name(host_decl);
                if (fn == NULL && host_name != NULL) {
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        host_name, callee);
                    fn = llvm_lookup_function(ctx, full_name);
                }
                if (fn != NULL) {
                    for (int i = 0; i < ctx->class_type_count; i++) {
                        if (ctx->class_types[i].struct_type == fn->ret_type)
                            return ctx->class_types[i].class_name;
                    }
                }
            }
        }
        return NULL;
    default:
        return NULL;
    }
}

static LLVMClassTypeEntry *
llvm_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef ty)
{
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (ctx->class_types[i].struct_type == ty)
            return &ctx->class_types[i];
    }
    return NULL;
}

ASTNode *
llvm_find_enum_decl(LLVMGenCtx *ctx, const char *enum_name)
{
    ASTNode **types = NULL;
    size_t type_count = 0;
    ASTNode *decl;

    if (ctx == NULL || enum_name == NULL)
        return NULL;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL, enum_name);
    if (decl != NULL)
        return decl;
    llvm_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && stmt->data.enum_decl.name != NULL
            && strcmp(stmt->data.enum_decl.name, enum_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static LLVMValueRef
llvm_emit_number(ASTNode *node, LLVMGenCtx *ctx)
{
    double val = node->data.number.value;

    /* Explicit 'L' suffix -> always i64 */
    if (node->data.number.is_long) {
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);
    }

    /* Check if integer fits in i32 */
    if (val == (int64_t)val && val >= -2147483648.0 && val <= 2147483647.0)
        return LLVMConstInt(ctx->type_i32, (unsigned long long)(int32_t)val, 1);

    /* Check if integer fits in i64 (beyond i32 range) */
    if (val == (double)(int64_t)val
        && val >= -9.2233720368547758e+18
        && val <=  9.2233720368547758e+18)
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);

    return LLVMConstReal(ctx->type_f64, val);
}

static LLVMValueRef
llvm_emit_string(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *str = node->data.string.value;
    LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str,
                                                    llvm_tmp_name(ctx));
    return global;
}

static size_t
llvm_count_banner_line_indent(const char *line_start, const char *line_end)
{
    size_t indent = 0;

    while (line_start < line_end) {
        if (*line_start == ' ' || *line_start == '\t') {
            indent++;
            line_start++;
            continue;
        }
        break;
    }
    return indent;
}

static bool
llvm_line_is_empty_with_only_ws(const char *line_start, const char *line_end)
{
    for (const char *p = line_start; p < line_end; p++) {
        if (*p != ' ' && *p != '\t')
            return false;
    }
    return true;
}

static char *llvm_normalize_banner_string_literal_scratch(const char *src,
                                                          PgyArena *arena);
