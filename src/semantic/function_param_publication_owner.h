#ifndef PGY_FUNCTION_PARAM_PUBLICATION_OWNER_H
#define PGY_FUNCTION_PARAM_PUBLICATION_OWNER_H
#include "type_checker.h"
/* Private snapshots owned by publication.c. Escape diagnostics borrow only
 * its checked admitted view; other consumers use the checked queries. */
typedef struct IndexedStringPermanentElementRecipe IndexedStringPermanentElementRecipe;
typedef struct {
    uint32_t id;
    ParamMode mode;
    const ASTNode *type_node;
    uint32_t type_node_id;
    TypeKind kind;
    bool anchored;
} PublishedFormal;
typedef struct {
    ASTNode *decl;
    Type *function_type;
    Type **types;
    size_t count;
    uint32_t id;
    char *module;
    PublishedFormal *formals;
    const ASTNode *body;
    uint32_t body_id;
    bool normal_exit_closed;
    /* Optional eligibility snapshot: inferred/synthetic headers may change
     * legitimately before finalization; only the scalar query rejects drift. */
    Type *return_type;
    const ASTNode *return_node;
    uint32_t return_node_id;
    bool async;
} PendingRoutine;
typedef struct {
    ASTNode *call, *site, *callee, *caller;
    size_t ordinal;
    const Type *type;
    char *borrowed, *destination, *fix;
    uint32_t call_id, site_id, callee_id, caller_id;
    char *module;
    const ASTNode *call_target;
    uint32_t call_target_id;
    size_t argument_count;
    size_t formal_ordinal;
    uint32_t formal_id, site_binding_id, target_binding_id;
    ASTNodeType site_shape, target_shape;
    char *target_name;
    const ASTNode *call_receiver;
    uint32_t call_receiver_id;
    const Type *call_receiver_type;
} PendingRefCall;
typedef struct {
    const ASTNode *call, *caller;
    uint32_t call_id, caller_id;
    ASTNodeType caller_shape;
    const Type *result_type;
    TypeKind result_kind;
    char *module;
} PendingReadPayloadLoan;
typedef struct {
    uint32_t slot_binding_id, token_binding_id;
    const Type *slot_type, *token_type;
} SemanticPublishedSlotBirth;
typedef struct {
    const ASTNode *qualifier, *enum_decl;
    uint32_t qualifier_id, qualifier_binding_id, member_id, enum_decl_id;
    const Type *enum_type;
    size_t variant_ordinal, variant_count, payload_count;
    char *qualifier_name, *enum_name, *member_name;
} SemanticPublishedUnitEnumSelection;
bool semantic_param_publication_read_payload_loan_current(const SemanticContext *ctx,
    const PendingReadPayloadLoan *loan, uint32_t *owned_receiver_binding);
bool semantic_param_publication_record_owned_slot_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *slot_type, uint32_t slot_binding_id,
    const Type *token_type, uint32_t token_binding_id, const char *token_name);
bool semantic_param_publication_owned_slot_binding(const SemanticContext *ctx,
    const ASTNode *declaration, SemanticPublishedSlotBirth *birth);
bool semantic_param_publication_slot_token_operand(const SemanticContext *ctx, const ASTNode *operand);
bool semantic_param_publication_record_slot_token_operand(SemanticContext *ctx, ASTNode *call,
    size_t ordinal, const Symbol *slot, const Symbol *token, const Type *slot_type);
bool semantic_param_publication_ref_call_current(
    const SemanticContext *ctx, const PendingRefCall *row);
typedef struct {
    ASTNode *call, *caller;
    uint32_t call_id, caller_id, callee_id;
    size_t caller_index, callee_index;
} PendingBodyCall;
/* Constructor owner produces/current-checks the distinct typed field mapping.
 * Publication owns its transferred lifetime, not a second call identity. */
typedef struct {
    size_t field_index;
    ClassField *field;
    uint32_t field_id;
    ASTNode *type_node;
    uint32_t type_id;
    char *field_name, *argument_name, *field_type_name, *argument_type_name, *type_syntax_name;
} SemanticNominalResultField;
typedef struct {
    ASTNode *decl;
    uint32_t decl_id;
    TypeNominalFlavor flavor;
    bool is_struct;
    char *decl_name, *result_type_name;
    size_t field_count;
    SemanticNominalResultField *fields;
} SemanticNominalResultSnapshot;
bool semantic_nominal_result_snapshot_current(const SemanticContext *ctx,
    const ASTNode *call, const SemanticNominalResultSnapshot *snapshot);
