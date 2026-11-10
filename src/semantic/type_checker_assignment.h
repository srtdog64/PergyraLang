#ifndef PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H
#define PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H

#include "type_checker.h"

Type *type_check_assignment(ASTNode *expr, SemanticContext *ctx);
bool semantic_admit_binding_replacement(ASTNode *target, SemanticContext *ctx);
bool semantic_admit_value_mutation(ASTNode *place, SemanticContext *ctx);

#endif /* PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H */
