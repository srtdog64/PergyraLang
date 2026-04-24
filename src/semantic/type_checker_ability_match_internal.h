#ifndef PERGYRA_TYPE_CHECKER_ABILITY_MATCH_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_ABILITY_MATCH_INTERNAL_H

#include "type_checker_internal.h"

bool ability_ref_matches(ASTNode *program, ASTNode *impl_ref, ASTNode *required_ref);
bool role_decl_has_ability(ASTNode *role, ASTNode *program,
                           ASTNode *ability_ref, int depth);
bool subject_type_has_ability(ASTNode *program, const char *type_name,
                              ASTNode *ability_ref);
ASTNode *subject_type_find_base_ability_impl(ASTNode *program,
                                             const char *type_name,
                                             const char *ability_name);

#endif