bool semantic_nominal_result_snapshot_syntax_current(const ASTNode *call,
    const SemanticNominalResultSnapshot *snapshot);
void semantic_nominal_result_snapshot_destroy(SemanticNominalResultSnapshot *snapshot);
typedef struct {
    const ASTNode *node;
    uint32_t id;
    ASTNodeType shape;
    uint32_t binding;
    const Type *type;
    size_t formal_ordinal;
    uint32_t formal_id;
} PublishedCallArgument;
typedef struct {
    const ASTNode *declaration, *initializer, *caller;
    uint32_t declaration_id, initializer_id, caller_id, binding_id, source_id;
    ASTNodeType initializer_shape;
    bool present, available, dependent;
} IndexedStringLetLineage;
typedef struct FunctionParamFlowIndexClaims FunctionParamFlowIndexClaims;
typedef struct PendingPrivateStringPush PendingPrivateStringPush;
typedef struct IndexedStringFieldRecipe IndexedStringFieldRecipe;
typedef struct IndexedStringNonownerRisk IndexedStringNonownerRisk;
typedef struct IndexedStringDefinitionRecipe IndexedStringDefinitionRecipe;
typedef struct {
    const ASTNode *body;
    uint32_t body_id;
    FunctionParamFlowIndexClaims *syntax;
    PublishedCallArgument *sources;
    size_t source_count, sealed_source_count;
} PublishedOwnedStringResult;
typedef struct {
    const ASTNode *source;
    uint32_t source_id, binding_id, category;
    const Type *type;
    LambdaCaptureMode mode;
    bool copy_escape;
    char *name, *type_name;
} PublishedDeferredCapture;
typedef struct {
    const PublishedDeferredCapture *captures;
    size_t capture_count;
} SemanticPublishedDeferred;
typedef struct {
    const ASTNode *receiver, *index, *declaration, *field_type_node, *root;
    const ASTNode *member, *caller;
    uint32_t receiver_id, index_id, declaration_id, field_id, field_type_id;
    uint32_t root_id, root_binding_id, member_id, caller_id;
    size_t field_ordinal, field_count;
    const Type *receiver_type, *index_type, *field_type, *result_type;
    TypeNominalFlavor declaration_flavor;
    AccessModifier access;
    bool declaration_struct, field_is_string;
    bool explicit_access, mutable_field, vessel_field, default_field;
    double index_number;
    int64_t index_long;
    unsigned index_flags;
    ASTNodeType index_shape;
    char *name, *index_name, *field_type_name, *declaration_name, *root_name;
    bool present;
} PublishedProjection;
/* Borrowed typed identity only; it does not prove a field's backing lifetime. */
typedef struct {
    const ASTNode *member, *declaration, *field_type_node;
    uint32_t member_id, declaration_id, field_id, field_type_id;
    size_t field_ordinal, field_count;
    const Type *nominal_type, *field_type;
} SemanticPublishedSelectedField;
typedef struct {
    const ASTNode *node, *target, *caller;
    uint32_t id, target_id, caller_id, builtin, declaration, value_binding, binding_id;
    bool value_present, copy_escape, meta, execution_present, reachable;
    bool call_present, kind_present, stdlib, declared_callable, local, cast_present;
    bool assignment_replacement;
    bool channel_binding;
    bool owned_slot_binding;
    PublishedOwnedStringResult *owned_string_result;
    bool result_allocator_binding, allocator_invalidated;
    const ASTNode *allocator_initializer, *allocator_type_node;
    uint32_t allocator_initializer_id, allocator_type_node_id;
    const Type *allocator_type;
    bool slot_token_operand;
    size_t slot_token_ordinal;
    SemanticPublishedSlotBirth slot_birth;
    const Type *slot_birth_inner_type;
    bool slot_birth_secure;
    const ASTNode *slot_birth_annotation, *owned_slot_declaration;
    uint32_t slot_birth_annotation_id, owned_slot_declaration_id;
    const Type *slot_birth_annotation_type;
    size_t read_payload_loan_index;
    SemanticPublishedUnitEnumSelection *unit_enum_selection;
    bool empty_map_binding, slot_write;
    const ASTNode *map_type_node;
    uint32_t map_type_id;
    const Type *map_key_type, *map_value_type;
    const Type *write_receiver_type, *write_payload_type;
    const Type *write_inner_type;
    SlotAccessMode write_access_mode;
    bool write_secure;
    const ASTNode *empty_map_declaration;
    uint32_t empty_map_declaration_id;
    const ASTNode *channel_type_node;
    uint32_t channel_type_id;
    const Type *channel_element_type;
    bool deferred_scan, deferred_checked;
    ASTNodeType deferred_shape;
    PublishedDeferredCapture *deferred_captures;
    size_t deferred_capture_count;
    /* Owned syntax receipt for deferred nodes or checked typed storage. */
    FunctionParamFlowIndexClaims *deferred_syntax;
    /* A call-time storage premise, never a standalone non-retention grant. */
    bool private_array_push, private_physical_mutation;
    bool private_exclusive, private_escaped, private_pending;
    const ASTNode *private_binding, *private_initializer;
    const ASTNode *private_binding_type_node;
    uint32_t private_binding_id, private_initializer_id;
    uint32_t private_binding_type_id;
    const Type *string_array_type;
    const ASTNode *assignment_rhs;
    uint32_t assignment_rhs_id, assignment_rhs_binding;
    ASTNodeType assignment_rhs_shape;
    uint32_t category;
    ASTNodeType value_shape;
    bool origin_ready, string_value, physical_scalar, builtin_string_concat;
    /* Indexed-String owner issues this source at the typed value's program
     * point; zero is independent, UINT32_MAX is explicitly unproved. */
    bool indexed_string_source_present;
    uint32_t indexed_string_source_id;
    IndexedStringLetLineage indexed_string_let;
    bool indexed_string_shallow_retention;
    PendingPrivateStringPush *private_string_push;
    IndexedStringFieldRecipe *indexed_string_field;
    IndexedStringNonownerRisk *indexed_string_nonowner;
    IndexedStringDefinitionRecipe *indexed_string_definition;
    IndexedStringPermanentElementRecipe *indexed_string_permanent;
    /* Borrowed exact-use expectation; only the declaration row owns payload. */
    const IndexedStringDefinitionRecipe *indexed_string_definition_use;
    bool indexed_array_source_present;
    uint32_t indexed_array_source_id;
    const Type *value_type;
    PublishedProjection projection;
    const ASTNode *unmodelled_constructor, *call_receiver;
    uint32_t unmodelled_constructor_id, call_receiver_id;
    const Type *unmodelled_constructor_type;
    const ASTNode *selected_callable;
    uint32_t selected_callable_id;
    /* Binary concat uses these same owned rows as typed operand snapshots. */
    PublishedCallArgument *arguments;
    ASTNodeType target_shape;
    uint32_t target_binding;
    size_t argument_count;
    size_t routine_index;
    char *target_name;
    char *value_type_name, *relation_argument_type, *relation_result_type;
    const ASTNode *relation_argument;
    const Type *relation_input_type, *relation_output_type;
    uint32_t relation_argument_id;
    size_t relation_argument_count;
    bool result_relation;
    SemanticNominalResultSnapshot *nominal_result;
} PublishedNode;
struct FunctionParamPublicationStore {
    bool pending, inputs_ready, finalized;
    ASTNode *program;
    uint32_t program_id;
    size_t statement_count;
    PendingRoutine *routines; size_t routine_count, routine_capacity;
    PendingRefCall *calls; size_t call_count, call_capacity;
    PendingBodyCall *body_calls; size_t body_call_count, body_call_capacity;
    PendingReadPayloadLoan *read_loans; size_t read_loan_count, read_loan_capacity;
    PublishedNode *nodes; size_t node_count, node_capacity;
    size_t indexed_string_shallow_retention_count;
    size_t private_string_push_count;
    size_t indexed_string_field_count;
    size_t indexed_string_nonowner_count;
    size_t indexed_string_definition_count;
    size_t indexed_string_permanent_count;
};
typedef struct {
    const ASTNode *program;
    uint32_t program_id;
    const PendingRoutine *routines;
    size_t routine_count;
    const PendingRefCall *calls;
    size_t call_count;
    const PendingBodyCall *body_calls;
    size_t body_call_count;
    const PendingReadPayloadLoan *read_loans;
    size_t read_loan_count;
} SemanticParamPublicationView;
bool function_param_flow_return_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingRefCall *row, ASTNode **escape_site);
bool function_param_flow_read_payload_no_escape(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingReadPayloadLoan *row, ASTNode **escape_site);
bool semantic_param_publication_view_current(const SemanticContext *ctx, const SemanticParamPublicationView *view);
bool semantic_param_publication_admitted_view(const SemanticContext *ctx, SemanticParamPublicationView *view);
bool semantic_consume_param_publication(SemanticContext *ctx, const SemanticParamPublicationView *view);
bool semantic_param_publication_record_checked_result_relation(SemanticContext *ctx, ASTNode *call,
    const Type *argument_type, const Type *result_type, uint32_t selected_decl, size_t payload_ordinal);
