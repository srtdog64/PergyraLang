#ifndef PERGYRA_TYPE_CHECKER_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_INTERNAL_H

#include "type_checker.h"

bool type_is_constructed_named(const Type *type, const char *name);
bool type_is_qubit(const Type *type);
bool type_is_slot_handle(const Type *type);
bool type_is_owned_slot_handle(const Type *type);
bool type_is_read_view(const Type *type);
bool type_is_write_view(const Type *type);
bool type_is_move_token(const Type *type);
bool type_is_resource_handle(const Type *type);
bool type_is_anchored_resource_handle(const Type *type);
bool type_is_movable_resource_handle(const Type *type);
bool type_is_capability_bearing(const Type *type);
bool type_is_subject_type(const Type *type, SemanticContext *ctx);
bool type_requires_boundary_borrow_tracking(const Type *type, SemanticContext *ctx);
void semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask);

bool consume_qubit_value(ASTNode *expr, SemanticContext *ctx,
                         const char *action);
Type *type_check_qubit_use(ASTNode *expr, SemanticContext *ctx);
bool identifier_is_borrowed_boundary_param(ASTNode *expr, SemanticContext *ctx);

/* Currently-resolved nominal host (class/zone/world/relation/effect)
 * declaration for `ctx`.  Promoted to extern so visibility/access
 * helpers in type_checker_visibility.c can reach it without depending
 * on the .inc include order. */
ASTNode *current_host_decl(SemanticContext *ctx);
ASTNode *find_type_alias_decl(ASTNode *program, const char *name);
ASTNode *find_type_decl_by_name(ASTNode *program, const char *type_name);
ASTNode *find_ability_decl_by_name(ASTNode *program, const char *name);
ASTNode *find_zone_domain_slot(ASTNode *zone, const char *slot_name);
size_t generic_params_required_count(GenericParams *params);
ASTNode **collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                              GenericParams *provided_args,
                                              const ASTNode *site,
                                              SemanticContext *ctx,
                                              const char *owner_kind,
                                              const char *owner_name,
                                              size_t *out_count);
char *format_type_constraint_bounds(TypeConstraint *tc);
void semantic_type_resolution_record_type_ref_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    const ASTNode *provider_type_ref,
    const char *reason);
void semantic_type_resolution_collect_type_refs(
    ASTNode *type_node,
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    const char *reason);
void semantic_type_resolution_collect_generic_contract_inventory(
    GenericParams *gp,
    WhereClause *wc,
    SemanticContext *ctx,
    const ASTNode *owner,
    const char *owner_kind,
    const char *owner_name);
void semantic_type_resolution_record_string_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    const char *provider_name,
    const char *reason);
void semantic_type_resolution_precollect_required_abilities(
    ASTNode **ability_refs,
    size_t ability_count,
    SemanticContext *ctx,
    const ASTNode *owner,
    const char *consumer_name,
    const char *reason);
void semantic_type_resolution_register_top_level_decl(ASTNode *stmt,
                                                      SemanticContext *ctx);
void semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                           const ASTNode *site,
                                                           const char *label);
void semantic_type_resolution_record_local_contract_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_label,
    const ASTNode *provider_site,
    const char *provider_label,
    const char *reason);
char *semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                                     const char *slot_name);
char *semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                                 const char *state_name);
char *semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                               const char *slot_name);
char *semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                                const char *slot_name);
char *semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                                const char *state_name);
char *semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                                     const char *target_slot_name,
                                                     const char *source_slot_name,
                                                     const char *target_field_name,
                                                     const char *source_field_name);
char *semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                           const char *slot_name,
                                                           const char *field_path);
ASTNode *semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                         const char *slot_name,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_record_named_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    TypeResolutionNodeKind provider_kind,
    const ASTNode *provider_site,
    const char *provider_name,
    const char *reason);
size_t type_resolution_intern_node(TypeResolutionGraph *graph,
                                   TypeResolutionNodeKind kind,
                                   const ASTNode *site,
                                   const char *label);
void type_resolution_add_edge(TypeResolutionGraph *graph,
                              size_t from,
                              size_t to,
                              const char *reason);
bool type_resolution_find_path(TypeResolutionGraph *graph,
                               size_t current,
                               size_t goal,
                               bool *visited,
                               size_t *path,
                               size_t *path_len,
                               size_t path_cap);
char *type_resolution_format_cycle(TypeResolutionGraph *graph,
                                   size_t *path,
                                   size_t path_len,
                                   size_t closing_node);

/* Resource-handle compile-time state tracking.
 * Today the richer semantic state machine is QubitSlot-specific. */
QubitSemanticState get_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx);
bool set_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx,
                              QubitSemanticState new_state);
const char *qubit_state_name(QubitSemanticState state);
Type *type_get_constructed_arg(const Type *type, size_t index);
Type *wrap_constructed(Type *constructor, Type *inner);

/* Compile-time entanglement pool tracking */
int32_t alloc_entangle_pool(SemanticContext *ctx);
int32_t get_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx);
void    set_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx,
                                int32_t pool_id);
void    merge_entangle_pools(SemanticContext *ctx,
                             int32_t dst_pool, int32_t src_pool);
void    propagate_collapse_to_pool(SemanticContext *ctx, int32_t pool_id);

Type *type_check_stdlib_call(ASTNode *expr, const char *name,
                             SemanticContext *ctx);

#endif
