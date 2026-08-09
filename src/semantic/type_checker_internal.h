#ifndef PERGYRA_TYPE_CHECKER_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_INTERNAL_H

#include "type_checker.h"
#include "type_checker_domain_internal.h"
#include "type_checker_resolution_graph_internal.h"
#include "type_checker_resolution_internal.h"
#include "compiler/decl_field_model.h"

bool type_is_constructed_named(const Type *type, const char *name);
bool type_is_qubit(const Type *type);
bool type_is_slot_handle(const Type *type);
bool type_is_owned_slot_handle(const Type *type);
bool type_is_subject_host_slot_handle(const Type *type, SemanticContext *ctx);
bool type_is_read_view(const Type *type);
bool type_is_write_view(const Type *type);
bool type_is_move_token(const Type *type);
bool type_is_resource_handle(const Type *type);
bool type_is_anchored_resource_handle(const Type *type);
bool type_is_builtin_owner_handle(const Type *type);
bool type_is_movable_resource_handle(const Type *type);
bool semantic_require_no_live_text_builder(Scope *scope,
                                           ASTNode *site,
                                           SemanticContext *ctx,
                                           const char *boundary);
bool semantic_find_active_slot_view(Scope *scope,
                                    const char **view_name_out,
                                    const char **view_kind_out,
                                    const char **source_slot_out);
bool semantic_find_active_slot_view_for_source(Scope *scope,
                                               const char *source_slot,
                                               const char **view_name_out,
                                               const char **view_kind_out,
                                               bool *is_write_view_out);
bool semantic_reject_active_slot_owner_escape(ASTNode *site,
                                              SemanticContext *ctx,
                                              const char *escape_kind,
                                              const char *escape_name);
ASTNode *semantic_lookup_function_param_contract(SemanticContext *ctx,
                                                 const char *display_name,
                                                 size_t arg_index,
                                                 ParamMode *mode_out);
unsigned semantic_callable_param_escape_summary(
    ASTNode *callee_decl,
    size_t arg_index,
    SemanticContext *ctx);
/* Body-summary inventory readers. Each returns true when the callee's
 * recorded body_summary_mask positively proves the named fact bit is absent
 * from the callee body. False means inventory is missing or the bit is set,
 * so the caller must fall back to its existing analyzer path. Consumers
 * should use these helpers instead of re-walking the callee body. */
bool semantic_callable_summary_proves_no_drop_resource(SemanticContext *ctx,
                                                       ASTNode *callee_decl);
bool semantic_callable_summary_proves_no_spawn_task(SemanticContext *ctx,
                                                    ASTNode *callee_decl);
bool semantic_callable_summary_proves_no_send_channel(SemanticContext *ctx,
                                                      ASTNode *callee_decl);
bool semantic_callable_summary_proves_no_zone_requirement(SemanticContext *ctx,
                                                          ASTNode *callee_decl);
bool semantic_callable_summary_has_spawn_task(SemanticContext *ctx,
                                              ASTNode *callee_decl);
bool semantic_callable_summary_has_send_channel(SemanticContext *ctx,
                                                ASTNode *callee_decl);
bool semantic_callable_type_summary_has_spawn_task(const Type *function_type);
bool semantic_callable_type_summary_has_send_channel(const Type *function_type);
bool semantic_callable_type_requires_intent_authority(const Type *function_type);
bool semantic_param_summary_has_any_escape(unsigned summary_mask);
bool semantic_param_summary_has_return_escape(unsigned summary_mask);
bool semantic_param_summary_has_channel_escape(unsigned summary_mask);
bool semantic_param_summary_has_call_escape(unsigned summary_mask);
void semantic_validate_function_call_generic_where(ASTNode *expr,
                                                   SemanticContext *ctx,
                                                   const char *display_name,
                                                   size_t provided,
                                                   Type **call_arg_types);