bool semantic_param_publication_record_checked_unit_enum_selection(
    SemanticContext *ctx, ASTNode *member, ASTNode *enum_decl,
    const Type *enum_type, size_t variant_ordinal);
bool semantic_param_publication_checked_unit_enum_selection(
    const SemanticContext *ctx, const ASTNode *member);
/* Ownership transfers only on success; failure leaves the producer responsible. */
bool semantic_param_publication_record_nominal_result(SemanticContext *ctx,
    ASTNode *call, SemanticNominalResultSnapshot *snapshot);
void semantic_param_publication_retire_value(PublishedNode *row);
bool semantic_param_publication_reserve_storage(void **rows, size_t *capacity, size_t count, size_t size);
bool semantic_param_publication_checked_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains);
bool semantic_param_publication_checked_backing_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains);
bool semantic_param_publication_record_empty_map_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *map_type);
bool semantic_param_publication_empty_map_binding(const SemanticContext *ctx, const ASTNode *declaration);
bool semantic_param_publication_record_slot_write(SemanticContext *ctx, ASTNode *call,
    const Type *receiver_type, const Type *payload_type);
bool semantic_param_publication_checked_slot_write(const SemanticContext *ctx, const ASTNode *call);
/* Compilation-owned obligations: pending is never a published summary. */
bool semantic_param_publication_begin(SemanticContext *ctx);
bool semantic_param_publication_pending(const SemanticContext *ctx);
bool semantic_param_publication_inputs_ready(const SemanticContext *ctx);
bool semantic_param_publication_ready(const SemanticContext *ctx);
uint32_t semantic_param_publication_binding(const SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_argument_borrowed_for_call(
    const SemanticContext *ctx, const ASTNode *call, size_t ordinal);
bool semantic_param_publication_string_argument_read_only(
    const SemanticContext *ctx, const ASTNode *call, size_t ordinal);
bool semantic_param_publication_record_string_concat(SemanticContext *ctx, ASTNode *value);
bool semantic_param_publication_call_analysis_unmodelled(
    const SemanticContext *ctx, const ASTNode *call);
/* Complete current field/input mapping, including the empty field set.
 * This does not certify fresh allocation or absence of evaluation effects. */
bool semantic_param_publication_nominal_result_complete(const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_record_channel_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *channel_type);
bool semantic_param_publication_channel_binding(const SemanticContext *ctx, const ASTNode *declaration);
bool semantic_param_publication_return_backing_required(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal, bool *required);
bool semantic_param_publication_routine_return_dependency_is_scalar(
    const SemanticContext *ctx, const ASTNode *decl);
bool semantic_param_publication_routine_no_output_header_ready(
    const SemanticContext *ctx, const ASTNode *decl);
bool semantic_param_publication_routine_relation_header_ready(
    const SemanticContext *ctx, const ASTNode *decl);
bool semantic_param_publication_value_is_physical_scalar(
    const SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_value_has_static_string_backing(
    const SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_parameter_is_anchored_handle(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal, bool *anchored);
bool semantic_param_publication_routine_inputs_current(
    const SemanticContext *ctx, const ASTNode *decl, bool require_body);
bool semantic_param_publication_record_routine(SemanticContext *ctx, ASTNode *decl,
    size_t count, Type **types, Type *function_type, bool normal_exit_closed);
bool semantic_param_publication_record_call(SemanticContext *ctx, ASTNode *call, ASTNode *site,
    ASTNode *callee, size_t ordinal, const Type *type, const char *borrowed,
    const char *destination, const char *fix);
bool semantic_param_publication_record_body_call(SemanticContext *ctx, ASTNode *call);
/* Native ownership category is not a fresh-payload/copy/cleanup certificate. */
typedef struct {
    bool ok, copy_escape, meta;
    uint32_t category;
    bool origin_ready;
    const Type *type;
} SemanticPublishedValue;
bool semantic_param_publication_begin_value(SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_record_value(SemanticContext *ctx, ASTNode *value,
    const Type *type, bool meta);
void semantic_param_publication_projection_record_identifier(SemanticContext *ctx,
    ASTNode *value);
void semantic_param_publication_projection_record_array(SemanticContext *ctx,
    ASTNode *value, ASTNode *receiver, ASTNode *index, const Type *receiver_type,
    const Type *index_type, const Type *result_type);
void semantic_param_publication_projection_destroy(PublishedNode *row);
bool semantic_param_publication_projection_root_current(const SemanticContext *ctx,
    const ASTNode *value, ASTNode **caller_out, ASTNode **root_out);
bool semantic_param_publication_member_projection_syntax_current(const ASTNode *value,
    const ASTNode *caller, const PublishedProjection *projection);
bool semantic_param_publication_selected_member_field_current(const SemanticContext *ctx,
    const ASTNode *value, ASTNode **caller_out, ASTNode **root_out,
    SemanticPublishedSelectedField *field_out);
bool semantic_param_publication_constructor_field_actual_current(const SemanticContext *ctx,
    const ASTNode *call, const SemanticPublishedSelectedField *field,
    ASTNode **actual_out, size_t *actual_ordinal_out);
bool semantic_param_publication_record_execution(SemanticContext *ctx,
    const ASTNode *statement, bool reachable);
bool semantic_param_publication_record_checked_call(SemanticContext *ctx, ASTNode *call);
bool semantic_param_publication_record_selected_callable(SemanticContext *ctx,
    ASTNode *call, ASTNode *decl);
bool semantic_param_publication_record_deferred_capture(SemanticContext *ctx, ASTNode *lambda,
    ASTNode *source, uint32_t binding_id, const Type *type, LambdaCaptureMode mode);
bool semantic_param_publication_record_deferred_scan(SemanticContext *ctx, ASTNode *boundary);
bool semantic_param_publication_record_checked_deferred(SemanticContext *ctx, ASTNode *boundary);
bool semantic_param_publication_query_deferred(const SemanticContext *ctx,
    const ASTNode *boundary, SemanticPublishedDeferred *view);
void semantic_param_publication_retire_deferred(PublishedNode *row);
bool semantic_param_publication_record_checked_numeric_cast(SemanticContext *ctx, ASTNode *cast);
bool semantic_param_publication_checked_numeric_cast(const SemanticContext *ctx, const ASTNode *cast);
SemanticPublishedValue semantic_param_publication_value(const SemanticContext *ctx,
    const ASTNode *value);
bool semantic_param_publication_execution(const SemanticContext *ctx,
    const ASTNode *statement, bool *reachable);
bool semantic_param_publication_local_call(const SemanticContext *ctx,
    const ASTNode *call, size_t ordinal);
bool semantic_param_publication_checked_argument(const SemanticContext *ctx,
    const ASTNode *call, size_t ordinal);
bool semantic_param_publication_checked_call(const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_record_result_allocator_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *type);
void semantic_param_publication_invalidate_allocator_binding(SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_checked_finish_independent(const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_checked_readfile_independent(const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_checked_local_owner_constructor(
    const SemanticContext *ctx, const ASTNode *call);
ASTNode *semantic_param_publication_direct_callee(const SemanticContext *ctx, const ASTNode *call);
ASTNode *semantic_param_publication_intent_callee(const SemanticContext *ctx, const ASTNode *call);
ASTNode *semantic_param_publication_slice_view_receiver(const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_bodyless_selected_call_current(
    const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_instance_receiver(const SemanticContext *ctx,
    const ASTNode *call, ASTNode **receiver_out);
ASTNode *semantic_param_publication_declared_result(const SemanticContext *ctx,
    const ASTNode *call, const Type **result_type);
bool semantic_param_publication_routine_result_current(const SemanticContext *ctx,
    const ASTNode *decl, const Type *result_type);
bool semantic_param_publication_record_assignment_replacement(SemanticContext *ctx,
    ASTNode *assignment, uint32_t binding, const Type *target_type);
bool semantic_param_publication_assignment_replacement(const SemanticContext *ctx, const ASTNode *assignment);
bool semantic_param_publication_value_call_target(const SemanticContext *ctx, const ASTNode *call);
/* Candidate only. A NULL owner supports claim replay, never CALL admission;
 * consumers additionally need the current routine's closed graph witness. */
bool semantic_param_publication_private_array_push(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *owner);
bool semantic_param_publication_private_physical_array_mutation(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *owner);
ASTNode *semantic_param_publication_callee(const SemanticContext *ctx,
    const ASTNode *call, size_t ordinal);
bool semantic_param_publication_formal_for_actual(const SemanticContext *ctx,
    const ASTNode *call, size_t actual, size_t *formal);
bool semantic_param_publication_selected_formal_for_actual(SemanticContext *ctx,
    const ASTNode *call, const ASTNode *decl, size_t actual, size_t *formal);
bool semantic_param_publication_finalize(SemanticContext *ctx);
void semantic_param_publication_destroy(SemanticContext *ctx);
#endif
