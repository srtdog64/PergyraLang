/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — expression emission
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static void
llvm_append_mangled_suffix(char *buf, size_t buf_size, const char *suffix)
{
    if (buf == NULL || buf_size == 0 || suffix == NULL)
        return;

    size_t len = strlen(buf);
    if (len >= buf_size - 1)
        return;

    buf[len++] = '_';

    size_t remaining = buf_size - len - 1;
    size_t suffix_len = strlen(suffix);
    if (suffix_len > remaining)
        suffix_len = remaining;

    memcpy(buf + len, suffix, suffix_len);
    buf[len + suffix_len] = '\0';
}

static bool
llvm_is_upper_ident(ASTNode *node)
{
    if (node == NULL || node->type != AST_IDENTIFIER
        || node->data.identifier.name == NULL
        || node->data.identifier.name[0] == '\0')
        return false;

    return node->data.identifier.name[0] >= 'A'
        && node->data.identifier.name[0] <= 'Z';
}

static const char *
llvm_call_arg_device_inner(LLVMGenCtx *ctx, ASTNode *node)
{
    if (node != NULL && node->type == AST_IDENTIFIER)
        return llvm_lookup_device_slot_inner(ctx, node->data.identifier.name);
    return NULL;
}

static LLVMValueRef
llvm_array_data_ptr(LLVMGenCtx *ctx, LLVMValueRef array_value)
{
    return LLVMBuildExtractValue(ctx->builder, array_value, 0, llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_array_length_i64(LLVMGenCtx *ctx, LLVMValueRef array_value)
{
    return LLVMBuildExtractValue(ctx->builder, array_value, 1, llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_build_option_value(LLVMGenCtx *ctx, LLVMTypeRef inner_ty,
                        LLVMValueRef has_value, LLVMValueRef value)
{
    LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
        (LLVMTypeRef[]){ ctx->type_i32, inner_ty }, 2, 0);
    LLVMValueRef tag = LLVMBuildSelect(ctx->builder, has_value,
        LLVMConstInt(ctx->type_i32, 0, 0),
        LLVMConstInt(ctx->type_i32, 1, 0),
        llvm_tmp_name(ctx));
    LLVMValueRef option = LLVMGetUndef(option_ty);
    option = LLVMBuildInsertValue(ctx->builder, option, tag, 0, llvm_tmp_name(ctx));
    option = LLVMBuildInsertValue(ctx->builder, option, value, 1, llvm_tmp_name(ctx));
    return option;
}

static LLVMValueRef
llvm_emit_subject_projection(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    const char *source_class_name;
    LLVMClassTypeEntry *target_cls;
    LLVMClassTypeEntry *source_cls;
    LLVMVarEntry *source_var;
    LLVMValueRef source_base;
    LLVMValueRef projected;

    if (node == NULL || node->data.call.arg_count != 2)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    target_arg = node->data.call.arguments[0];
    source_arg = node->data.call.arguments[1];
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL
        || source_arg == NULL || source_arg->type != AST_IDENTIFIER
        || source_arg->data.identifier.name == NULL) {
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    target_cls = llvm_lookup_class(ctx, target_arg->data.identifier.name);
    source_var = llvm_scope_lookup(ctx, source_arg->data.identifier.name);
    source_class_name = llvm_lookup_var_class(ctx, source_arg->data.identifier.name);
    source_cls = source_class_name != NULL
        ? llvm_lookup_class(ctx, source_class_name) : NULL;
    if (target_cls == NULL || source_var == NULL || source_cls == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    source_base = source_var->alloca;
    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
            source_var->alloca, llvm_tmp_name(ctx));
    }

    projected = LLVMConstNull(target_cls->struct_type);
    for (int i = 0; i < target_cls->field_count; i++) {
        LLVMClassFieldInfo *target_field = &target_cls->fields[i];
        int source_index;
        LLVMClassFieldInfo *source_field = NULL;
        LLVMValueRef field_ptr;
        LLVMValueRef field_value;

        if (target_field->field_name == NULL)
            continue;

        source_index = llvm_class_field_index(source_cls, target_field->field_name);
        if (source_index < 0)
            continue;

        for (int j = 0; j < source_cls->field_count; j++) {
            if (source_cls->fields[j].index == source_index) {
                source_field = &source_cls->fields[j];
                break;
            }
        }
        if (source_field == NULL || source_field->field_type == NULL)
            continue;

        field_ptr = LLVMBuildStructGEP2(ctx->builder, source_cls->struct_type,
            source_base, (unsigned)source_index, llvm_tmp_name(ctx));
        field_value = LLVMBuildLoad2(ctx->builder, source_field->field_type,
            field_ptr, llvm_tmp_name(ctx));
        projected = LLVMBuildInsertValue(ctx->builder, projected, field_value,
            (unsigned)target_field->index, llvm_tmp_name(ctx));
    }

    return projected;
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
        args = calloc(argc, sizeof(LLVMValueRef));
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

    free(args);
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
        LLVMTypeRef *ptypes = calloc(pc > 0 ? pc : 1, sizeof(LLVMTypeRef));
        size_t real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            ptypes[real_pc++] = (p->type != NULL)
                ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
        }
        LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
        LLVMValueRef mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
        llvm_register_function(ctx, mangled, mono_fn, ft, ret);
        free(ptypes);

        ctx->current_function = mono_fn;
        ctx->current_ret_type = ret;
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, mono_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (p->type == NULL && strcmp(p->name, "self") == 0)
                continue;
            LLVMTypeRef pt = (p->type != NULL)
                ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
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
        args = calloc(argc, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(call->data.call.arguments[i], ctx);
    }

    callee_entry = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    spawn_fn = llvm_lookup_function(ctx, "pgy_async_spawn_export");
    malloc_fn = llvm_lookup_function(ctx, "malloc");
    free_fn = llvm_lookup_function(ctx, "free");
    if (spawn_fn == NULL || malloc_fn == NULL || free_fn == NULL) {
        free(args);
        return LLVMConstNull(ctx->type_task_handle);
    }
    if (callee_entry == NULL) {
        free(args);
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
        LLVMTypeRef *field_types = calloc(argc, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < argc; i++)
            field_types[i] = LLVMTypeOf(args[i]);
        arg_struct_type = LLVMStructTypeInContext(ctx->context, field_types, (unsigned)argc, 0);
        free(field_types);
    }

    ctx->current_function = wrapper_fn;
    ctx->current_ret_type = ctx->type_i8ptr;
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    llvm_scope_push(ctx);

    LLVMValueRef loaded_args_storage[16];
    LLVMValueRef *loaded_args = loaded_args_storage;
    if (argc > 16)
        loaded_args = calloc(argc, sizeof(LLVMValueRef));

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
    if (loaded_args != loaded_args_storage)
        free(loaded_args);

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

    free(args);
    return handle;
}

static const char *
llvm_operator_overload_suffix(TokenType op)
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

static ASTNode *
llvm_find_enum_decl(LLVMGenCtx *ctx, const char *enum_name)
{
    if (ctx == NULL || ctx->hir == NULL || enum_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
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

static LLVMValueRef
llvm_emit_boolean(ASTNode *node, LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, node->data.boolean.value ? 1 : 0, 0);
}

static LLVMValueRef
llvm_direct_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                      const char *inner)
{
    LLVMTypeRef inner_ty;
    LLVMValueRef value_ptr;

    if (slot_var == NULL || inner == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 0, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, inner_ty, value_ptr, llvm_tmp_name(ctx));
}

static void
llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                       LLVMValueRef value)
{
    LLVMValueRef value_ptr;
    LLVMValueRef occupied_ptr;

    if (slot_var == NULL || value == NULL)
        return;

    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 0, llvm_tmp_name(ctx));
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, value, value_ptr);
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        occupied_ptr);
}

