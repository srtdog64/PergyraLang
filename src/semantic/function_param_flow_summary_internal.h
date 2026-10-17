#ifndef PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_SUMMARY_INTERNAL_H
#define PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_SUMMARY_INTERNAL_H

#include "type_checker.h"
#include "slot_analyzer_internal.h"
#include "function_param_publication_owner.h"

typedef enum
{
    FUNCTION_PARAM_FLOW_UNSEEN = 0,
    FUNCTION_PARAM_FLOW_COMPUTING,
    FUNCTION_PARAM_FLOW_EVALUATED,
    FUNCTION_PARAM_FLOW_COMPLETE
} FunctionParamFlowSummaryState;

typedef enum {
    FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED = 0,
    FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR,
    FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME,
    FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN
} FunctionParamFlowOutputProducer;

typedef enum {
    FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED = 0,
    FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_PHYSICAL,
    FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT,
    FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT
} FunctionParamFlowResultArgumentKind;

typedef struct
{
    uint32_t function_id;
    size_t param_index;
    ASTNode *function_decl;
    unsigned mask;
    /* Independent descriptor-retention facet; Slot access bits do not prove it. */
    bool array_storage_unproved;
    bool array_elements_unproved;
    bool returned_backing_unproved;
    bool returned_backing_pending;
    bool analysis_unsupported;
    bool *output_dependencies;
    size_t output_count;
    bool output_dependencies_ready;
    FunctionParamFlowOutputProducer output_producer;
    FunctionParamFlowSummaryState state;
} FunctionParamFlowSummaryEntry;

typedef struct {
    ASTNode *node;
    uint32_t id;
    ASTNodeType shape;
    uint64_t *origins;
    size_t origin_epoch, visit_epoch, work_epoch;
    /* An ordinal-free declared-result dependency, never an invented input. */
    uint32_t result_callee_id;
    const Type *result_type;
    size_t result_argument_count;
    unsigned char *result_argument_kinds;
    const ASTNode *receiver;
    const Type *receiver_type;
    uint32_t receiver_id, receiver_callee_id, receiver_formal_id;
} FunctionParamFlowBackingNode;
typedef struct {
    uint32_t binding;
    uint64_t *origins, *destinations;
    size_t origin_read_epoch, destination_read_epoch;
    bool local;
} FunctionParamFlowBackingBinding;
typedef struct FunctionParamFlowIndexClaims FunctionParamFlowIndexClaims;
typedef struct {
    ASTNode *call;
    uint32_t call_id, callee_id;
    size_t ordinal;
} FunctionParamFlowPendingRelation;
typedef struct {
    size_t origin_count, origin_words;
    size_t destination_count, destination_words;
    size_t input_ordinal;
    uint32_t input_id;
    size_t epoch, evaluation_pass;
    unsigned *effects;
    uint64_t *outputs;
    FunctionParamFlowBackingNode *nodes;
    size_t node_count;
    FunctionParamFlowBackingBinding *bindings;
    size_t binding_count;
    FunctionParamFlowPendingRelation *pending_relations;
    size_t pending_relation_count;
    size_t sealed_pending_relation_count;
    bool analysis_unsupported;
    FunctionParamFlowPendingRelation unsupported_relation;
    bool building, complete, pending, changed;
    bool private_stores_closed;
    FunctionParamFlowIndexClaims *claims;
    const Type *declared_result_type;
} FunctionParamFlowBackingFact;
typedef struct {
    FunctionParamFlowSummaryStore *store;
    SlotFunctionLookup lookup;
    ASTNode *decl;
    FunctionParamFlowBackingFact *fact;
    bool private_stores_closed;
    ASTNode *loan_seed, *consumer, *escape_site;
    uint32_t loan_owner_slot_id;
    size_t loan_work;
    bool loan_seed_seen;
    bool checking_declared_result, result_return_seen;
} ReturnedBacking;
bool backing_merge(size_t words, uint64_t *into, const uint64_t *from);
void backing_effect(ReturnedBacking *state, const uint64_t *origins, unsigned effect);
bool backing_any_origin(const ReturnedBacking *state, const uint64_t *origins);
FunctionParamFlowBackingBinding *backing_binding(ReturnedBacking *state, uint32_t id, bool insert);
size_t backing_node(ReturnedBacking *state, ASTNode *node);
bool backing_work(ReturnedBacking *state, size_t at);
void backing_consumers(ReturnedBacking *state, ASTNode *node);
bool function_param_flow_read_payload_no_escape(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingReadPayloadLoan *row,
    ASTNode **escape_site);