bool semantic_reject_active_slot_view_boundary(ASTNode *site,
                                               SemanticContext *ctx,
                                               const char *boundary_name,
                                               const char *resume_detail,
                                               const char *fix_action);
bool type_is_general_boundary_type(const Type *type, SemanticContext *ctx);
bool type_is_capability_bearing(const Type *type);
bool type_is_worker_boundary_unsafe_storage(const Type *type);
const char *worker_boundary_storage_display_name(const Type *type);
bool type_is_detached_worker_boundary_unsafe_storage(const Type *type);
const char *detached_worker_boundary_storage_display_name(const Type *type);
bool type_is_subject_type(const Type *type, SemanticContext *ctx);
bool type_is_class_object_type(const Type *type, SemanticContext *ctx);
bool type_requires_boundary_borrow_tracking(const Type *type, SemanticContext *ctx);
const char *type_name_or_unknown(const Type *type);
bool name_looks_qualified(const char *name);
char *tc_strdup_fmt(const char *fmt, ...);
void semantic_format_function_signature(const Type *type,
                                        char *out,
                                        size_t out_cap);
void reject_if_embedded_world_zone_mutation(SemanticContext *ctx,
                                            ASTNode *site,
                                            ASTNode *target,
                                            const char *op_name);
void semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask);
void semantic_record_capability(SemanticContext *ctx, uint32_t capability_mask);
void semantic_record_body_summary(SemanticContext *ctx, uint32_t summary_mask);
void semantic_record_callee_body_summary(SemanticContext *ctx,
                                         const Type *callee_type);
void semantic_record_callable_decl_summary(SemanticContext *ctx,
                                           ASTNode *callable_decl,
                                           const Type *callable_type,
                                           uint32_t declared_effects);
bool semantic_format_secure_token_name(char *out,
                                       size_t out_size,
                                       const char *slot_name,
                                       ASTNode *site,
                                       SemanticContext *ctx);
void effect_mask_to_string(uint32_t mask, char *buf, size_t buf_size);
Type *create_overlay_nominal_type(const char *name);
size_t overlay_field_count(ASTNode *decl);
ASTNode *overlay_field_decl_at(ASTNode *decl,
                               size_t index,
                               const char **field_name_out);
bool decl_is_subject_host(const ASTNode *decl);
/* F2 (docs/144) Phase 3: these accessors now read the pre-semantic field-shape
   model and return PgyDeclField by value. "Not found" is signalled by a zeroed
   result (name == NULL). The returned name/type_ast point into the AST and stay
   valid for the whole compile. */
PgyDeclField subject_host_field_at(ASTNode *decl, size_t index);
size_t projection_source_field_count(ASTNode *decl);
PgyDeclField projection_source_field_at(ASTNode *decl, size_t index);
Type *projection_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
Type *expr_ops_projection_member(ASTNode *expr, ASTNode *member_object,
                                 const char *member_name, Type *object_type,
                                 SemanticContext *ctx);
char *format_generic_subject_signature(const char *name,
                                       GenericParams *params);
const char *format_generic_subject_signature_scratch(SemanticContext *ctx,
                                                     const char *name,
                                                     GenericParams *params);
TypeNominalFlavor nominal_flavor_from_decl(const ASTNode *decl);
uint32_t declared_effects_from_function_node(ASTNode *node,
                                             SemanticContext *ctx,
                                             bool *has_contract_out);
Type *type_check_func_resolve_param_type(FuncParam *param,
                                         SemanticContext *ctx);
Type *type_check_func_resolve_return_type(ASTNode *func_decl,
                                          SemanticContext *ctx);
Type *type_check_signature_resolve_type_ref(ASTNode *type_ref,
                                            SemanticContext *ctx);
const char *type_check_func_current_implicit_self_host_name(
    SemanticContext *ctx);
bool type_check_func_symbol_is_self_host(Symbol *sym);
void type_check_func_validate_identifier_hygiene(ASTNode *node,
                                                 SemanticContext *ctx,
                                                 const char *role,
                                                 const char *name);
