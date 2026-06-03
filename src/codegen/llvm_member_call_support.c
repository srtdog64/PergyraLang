#ifdef PGY_LLVM_ENABLED

#include "llvm_member_call_internal.h"

#include <stdio.h>
#include <string.h>

#include "llvm_inventory_host_methods.h"

LLVMValueRef
llvm_member_call_error_recovery(LLVMGenCtx *ctx, ASTNode *node,
                                const char *class_name,
                                const char *method_name,
                                const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM member call %s.%s %s",
            class_name != NULL ? class_name : "<unknown>",
            method_name != NULL ? method_name : "<unknown>",
            message != NULL ? message : "failed to lower");
    }
    return NULL;
}

LLVMValueRef *
llvm_member_call_alloc_args(LLVMGenCtx *ctx, ASTNode *node,
                            const char *class_name,
                            const char *method_name,
                            size_t argc)
{
    LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                                          (argc + 1) * sizeof(LLVMValueRef));
    if (args == NULL) {
        llvm_member_call_error_recovery(ctx, node, class_name, method_name,
            "could not allocate argument storage");
    }
    return args;
}

bool
llvm_member_call_store_arg(LLVMGenCtx *ctx, ASTNode *node,
                           const char *class_name,
                           const char *method_name,
                           LLVMValueRef *args,
                           size_t index,
                           LLVMValueRef value)
{
    if (value != NULL) {
        args[index + 1] = value;
        return true;
    }
    llvm_member_call_error_recovery(ctx, node, class_name, method_name,
        "could not lower an argument");
    return false;
}

LLVMValueRef
llvm_member_call_adjust_pointer_self_arg(LLVMGenCtx *ctx,
                                         const MIRDeclMethod *method_meta,
                                         ASTNode *method_decl,
                                         size_t logical_index,
                                         ASTNode *arg_node,
                                         LLVMValueRef arg_val)
{
    size_t logical_idx = 0;

    if (method_meta == NULL
        && (method_decl == NULL || method_decl->type != AST_FUNC_DECL))
        return arg_val;

    size_t method_param_count = method_meta != NULL
        ? llvm_mir_decl_method_param_count(method_meta)
        : ast_func_param_count(method_decl);

    for (size_t pk = 0; pk < method_param_count; pk++) {
        FuncParam *p = method_meta != NULL
            ? llvm_mir_decl_method_param(method_meta, pk)
            : ast_func_param(method_decl, pk);
        const char *ptn = NULL;
        LLVMClassTypeEntry *param_cls = NULL;

        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        if (logical_idx != logical_index) {
            logical_idx++;
            continue;
        }

        ptn = llvm_mir_decl_method_param_type_name(method_meta, pk);
        if (ptn == NULL && p->type != NULL && p->type->type == AST_TYPE)
            ptn = ast_type_name(p->type);
        param_cls = ptn != NULL ? llvm_lookup_class(ctx, ptn) : NULL;
        if (param_cls != NULL && param_cls->is_pointer_self_host
            && arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
            const char *arg_name = ast_identifier_name(arg_node);
            LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
            if (arg_var != NULL) {
                if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
                    return LLVMBuildLoad2(ctx->builder, arg_var->type,
                        arg_var->alloca, llvm_tmp_name(ctx));
                return arg_var->alloca;
            }
        }
        break;
    }

    return arg_val;
}

char *
llvm_member_call_mangle_method_name(LLVMGenCtx *ctx, ASTNode *node,
                                    const char *class_name,
                                    const char *method_name)
{
    size_t class_len;
    size_t method_len;
    size_t fn_len;
    char *full_name;

    if (class_name == NULL || method_name == NULL) {
        llvm_member_call_error_recovery(ctx, node, class_name, method_name,
            "requires a concrete receiver and method name");
        return NULL;
    }

    class_len = strlen(class_name);
    method_len = strlen(method_name);
    if (method_len > ((size_t)-1) - class_len - 2) {
        llvm_member_call_error_recovery(ctx, node, class_name, method_name,
            "method name is too large");
        return NULL;
    }

    fn_len = class_len + method_len + 2;
    full_name = pgy_arena_alloc(&ctx->scratch, fn_len);
    if (full_name == NULL) {
        llvm_member_call_error_recovery(ctx, node, class_name, method_name,
            "could not allocate method name");
        return NULL;
    }
    snprintf(full_name, fn_len, "%s_%s", class_name, method_name);
    return full_name;
}

#endif
