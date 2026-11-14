#ifndef PERGYRA_TYPE_CHECKER_ABILITY_WHERE_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_ABILITY_WHERE_INTERNAL_H

#include "type_checker_internal.h"

bool validate_ability_decl_where_clause_reference(ASTNode *ability_decl,
                                                  ASTNode *ability_ref,
                                                  const ASTNode *site,
                                                  SemanticContext *ctx,
                                                  const char *owner_label,
                                                  const char *owner_name);

#endif
