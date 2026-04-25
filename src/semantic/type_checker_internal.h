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
bool semantic_find_active_slot_view(Scope *scope,
                                    const char **view_name_out,
                                    const char **view_kind_out,
                                    const char **source_slot_out);
bool type_is_general_boundary_type(const Type *type, SemanticContext *ctx);
bool type_is_capability_bearing(const Type *type);
bool type_is_subject_type(const Type *type, SemanticContext *ctx);
bool type_is_class_object_type(const Type *type, SemanticContext *ctx);
bool type_requires_boundary_borrow_tracking(const Type *type, SemanticContext *ctx);
Type *resolve_named_type(const char *name, SemanticContext *ctx,
                         const ASTNode *site);
const char *type_name_or_unknown(const Type *type);
bool name_looks_qualified(const char *name);
void semantic_format_function_signature(const Type *type,
                                        char *out,
                                        size_t out_cap);
void semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask);
void semantic_record_body_summary(SemanticContext *ctx, uint32_t summary_mask);
void semantic_record_callee_body_summary(SemanticContext *ctx,
                                         const Type *callee_type);
void semantic_record_callable_decl_summary(SemanticContext *ctx,
                                           ASTNode *callable_decl,
                                           uint32_t declared_effects);
Type *create_overlay_nominal_type(const char *name);
size_t overlay_field_count(ASTNode *decl);
ASTNode *overlay_field_decl_at(ASTNode *decl,
                               size_t index,
                               const char **field_name_out);
bool decl_is_subject_host(const ASTNode *decl);
ClassField *subject_host_field_at(ASTNode *decl, size_t index);
char *format_generic_subject_signature(const char *name,
                                       GenericParams *params);
TypeNominalFlavor nominal_flavor_from_decl(const ASTNode *decl);
uint32_t declared_effects_from_function_node(ASTNode *node,
                                             SemanticContext *ctx,
                                             bool *has_contract_out);
char *flatten_static_member_access(const ASTNode *expr, char separator);
Symbol *lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx);
void mark_world_embedded_zone_arguments(ASTNode *call, SemanticContext *ctx);
bool expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx);
bool expr_is_qubit_claim(const ASTNode *expr);
bool expr_is_device_slot_claim(const ASTNode *expr);
bool expr_is_movable_resource_transfer_source(const ASTNode *expr);
bool slot_transfer_compatible(const Type *from, const Type *to);
void validate_class_where_clause_specialization_ast(ASTNode *class_decl,
                                                    ASTNode *specialized_type,
                                                    ASTNode *site,
                                                    SemanticContext *ctx);

extern size_t g_resolve_type_node_calls;
extern size_t g_resolve_type_node_unique_nodes;
extern size_t g_resolve_type_node_cache_hits;
extern size_t g_resolve_type_node_cache_misses;

bool consume_qubit_value(ASTNode *expr, SemanticContext *ctx,
                         const char *action);
bool type_check_defer_body_flow(ASTNode *body, SemanticContext *ctx);
bool type_check_parallel_block_flow(ASTNode *node, SemanticContext *ctx);
Type *type_check_qubit_use(ASTNode *expr, SemanticContext *ctx);
bool identifier_is_borrowed_boundary_param(ASTNode *expr, SemanticContext *ctx);
size_t count_subject_domain_slots(ASTNode **slots, size_t slot_count);
size_t count_object_domain_slots(ASTNode **slots, size_t slot_count);
size_t count_bindable_domain_slots(ASTNode **slots,
                                   size_t slot_count,
                                   ASTNode **refreshes,
                                   size_t refresh_count);
bool type_check_projection_contract(ASTNode **slots,
                                    size_t slot_count,
                                    const char *owner_label,
                                    const char *owner_name,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name);
bool type_check_overlay_decl_common(ASTNode *node,
                                    SemanticContext *ctx,
                                    const char *name,
                                    SymbolKind kind,
                                    ASTNode **shared_fields,
                                    size_t shared_count,
                                    ASTNode **methods,
                                    size_t method_count,
                                    const char *kind_name);
