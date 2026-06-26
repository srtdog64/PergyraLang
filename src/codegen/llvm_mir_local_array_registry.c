/*
 * LLVM MIR source-local array registry facts.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_array_registry.h"

#include "llvm_internal_api.h"
#include "llvm_mir_local_element_type.h"
#include "parser/ast_api.h"

bool
llvm_mir_register_source_local_array_fact(const MIRRoutine *routine,
                                          LLVMGenCtx *ctx,
                                          const MIRInstruction *inst,
                                          const char *base_name,
                                          ASTNode *value_expr,
                                          LLVMValueRef alloca)
{
    LLVMTypeRef elem_type = NULL;
    char elem_name_buf[256];
    const char *elem_name = NULL;
    const char *source_type_name;

    if (routine == NULL || ctx == NULL || inst == NULL || base_name == NULL
        || alloca == NULL || value_expr == NULL
        || value_expr->type != AST_ARRAY_LITERAL) {
        return true;
    }

    source_type_name = mir_routine_source_local_type_name(routine, base_name);
    elem_name_buf[0] = '\0';
    if (source_type_name != NULL
        && llvm_constructed_arg_name_copy(source_type_name, 0,
            elem_name_buf, sizeof(elem_name_buf))) {
        elem_name = elem_name_buf;
    }

    if (ast_array_literal_count(value_expr) > 0
        && ast_array_literal_element(value_expr, 0) != NULL) {
        elem_type = llvm_stmt_infer_expr_type(ctx,
            ast_array_literal_element(value_expr, 0));
    } else {
        if (source_type_name != NULL) {
            elem_type = llvm_mir_local_elem_type_from_type_name(
                ctx, source_type_name);
        }
        if (elem_type == NULL)
            elem_type = llvm_mir_local_elem_type_from_layout(
                ctx, inst->type_layout);
    }

    if (!llvm_mir_local_require_elem_type(ctx, value_expr, elem_type,
            base_name)) {
        return false;
    }

    llvm_register_array_var_binding(ctx, base_name, alloca, elem_type,
        elem_name, (int64_t)ast_array_literal_count(value_expr));
    return !ctx->has_error;
}

#endif