void type_check_func_validate_param_boundary(ASTNode *node,
                                             SemanticContext *ctx,
                                             const char *func_name,
                                             FuncParam *param,
                                             Type *param_type);
void type_check_func_validate_return_boundary(ASTNode *node,
                                              SemanticContext *ctx,
                                              Type *return_type);
char *flatten_static_member_access(const ASTNode *expr, char separator);
Type *expr_current_host_field_type(SemanticContext *ctx,
                                   const char *field_name);
ASTNode *expr_current_host_method_decl(SemanticContext *ctx,
                                       const char *method_name);
Type *expr_host_method_function_type(SemanticContext *ctx,
                                     ASTNode *host_decl,
                                     const char *method_name);
typedef struct ExprHostMethodGenericBindings {
    GenericParams *params;
    Type **types;
    size_t count;
    bool valid;
} ExprHostMethodGenericBindings;
bool expr_host_method_generic_bindings_init(
    ExprHostMethodGenericBindings *bindings,
    ASTNode *call,
    ASTNode *method,
    SemanticContext *ctx,
    const char *method_name);
void expr_host_method_generic_infer_argument(
    ExprHostMethodGenericBindings *bindings,
    Type *formal,
    Type *actual,
    ASTNode *host_decl,
    const Type *receiver_type);
bool expr_host_method_generic_bindings_finalize(
    ExprHostMethodGenericBindings *bindings,
    ASTNode *call,
    ASTNode *method,
    ASTNode *host_decl,
    const Type *receiver_type,
    SemanticContext *ctx,
    const char *method_name);
Type *expr_host_method_generic_substitute(
    Type *type,
    ASTNode *host_decl,
    const Type *receiver_type,
    const ExprHostMethodGenericBindings *bindings);
void expr_host_method_generic_validate_constraints(
    const ExprHostMethodGenericBindings *bindings,
    ASTNode *call,
    ASTNode *method,
    SemanticContext *ctx,
    const char *method_name);
void expr_host_method_generic_bindings_destroy(
    ExprHostMethodGenericBindings *bindings);
Type *expr_type_check_host_method_call_on_host(ASTNode *expr,
                                               ASTNode *host_decl,
                                               ASTNode *method,
                                               const Type *receiver_type,
                                               SemanticContext *ctx);
Type *expr_type_check_host_method_call(ASTNode *expr,
                                       ASTNode *method,
                                       SemanticContext *ctx);
Type *expr_type_for_enum_variant_projection(SemanticContext *ctx,
                                            ASTNode *site,
                                            const Type *enum_type,
                                            const char *variant_name);
Type *expr_type_for_enum_payload_field(SemanticContext *ctx,
                                       ASTNode *site,
                                       const Type *payload_type,
                                       const char *field_name);
Type *semantic_host_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
ASTNode *semantic_host_decl_for_type(SemanticContext *ctx, const Type *type);
ASTNode **semantic_host_decl_methods(ASTNode *decl, size_t *method_count);
bool expr_type_is_nominal_host_type(const Type *type,
                                    SemanticContext *ctx);
bool expr_member_is_static_access(const ASTNode *expr);
Symbol *lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx);
bool semantic_reject_lambda_unsupported_captures(ASTNode *lambda,
                                                 SemanticContext *ctx,
                                                 bool allow_copy_capture);
Type *type_check_lambda_expression(ASTNode *expr, SemanticContext *ctx);
void mark_world_embedded_zone_arguments(ASTNode *call, SemanticContext *ctx);
void semantic_reject_zone_subject_embedding(ASTNode *call, ASTNode *zone_decl,
                                            const char *zone_name,
                                            SemanticContext *ctx);
void semantic_reject_world_zone_member_escape(ASTNode *node,
                                              SemanticContext *ctx);
