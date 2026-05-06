#include "mir_cfg_contract_control.h"

bool
mir_stmt_ast_is_cfg_owned_control(const ASTNode *ast)
{
    if (ast == NULL)
        return false;

    switch (ast->type) {
    case AST_WITH_STMT:
    case AST_UNSAFE_BLOCK:
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_SELECT_STMT:
    case AST_MATCH_STMT:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_RETURN:
        return true;
    default:
        return false;
    }
}
