#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include "llvm_internal_api.h"

LLVMTypeRef
llvm_collection_required_value_type(LLVMGenCtx *ctx, ASTNode *node,
                                    const char *collection_kind,
                                    const char *var_name,
                                    const char *type_name,
                                    LLVMValueRef *out)
{
    if (type_name == NULL || type_name[0] == '\0') {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation requires concrete element/value type metadata for '%s'",
                collection_kind != NULL ? collection_kind : "collection",
                var_name != NULL ? var_name : "<collection>");
        }
        if (out != NULL)
            *out = NULL;
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, type_name);
}

LLVMFuncEntry *
llvm_required_collection_function(LLVMGenCtx *ctx,
                                  ASTNode *node,
                                  const char *callee_name,
                                  const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "collection operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

LLVMVarEntry *
llvm_collection_required_receiver_var(LLVMGenCtx *ctx,
                                      ASTNode *node,
                                      ASTNode *receiver,
                                      const char *callee_name,
                                      const char *collection_kind,
                                      LLVMValueRef *out)
{
    const char *kind;
    const char *name;
    LLVMVarEntry *var;

    kind = collection_kind != NULL ? collection_kind : "collection";
    if (receiver == NULL || receiver->type != AST_IDENTIFIER) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation '%s' requires an identifier receiver",
                kind,
                callee_name != NULL ? callee_name : "collection operation");
        }
        if (out != NULL)
            *out = NULL;
        return NULL;
    }

    name = ast_identifier_name(receiver);
    var = llvm_scope_lookup(ctx, name);
    if (var == NULL) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation '%s' requires registered %s local '%s'",
                kind,
                callee_name != NULL ? callee_name : "collection operation",
                kind,
                name != NULL ? name : "<collection>");
        }
        if (out != NULL)
            *out = NULL;
        return NULL;
    }
    return var;
}

bool
llvm_collection_extended_error_out(LLVMGenCtx *ctx, ASTNode *node,
                                   LLVMValueRef *out, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM collection extended builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

#endif