bool expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx);
bool expr_is_qubit_claim(const ASTNode *expr);
bool expr_is_device_slot_claim(const ASTNode *expr);
bool expr_is_movable_resource_transfer_source(const ASTNode *expr);
bool slot_transfer_compatible(const Type *from, const Type *to);
void validate_class_where_clause_specialization_ast(ASTNode *class_decl,
                                                    ASTNode *specialized_type,
                                                    ASTNode *site,
                                                    SemanticContext *ctx);

bool consume_qubit_value(ASTNode *expr, SemanticContext *ctx,
                         const char *action);
bool consume_array_storage_binding(ASTNode *expr, SemanticContext *ctx);
bool type_check_defer_body_flow(ASTNode *body, SemanticContext *ctx);
bool type_check_parallel_block_flow(ASTNode *node, SemanticContext *ctx);
/* docs/181 SS1 rungs 0-2 (type_checker_flow_parallel_join.c) */
bool type_check_parallel_join_admit(ASTNode *node, SemanticContext *ctx,
                                    Type **elem_type_out);
Type *type_check_parallel_join_expr_type(ASTNode *node,
                                         SemanticContext *ctx);
bool type_check_match_case_patterns(ASTNode *mc,
                                    Type *subj_type,
                                    SemanticContext *ctx);
void check_match_redundancy(ASTNode *node,
                            Type *subj_type,
                            SemanticContext *ctx);
void check_match_exhaustiveness(ASTNode *node,
                                Type *subj_type,
                                SemanticContext *ctx);
bool match_stmt_has_total_case_coverage(ASTNode *node,
                                        Type *subj_type,
                                        SemanticContext *ctx);
Type *type_check_qubit_use(ASTNode *expr, SemanticContext *ctx);
bool identifier_is_borrowed_boundary_param(ASTNode *expr, SemanticContext *ctx);
/* Currently-resolved nominal host (class/zone/world/relation/effect)
 * declaration for `ctx`.  Visibility/access helpers use this through an
 * explicit owner seam rather than include-order coupling. */
ASTNode *current_host_decl(SemanticContext *ctx);
bool semantic_build_host_decl_index(SemanticContext *ctx, ASTNode *program);
ASTNode *semantic_host_index_find_decl_by_name(SemanticContext *ctx,
                                               ASTNodeType decl_type,
                                               const char *name);
ASTNode *semantic_host_index_find_top_level_decl_by_label(
    SemanticContext *ctx,
    const char *label,
    TypeResolutionNodeKind kind);
ASTNode *semantic_host_index_find_next_decl_of_type(SemanticContext *ctx,
                                                    ASTNodeType decl_type,
                                                    const ASTNode *after);
ASTNode *semantic_find_type_alias_decl_by_name(SemanticContext *ctx,
                                               const char *name);
ASTNode *semantic_find_zone_decl_by_name(SemanticContext *ctx,
                                         const char *name);
ASTNode *semantic_find_relation_decl_by_name(SemanticContext *ctx,
                                             const char *name);
ASTNode *semantic_find_effect_decl_by_name(SemanticContext *ctx,
                                           const char *name);
ASTNode *semantic_find_world_decl_by_name(SemanticContext *ctx,
                                          const char *name);
ASTNode *semantic_find_party_decl_by_name(SemanticContext *ctx,
                                          const char *name);
ASTNode *semantic_find_roster_decl_by_name(SemanticContext *ctx,
                                           const char *name);
ASTNode *semantic_find_class_decl_by_name(SemanticContext *ctx,
                                          const char *name);
ASTNode *semantic_find_intent_decl_by_name(SemanticContext *ctx,
                                           const char *name);
ASTNode *semantic_find_ability_decl_by_name(SemanticContext *ctx,
                                            const char *name);
ASTNode *semantic_find_enum_decl_by_name(SemanticContext *ctx,
                                         const char *name);
ASTNode *semantic_find_function_decl_by_name(SemanticContext *ctx,
                                             const char *name);
ASTNode *semantic_find_callable_decl_by_name(SemanticContext *ctx,
                                             const char *name);
