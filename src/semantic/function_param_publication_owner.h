#ifndef PGY_FUNCTION_PARAM_PUBLICATION_OWNER_H
#define PGY_FUNCTION_PARAM_PUBLICATION_OWNER_H
#include "type_checker.h"
/* Private snapshots owned by publication.c. Escape diagnostics borrow only
 * its checked admitted view; other consumers use the checked queries. */
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
} PendingRefCall;
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
    char *decl_name, *result_type_name;
    size_t field_count;
    SemanticNominalResultField *fields;
} SemanticNominalResultSnapshot;
bool semantic_nominal_result_snapshot_current(const SemanticContext *ctx,
    const ASTNode *call, const SemanticNominalResultSnapshot *snapshot);
void semantic_nominal_result_snapshot_destroy(SemanticNominalResultSnapshot *snapshot);
typedef struct {
    const ASTNode *node;
    uint32_t id;
    ASTNodeType shape;
    uint32_t binding;
} PublishedCallArgument;
typedef struct {
    const ASTNode *node, *target, *caller;
    uint32_t id, target_id, caller_id, builtin, declaration, value_binding, binding_id;
    bool value_present, copy_escape, meta, execution_present, reachable;
    bool call_present, kind_present, stdlib, local, cast_present;
    bool assignment_replacement;
    const ASTNode *assignment_rhs;
    uint32_t assignment_rhs_id, assignment_rhs_binding;
    ASTNodeType assignment_rhs_shape;
    uint32_t category;
    ASTNodeType value_shape;
    bool origin_ready, string_value, physical_scalar;
    PublishedCallArgument *arguments;
    ASTNodeType target_shape;
    uint32_t target_binding;
    size_t argument_count;
    size_t routine_index;
    char *target_name;
    char *value_type_name, *relation_argument_type, *relation_result_type;
    const ASTNode *relation_argument;
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
    PublishedNode *nodes; size_t node_count, node_capacity;
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
} SemanticParamPublicationView;
bool semantic_param_publication_view_current(const SemanticContext *ctx, const SemanticParamPublicationView *view);
bool semantic_param_publication_admitted_view(const SemanticContext *ctx, SemanticParamPublicationView *view);
bool semantic_consume_param_publication(SemanticContext *ctx, const SemanticParamPublicationView *view);
bool semantic_param_publication_record_checked_result_relation(SemanticContext *ctx, ASTNode *call,
    const Type *argument_type, const Type *result_type, uint32_t selected_decl, size_t payload_ordinal);
/* Ownership transfers only on success; failure leaves the producer responsible. */
bool semantic_param_publication_record_nominal_result(SemanticContext *ctx,
    ASTNode *call, SemanticNominalResultSnapshot *snapshot);
void semantic_param_publication_retire_value(PublishedNode *row);
bool semantic_param_publication_reserve_storage(void **rows, size_t *capacity, size_t count, size_t size);
bool semantic_param_publication_checked_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains);
/* Compilation-owned obligations: pending is never a published summary. */
bool semantic_param_publication_begin(SemanticContext *ctx);
bool semantic_param_publication_pending(const SemanticContext *ctx);
bool semantic_param_publication_inputs_ready(const SemanticContext *ctx);
bool semantic_param_publication_ready(const SemanticContext *ctx);
uint32_t semantic_param_publication_binding(const SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_argument_borrowed_for_call(
    const SemanticContext *ctx, const ASTNode *call, size_t ordinal);
bool semantic_param_publication_routine_return_dependency_is_scalar(
    const SemanticContext *ctx, const ASTNode *decl);
bool semantic_param_publication_routine_no_output_header_ready(
    const SemanticContext *ctx, const ASTNode *decl);
bool semantic_param_publication_routine_relation_header_ready(
    const SemanticContext *ctx, const ASTNode *decl);
bool semantic_param_publication_value_is_physical_scalar(
    const SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_parameter_is_anchored_handle(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal, bool *anchored);
bool semantic_param_publication_routine_inputs_current(
    const SemanticContext *ctx, const ASTNode *decl, bool require_body);
bool semantic_param_publication_record_routine(SemanticContext *ctx, ASTNode *decl,
    size_t count, Type **types, Type *function_type);
bool semantic_param_publication_record_call(SemanticContext *ctx, ASTNode *call, ASTNode *site,
    ASTNode *callee, size_t ordinal, const Type *type, const char *borrowed,
    const char *destination, const char *fix);
bool semantic_param_publication_record_body_call(SemanticContext *ctx, ASTNode *call);
/* Native ownership category is not a fresh-payload/copy/cleanup certificate. */
typedef struct { bool ok, copy_escape, meta; uint32_t category; bool origin_ready; } SemanticPublishedValue;
bool semantic_param_publication_begin_value(SemanticContext *ctx, const ASTNode *value);
bool semantic_param_publication_record_value(SemanticContext *ctx, ASTNode *value,
    const Type *type, bool meta);
bool semantic_param_publication_record_execution(SemanticContext *ctx,
    const ASTNode *statement, bool reachable);
bool semantic_param_publication_record_checked_call(SemanticContext *ctx, ASTNode *call);
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
ASTNode *semantic_param_publication_direct_callee(const SemanticContext *ctx, const ASTNode *call);
bool semantic_param_publication_record_assignment_replacement(SemanticContext *ctx,
    ASTNode *assignment, uint32_t binding, const Type *target_type);
bool semantic_param_publication_assignment_replacement(const SemanticContext *ctx, const ASTNode *assignment);
bool semantic_param_publication_value_call_target(const SemanticContext *ctx, const ASTNode *call);
ASTNode *semantic_param_publication_callee(const SemanticContext *ctx,
    const ASTNode *call, size_t ordinal);
bool semantic_param_publication_finalize(SemanticContext *ctx);
void semantic_param_publication_destroy(SemanticContext *ctx);
#endif
