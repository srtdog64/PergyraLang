#include "codegen_match_subject_lookup.h"

#include "../parser/ast.h"

ASTNode *
pgy_codegen_match_subject_for_case(ASTNode *func_decl, ASTNode *case_node)
{
    if (func_decl == NULL || case_node == NULL)
        return NULL;
    if (func_decl->type != AST_FUNC_DECL || case_node->type != AST_MATCH_CASE)
        return NULL;
    return ast_find_match_subject_for_case(ast_func_body(func_decl),
                                           case_node);
}