bool type_check_domain_slots(ASTNode **slots,
                             size_t slot_count,
                             SemanticContext *ctx,
                             const char *kind_name);
bool type_check_domain_slot_initializers(ASTNode **slots,
                                         size_t slot_count,
                                         SemanticContext *ctx,
                                         const char *kind_name);
ASTNode *find_zone_effect_slot(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_relation_slot(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_state(ASTNode *zone, const char *state_name);
bool resolve_zone_effect_state(ASTNode *zone,
                               ASTNode *site,
                               const char *state_name,
                               SemanticContext *ctx,
                               const char *action_name,
                               const char **effect_slot_name,
                               const char **target_slot_name);
bool resolve_zone_relation_state(ASTNode *zone,
                                 ASTNode *site,
                                 const char *state_name,
                                 SemanticContext *ctx,
                                 const char *action_name,
                                 const char **relation_slot_name,
                                 const char **left_slot_name,
                                 const char **right_slot_name);
bool type_check_zone_participant_authority(ASTNode *zone,
                                           ASTNode *site,
                                           const char *participant_slot_name,
                                           SemanticContext *ctx,
                                           const char *action_name);
bool type_check_zone_projection_contract(ASTNode *zone,
                                         ASTNode *site,
                                         const char *object_slot_name,
                                         const char *source_slot_name,
                                         SemanticContext *ctx,
                                         const char *action_name);
bool type_check_zone_effect_contract(ASTNode *zone,
                                     ASTNode *apply_like,
                                     const char *effect_slot_name,
                                     const char *target_slot_name,
                                     SemanticContext *ctx,
                                     const char *action_name);
bool type_check_zone_relation_contract(ASTNode *zone,
                                       ASTNode *link_like,
                                       const char *relation_slot_name,
                                       const char *left_slot_name,
                                       const char *right_slot_name,
                                       SemanticContext *ctx,
                                       const char *action_name);

/* Currently-resolved nominal host (class/zone/world/relation/effect)
 * declaration for `ctx`.  Promoted to extern so visibility/access
 * helpers in type_checker_visibility.c can reach it without depending
 * on the .inc include order. */
ASTNode *current_host_decl(SemanticContext *ctx);
ASTNode *find_type_alias_decl(ASTNode *program, const char *name);
ASTNode *find_type_decl_by_name(ASTNode *program, const char *type_name);
ASTNode *find_ability_decl_by_name(ASTNode *program, const char *name);
ASTNode *find_callable_decl_by_name(ASTNode *program, const char *name);
ASTNode *find_domain_decl_by_name(ASTNode *program,
                                  ASTNodeType decl_type,
                                  const char *name);
ASTNode *constructor_decl_for_symbol_kind(ASTNode *program,
                                          SymbolKind kind,
                                          const char *name);
ASTNode *find_subject_host_decl_by_name(ASTNode *program,
                                        const char *type_name);
ASTNode *find_zone_domain_slot(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_authority(ASTNode *zone, const char *slot_name);
ASTNode *resolve_zone_subject_slot_for_participant(ASTNode *zone,
                                                   SemanticContext *ctx,
                                                   const char *participant_alias,
                                                   const char *participant_type_name,
                                                   bool *ambiguous_out);
ASTNode *semantic_world_find_zone_slot_local(ASTNode *world,
                                             const char *slot_name);
const char *resource_handle_display_name(const Type *type);
const char *format_effective_generic_type_list_scratch(SemanticContext *ctx,
                                                       const char *name,
                                                       Type **types,
                                                       size_t count);
void semantic_ctx_mark_embedded_world_zone_name(SemanticContext *ctx,
                                                const char *name,
                                                const char *world_name,
                                                const char *slot_name);
void semantic_stage_world_local_contracts(ASTNode *world_decl,
                                          SemanticContext *ctx);
void semantic_stage_zone_local_contracts(ASTNode *zone_decl);
void semantic_stage_world_local_contract_from_label(ASTNode *world_decl,
                                                    const char *label,
                                                    SemanticContext *ctx);
void semantic_stage_zone_local_contract_from_label(ASTNode *zone_decl,
                                                   const char *label,
                                                   SemanticContext *ctx);
bool semantic_stage_should_defer_to_graph(ASTNode *type_node,
                                          SemanticContext *ctx,
                                          const ASTNode *consumer_site,
                                          const char *consumer_name,
                                          const char *reason);
void semantic_stage_record_legacy_family(SemanticContext *ctx,
                                         const char *reason);
const char *semantic_symbol_kind_label(SymbolKind kind);
const char *intent_step_single_who_alias(const ASTNode *step);
ASTNode *find_intent_involves_local(ASTNode *intent, const char *alias);
ASTNode *intent_step_resolve_transfer_target_involves(
    ASTNode *intent_decl,
    ASTNode *step,
    const char **resolved_alias_out);
const char *intent_involves_type_name(ASTNode *involves);
bool domain_has_subject_slot_type(ASTNode **slots,
                                  size_t slot_count,
                                  SemanticContext *ctx,
                                  const char *type_name);
bool zone_has_effect_layer_type(ASTNode *zone, const char *effect_name);
bool decl_is_projection_source(const ASTNode *decl);
const char *projection_refresh_source_field_name(ASTNode *refresh,
                                                 const char *target_field_name);
bool projection_target_decl_has_field(ASTNode *target_decl,
                                      const char *field_name);
size_t generic_params_required_count(GenericParams *params);
void validate_where_clause_bounds(WhereClause *wc,
                                  SemanticContext *ctx,
                                  ASTNode *owner);
void validate_generic_param_defaults(GenericParams *gp,
                                     SemanticContext *ctx,
                                     ASTNode *owner,
                                     const char *kind_name);
void validate_generic_param_default_bounds(GenericParams *gp,
                                           WhereClause *wc,
                                           SemanticContext *ctx,
                                           ASTNode *owner,
                                           const char *owner_kind,
                                           const char *owner_name);
ASTNode **collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                              GenericParams *provided_args,
                                              const ASTNode *site,
                                              SemanticContext *ctx,
                                              const char *owner_kind,
                                              const char *owner_name,
                                              size_t *out_count);
char *format_type_constraint_bounds(TypeConstraint *tc);
int find_generic_param_index(GenericParams *gp, const char *param_name);
bool concrete_type_satisfies_bound(Type *concrete_type,
                                   ASTNode *bound_node,
                                   SemanticContext *ctx);
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
void semantic_type_resolution_record_resolved_type(SemanticContext *ctx,
                                                   ASTNode *type_node,
                                                   Type *resolved_type);
void semantic_type_resolution_record_owned_resolved_type(SemanticContext *ctx,
                                                         ASTNode *type_node,
                                                         Type *resolved_type);
Type *semantic_type_resolution_lookup_resolved_type(SemanticContext *ctx,
                                                    ASTNode *type_node);
Type *semantic_type_resolution_lookup_or_materialize(SemanticContext *ctx,
                                                     ASTNode *type_node);
void semantic_type_resolution_free_metadata(SemanticContext *ctx);
void semantic_type_resolution_try_record_stable_constructed_type(SemanticContext *ctx,
                                                                 ASTNode *type_node);
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
void semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                         SemanticContext *ctx,
                                                         const char *fallback_name);
void semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                           SemanticContext *ctx);
void semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                        SemanticContext *ctx);
void semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                          SemanticContext *ctx);
void semantic_type_resolution_precollect_world_inventory(ASTNode *world_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_zone_refresh_projection_map(
    ASTNode *zone_decl,
    ASTNode *refresh,
    SemanticContext *ctx,
    const char *consumer_label);
void semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                          SemanticContext *ctx);
bool type_check_intent_decl(ASTNode *node, SemanticContext *ctx);
bool type_check_event_decl(ASTNode *node, SemanticContext *ctx);
bool type_check_event_subscription(ASTNode *node, SemanticContext *ctx,
                                   const char *op_name);
