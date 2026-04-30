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
        size_t param_count = type_node->data.event_handler_type.param_count;
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef ret_type = ctx->type_void;
        LLVMTypeRef fn_type;

        if (type_node->data.event_handler_type.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx,
                type_node->data.event_handler_type.return_type);

        if (param_count > 0) {
            /* Param-type buffer is consumed by LLVMFunctionType (which
             * copies contents) and never retained by the caller. */
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            if (param_types == NULL)
                return LLVMPointerType(LLVMFunctionType(ret_type, NULL, 0, 0), 0);
            for (size_t i = 0; i < param_count; i++) {
                param_types[i] = ast_type_to_llvm(ctx,
                    type_node->data.event_handler_type.param_types[i]);
            }
        }

        fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
        /* param_types is ctx->scratch-owned. */
        return LLVMPointerType(fn_type, 0);
    }

    /* Tuple type: anonymous struct { T0, T1, ... } */
    if (type_node->type == AST_TYPE
        && type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        size_t n = type_node->data.type.tuple_element_count;
        /* Field-type buffer is consumed by LLVMStructTypeInContext (copies). */
        LLVMTypeRef *fields = pgy_arena_calloc(&ctx->scratch,
            n * sizeof(LLVMTypeRef));
        if (fields == NULL)
            return ctx->type_i32;
        for (size_t i = 0; i < n; i++)
            fields[i] = ast_type_to_llvm(ctx,
                type_node->data.type.tuple_elements[i]);
        LLVMTypeRef result = LLVMStructTypeInContext(ctx->context, fields,
            (unsigned)n, 0);
        /* fields is ctx->scratch-owned. */
        return result;
    }

    if (type_node->type == AST_TYPE && type_node->data.type.name != NULL) {
        char *full_name = llvm_render_type_name_scratch(type_node, &ctx->scratch);
        if (full_name == NULL || full_name[0] == '\0') {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM type rendering requires concrete type metadata; silent Int fallback is not allowed");
            return ctx->type_i32;
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
    return ctx->type_i32;
}

#endif /* PGY_LLVM_ENABLED */
