#ifndef PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H
#define PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H

#include "type_checker.h"

Type *type_check_assignment(ASTNode *expr, SemanticContext *ctx);

#endif /* PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H */
