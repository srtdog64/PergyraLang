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

bool any_subject_role_has_ability(SemanticContext *ctx, ASTNode *ability_ref);

ASTNode *semantic_find_role_decl(ASTNode *program, const char *role_name);

ASTNode *semantic_find_role_decl_by_name(SemanticContext *ctx,
                                         const char *role_name);

ASTNode *semantic_find_role_decl_for_type_name(SemanticContext *ctx,
                                               const char *type_name);
ASTNode *semantic_find_next_role_decl_for_type_name(SemanticContext *ctx,
                                                    const char *type_name,
                                                    const ASTNode *after);

ASTNode *semantic_role_for_type_node(ASTNode *role_decl);

const char *semantic_role_for_type_name(ASTNode *role_decl);

ASTNode *any_subject_role_find_base_ability_impl(SemanticContext *ctx,
                                                 const char *ability_name);

void validate_ability_require_fields_for_role(ASTNode *role_decl,
                                              ASTNode *ability_decl,
                                              ASTNode *ability_ref,
                                              SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_DECLS_A_HELPERS_INTERNAL_H */
