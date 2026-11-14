#ifndef PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H
#define PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H

#include "../type_checker.h"

Type *type_check_assignment(ASTNode *expr, SemanticContext *ctx);
bool semantic_admit_binding_replacement(ASTNode *target, SemanticContext *ctx);
bool semantic_admit_value_mutation(ASTNode *place, SemanticContext *ctx);
/* Marks `binding` as a name for storage that `owner` (a for-in iterable or a
 * match subject) holds; changing that storage takes the owner's permission. */
void semantic_mark_storage_alias_binding(Symbol *binding, ASTNode *owner,
                                         SemanticContext *ctx);

#endif /* PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H */
