#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_intent_policy.h"

#include "../parser/ast_api.h"

ASTNode *
llvm_non_mir_intent_call_binding_at(ASTNode *intent_decl, size_t index,
                                    size_t *binding_count_out)
{
    size_t binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    ASTNode **bindings = ast_intent_decl_bindings(intent_decl, &binding_count);
    ASTNode **involves = ast_intent_decl_involves(intent_decl, &involve_count);
    ASTNode **values = ast_intent_decl_values(intent_decl, &value_count);

    if (binding_count_out != NULL) {
        *binding_count_out = binding_count > 0
            ? binding_count
            : (involve_count + value_count);
    }
    if (binding_count > 0)
        return (bindings != NULL && index < binding_count)
            ? bindings[index]
            : NULL;
    if (index < involve_count)
        return involves != NULL ? involves[index] : NULL;
    index -= involve_count;
    return (values != NULL && index < value_count) ? values[index] : NULL;
}

bool
llvm_intent_call_arg_can_take_subject_address(ASTNode *arg_node)
{
    if (arg_node == NULL)
        return false;
    return arg_node->type == AST_IDENTIFIER
        || arg_node->type == AST_MEMBER_ACCESS
        || arg_node->type == AST_ARRAY_ACCESS;
}

#endif /* PGY_LLVM_ENABLED */
