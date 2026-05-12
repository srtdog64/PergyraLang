#ifdef PGY_LLVM_ENABLED

#include "llvm_member_call_internal.h"

#include <stdio.h>
#include <string.h>

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