typedef struct {
    ASTNode *node;
    uint32_t id, binding;
    ASTNodeType shape;
    bool semantic_promoted, snapshot_active, snapshot_complete, snapshot_semantic_complete;
    bool control_projected, control_projection_active;
    size_t control_step_ordinal, control_root_ordinal;
    size_t control_incoming_count, control_outgoing_count;
    int match_family;
    int syntax_operator;
    bool parallel_join;
    char *syntax_name;
    char **generic_names;
    size_t generic_name_count;
    double syntax_number;
    int64_t syntax_long;
    unsigned syntax_literal_flags;
    ASTNode **children;
    uint32_t *child_ids;
    size_t *child_claim_ordinals;
    size_t child_count;
    bool consumer_empty;
    bool assignment_replacement, local_alias;
    bool channel_binding;
    bool empty_map_binding, slot_write, local_owner_constructor;
    bool owned_slot_binding;
    bool slot_token_operand;
    bool private_array_push;
    bool execution_present, reachable, physical_value;
    /* Exact typed proof that this member's qualifier is a nonexecuting enum
     * namespace edge. Syntax children remain sealed independently above. */
    bool unit_enum_selection;
    bool value_ready;
    bool value_meta;
    bool value_origin_ready;
    const Type *value_type;
    bool numeric_cast;
    char *cast_target;
    bool result_relation, result_depends, external_retains;
    bool backing_relation, backing_depends, backing_external_retains;
    char *result_type_name;
} FunctionParamFlowIndexClaim;
struct FunctionParamFlowIndexClaims {
    FunctionParamFlowIndexClaim *rows;
    size_t count;
    bool failed;
};

typedef struct {
    ASTNode *node;
    uint32_t syntax_id;
    size_t claim_ordinal, root_ordinal;
} FunctionParamFlowControlStep;
typedef struct {
    FunctionParamFlowControlStep *steps;
    size_t step_count, step_capacity;
    size_t *root_offsets;
    size_t *root_claim_ordinals;
    size_t root_count, projected_claim_count;
    bool ready;
} FunctionParamFlowControlProjection;

typedef struct
{
    uint32_t function_id;
    ASTNode *function_decl;
    ASTNode ***roots_by_param;
    size_t *root_counts;
    size_t *root_capacities;
    size_t param_count;
    size_t statement_count;
    size_t relevant_root_count;
    ASTNode **dependency_roots;
    uint32_t *dependency_root_ids;
    size_t dependency_root_count;
    ASTNode *dependency_body;
    uint32_t dependency_body_id;
    bool dependency_roots_ready;
    bool mutable_output_required;
    bool private_local_stores;
    size_t failed_no_store_epoch;
    size_t admitted_no_store_epoch, admitted_no_store_position;
    /* Original call identities; zero-argument edges need no invented ordinal. */
    ASTNode **no_store_calls;
    size_t no_store_call_count;
    ASTNode **local_definitions;
    uint32_t *local_ids;
    size_t local_count;
    FunctionParamFlowBackingFact **backing_frames;
    size_t backing_frame_count;
    FunctionParamFlowBackingFact *declared_result;
    FunctionParamFlowIndexClaims *claims;
    FunctionParamFlowControlProjection *control_projection;
} FunctionParamFlowProgramPointIndex;

struct FunctionParamFlowSummaryStore
{
    SemanticContext *ctx;
    ASTNode *program_root;
    FunctionParamFlowSummaryEntry *entries;
    size_t count;
    size_t capacity;
    size_t *hash;
    size_t hash_capacity;
    FunctionParamFlowProgramPointIndex *program_point_indexes;
    size_t program_point_index_count;
    size_t program_point_index_capacity;
    size_t active_start;
    bool solving;
    bool changed;
    bool active_had_recursion;
    bool failed;
    size_t body_evaluations;
    size_t cache_hits;
    size_t recursion_hits;
    size_t fixed_point_passes;
    size_t work_units;
    size_t work_budget;
    size_t demand_epoch;
    size_t *admitted_no_store_indexes;
    size_t admitted_no_store_count;
    size_t indexed_statement_visits;
    size_t indexed_program_points;
};

void function_param_flow_program_point_index_destroy(
    FunctionParamFlowProgramPointIndex *index);
bool function_param_flow_control_append(SemanticContext *ctx,
    FunctionParamFlowControlProjection *projection, ASTNode *node, size_t claim, size_t root);
void function_param_flow_backing_control_roots(ReturnedBacking *state,
    const FunctionParamFlowControlProjection *projection);
void function_param_flow_fail(FunctionParamFlowSummaryStore *store, ASTNode *site, const char *reason);
bool function_param_flow_program_point_index_build(FunctionParamFlowSummaryStore *store,
    ASTNode *function_decl, FunctionParamFlowProgramPointIndex **index_out);
