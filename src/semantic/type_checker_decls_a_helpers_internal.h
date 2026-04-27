/* Helpers previously living as static include-fragment code, promoted to
 * external linkage so role/party/roster declaration validators can call them
 * across translation unit boundaries.
 *
 * Definitions now live in focused semantic owner TUs; this header preserves
 * the narrow dependency seam without relying on include order.
 */
#ifndef PERGYRA_TYPE_CHECKER_DECLS_A_HELPERS_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_DECLS_A_HELPERS_INTERNAL_H

#include "type_checker_internal.h"

bool any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref);

ASTNode *any_subject_role_find_base_ability_impl(ASTNode *program,
                                                 const char *ability_name);

void validate_ability_require_fields_for_role(ASTNode *role_decl,
                                              ASTNode *ability_decl,
                                              ASTNode *ability_ref,
                                              SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_DECLS_A_HELPERS_INTERNAL_H */
