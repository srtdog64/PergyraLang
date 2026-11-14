#ifndef PERGYRA_TYPE_CHECKER_MODULE_CONTRACT_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_MODULE_CONTRACT_INTERNAL_H

#include "type_checker_internal.h"

ASTNode *resolve_required_ability_decl(ASTNode *ability_ref,
                                       const ASTNode *site,
                                       SemanticContext *ctx,
                                       const char *owner_label,
                                       const char *owner_name);
void validate_action_required_abilities(ASTNode *node,
                                        ASTNode *enclosing_nominal,
                                        SemanticContext *ctx);

#endif
