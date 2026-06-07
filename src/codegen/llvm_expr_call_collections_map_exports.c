#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_map_exports.h"

#include "codegen_hashmap_key_policy.h"
#include "llvm_internal_api.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

LLVMFuncEntry *
llvm_required_hashmap_raw_export(LLVMGenCtx *ctx,
                                 ASTNode *node,
                                 const char *callee_name,
                                 const char *operation,
                                 const char *key_name)
{
    char export_name[64];
    LLVMFuncEntry *fn;

    if (!pgy_hashmap_key_raw_export_name(operation, key_name,
            export_name, sizeof(export_name))) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM collection operation '%s' requires stable HashMap<Bool|Int|Long|String, T> key metadata",
                callee_name != NULL ? callee_name : "HashMap operation");
        }
        return NULL;
    }

    fn = llvm_lookup_function(ctx, export_name);
    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "HashMap operation",
            export_name);
    }
    return fn;
}

LLVMFuncEntry *
llvm_required_hashmap_raw_string_value_export(LLVMGenCtx *ctx,
                                              ASTNode *node,
                                              const char *callee_name,
                                              const char *operation,
                                              const char *key_name)
{
    char export_name[80];
    LLVMFuncEntry *fn;

    if (!pgy_hashmap_key_raw_string_value_export_name(operation, key_name,
            export_name, sizeof(export_name))) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM collection operation '%s' requires stable HashMap<Bool|Int|Long|String, String> key metadata",
                callee_name != NULL ? callee_name : "HashMap operation");
        }
        return NULL;
    }

    fn = llvm_lookup_function(ctx, export_name);
    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "HashMap operation",
            export_name);
    }
    return fn;
}

LLVMTypeRef
llvm_hashmap_key_array_type(LLVMGenCtx *ctx, const char *key_name)
{
    switch (pgy_hashmap_key_kind_from_name(key_name)) {
    case PGY_HASHMAP_KEY_INT:
        return ctx->array_type_Int;
    case PGY_HASHMAP_KEY_LONG:
        return ctx->array_type_Long;
    case PGY_HASHMAP_KEY_BOOL:
        return ctx->array_type_Bool;
    case PGY_HASHMAP_KEY_STRING:
        return ctx->array_type_String;
    case PGY_HASHMAP_KEY_UNKNOWN:
        return NULL;
    }
    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
