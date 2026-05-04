#ifndef PERGYRA_TYPE_CHECKER_FLOW_LOOPS_H
#define PERGYRA_TYPE_CHECKER_FLOW_LOOPS_H

#include "type_checker_flow_internal.h"

bool type_check_for_loop(ASTNode *node, SemanticContext *ctx);
bool type_check_while_loop(ASTNode *node, SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_LOOPS_H */
