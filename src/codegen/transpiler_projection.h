/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend projection provenance and nominal type predicates.
 */

#ifndef PERGYRA_TRANSPILER_PROJECTION_H
#define PERGYRA_TRANSPILER_PROJECTION_H

#include "transpiler.h"
#include "transpiler_decl_lookup.h"

bool transpiler_domain_slot_view_is_projection_slot(
    const TranspilerHostedDomainSlotView *slot_view,
    size_t index,
    ASTNode **refreshes,
    size_t refresh_count);
const char *transpiler_domain_slot_view_bindable_name(
    const TranspilerHostedDomainSlotView *slot_view,
    size_t nth);
const char *transpiler_current_overlay_domain_slot_type_name(
    TranspilerCtx *ctx,
    const char *slot_name);
bool transpiler_current_overlay_domain_slot_is_projection(
    TranspilerCtx *ctx,
    const char *slot_name);
bool transpiler_zone_domain_slot_is_projection(TranspilerCtx *ctx,
                                               ASTNode *zone_decl,
                                               const char *slot_name);
bool transpiler_current_world_has_field(TranspilerCtx *ctx,
                                        const char *field_name);
ASTNode *transpiler_find_zone_state_decl(ASTNode *zone_decl,
                                         const char *state_name);
ASTNode *transpiler_find_world_state_decl(ASTNode *world_decl,
                                          const char *state_name);
bool transpiler_zone_has_layer_slot(TranspilerCtx *ctx,
                                    ASTNode *zone_decl,
                                    const char *slot_name);
bool transpiler_world_has_zone_slot(TranspilerCtx *ctx,
                                    ASTNode *world_decl,
                                    const char *slot_name);
ASTNode *transpiler_resolve_world_zone_decl(TranspilerCtx *ctx,
                                            ASTNode *world_decl,
                                            const char *slot_name);
int resolve_projection_source_path_by_name(TranspilerCtx *ctx,
                                           const char *source_type_name,
                                           const char *field_name,
                                           unsigned depth,
                                           char **path_out);
char *emit_projection_literal_by_name(TranspilerCtx *ctx,
                                      const char *target_type_name,
                                      const char *source_type_name,
                                      ASTNode *refresh,
                                      const char *source_expr);
bool transpiler_projection_type_is_struct_like(TranspilerCtx *ctx,
                                               const char *type_name);
bool is_subject_type_name(TranspilerCtx *ctx, const char *type_name);
bool is_nominal_host_type_name(TranspilerCtx *ctx, const char *type_name);

#endif /* PERGYRA_TRANSPILER_PROJECTION_H */