uint32_t semantic_current_routine_syntax_id(SemanticContext *ctx);
ASTNode *semantic_constructor_decl_for_symbol_kind(SemanticContext *ctx,
                                                  SymbolKind kind,
                                                  const char *name);
ASTNode *find_zone_domain_slot(ASTNode *zone, const char *slot_name);
ASTNode *find_zone_authority(ASTNode *zone, const char *slot_name);
ASTNode *resolve_zone_subject_slot_for_participant(ASTNode *zone,
                                                   SemanticContext *ctx,
                                                   const char *participant_alias,
                                                   const char *participant_type_name,
                                                   bool *ambiguous_out);
ASTNode *semantic_stage_domain_find_zone_decl(SemanticContext *ctx,
                                             const char *zone_name);
ASTNode *semantic_world_find_zone_slot_local(ASTNode *world,
                                             const char *slot_name);
ASTNode *semantic_world_find_state_local(ASTNode *world,
                                         const char *state_name);
ASTNode *semantic_zone_find_layer_slot_local(ASTNode *zone,
                                             const char *slot_name);
ASTNode *semantic_zone_find_state_local(ASTNode *zone,
                                        const char *state_name);
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
void semantic_stage_record_alias_diagnostic_unresolved(ASTNode *alias_decl,
                                                       SemanticContext *ctx);
const char *semantic_symbol_kind_label(SymbolKind kind);
typedef enum IntentStepWhereProvenance {
    INTENT_STEP_WHERE_PROVENANCE_INHERITED_ACTION,
    INTENT_STEP_WHERE_PROVENANCE_DERIVED_USING,
    INTENT_STEP_WHERE_PROVENANCE_DERIVED_TRANSFER
} IntentStepWhereProvenance;

const char *intent_step_single_who_alias(const ASTNode *step);
bool intent_step_set_where_type_name(ASTNode *step,
                                     const char *zone_name,
                                     IntentStepWhereProvenance provenance);
ASTNode *find_intent_involves_local(ASTNode *intent, const char *alias);
ASTNode *find_intent_value_local(ASTNode *intent, const char *alias);
ASTNode *intent_step_resolve_transfer_target_involves(
    ASTNode *intent_decl,
    ASTNode *step,
    const char **resolved_alias_out);
bool type_check_intent_update_existing_signature(ASTNode *intent,
                                                 Symbol *existing,
                                                 SemanticContext *ctx);
void type_check_intent_resolve_binding_types(ASTNode *intent,
                                             SemanticContext *ctx);
void type_check_intent_declare_binding_symbols(ASTNode *intent,
                                               SemanticContext *ctx);
Type *intent_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
bool intent_typed_validate_topology(ASTNode *intent, SemanticContext *ctx);
bool intent_typed_resolve_step_branches(ASTNode *step,
                                        Type *outcome_type,
                                        SemanticContext *ctx,
                                        Type **success_payload_out,
                                        Type **failure_payload_out);
bool intent_typed_resolve_terminal_result(ASTNode *intent,
                                          bool success_terminal,
                                          size_t failure_index,
                                          ASTNode *source_step,
                                          Type *return_type,
                                          Type *source_payload_type,
                                          SemanticContext *ctx);
bool intent_typed_declare_payload_binding(ASTNode *step,
                                          bool success_branch,
                                          Type *payload_type,
                                          SemanticContext *ctx);
ASTNode *intent_typed_step_for_failure_terminal(ASTNode *intent, size_t index);
Type *intent_normalize_type(Type *type);
Type *intent_resolve_involves_type(ASTNode *involves, SemanticContext *ctx);
Type *intent_resolve_value_type(ASTNode *value, SemanticContext *ctx);
Type *intent_resolve_step_where_type(ASTNode *step, SemanticContext *ctx);
ASTNode *intent_find_zone_decl_for_type(Type *type, SemanticContext *ctx);
ASTNode *intent_resolve_step_where_zone_decl(ASTNode *step,
                                             SemanticContext *ctx,
                                             Type **type_out);
