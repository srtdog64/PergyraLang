#ifndef PGY_TYPE_CHECKER_WORLD_INTERNAL_H
#define PGY_TYPE_CHECKER_WORLD_INTERNAL_H

#include "type_checker_internal.h"

Type *world_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
Type *world_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx);

ASTNode *find_world_zone_slot_local(ASTNode *world, const char *slot_name);
ASTNode *find_world_state_local(ASTNode *world, const char *state_name);
ASTNode *find_world_state_before_local(ASTNode *world,
                                       const char *state_name,
                                       size_t limit);
ASTNode *resolve_world_zone_decl_local(ASTNode *world,
                                       SemanticContext *ctx,
                                       const char *slot_name);
const char *resolve_world_plain_zone_input_name(ASTNode *world,
                                                const char *input_name);
ASTNode *find_zone_layer_slot_local(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_state_decl_local(ASTNode *zone, const char *state_name);
bool resolve_world_zone_state(ASTNode *world,
                              ASTNode *site,
                              const char *state_name,
                              SemanticContext *ctx,
                              const char *action_name,
                              const char **zone_slot_name_out);
void type_check_world_states(ASTNode *world, SemanticContext *ctx);

#endif /* PGY_TYPE_CHECKER_WORLD_INTERNAL_H */
