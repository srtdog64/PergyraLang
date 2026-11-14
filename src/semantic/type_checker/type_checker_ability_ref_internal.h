#ifndef PERGYRA_TYPE_CHECKER_ABILITY_REF_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_ABILITY_REF_INTERNAL_H

#include "type_checker_internal.h"

const char *ability_ref_name(ASTNode *ability_ref);
char *ability_ref_display(ASTNode *ability_ref);
char *ability_decl_signature_display(const char *ability_name, GenericParams *params);
char *ability_ref_effective_display(ASTNode *ability_decl, ASTNode *ability_ref);

#endif