static void
llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef occupied_ptr;

    if (slot_var == NULL)
        return;

    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_var->type,
        slot_var->alloca, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
        occupied_ptr);
}

static LLVMVarEntry *
llvm_resolve_slot_target(LLVMGenCtx *ctx, ASTNode *slot_arg,
                         const char **inner_out,
                         const char **source_name_out,
                         bool *secure_out)
{
    const char *inner = "Int";
    const char *source_name = NULL;
    bool is_secure = false;

    if (slot_arg == NULL)
        return NULL;

    if (slot_arg->type == AST_IDENTIFIER) {
        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, slot_arg->data.identifier.name);
        if (view != NULL) {
            source_name = view->source_slot;
            inner = view->inner_type;
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        } else {
            source_name = slot_arg->data.identifier.name;
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    } else if (slot_arg->type == AST_CALL
               && slot_arg->data.call.callee != NULL
               && slot_arg->data.call.callee->type == AST_IDENTIFIER
               && slot_arg->data.call.arg_count >= 1
               && slot_arg->data.call.arguments[0] != NULL
               && slot_arg->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee = slot_arg->data.call.callee->data.identifier.name;
        if (callee != NULL
            && (strcmp(callee, "ViewRead") == 0
                || strcmp(callee, "ViewWrite") == 0
                || strcmp(callee, "Move") == 0)) {
            source_name = slot_arg->data.call.arguments[0]->data.identifier.name;
            inner = llvm_lookup_slot_inner(ctx, source_name);
            is_secure = llvm_lookup_slot_is_secure(ctx, source_name);
        }
    }

    if (inner == NULL)
        inner = "Int";
    if (inner_out != NULL)
        *inner_out = inner;
    if (source_name_out != NULL)
        *source_name_out = source_name;
    if (secure_out != NULL)
        *secure_out = is_secure;
    return source_name != NULL ? llvm_scope_lookup(ctx, source_name) : NULL;
}

