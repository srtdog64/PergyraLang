#ifndef PERGYRA_TYPE_CHECKER_ABILITY_MATCH_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_ABILITY_MATCH_INTERNAL_H

#include "type_checker_internal.h"

bool semantic_role_decl_has_ability(SemanticContext *ctx, ASTNode *role,
                                    ASTNode *ability_ref);
bool semantic_subject_type_has_ability(SemanticContext *ctx,
                                       const char *type_name,
                                       ASTNode *ability_ref);
ASTNode *semantic_subject_type_find_base_ability_impl(SemanticContext *ctx,
                                                      const char *type_name,
                                                      const char *ability_name);

#endif
