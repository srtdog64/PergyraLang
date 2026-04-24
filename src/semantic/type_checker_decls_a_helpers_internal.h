/* Helpers previously living as static in type_checker_decls_a.inc /
 * type_checker_decls_domain_helpers.inc, promoted to external linkage so
 * role/party/roster declaration validators (split into their own TUs in
 * the 3-B slice) can call them across translation unit boundaries.
 *
 * Definitions still live in their original `.inc` files; only linkage
 * is lifted.  See docs/101_semantic_split_template.md §8 for the
 * externalization pattern.
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
