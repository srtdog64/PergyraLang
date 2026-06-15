/*
 * LLVM MIR local Array/Slice element type facts.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_element_type.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "parser/ast_api.h"

LLVMTypeRef
llvm_mir_local_elem_type_from_type_name(LLVMGenCtx *ctx,
                                        const char *type_name)
{
    char inner_name[256];

    if (ctx == NULL || type_name == NULL)
        return NULL;
    switch (pgy_classify_type(type_name)) {
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
        break;
    default:
        return NULL;
    }
    if (!llvm_constructed_arg_name_copy(type_name, 0,
            inner_name, sizeof(inner_name))) {
        return NULL;
    }
    if (inner_name[0] == '\0' || strcmp(inner_name, "Unknown") == 0)
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_name);
}

LLVMTypeRef
llvm_mir_local_elem_type_from_layout(LLVMGenCtx *ctx,
                                     const MIRTypeLayout *layout)
{
    return layout != NULL
        ? llvm_mir_local_elem_type_from_type_name(ctx, layout->abi_type_name)
        : NULL;
}

LLVMTypeRef
llvm_mir_local_elem_type_from_type_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    const char *type_name;
    GenericParams *args;
    GenericParam *first_arg;
    const char *inner_name;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    type_name = ast_type_name(type_node);
    if (type_name == NULL)
        return NULL;
    if (strcmp(type_name, "Array") != 0
        && strcmp(type_name, "Slice") != 0
        && strncmp(type_name, "Array<", 6) != 0
        && strncmp(type_name, "Slice<", 6) != 0) {
        return NULL;
    }
    args = ast_type_generic_args(type_node);
    if (args == NULL || ast_generic_param_count(args) == 0)
        return NULL;
    first_arg = ast_generic_param_at(args, 0);
    inner_name = ast_generic_param_name(first_arg);
    if (inner_name == NULL || inner_name[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_name);
}

bool
llvm_mir_local_require_elem_type(LLVMGenCtx *ctx, ASTNode *site,
                                 LLVMTypeRef elem_type,
                                 const char *surface)
{
    if (ctx != NULL && elem_type != NULL && !ctx->has_error)
        return true;
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx,
            site,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR local '%s' requires concrete Array<T>/Slice<T> element metadata",
            surface != NULL ? surface : "<local>");
    }
    return false;
}

#endif /* PGY_LLVM_ENABLED */
