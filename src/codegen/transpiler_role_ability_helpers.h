#ifndef PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H
#define PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H

#include "transpiler.h"

bool role_has_ability(ASTNode *role, const char *ability_name);
bool role_has_method(ASTNode *role, const char *method_name);
char *render_ability_ref_vtable_tag(ASTNode *ability_ref);

#endif /* PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H */