static LLVMValueRef
llvm_emit_identifier(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.identifier.name;

    /* Slot sugar: auto-Read — call pgy_read_T(&slot) instead of loading struct */
    if (!ctx->suppress_slot_auto_read) {
        const char *inner = llvm_lookup_slot_inner(ctx, name);
        if (inner != NULL) {
            LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
            if (var != NULL) {
                bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
                char fn_name[64];
                snprintf(fn_name, sizeof(fn_name),
                    is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
                if (fn != NULL) {
                    if (is_secure) {
                        LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, name);
                        if (token_var != NULL) {
                            LLVMValueRef args[] = { var->alloca, token_var->alloca };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                                 args, 2, llvm_tmp_name(ctx));
                        }
                    }
                    LLVMValueRef args[] = { var->alloca };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                         args, 1, llvm_tmp_name(ctx));
                }
                return llvm_direct_slot_read(ctx, var, inner);
            }
        }
    }

    /* Look up in scope */
    LLVMVarEntry *entry = llvm_scope_lookup(ctx, name);
    if (entry != NULL)
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));

    if (ctx->current_class_name != NULL && strcmp(name, "self") != 0) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (cls != NULL && self_var != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef base_ptr = LLVMBuildLoad2(ctx->builder,
                    self_var->type, self_var->alloca, llvm_tmp_name(ctx));
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, base_ptr, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }
        }
    }

    /* Look up as function (for passing as value) */
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, name);
    if (fn != NULL)
        return fn->fn;

    /* Bare enum variant identifier */
    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0);
    }

    /* Unknown — default to 0 */
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef left  = llvm_emit_expression(node->data.binary.left, ctx);
    LLVMValueRef right = llvm_emit_expression(node->data.binary.right, ctx);
    if (left == NULL || right == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMTypeRef left_type  = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);
    {
        const char *suffix = llvm_operator_overload_suffix(
            node->data.binary.op.type);
        const char *type_name = llvm_expr_custom_type_name(
            node->data.binary.left, ctx);

        if (type_name == NULL && left_type == right_type) {
            const char *primitive_suffix = llvm_type_to_suffix(ctx, left_type);
            if (primitive_suffix != NULL
                && strcmp(primitive_suffix, "Unknown") != 0) {
                type_name = primitive_suffix;
            }
        }

        if (type_name != NULL && suffix != NULL) {
            char fn_name[256];
            snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, type_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                LLVMValueRef args[] = { left, right };
                if (fn->ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }
                return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                      args, 2, llvm_tmp_name(ctx));
            }
        }
    }

    /* String + String → StringConcat */
    if (left_type == ctx->type_i8ptr && right_type == ctx->type_i8ptr
        && node->data.binary.op.type == TOKEN_PLUS) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
        if (fn != NULL) {
            LLVMValueRef args[] = { left, right };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        }
    }

    /* Promote: if one side is double, convert the other */
    bool is_float = (left_type == ctx->type_f64 || left_type == ctx->type_f32
                  || right_type == ctx->type_f64 || right_type == ctx->type_f32);

    if (is_float) {
        if (left_type == ctx->type_i32)
            left = LLVMBuildSIToFP(ctx->builder, left, ctx->type_f64,
                                    llvm_tmp_name(ctx));
        if (right_type == ctx->type_i32)
            right = LLVMBuildSIToFP(ctx->builder, right, ctx->type_f64,
                                     llvm_tmp_name(ctx));
    }

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.binary.op.type) {
    case TOKEN_PLUS:
        return is_float
            ? LLVMBuildFAdd(ctx->builder, left, right, tmp)
            : LLVMBuildAdd(ctx->builder, left, right, tmp);

    case TOKEN_MINUS:
        return is_float
            ? LLVMBuildFSub(ctx->builder, left, right, tmp)
            : LLVMBuildSub(ctx->builder, left, right, tmp);

    case TOKEN_STAR:
        return is_float
            ? LLVMBuildFMul(ctx->builder, left, right, tmp)
            : LLVMBuildMul(ctx->builder, left, right, tmp);

    case TOKEN_SLASH:
        return is_float
            ? LLVMBuildFDiv(ctx->builder, left, right, tmp)
            : LLVMBuildSDiv(ctx->builder, left, right, tmp);

    case TOKEN_PERCENT:
        return LLVMBuildSRem(ctx->builder, left, right, tmp);

    case TOKEN_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, tmp);

    case TOKEN_NOT_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealONE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, tmp);

    case TOKEN_LESS:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, tmp);

    case TOKEN_LESS_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, tmp);

    case TOKEN_GREATER:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, tmp);

    case TOKEN_GREATER_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, tmp);

    case TOKEN_AND:
        return LLVMBuildAnd(ctx->builder, left, right, tmp);

    case TOKEN_OR:
        return LLVMBuildOr(ctx->builder, left, right, tmp);

    default:
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}

