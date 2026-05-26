/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Domain query helper declarations for builtin predicates.
 */

#ifndef PERGYRA_TYPE_CHECKER_BUILTINS_QUERY_DOMAIN_H
#define PERGYRA_TYPE_CHECKER_BUILTINS_QUERY_DOMAIN_H

#include <stddef.h>

#include "../parser/ast.h"
#include "type_system.h"

typedef struct SemanticContext SemanticContext;

ASTNode *find_zone_domain_slot_local(ASTNode *zone, const char *slot_name);
ASTNode *builtin_find_zone_layer_slot_local(ASTNode *zone,
                                            const char *slot_name);
ASTNode *find_domain_projection_slot_local(ASTNode **slots, size_t slot_count,
                                           ASTNode **refreshes,
                                           size_t refresh_count,
                                           const char *slot_name);
ASTNode *find_zone_projection_slot_local(ASTNode *zone,
                                         const char *slot_name);
ASTNode *find_zone_state_decl_local_builtin(ASTNode *zone,
                                            const char *state_name);
ASTNode *builtin_resolve_world_zone_decl_local(SemanticContext *ctx,
                                               ASTNode *world,
                                               const char *slot_name);
ASTNode *current_projection_host_decl(SemanticContext *ctx,
                                      const char **label_out,
                                      ASTNode ***slots_out,
                                      size_t *slot_count_out);

bool decl_is_subject_nominal(ASTNode *decl);

#endif /* PERGYRA_TYPE_CHECKER_BUILTINS_QUERY_DOMAIN_H */