bool type_check_event_invoke_stmt(ASTNode *node, SemanticContext *ctx);
bool semantic_check_body_flow(ASTNode *body,
                              SemanticContext *ctx,
                              bool *must_return_out);
void semantic_type_resolution_precollect_relation_inventory(ASTNode *relation_decl,
                                                            SemanticContext *ctx);
void semantic_type_resolution_precollect_effect_inventory(ASTNode *effect_decl,
                                                          SemanticContext *ctx);
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
int resolve_projection_source_field_path(ASTNode *program_root,
                                         ASTNode *source_decl,
                                         const char *field_name,
                                         SemanticContext *ctx,
                                         char **path_out,
                                         Type **field_type_out);
void semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                         SemanticContext *ctx);
void semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                        SemanticContext *ctx);
void semantic_stage_method_array(ASTNode **methods,
                                 size_t method_count,
                                 SemanticContext *ctx,
                                 const char *fallback_name);
ASTNode *semantic_find_top_level_decl_by_label(ASTNode *program,
                                               const char *label,
                                               TypeResolutionNodeKind kind);
ASTNode *semantic_find_graph_host_decl(ASTNode *program,
                                       const char *label);
void semantic_stage_top_level_decl(ASTNode *decl, SemanticContext *ctx);
void semantic_type_resolution_record_named_dependency(
    SemanticContext *ctx,
    const ASTNode *consumer_site,
    const char *consumer_name,
    TypeResolutionNodeKind provider_kind,
    const ASTNode *provider_site,
    const char *provider_name,
    const char *reason);