ASTNode *intent_find_effect_decl_by_name(const char *effect_name,
                                         SemanticContext *ctx);
const char *intent_step_where_source_label(const ASTNode *step);
const char *intent_involves_type_name(ASTNode *involves);
bool domain_has_subject_slot_type(ASTNode **slots,
                                  size_t slot_count,
                                  SemanticContext *ctx,
                                  const char *type_name);
const char *find_action_binding_type_name(ASTNode *func,
                                          ASTNode *enclosing_nominal,
                                          SemanticContext *ctx,
                                          const char *binding_name);
bool zone_has_authority_for_subject_type(ASTNode *zone,
                                         SemanticContext *ctx,
                                         const char *type_name);
Type *domain_lookup_slot_type_metadata(ASTNode *slot, SemanticContext *ctx);
Type *domain_lookup_shared_type_metadata(ASTNode *shared, SemanticContext *ctx);
Type *domain_lookup_named_type_metadata(ASTNode *type_ref, SemanticContext *ctx);
ASTNode *find_domain_slot_local(ASTNode **slots,
                                size_t slot_count,
                                const char *slot_name);
bool zone_has_effect_layer_type(ASTNode *zone, const char *effect_name);
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
Type **collect_effective_generic_arg_types(GenericParams *decl_params,
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
Type *ability_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
bool type_check_intent_decl(ASTNode *node, SemanticContext *ctx);
void type_check_intent_step_transfer_contract(ASTNode *node,
                                              ASTNode *step,
                                              SemanticContext *ctx);
bool type_check_event_decl(ASTNode *node, SemanticContext *ctx);
bool type_check_event_subscription(ASTNode *node, SemanticContext *ctx,
                                   const char *op_name);
bool type_check_event_invoke_stmt(ASTNode *node, SemanticContext *ctx);
bool type_check_bind_stmt(ASTNode *node, SemanticContext *ctx);
typedef struct SemanticBodyFlowSummary {
    bool has_fallthrough;
    bool has_return;
    bool has_break;
    bool has_continue;
    bool has_defer;
    bool must_return;
} SemanticBodyFlowSummary;
bool semantic_check_body_flow_summary(ASTNode *body,
                                      SemanticContext *ctx,
                                      SemanticBodyFlowSummary *summary_out);
bool semantic_check_body_flow(ASTNode *body,
                              SemanticContext *ctx,
                              bool *must_return_out);
/* Resource-handle compile-time state tracking.
 * Today the richer semantic state machine is QubitSlot-specific. */
QubitSemanticState get_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx);
bool set_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx,
                              QubitSemanticState new_state);
const char *qubit_state_name(QubitSemanticState state);
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
bool decl_is_projection_source(const ASTNode *decl);
const char *projection_refresh_source_field_name(ASTNode *refresh,
                                                 const char *target_field_name);
bool projection_target_decl_has_field(ASTNode *target_decl,
                                      const char *field_name);
Type *type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                      const char *display_name,
                                      SemanticContext *ctx);
bool type_check_constructor_symbol_call(ASTNode *expr,
                                        Symbol *sym,
                                        const char *display_name,
                                        SemanticContext *ctx,
                                        Type **type_out);

Type *type_check_stdlib_call(ASTNode *expr, const char *name,
                             SemanticContext *ctx);
bool check_call_arity(ASTNode *expr, size_t expected, const char *name,
                      SemanticContext *ctx);
bool decl_is_subject_nominal(ASTNode *decl);
int semantic_resolve_projection_source_field_type(SemanticContext *ctx,
                                                  ASTNode *source_decl,
                                                  const char *field_name,
                                                  Type **field_type_out);
Type *type_check_to_tobject(ASTNode *call, SemanticContext *ctx);
Type *type_check_to_object(ASTNode *call, SemanticContext *ctx);

#endif
