#ifndef PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H
#define PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H

#include "transpiler.h"

bool role_has_ability(ASTNode *role, const char *ability_name);
bool role_has_method(ASTNode *role, const char *method_name);
char *render_ability_ref_vtable_tag(ASTNode *ability_ref);
char *render_ability_ref_vtable_tag_in_ctx(TranspilerCtx *ctx,
                                           ASTNode *ability_ref);
char *transpiler_party_slot_first_ability_tag(ASTNode *party_decl,
                                              const char *slot_name);
char *transpiler_party_slot_method_ability_tag(TranspilerCtx *ctx,
                                               ASTNode *party_decl,
                                               const char *slot_name,
                                               const char *method_name);

#endif /* PERGYRA_TRANSPILER_ROLE_ABILITY_HELPERS_H */
