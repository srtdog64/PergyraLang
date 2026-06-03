#include "mir_cfg_contract_control.h"

bool
mir_stmt_ast_type_is_unconditional_cfg_owned_control(ASTNodeType type)
{
    switch (type) {
    case AST_WITH_STMT:
    case AST_UNSAFE_BLOCK:
    case AST_RETURN:
        return true;
    default:
        return false;
    }
}

bool
mir_stmt_ast_type_is_cfg_owned_control(ASTNodeType type)
{
    if (mir_stmt_ast_type_is_unconditional_cfg_owned_control(type))
        return true;
    switch (type) {
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_SELECT_STMT:
    case AST_MATCH_STMT:
    case AST_BREAK:
    case AST_CONTINUE:
        return true;
    default:
        return false;
    }
}

bool
mir_stmt_ast_type_is_cfg_container(ASTNodeType type)
{
    return type == AST_DEFER_STMT || mir_stmt_ast_type_is_cfg_owned_control(type);
}

bool
mir_stmt_ast_is_unconditional_cfg_owned_control(const ASTNode *ast)
{
    if (ast == NULL)
        return false;
    return mir_stmt_ast_type_is_unconditional_cfg_owned_control(ast->type);
}

bool
mir_stmt_ast_is_cfg_owned_control(const ASTNode *ast)
{
    if (ast == NULL)
        return false;
    return mir_stmt_ast_type_is_cfg_owned_control(ast->type);
}
