/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend AST type-node lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"

LLVMTypeRef
ast_type_to_llvm(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (type_node == NULL)
        return ctx->type_void;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = ast_event_handler_param_count(type_node);
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef ret_type = ctx->type_void;
        LLVMTypeRef fn_type;

        ASTNode *return_type = ast_event_handler_return_type(type_node);
        if (return_type != NULL) {
            ret_type = ast_type_to_llvm(ctx,
                return_type);
            if (ctx->has_error || ret_type == NULL)
                return NULL;
        }

        if (param_count > 0) {
            /* Param-type buffer is consumed by LLVMFunctionType (which
             * copies contents) and never retained by the caller. */
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            if (param_types == NULL) {
                llvm_set_error_at_with_hints(ctx, type_node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM event-handler type parameter allocation failed");
                return NULL;
            }
            for (size_t i = 0; i < param_count; i++) {
                param_types[i] = ast_type_to_llvm(ctx,
                    ast_event_handler_param_type(type_node, i));
                if (ctx->has_error || param_types[i] == NULL)
                    return NULL;
            }
        }

        fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
        /* param_types is ctx->scratch-owned. */
        return LLVMPointerType(fn_type, 0);
    }

    /* Tuple type: anonymous struct { T0, T1, ... } */
    if (type_node->type == AST_TYPE
        && ast_type_tuple_element_count(type_node) > 0) {
        size_t n = ast_type_tuple_element_count(type_node);
        /* Field-type buffer is consumed by LLVMStructTypeInContext (copies). */
        LLVMTypeRef *fields = pgy_arena_calloc(&ctx->scratch,
            n * sizeof(LLVMTypeRef));
        if (fields == NULL) {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM tuple type field allocation failed");
            return NULL;
        }
        for (size_t i = 0; i < n; i++) {
            fields[i] = ast_type_to_llvm(ctx,
                ast_type_tuple_element(type_node, i));
            if (ctx->has_error || fields[i] == NULL)
                return NULL;
        }
        LLVMTypeRef result = LLVMStructTypeInContext(ctx->context, fields,
            (unsigned)n, 0);
        /* fields is ctx->scratch-owned. */
        return result;
    }

    if (ast_type_name(type_node) != NULL) {
        char *full_name = llvm_render_type_name_scratch_in_ctx(
            ctx, type_node, &ctx->scratch);
        if (full_name == NULL || full_name[0] == '\0') {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM type rendering requires concrete type metadata; silent Int fallback is not allowed");
            return NULL;
        }
        LLVMTypeRef resolved = pergyra_type_to_llvm(ctx, full_name);
        return resolved;
    }

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, type_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM AST type mapping requires AST_TYPE or event handler metadata; silent i32 fallback is not allowed");
    }
    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