bool function_param_flow_local_replacement(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *assignment);
void function_param_flow_program_point_indexes_destroy(FunctionParamFlowSummaryStore *store);
bool function_param_flow_returned_backing_effects(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs, size_t output_count, bool *pending,
    bool private_stores_closed);
bool function_param_flow_callee_relation_ready(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal);
bool function_param_flow_callee_relation_pending(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal);
void function_param_flow_callee_unfinished(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal, FunctionParamFlowBackingFact *fact);
bool function_param_flow_output_current(FunctionParamFlowSummaryStore *store, size_t entry_index, bool pending_proof);
bool function_param_flow_backing_required(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, bool *required);
bool function_param_flow_complete_pending_relations(FunctionParamFlowSummaryStore *store);
bool function_param_flow_backing_unsupported_call(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowBackingFact *fact, ASTNode *call);
bool function_param_flow_backing_unsupported(FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t ordinal);
bool function_param_flow_unproved_current(FunctionParamFlowSummaryStore *store, size_t index);
void function_param_flow_complete_return_relations(FunctionParamFlowSummaryStore *store);
bool function_param_flow_output_dependency_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t source_ordinal, size_t output_ordinal, bool *dependent);
bool function_param_flow_mutable_output_required(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node);
bool function_param_flow_no_store_call_append(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *call);
bool function_param_flow_no_store_graph(FunctionParamFlowSummaryStore *store, ASTNode *decl);
bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl);
bool function_param_flow_declared_result_closed(FunctionParamFlowSummaryStore *store,
    ASTNode *call, FunctionParamFlowBackingFact *consumer);
FunctionParamFlowBackingFact *function_param_flow_declared_result_frame_current(
    FunctionParamFlowSummaryStore *store, ASTNode *call, FunctionParamFlowBackingFact *consumer);
bool function_param_flow_string_result_source_current(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source);
bool function_param_flow_nonlocal_input_current(SemanticContext *ctx,
    ASTNode *routine, size_t formal_ordinal);
bool function_param_flow_nonlocal_result(SemanticContext *ctx, ASTNode *call,
    bool *independent_physical_array);
bool function_param_flow_physical_result_header_current(SemanticContext *ctx, ASTNode *routine);
bool function_param_flow_nonlocal_physical_input_current(SemanticContext *ctx,
    ASTNode *call, size_t actual);
bool function_param_flow_typed_expression_children_current(SemanticContext *ctx,
    ASTNode *caller, const ASTNode *expression);
bool function_param_flow_string_result_nonlocal_current(SemanticContext *ctx,
    const ASTNode *call, ASTNode *caller, size_t formal_ordinal);
size_t function_param_flow_claim_child_count(const ASTNode *node, bool *supported);
ASTNode *function_param_flow_claim_child(const ASTNode *node, size_t ordinal);
bool function_param_flow_receiver_independent(ReturnedBacking *state, ASTNode *call);
ASTNode *function_param_flow_receiver_readonly_current(FunctionParamFlowSummaryStore *store,
    ASTNode *call, bool demand);
bool function_param_flow_index_roots_current(const FunctionParamFlowProgramPointIndex *index);
uint32_t function_param_flow_backing_place(ReturnedBacking *state, ASTNode *node);
bool function_param_flow_index_claim_record(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node);
bool function_param_flow_index_claims_snapshot(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node);
bool function_param_flow_syntax_claims_snapshot(SemanticContext *ctx,
    FunctionParamFlowIndexClaims **claims, ASTNode *node);
bool function_param_flow_syntax_claims_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims);
void function_param_flow_claims_destroy(FunctionParamFlowIndexClaims *claims);
bool function_param_flow_claim_record(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node);
bool function_param_flow_index_claims_current(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index);
bool function_param_flow_claims_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, bool semantic);
bool function_param_flow_control_expands(ASTNodeType shape);
bool function_param_flow_consumer_empty_derive(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, size_t ordinal);
bool function_param_flow_consumer_empty_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, ASTNode *node, bool *empty);
bool function_param_flow_reserve_entries(FunctionParamFlowSummaryStore *store,
    size_t minimum);
FunctionParamFlowSummaryStore *function_param_flow_store_get(const SlotFunctionLookup *lookup);
unsigned function_param_flow_summary_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t ordinal);

size_t function_param_flow_find(const FunctionParamFlowSummaryStore *store,
    uint32_t function_id, size_t param_index);
unsigned function_param_array_storage_unproved_in_program_points(
    ASTNode *const *roots, size_t count, const SlotSummaryOrigin *origin,
    const SlotFunctionLookup *lookup, bool *payload_read);

#endif
