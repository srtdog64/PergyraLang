#ifndef PERGYRA_MIR_CFG_CONTRACT_CONTROL_H
#define PERGYRA_MIR_CFG_CONTRACT_CONTROL_H

#include "../parser/ast.h"

/* CFG-owned control containers are lowered by HIR/MIR into explicit block
 * edges. They must not survive as fallback MIR_STMT instructions. */
static bool
mir_stmt_ast_is_cfg_owned_control(const ASTNode *ast)
{
    if (ast == NULL)
        return false;

    switch (ast->type) {
    case AST_WITH_STMT:
    case AST_PARALLEL_BLOCK:
    case AST_UNSAFE_BLOCK:
    case AST_DEFER_STMT:
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

#endif
