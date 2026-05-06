#ifndef PERGYRA_MIR_CFG_CONTRACT_CONTROL_H
#define PERGYRA_MIR_CFG_CONTRACT_CONTROL_H

#include "../parser/ast.h"

bool mir_stmt_ast_is_cfg_owned_control(const ASTNode *ast);
bool mir_stmt_ast_type_is_cfg_owned_control(ASTNodeType type);

#endif