void semantic_type_resolution_precollect_program(ASTNode *program,
                                                 SemanticContext *ctx);
void semantic_run_type_resolution_worklist(ASTNode *program,
                                           SemanticContext *ctx,
                                           size_t *topo_order,
                                           size_t topo_count);
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
bool type_resolution_validate_graph(SemanticContext *ctx);
bool type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                      size_t **out_order,
                                      size_t *out_count);
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

/* Cross-TU helpers promoted from static to extern to support the
 * type_checker_decls_domain_helpers.c translation unit extraction
 * (5-A slice, docs/101_semantic_split_template.md). */
const char *type_name_or_unknown(const Type *type);
Type *resolve_named_type(const char *name, SemanticContext *ctx,
                         const ASTNode *site);
bool decl_is_projection_source(const ASTNode *decl);
const char *projection_refresh_source_field_name(ASTNode *refresh,
                                                 const char *target_field_name);
bool projection_target_decl_has_field(ASTNode *target_decl,
                                      const char *field_name);
Type *type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                      const char *display_name,
                                      SemanticContext *ctx);

/* Helper-axis promotions for the helpers_late.c TU (docs/101). */
ASTNode *overlay_field_decl_at(ASTNode *decl, size_t index,
                               const char **field_name_out);
void semantic_ctx_mark_embedded_world_zone_name(SemanticContext *ctx,
                                                const char *name,
                                                const char *world_name,
                                                const char *slot_name);
void semantic_format_function_signature(const Type *type, char *out,
                                        size_t out_cap);
ASTNode *find_callable_decl_by_name(ASTNode *program, const char *name);
const char *resource_handle_display_name(const Type *type);
const char *format_effective_generic_type_list_scratch(SemanticContext *ctx,
                                                       const char *name,
                                                       Type **types,
                                                       size_t count);

Type *type_check_stdlib_call(ASTNode *expr, const char *name,
                             SemanticContext *ctx);
bool check_call_arity(ASTNode *expr, size_t expected, const char *name,
                      SemanticContext *ctx);
ASTNode *find_named_class_decl(ASTNode *program, const char *name);
bool decl_is_subject_nominal(ASTNode *decl);
int resolve_projection_source_field_type_rec(ASTNode *program,
                                             ASTNode *source_decl,
                                             const char *field_name,
                                             unsigned depth,
                                             SemanticContext *ctx,
                                             Type **field_type_out);
Type *type_check_to_tobject(ASTNode *call, SemanticContext *ctx);
Type *type_check_to_object(ASTNode *call, SemanticContext *ctx);

#endif