static LLVMValueRef
llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.unary.op.type == TOKEN_QUESTION) {
        LLVMValueRef result = llvm_emit_expression(node->data.unary.operand, ctx);
        LLVMTypeRef result_ty = LLVMTypeOf(result);
        unsigned field_count = LLVMCountStructElementTypes(result_ty);

        if (result == NULL || LLVMGetTypeKind(result_ty) != LLVMStructTypeKind
            || field_count < 2 || ctx->current_function == NULL) {
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        LLVMTypeRef fields[8];
        LLVMGetStructElementTypes(result_ty, fields);

        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, result, 0, llvm_tmp_name(ctx));
        LLVMValueRef is_ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));

        LLVMValueRef ok_alloca = llvm_create_entry_alloca(ctx, fields[1], llvm_tmp_name(ctx));
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.ok");
        LLVMBasicBlockRef err_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.err");
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.cont");

        LLVMBuildCondBr(ctx->builder, is_ok, ok_bb, err_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
        {
            LLVMValueRef ok_value = LLVMBuildExtractValue(ctx->builder, result, 1,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, ok_value, ok_alloca);
            LLVMBuildBr(ctx->builder, cont_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, err_bb);
        if (ctx->current_ret_type == result_ty) {
            LLVMBuildRet(ctx->builder, result);
        } else {
            LLVMBuildUnreachable(ctx->builder);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        return LLVMBuildLoad2(ctx->builder, fields[1], ok_alloca, llvm_tmp_name(ctx));
    }

    LLVMValueRef operand = llvm_emit_expression(node->data.unary.operand, ctx);
    if (operand == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.unary.op.type) {
    case TOKEN_MINUS:
        if (LLVMTypeOf(operand) == ctx->type_f64 ||
            LLVMTypeOf(operand) == ctx->type_f32)
            return LLVMBuildFNeg(ctx->builder, operand, tmp);
        return LLVMBuildNeg(ctx->builder, operand, tmp);

    case TOKEN_NOT:
        return LLVMBuildNot(ctx->builder, operand, tmp);

    default:
        return operand;
    }
}

static LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.call.callee == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Method call: obj.method(args) */
    if (node->data.call.callee->type == AST_MEMBER_ACCESS) {
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
                                if (method_idx >= 0) break;
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

                                /* Build the function type: ret(self_ptr, user_args...) */
                                size_t argc = node->data.call.arg_count;
                                LLVMTypeRef *fn_params = calloc(argc + 1,
                                    sizeof(LLVMTypeRef));
                                fn_params[0] = ctx->type_i8ptr; /* self */
                                for (size_t ai = 0; ai < argc; ai++)
                                    fn_params[ai + 1] = ctx->type_i32; /* TODO: resolve arg types */

                                /* Determine return type from callee name lookup:
                                 * search registered role methods for this name */
                                LLVMTypeRef ret_type = ctx->type_i32; /* default */
                                /* Look for any Role_MethodName function */
                                for (int ri = 0; ri < ctx->func_count; ri++) {
                                    const char *fn_name = LLVMGetValueName(ctx->functions[ri].fn);
                                    if (fn_name != NULL
                                        && strlen(fn_name) > strlen(method_name) + 1) {
                                        const char *suffix = fn_name + strlen(fn_name) - strlen(method_name);
                                        if (suffix > fn_name && *(suffix - 1) == '_'
                                            && strcmp(suffix, method_name) == 0) {
                                            ret_type = ctx->functions[ri].ret_type;
                                            break;
                                        }
                                    }
                                }

                                LLVMTypeRef fn_type = LLVMFunctionType(ret_type,
                                    fn_params, (unsigned)(argc + 1), 0);
                                LLVMTypeRef fn_ptr_ty = LLVMPointerType(fn_type, 0);

                                /* Load the function pointer from vtable */
                                LLVMValueRef fn_ptr = LLVMBuildLoad2(ctx->builder,
                                    fn_ptr_ty, fn_ptr_field, llvm_tmp_name(ctx));

                                /* Build args: self (party_alloca as i8*) + user args */
                                LLVMValueRef *args = calloc(argc + 1,
                                    sizeof(LLVMValueRef));
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
                                free(fn_params);
                                free(args);
                                return result;
                            }
                        }
                    }
                }
            }
        }

        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
            && method_name != NULL) {
            if (llvm_is_upper_ident(obj_node)) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                         obj_node->data.identifier.name, method_name);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                if (fn != NULL) {
                    return llvm_emit_function_call_args(ctx, fn,
                        node->data.call.arguments, node->data.call.arg_count);
                }
            }

            const char *var_name = obj_node->data.identifier.name;
            const char *class_name = llvm_lookup_var_class(ctx, var_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);

            if (class_name != NULL && var != NULL) {
                LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
                if (cls != NULL) {
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                             class_name, method_name);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                    if (fn != NULL) {
                        /* Build args: self ptr + user args */
                        size_t argc = node->data.call.arg_count;
                        LLVMValueRef *args = calloc(argc + 1,
                                                     sizeof(LLVMValueRef));
                        /* Self is always passed as i8* (opaque ptr).
                         * var->alloca is ptr-to-struct, which is ptr. */
                        args[0] = var->alloca;
                        for (size_t i = 0; i < argc; i++) {
                            args[i + 1] = llvm_emit_expression(
                                node->data.call.arguments[i], ctx);
                        }

                        LLVMValueRef result;
                        if (fn->ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, fn->fn_type,
                                fn->fn, args, (unsigned)(argc + 1), "");
                            result = LLVMConstInt(ctx->type_i32, 0, 0);
                        } else {
                            result = LLVMBuildCall2(ctx->builder,
                                fn->fn_type, fn->fn, args,
                                (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        }
                        free(args);
                        return result;
                    }
                }
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Get callee name */
    const char *callee_name = NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        callee_name = node->data.call.callee->data.identifier.name;

    if (callee_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee_name);
        if (variant != NULL) {
            ASTNode *enum_decl = llvm_find_enum_decl(ctx, variant->enum_name);
            LLVMClassTypeEntry *enum_cls = llvm_lookup_class(ctx, variant->enum_name);
            if (enum_decl != NULL && enum_cls != NULL) {
                size_t variant_index = (size_t)variant->value;
                size_t param_count =
                    (enum_decl->data.enum_decl.variant_param_counts != NULL)
                    ? enum_decl->data.enum_decl.variant_param_counts[variant_index] : 0;
                LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
                enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                    LLVMConstInt(ctx->type_i32,
                        (unsigned long long)variant->value, 0),
                    0, llvm_tmp_name(ctx));

                if (param_count > 0) {
                    int field_idx = llvm_class_field_index(enum_cls, callee_name);
                    if (field_idx > 0) {
                        LLVMTypeRef payload_ty = enum_cls->fields[field_idx].field_type;
                        LLVMValueRef payload = LLVMGetUndef(payload_ty);
                        LLVMClassTypeEntry *payload_cls =
                            llvm_lookup_class_by_type(ctx, payload_ty);

                        for (size_t i = 0; i < param_count
                             && i < node->data.call.arg_count; i++) {
                            LLVMValueRef arg = llvm_emit_expression(
                                node->data.call.arguments[i], ctx);
                            if (arg == NULL)
                                continue;
                            if (payload_cls != NULL
                                && i < (size_t)payload_cls->field_count
                                && payload_cls->fields[i].field_type != LLVMTypeOf(arg)) {
                                LLVMTypeRef target_ty = payload_cls->fields[i].field_type;
                                if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
                                    && (LLVMTypeOf(arg) == ctx->type_i32
                                        || LLVMTypeOf(arg) == ctx->type_i64)) {
                                    arg = (LLVMGetIntTypeWidth(target_ty)
                                        > LLVMGetIntTypeWidth(LLVMTypeOf(arg)))
                                        ? LLVMBuildSExt(ctx->builder, arg, target_ty,
                                            llvm_tmp_name(ctx))
                                        : LLVMBuildTrunc(ctx->builder, arg, target_ty,
                                            llvm_tmp_name(ctx));
                                }
                            }
                            payload = LLVMBuildInsertValue(ctx->builder, payload, arg,
                                (unsigned)i, llvm_tmp_name(ctx));
                        }
                        enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                            payload, (unsigned)field_idx, llvm_tmp_name(ctx));
                    }
                }
                return enum_val;
            }
        }
    }

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee_name);
        if (cls != NULL) {
            LLVMValueRef object = LLVMGetUndef(cls->struct_type);
            for (size_t i = 0; i < node->data.call.arg_count
                 && i < (size_t)cls->field_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    node->data.call.arguments[i], ctx);
                if (arg == NULL)
                    continue;
                object = LLVMBuildInsertValue(ctx->builder, object, arg,
                    (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
            }
            return object;
        }
    }

    if ((strcmp(callee_name, "ToDto") == 0 || strcmp(callee_name, "ToObject") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_subject_projection(node, ctx);
    }

    /* Built-in: Log */
    if (strcmp(callee_name, "Log") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arg = llvm_emit_expression(
            node->data.call.arguments[0], ctx);
        if (arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Select the right log function based on arg type */
        LLVMTypeRef arg_type = LLVMTypeOf(arg);
        const char *log_fn_name = "pgy_log_int";

        if (arg_type == ctx->type_i64)        log_fn_name = "pgy_log_long";
        else if (arg_type == ctx->type_f32)   log_fn_name = "pgy_log_float";
        else if (arg_type == ctx->type_f64)   log_fn_name = "pgy_log_double";
        else if (arg_type == ctx->type_i1)    log_fn_name = "pgy_log_bool";
        else if (arg_type == ctx->type_i8ptr) log_fn_name = "pgy_log_string";

        LLVMFuncEntry *log_fn = llvm_lookup_function(ctx, log_fn_name);
        if (log_fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { arg };
        LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn,
                       args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: ClaimSlot<T>() — handled mostly in let_decl, but standalone */
    if (strcmp(callee_name, "ClaimSlot") == 0
        || strcmp(callee_name, "ClaimSecureSlot") == 0) {
        if (strcmp(callee_name, "ClaimSecureSlot") == 0) {
            LLVMTypeRef slot_ty = llvm_secure_slot_struct_type(ctx, "Int");
            LLVMTypeRef token_ty = llvm_secure_token_type(ctx, "Int");
            LLVMValueRef slot_tmp = llvm_create_entry_alloca(ctx, slot_ty, llvm_tmp_name(ctx));
            LLVMValueRef token_tmp = llvm_create_entry_alloca(ctx, token_ty, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), slot_tmp);
            LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_tmp);

            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, slot_tmp, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
                slot_tmp, ctx->type_i64, llvm_tmp_name(ctx));
            LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
                LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
                llvm_tmp_name(ctx));
            LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, slot_tmp, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
            LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
                token_ty, token_tmp, 0, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
            LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
                token_ty, token_tmp, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                token_write_ptr);
            LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
                token_ty, token_tmp, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                token_read_ptr);
            return LLVMBuildLoad2(ctx->builder, slot_ty, slot_tmp, llvm_tmp_name(ctx));
        } else {
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "pgy_claim_Int");
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   NULL, 0, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_claim_device_Int");
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              NULL, 0, llvm_tmp_name(ctx));
    }

    /* Built-in: Write(slot, value) */
    if (strcmp(callee_name, "Write") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Resolve slot inner type */
        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (!is_secure) {
                llvm_direct_slot_write(ctx, slot_var, val);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, val, token_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        } else {
            LLVMValueRef args[] = { slot_var->alloca, val };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Read(slot) */
    if (strcmp(callee_name, "Read") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (!is_secure)
                return llvm_direct_slot_read(ctx, slot_var, inner);
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 2, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef args[] = { slot_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 1, llvm_tmp_name(ctx));
        }
    }

    /* Built-in: Release(slot) */
    if (strcmp(callee_name, "Release") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && is_secure)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (fn == NULL) {
            llvm_direct_slot_release(ctx, slot_var);
        } else if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            LLVMValueRef args[] = { slot_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }

        /* Mark slot as explicitly released */
        if (source_name != NULL) {
            const char *sname = source_name;
            for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                if (strcmp(ctx->slot_vars[ri].var_name, sname) == 0) {
                    ctx->slot_vars[ri].released = true;
                    break;
                }
            }
        }

        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "DeviceWrite") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", inner);
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
        if (inner == NULL) inner = "Int";
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
        if (inner == NULL) inner = "Int";
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
        if (inner == NULL) inner = "Int";
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
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.call.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
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

    /* Built-in: Ok(value) → { .ok=true, .value=value } */
    if (strcmp(callee_name, "Ok") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val), ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) → { .ok=false, .value=value } */
    if (strcmp(callee_name, "Err") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        if (LLVMTypeOf(val) != ctx->type_i8ptr) {
            if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, ctx->type_i8ptr, llvm_tmp_name(ctx));
            } else {
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: IsOk(result) → extract ok field */
    if (strcmp(callee_name, "IsOk") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) → !ok */
    if (strcmp(callee_name, "IsErr") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) → extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOr(result, default) → ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef def = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) → { .tag=PgyOptionSome, .value=value } */
    if (strcmp(callee_name, "Some") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val) }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o, val, 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: None() → { .tag=PgyOptionNone, .value=zero } */
    if (strcmp(callee_name, "None") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef value_ty = ctx->type_i32;
        if (LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 2) {
            LLVMTypeRef fields[2];
            LLVMGetStructElementTypes(ctx->current_ret_type, fields);
            if (fields[0] == ctx->type_i32)
                value_ty = fields[1];
        }
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, value_ty }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstNull(value_ty), 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: IsSome(option) / IsNone(option) */
    if ((strcmp(callee_name, "IsSome") == 0 || strcmp(callee_name, "IsNone") == 0)
        && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, o, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                strcmp(callee_name, "IsSome") == 0 ? 0 : 1, 0),
            llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOption(option) → extract value field */
    if (strcmp(callee_name, "UnwrapOption") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, o, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "Cancel") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_cancel_export");
        LLVMValueRef task = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (fn != NULL) {
            LLVMValueRef args[] = { task };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "IsCancelled") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_is_cancelled_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                NULL, 0, llvm_tmp_name(ctx));
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySend") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "SendTimeout") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val, timeout };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if ((strcmp(callee_name, "TryRecv") == 0 && node->data.call.arg_count == 1)
        || (strcmp(callee_name, "RecvTimeout") == 0 && node->data.call.arg_count == 2)) {
        ASTNode *channel = node->data.call.arguments[0];
        const char *inner = "Int";
        LLVMVarEntry *ch_var = NULL;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *lookup_inner = llvm_lookup_channel_inner(ctx, name);
                if (lookup_inner != NULL)
                    inner = lookup_inner;
            }
        }

        LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, inner);
        if (ch_var != NULL) {
            LLVMValueRef tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);

            char fname[128];
            if (strcmp(callee_name, "TryRecv") == 0) {
                snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            } else {
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[1], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp, timeout };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            }
        }

        return llvm_build_option_value(ctx, value_ty,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstNull(value_ty));
    }

    if ((strcmp(callee_name, "ChannelReady") == 0
         || strcmp(callee_name, "ChannelLength") == 0
         || strcmp(callee_name, "ChannelCapacity") == 0
         || strcmp(callee_name, "ChannelSpace") == 0
         || strcmp(callee_name, "ChannelFull") == 0
         || strcmp(callee_name, "ChannelClosed") == 0)
        && node->data.call.arg_count == 1) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                snprintf(fname, sizeof(fname), "pgy_channel_%s_%s",
                    strcmp(callee_name, "ChannelReady") == 0 ? "ready" :
                    strcmp(callee_name, "ChannelLength") == 0 ? "length" :
                    strcmp(callee_name, "ChannelCapacity") == 0 ? "capacity" :
                    strcmp(callee_name, "ChannelSpace") == 0 ? "space" :
                    strcmp(callee_name, "ChannelFull") == 0 ? "full" :
                    "closed",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 1, llvm_tmp_name(ctx));
                }
            }
        }

        if (strcmp(callee_name, "ChannelLength") == 0
            || strcmp(callee_name, "ChannelCapacity") == 0
            || strcmp(callee_name, "ChannelSpace") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    size_t argc = node->data.call.arg_count;
    LLVMValueRef *args = calloc(argc > 0 ? argc : 1, sizeof(LLVMValueRef));
    for (size_t i = 0; i < argc; i++)
        args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);

    LLVMFuncEntry *func = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    if (func == NULL) {
        fprintf(stderr, "[llvm] warning: unknown function '%s'\n", callee_name);
        free(args);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    for (size_t i = 0; i < argc; i++) {
        ASTNode *arg_node = node->data.call.arguments[i];
        unsigned param_count = LLVMCountParams(func->fn);
        LLVMTypeRef param_ty = (i < param_count)
            ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
            : NULL;
        if (param_ty != NULL
            && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind
            && arg_node->type == AST_IDENTIFIER) {
            LLVMVarEntry *v = llvm_scope_lookup(ctx,
                arg_node->data.identifier.name);
            if (v != NULL)
                args[i] = v->alloca;
        }
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

    free(args);
    return result;
}

static LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.assignment.target == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (node->data.assignment.target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = node->data.assignment.target->data.array_access.array;
        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *name = array_node->data.identifier.name;
            LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
            LLVMValueRef idx = llvm_emit_expression(
                node->data.assignment.target->data.array_access.index, ctx);
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (arr_var != NULL && entry != NULL && idx != NULL && val != NULL) {
                LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type,
                    arr_var->alloca, llvm_tmp_name(ctx));
                LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
                LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type,
                    data_ptr, &idx, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Member assignment: obj.field = value */
    if (node->data.assignment.target->type == AST_MEMBER_ACCESS) {
        ASTNode *member_node = node->data.assignment.target;
        ASTNode *obj_node = member_node->data.member.object;
        const char *field_name = member_node->data.member.name;

        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
            && field_name != NULL) {
            const char *var_name = obj_node->data.identifier.name;
            LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
            const char *class_name = llvm_lookup_var_class(ctx, var_name);

            if (var != NULL && class_name != NULL) {
                LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
                if (cls != NULL) {
                    int field_idx = llvm_class_field_index(cls, field_name);
                    if (field_idx >= 0) {
                        LLVMValueRef val = llvm_emit_expression(
                            node->data.assignment.value, ctx);
                        if (val == NULL)
                            return LLVMConstInt(ctx->type_i32, 0, 0);

                        /* self: alloca holds pointer-to-struct */
                        LLVMValueRef base = var->alloca;
                        if (var->type == LLVMPointerType(
                                cls->struct_type, 0)) {
                            base = LLVMBuildLoad2(ctx->builder,
                                var->type, var->alloca,
                                llvm_tmp_name(ctx));
                        }
                        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, base,
                            (unsigned)field_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, val, gep);
                        return val;
                    }
                }
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    const char *name = NULL;
    if (node->data.assignment.target->type == AST_IDENTIFIER)
        name = node->data.assignment.target->data.identifier.name;

    if (name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL && ctx->current_class_name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (cls != NULL && self_var != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
                LLVMValueRef base_ptr;
                LLVMValueRef gep;
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                base_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
                    self_var->alloca, llvm_tmp_name(ctx));
                gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
    }
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Slot sugar: x = 5 → pgy_write_T(&x, 5) */
    const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
    if (slot_inner != NULL) {
        LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", slot_inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { var->alloca, val };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            llvm_direct_slot_write(ctx, var, val);
        }
        return val;
    }

    LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
    if (val == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMBuildStore(ctx->builder, val, var->alloca);
    return val;
}

static LLVMValueRef
llvm_emit_member_access(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.member.object;
    const char *field_name = node->data.member.name;

    if (obj_node == NULL || field_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (llvm_is_upper_ident(obj_node)) {
        LLVMEnumVariantEntry *variant =
            llvm_lookup_enum_variant_qualified(ctx,
                obj_node->data.identifier.name, field_name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32,
                (unsigned long long)variant->value, 0);
    }

    const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
    if (class_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    int field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (obj_node->type == AST_IDENTIFIER) {
        const char *var_name = obj_node->data.identifier.name;
        LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
        if (var != NULL) {
            LLVMValueRef base_ptr = var->alloca;
            if (var->type == LLVMPointerType(cls->struct_type, 0)) {
                base_ptr = LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                    llvm_tmp_name(ctx));
            }

            LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                cls->struct_type, base_ptr, (unsigned)field_idx,
                llvm_tmp_name(ctx));
            LLVMTypeRef field_type = cls->fields[field_idx].field_type;
            return LLVMBuildLoad2(ctx->builder, field_type, gep,
                llvm_tmp_name(ctx));
        }
    }

    LLVMValueRef obj_val = llvm_emit_expression(obj_node, ctx);
    if (obj_val == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (LLVMTypeOf(obj_val) == LLVMPointerType(cls->struct_type, 0)) {
        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
            cls->struct_type, obj_val, (unsigned)field_idx,
            llvm_tmp_name(ctx));
        LLVMTypeRef field_type = cls->fields[field_idx].field_type;
        return LLVMBuildLoad2(ctx->builder, field_type, gep,
            llvm_tmp_name(ctx));
    }

    if (LLVMTypeOf(obj_val) == cls->struct_type) {
        return LLVMBuildExtractValue(ctx->builder, obj_val,
            (unsigned)field_idx, llvm_tmp_name(ctx));
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}

LLVMValueRef
llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return NULL;

    switch (node->type) {
    case AST_NUMBER:        return llvm_emit_number(node, ctx);
    case AST_STRING:        return llvm_emit_string(node, ctx);
    case AST_BOOLEAN:       return llvm_emit_boolean(node, ctx);
    case AST_IDENTIFIER:    return llvm_emit_identifier(node, ctx);
    case AST_BINARY:        return llvm_emit_binary(node, ctx);
    case AST_UNARY:         return llvm_emit_unary(node, ctx);
    case AST_CALL:          return llvm_emit_call(node, ctx);
    case AST_ASSIGNMENT:    return llvm_emit_assignment(node, ctx);
    case AST_MEMBER_ACCESS: return llvm_emit_member_access(node, ctx);
    case AST_ARRAY_LITERAL: {
        size_t count = node->data.array_literal.count;
        const char *inner_name = "Int";
        LLVMTypeRef elem_type = ctx->type_i32;
        if (count > 0) {
            LLVMValueRef first = llvm_emit_expression(node->data.array_literal.elements[0], ctx);
            if (first != NULL) {
                elem_type = LLVMTypeOf(first);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        }

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        LLVMValueRef tmp = llvm_create_entry_alloca(ctx, array_type, llvm_tmp_name(ctx));
        char new_fn_name[64];
        char push_fn_name[64];
        snprintf(new_fn_name, sizeof(new_fn_name), "pgy_array_new_%s", inner_name);
        snprintf(push_fn_name, sizeof(push_fn_name), "pgy_array_push_%s", inner_name);
        LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, new_fn_name);
        LLVMFuncEntry *push_fn = llvm_lookup_function(ctx, push_fn_name);
        if (new_fn != NULL) {
            LLVMValueRef args[] = {
                LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0)
            };
            LLVMValueRef arr_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
                new_fn->fn, args, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, arr_val, tmp);
        }
        for (size_t i = 0; i < count; i++) {
            LLVMValueRef elem = llvm_emit_expression(node->data.array_literal.elements[i], ctx);
            if (push_fn != NULL && elem != NULL) {
                LLVMValueRef args[] = { tmp, elem };
                LLVMBuildCall2(ctx->builder, push_fn->fn_type, push_fn->fn, args, 2, "");
            }
        }
        return LLVMBuildLoad2(ctx->builder, array_type, tmp, llvm_tmp_name(ctx));
    }

    case AST_ARRAY_ACCESS: {
        /* arr[idx] → GEP + load */
        ASTNode *array_node = node->data.array_access.array;
        LLVMValueRef arr = llvm_emit_expression(array_node, ctx);
        LLVMValueRef idx = llvm_emit_expression(
            node->data.array_access.index, ctx);
        if (arr == NULL || idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMTypeRef arr_ty = LLVMTypeOf(arr);
        if (arr_ty == ctx->type_i8ptr) {
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                LLVMInt8TypeInContext(ctx->context),
                arr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder,
                LLVMInt8TypeInContext(ctx->context),
                gep, llvm_tmp_name(ctx));
        }

        if (LLVMGetTypeKind(arr_ty) == LLVMPointerTypeKind) {
            LLVMTypeRef elem_ty = LLVMGetElementType(arr_ty);
            if (elem_ty != NULL) {
                LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                    elem_ty, arr, &idx, 1, llvm_tmp_name(ctx));
                return LLVMBuildLoad2(ctx->builder, elem_ty,
                    gep, llvm_tmp_name(ctx));
            }
        }

        if (LLVMGetTypeKind(arr_ty) == LLVMStructTypeKind) {
            LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
            LLVMTypeRef elem_ty = ctx->type_i32;
            if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
                LLVMArrayVarEntry *entry = llvm_lookup_array_var(
                    ctx, array_node->data.identifier.name);
                if (entry != NULL)
                    elem_ty = entry->elem_type;
            }
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CONTEXT_ACCESS: {
        /* context.GetRole("slotName") → load role slot from self (i8*)
         * self is in scope as the party/systemic method's first param */
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (self_var == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        /* For now: return the self pointer cast — the role slot is
         * accessed through the party struct, which self points to */
        LLVMValueRef self_val = LLVMBuildLoad2(ctx->builder,
            ctx->type_i8ptr, self_var->alloca, llvm_tmp_name(ctx));
        return self_val;
    }

    case AST_PARTY_INSTANCE: {
        /* PartyType { slot1: val1, slot2: val2 }
         * → alloca struct, store fields, return value */
        const char *pty = node->data.party_instance.party_type;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, pty);
        if (cls == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx,
            cls->struct_type, llvm_tmp_name(ctx));

        /* Zero-initialize */
        LLVMValueRef zero = LLVMConstNull(cls->struct_type);
        LLVMBuildStore(ctx->builder, zero, alloca);

        /* Store each assignment */
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            const char *slot_name = node->data.party_instance.assignments[i].slot_name;
            ASTNode *val_node = node->data.party_instance.assignments[i].value;

            /* Find field index */
            for (int f = 0; f < cls->field_count; f++) {
                if (strcmp(cls->fields[f].field_name, slot_name) == 0) {
                    LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                        ctx->builder, cls->struct_type, alloca,
                        (unsigned)cls->fields[f].index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef val = llvm_emit_expression(val_node, ctx);
                    if (val != NULL)
                        LLVMBuildStore(ctx->builder, val, field_ptr);
                    break;
                }
            }
        }

        return LLVMBuildLoad2(ctx->builder, cls->struct_type,
            alloca, llvm_tmp_name(ctx));
    }

    case AST_TASK_GROUP: {
        /* TaskGroup { tasks... } → emit tasks sequentially (MVP) */
        for (size_t i = 0; i < node->data.task_group.task_count; i++) {
            if (node->data.task_group.tasks[i] != NULL)
                llvm_emit_expression(node->data.task_group.tasks[i], ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CHANNEL_SEND: {
        /* ch <- value → pgy_channel_send_T(&ch, value) */
        LLVMVarEntry *ch_var = NULL;
        const char *suffix = "Int";
        if (node->data.channel_send.channel != NULL
            && node->data.channel_send.channel->type == AST_IDENTIFIER) {
            const char *name = node->data.channel_send.channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *inner = llvm_lookup_channel_inner(ctx, name);
                if (inner != NULL)
                    suffix = inner;
            }
        }
        if (ch_var != NULL) {
            LLVMValueRef val = llvm_emit_expression(
                node->data.channel_send.value, ctx);
            char fname[128];
            snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suffix);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            if (fn != NULL && val != NULL) {
                LLVMValueRef args[] = { ch_var->alloca, val };
                return LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, llvm_tmp_name(ctx));
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    case AST_CHANNEL_RECV: {
        /* <- ch → pgy_channel_recv_val_T(&ch) */
        LLVMVarEntry *ch_var = NULL;
        const char *suffix = "Int";
        if (node->data.channel_recv.channel != NULL
            && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
            const char *name = node->data.channel_recv.channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *inner = llvm_lookup_channel_inner(ctx, name);
                if (inner != NULL)
                    suffix = inner;
            }
        }
        if (ch_var != NULL) {
            char fname[128];
            snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suffix);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            if (fn != NULL) {
                LLVMValueRef args[] = { ch_var->alloca };
                return LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 1, llvm_tmp_name(ctx));
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_SPAWN_EXPR:
        return llvm_emit_spawn_expr(node, ctx);

    case AST_AWAIT_EXPR:
        if (node->data.await_expr.expression != NULL) {
            ASTNode *inner_expr = node->data.await_expr.expression;
            const char *inner = NULL;
            bool is_remote = false;
            if (inner_expr->type == AST_IDENTIFIER)
                inner = llvm_lookup_future_inner(ctx, inner_expr->data.identifier.name);
            if (inner_expr->type == AST_IDENTIFIER)
                is_remote = llvm_lookup_future_is_remote(ctx, inner_expr->data.identifier.name);
            if (inner != NULL) {
                LLVMValueRef task = llvm_emit_expression(inner_expr, ctx);
                return llvm_await_task_handle(ctx, task, inner, is_remote);
            }
            return llvm_emit_expression(inner_expr, ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    case AST_LAMBDA_EXPR: {
        /* Generate a static LLVM function and return its pointer */
        int lid = ctx->lambda_counter++;
        int pc = (int)node->data.lambda_expr.param_count;

        /* Determine return type */
        LLVMTypeRef ret_type = ctx->type_i32;
        if (node->data.lambda_expr.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, node->data.lambda_expr.return_type);
        else if (node->data.lambda_expr.body != NULL
                 && node->data.lambda_expr.body->type == AST_BLOCK)
            ret_type = ctx->type_void;

        /* Parameter types (default i32) */
        LLVMTypeRef lparams[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            if (p->type == AST_LET_DECL && p->data.let_decl.type != NULL)
                lparams[j] = ast_type_to_llvm(ctx, p->data.let_decl.type);
            else
                lparams[j] = ctx->type_i32;
        }

        char lname[128];
        snprintf(lname, sizeof(lname), "pgy_lambda_%d", lid);
        LLVMTypeRef lft = LLVMFunctionType(ret_type,
            lparams, (unsigned)pc, 0);
        LLVMValueRef lfn = LLVMAddFunction(ctx->module, lname, lft);
        llvm_register_function(ctx, LLVMGetValueName(lfn),
            lfn, lft, ret_type);

        /* Save current builder state */
        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;

        ctx->current_function = lfn;
        ctx->current_ret_type = ret_type;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, lfn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        llvm_scope_push(ctx);
        for (int j = 0; j < pc; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            const char *pname = (p->type == AST_IDENTIFIER)
                ? p->data.identifier.name : p->data.let_decl.name;
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder,
                lparams[j], pname);
            LLVMBuildStore(ctx->builder, LLVMGetParam(lfn, (unsigned)j),
                alloca);
            llvm_scope_declare(ctx, pname, alloca, lparams[j]);
        }

        if (node->data.lambda_expr.body != NULL) {
            if (node->data.lambda_expr.body->type == AST_BLOCK) {
                llvm_emit_block(node->data.lambda_expr.body, ctx);
            } else {
                LLVMValueRef val = llvm_emit_expression(
                    node->data.lambda_expr.body, ctx);
                if (ret_type != ctx->type_void)
                    LLVMBuildRet(ctx->builder, val);
                else
                    LLVMBuildRetVoid(ctx->builder);
            }
        }

        /* Ensure terminator exists */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret_type == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder,
                    LLVMConstInt(ret_type, 0, 0));
        }

        llvm_scope_pop(ctx);

        /* Restore builder state */
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        return lfn;
    }

    case AST_EVENT_SUBSCRIBE: {
        /* event += handler → EventName_SUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn != NULL && ev_ptr != NULL) {
                LLVMValueRef args[] = { ev_ptr, hval };
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_UNSUBSCRIBE: {
        /* event -= handler → EventName_UNSUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn != NULL && ev_ptr != NULL) {
                LLVMValueRef args[] = { ev_ptr, hval };
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_INVOKE: {
        /* Emit(event, args...) → EventName_INVOKE(&event, args...) */
        ASTNode *evt = node->data.event_invoke.event;
        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);

            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.event_invoke.arg_count;
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.event_invoke.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    default:
        fprintf(stderr, "[llvm] warning: unhandled expression AST type %d\n",
                (int)node->type);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}


#endif /* PGY_LLVM_ENABLED */
