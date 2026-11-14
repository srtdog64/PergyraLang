#ifndef PERGYRA_HIR_LOWER_CFG_H
#define PERGYRA_HIR_LOWER_CFG_H

#include <stdbool.h>

#include "../hir.h"

bool hir_lower_func_body_cfg(ASTNode *body, HIRRoutine *routine);
bool hir_lower_intent_cfg(ASTNode *intent, HIRRoutine *routine);

bool hir_stmt_ast_is_cfg_owned_control(const ASTNode *ast);
bool hir_stmt_ast_type_is_cfg_owned_control(ASTNodeType type);
bool hir_stmt_ast_type_is_cfg_container(ASTNodeType type);
bool hir_stmt_ast_is_unconditional_cfg_owned_control(const ASTNode *ast);
bool hir_stmt_ast_type_is_unconditional_cfg_owned_control(ASTNodeType type);

#endif
