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
    /* An Array formal's buffer may be reallocated or freed through it (a
     * push, or a hand-off to a formal that may); false until the walk sees
     * one, so a cycle of hand-offs keeps it unless some routine on it does not. */
    bool array_buffer_unkept;
    bool returned_backing_unproved;
    bool returned_backing_pending;
    bool analysis_unsupported;
    bool *output_dependencies;
    size_t output_count;
    bool output_dependencies_ready;
    FunctionParamFlowOutputProducer output_producer;
    FunctionParamFlowSummaryState state;
    /* Demand epoch of this completed entry's last successful output replay. */
    size_t output_current_epoch;
    /* In the solving closure: an entry this one's routine read changed after
     * the read, so the next pass evaluates this entry again. */
    bool dirty;
    /* Head (plus one) of the closure's list of routines that read this entry. */
    size_t readers_plus_one;
} FunctionParamFlowSummaryEntry;

/* One routine that read a closure entry, recorded after the read returned. */
typedef struct {
    ASTNode *reader;
    size_t next_plus_one;
} FunctionParamFlowClosureRead;

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
    /* The receiver follows the method's receiver-formal summary instead of
     * a read-only proof. */
    bool receiver_summarized;
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
    /* The expression whose value carried a loan seed's first escape, where
     * a copy can stand in for it; NULL where none can (a receiver, an
     * inout argument). */
    ASTNode *escape_carrier;
    uint32_t loan_owner_slot_id;
    size_t loan_work;
    bool loan_seed_seen;
    bool checking_declared_result, result_return_seen;
} ReturnedBacking;
bool backing_merge(size_t words, uint64_t *into, const uint64_t *from);
void backing_effect(ReturnedBacking *state, const uint64_t *origins, unsigned effect);
/* backing_effect for the value of `carrier`, recorded as the escape carrier
 * when it is a loan seed's first escape. */
void backing_effect_carried(ReturnedBacking *state, const uint64_t *origins, unsigned effect,
    ASTNode *carrier);
bool backing_any_origin(const ReturnedBacking *state, const uint64_t *origins);
/* `held` has every origin in `origins` already. Storing them again into a
 * place that holds them adds no call escape: `carriers.names = names` after
 * `let names = carriers.names` puts back what `carriers` held. */
bool backing_origins_held(const ReturnedBacking *state, const uint64_t *origins, const uint64_t *held);
FunctionParamFlowBackingBinding *backing_binding(ReturnedBacking *state, uint32_t id, bool insert);
size_t backing_node(ReturnedBacking *state, ASTNode *node);
bool backing_work(ReturnedBacking *state, size_t at);
void backing_consumers(ReturnedBacking *state, ASTNode *node);
const uint64_t *backing_origin(ReturnedBacking *state, ASTNode *node);
/* Row lookups the origin, destination and consumer walks share. */
size_t backing_receiver_summary(ReturnedBacking *state, ASTNode *call, ASTNode *receiver);
FunctionParamFlowBackingBinding *backing_identifier_row(ReturnedBacking *state, ASTNode *value);
bool backing_storage_free_enum(ReturnedBacking *state, SemanticPublishedValue value);
/* Where a value's backing goes: its destinations, or every alias `id` of it. */
void backing_destination_value(ReturnedBacking *state, ASTNode *value, uint64_t *destinations);
void backing_destination_alias(ReturnedBacking *state, ASTNode *value, uint32_t id);
/* Handing a value that carries the frame's input to an inout formal is no
 * call escape by itself when the input is `ref`: a local holds of a ref
 * only what the ref lends without a refusal at the store (Strings,
 * scalars), never an alias of its storage, so a callee rewriting the
 * argument changes nothing of the input. What the callee keeps of the
 * formal is its summary's call or channel escape, added at the call. An
 * `inout` input is the caller's own place, which the routine may change:
 * a callee rewriting it changes what the caller lent for change, and an
 * alias of it written there is refused at the next use or at the exit. A
 * by-value input shares its caller's storage, and an alias of it written
 * by the callee changes the caller: that stays an escape. */
bool backing_inout_argument_leaves_input(const ReturnedBacking *state);
void backing_store(ReturnedBacking *state, ASTNode *target, const uint64_t *origins, bool replacement,
    ASTNode *carrier);
/* Applies an intent call's effect on its arguments' storage; false when the
 * call selects no intent. */
bool function_param_flow_intent_call(ReturnedBacking *state, ASTNode *call);
/* The callable each step action of the intent a call selects reaches, for
 * the currency check; NULL past the last action or for any other call. */
ASTNode *function_param_flow_intent_action_callee(SemanticContext *ctx, ASTNode *call, size_t step);
/* Whether `action` is a step action call of the intent `call` selects: a
 * relation through it belongs to the frame that holds `call`. */
bool function_param_flow_intent_step_call(SemanticContext *ctx, const ASTNode *call, const ASTNode *action);
size_t function_param_flow_receiver_formal_summary(ReturnedBacking *state, ASTNode *decl);
/* The ordinal an unsupported relation records for a method's receiver. */
#define FUNCTION_PARAM_FLOW_RECEIVER_ORDINAL (SIZE_MAX - 1)
/* Marks the frame unsupported when the method's receiver-formal summary is;
 * returns whether the frame is unsupported. */
bool function_param_flow_receiver_unsupported(ReturnedBacking *state, ASTNode *call, ASTNode *decl);
bool function_param_flow_read_payload_no_escape(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingReadPayloadLoan *row,
    ASTNode **escape_site);
/* A read-and-return callee lent `call`'s by-value argument, rooted at the
 * formal `owner`, and `caller` uses that argument again: admitted when the
 * loan stays in `caller`, or leaves it only as `caller`'s own summary returns
 * the formal, and nothing in `caller` changes the owner. When given,
 * `failure` says why a refused loan failed. */
typedef struct {
    bool owner_changed;      /* the loan stayed; only a change of its owner refused it */
    ASTNode *escape_carrier; /* the expression whose value took the loan out, or NULL */
} FunctionParamFlowLoanFailure;
bool function_param_flow_lent_argument_admitted(SemanticContext *ctx,
    ASTNode *caller, ASTNode *call, uint32_t owner, FunctionParamFlowLoanFailure *failure);
/* The backing `call`'s result carries never leaves `caller`. */
bool function_param_flow_call_result_stays(SemanticContext *ctx, ASTNode *caller, ASTNode *call);
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
    bool collection_effect_present;
    uint32_t collection_effect_kind, collection_effect_receiver, collection_effect_source;
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
    /* Selected intrinsic String independence, not element ownership. Child
     * identities/ordinals remain owned by this original claim. */
    bool readfile_result, readfile_result_projection, readfile_argument_projection;
    bool readfile_execution_required;
    size_t readfile_next_plus_one;
    char *result_type_name;
} FunctionParamFlowIndexClaim;
struct FunctionParamFlowIndexClaims {
    FunctionParamFlowIndexClaim *rows;
    size_t count;
    size_t readfile_first_plus_one;
    bool failed;
    /* Changes with every row added or promoted. The snapshot writes rows and
     * the control projection once, while the index is built and before any
     * check of it, so these are the only writes a passed check can miss. */
    size_t generation;
    /* The generation and index local count of the last index claims check
     * that passed (function_param_flow_index_claims_current). */
    size_t checked_generation, checked_locals;
    bool checked;
    /* For the loan order: each row's parent row, built for one generation,
     * and the row of the last node asked about. */
    size_t *order_parent, order_generation, order_site_row;
    const ASTNode *order_site;
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

/* What a rows check reads of one routine's store state: the entry and the
 * frame of each formal, the declared result frame and the claims. It is
 * settled when every entry is complete and every frame is built and neither
 * building nor pending; a settled entry or frame never changes again. The
 * version changes whenever a refresh finds the shape different. */
typedef struct {
    size_t *entries;
    FunctionParamFlowBackingFact **frames;
    size_t claim_count, version;
    bool claims_failed, settled, known;
} FunctionParamFlowRowsShape;
/* A routine's last passed rows check: its shape version, the callees it
 * reached, and the checks of state outside any shape that a reuse repeats
 * (no-store graphs and read-only receivers). */
typedef struct {
    bool passed;
    size_t version;
    ASTNode **callees, **no_store_routines, **receiver_calls;
    size_t callee_count, no_store_count, receiver_count;
} FunctionParamFlowRowsMemo;

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
    FunctionParamFlowRowsShape rows_shape;
    FunctionParamFlowRowsMemo rows_memo;
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
    /* Context-local synchronous replay barrier, never evidence or a cache. */
    bool validating_current;
    bool changed;
    bool active_had_recursion;
    bool failed;
    size_t body_evaluations;
    size_t cache_hits;
    size_t recursion_hits;
    size_t fixed_point_passes;
    size_t work_units;
    size_t work_budget;
    /* Frame node visits of the current demand closure, and the largest such
     * count of any closure. Telemetry only: frame rounds are bounded by each
     * frame's growth, not by work_budget. */
    size_t closure_frame_node_visits, max_closure_frame_node_visits;
    size_t demand_epoch;
    size_t *admitted_no_store_indexes;
    size_t admitted_no_store_count;
    /* Prefix of the admitted set already replayed without a capture in this
     * demand. The set only grows within a demand. */
    size_t admitted_no_store_replayed_epoch, admitted_no_store_replayed_count;
    size_t indexed_statement_visits;
    size_t indexed_program_points;
    /* Rows check memo telemetry: checks kept and reused. */
    size_t rows_memo_records, rows_memo_reuses;
    /* Reads inside the solving closure, the entry being evaluated (plus one),
     * and the evaluations a pass skipped because nothing they read changed. */
    FunctionParamFlowClosureRead *closure_reads;
    size_t closure_read_count, closure_read_capacity;
    size_t evaluating_plus_one;
    size_t skipped_evaluations;
    /* Field verdicts of settled struct formals, asked for after the solve;
     * a verdict being computed reads as unproved. */
    struct FunctionParamFlowFieldVerdict *field_verdicts;
    size_t field_verdict_count, field_verdict_capacity;
};

void function_param_flow_program_point_index_destroy(
    FunctionParamFlowProgramPointIndex *index);
bool function_param_flow_current_begin(SemanticContext *ctx, bool *previous);
void function_param_flow_current_end(SemanticContext *ctx, bool previous);
/* Only the explicit indexed-receipt preparation or an unprepared private receipt may
 * materialize. Never call after a failed current query as a fallback. */
bool function_param_flow_string_result_source_prepare(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source);
bool function_param_flow_private_string_receiver_replay(SemanticContext *ctx,
    ASTNode *routine, uint32_t binding);
bool function_param_flow_string_result_independent_replay(SemanticContext *ctx,
    const ASTNode *call);
bool function_param_flow_control_append(SemanticContext *ctx,
    FunctionParamFlowControlProjection *projection, ASTNode *node, size_t claim, size_t root);
void function_param_flow_backing_control_roots(ReturnedBacking *state,
    const FunctionParamFlowControlProjection *projection);
void function_param_flow_fail(FunctionParamFlowSummaryStore *store, ASTNode *site, const char *reason);
size_t function_param_flow_program_point_index_find(const FunctionParamFlowSummaryStore *store,
    uint32_t function_id);
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
typedef bool (*FunctionParamFlowInputCapture)(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowIndexClaims *claims, const ASTNode *call, void *user);
bool function_param_flow_backing_rows_capture(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, FunctionParamFlowInputCapture capture, void *user);
bool function_param_flow_no_store_graph_capture(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, FunctionParamFlowInputCapture capture, void *user);
bool function_param_flow_no_store_member_admitted(const FunctionParamFlowSummaryStore *store, size_t ordinal);
bool function_param_flow_no_store_admitted_current(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowInputCapture capture, void *user);
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
/* An unsupported callee relation a frame recorded still describes the
 * call and the callee's unsupported summary. */
bool function_param_flow_unsupported_relation_current(FunctionParamFlowSummaryStore *store,
    const FunctionParamFlowPendingRelation *edge);
bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl);
/* Rows check memo (function_param_flow_rows_check_memo.c). `current`: the
 * last passed check of index `ordinal` still holds, its shape unchanged and
 * its repeated checks passing. `record` keeps a check in place of the last
 * one, when it passed on a settled shape, and in a solve only without
 * pending relations; it takes the lists of `reached` either way. `note` adds a node to a list once. */
bool function_param_flow_rows_memo_current(FunctionParamFlowSummaryStore *store, size_t ordinal);
void function_param_flow_rows_memo_record(FunctionParamFlowSummaryStore *store, size_t ordinal,
    FunctionParamFlowRowsMemo *reached, bool passed);
bool function_param_flow_rows_memo_note(ASTNode ***list, size_t *count, ASTNode *node);
/* The verify mode's comparison: a full check reached what the memo holds. */
bool function_param_flow_rows_memo_matches(const FunctionParamFlowSummaryStore *store, size_t ordinal,
    const FunctionParamFlowRowsMemo *reached);
void function_param_flow_rows_memo_destroy(FunctionParamFlowProgramPointIndex *index);
bool function_param_flow_declared_result_closed(FunctionParamFlowSummaryStore *store,
    ASTNode *call, FunctionParamFlowBackingFact *consumer);
FunctionParamFlowBackingFact *function_param_flow_declared_result_frame_current(
    FunctionParamFlowSummaryStore *store, ASTNode *call, FunctionParamFlowBackingFact *consumer);
bool function_param_flow_string_result_source_current(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source);
bool function_param_flow_nonlocal_input_current(SemanticContext *ctx,
    ASTNode *routine, size_t formal_ordinal);
bool function_param_flow_private_string_receiver_current(SemanticContext *ctx,
    ASTNode *routine, uint32_t binding);
bool function_param_flow_string_result_independent_current(SemanticContext *ctx, const ASTNode *call);
bool function_param_flow_claim_capture(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node, bool semantic);
bool function_param_flow_readfile_claim_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, const FunctionParamFlowIndexClaim *row);
bool function_param_flow_readfile_claim_count(const FunctionParamFlowIndexClaims *claims,
    size_t *count);
bool function_param_flow_readfile_occurrence_ordinal(const FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, const ASTNode *execution_root, size_t *ordinal);
bool function_param_flow_readfile_execution_counts(const FunctionParamFlowIndexClaims *claims,
    size_t *required_calls, size_t *required_writes);
bool function_param_flow_readfile_occurrence_required(const FunctionParamFlowIndexClaims *claims,
    size_t ordinal, bool *required);
bool function_param_flow_readfile_write_required(const FunctionParamFlowIndexClaims *claims,
    size_t ordinal, bool *required);
bool function_param_flow_readfile_write_count(const FunctionParamFlowIndexClaims *claims,
    size_t *count);
bool function_param_flow_readfile_write_ordinal(const FunctionParamFlowIndexClaims *claims,
    const ASTNode *statement, size_t *ordinal);
/* Navigation result shape only: String acceptance is no nonlocal-source or
 * independence proof. The optional Array output requires its owned receipt. */
bool function_param_flow_navigation_result_shape(SemanticContext *ctx, ASTNode *call,
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

/* The entry for (routine, formal), added UNSEEN when it is new; SIZE_MAX
 * when the formal does not exist or the table cannot grow. */
size_t function_param_flow_add(FunctionParamFlowSummaryStore *store,
    ASTNode *function_decl, size_t param_index);
size_t function_param_flow_find(const FunctionParamFlowSummaryStore *store,
    uint32_t function_id, size_t param_index);
/* The routine being evaluated read closure entry `read`; false when the read
 * list cannot grow, after the store failed closed. */
bool function_param_flow_closure_read(FunctionParamFlowSummaryStore *store, size_t read);
/* Closure entry `changed` changed: every evaluated entry of a routine that read
 * it is evaluated again in the next pass. */
void function_param_flow_closure_changed(FunctionParamFlowSummaryStore *store, size_t changed);
/* What a reader of an entry can see. Outputs only grow, so their count
 * stands for the set. */
typedef struct {
    unsigned mask;
    bool storage, elements, buffer, unproved, pending, unsupported, ready, outputs_present;
    FunctionParamFlowOutputProducer producer;
    size_t outputs;
} FunctionParamFlowEntryFacts;
FunctionParamFlowEntryFacts function_param_flow_entry_facts(const FunctionParamFlowSummaryEntry *entry);
bool function_param_flow_entry_facts_equal(FunctionParamFlowEntryFacts a, FunctionParamFlowEntryFacts b);
/* One entry's summary from its body; `function_param_flow_evaluate` runs it as
 * the reader of what it demands. */
unsigned function_param_flow_evaluate_body(FunctionParamFlowSummaryStore *store, size_t index);
unsigned function_param_flow_evaluate(FunctionParamFlowSummaryStore *store, size_t index);
bool function_param_flow_worklist_verified(FunctionParamFlowSummaryStore *store, ASTNode *site);
/* `summary` is what the formal's summary says so far; its RETURN bit lets a
 * returned value carry an alias of the formal. `*buffer_unkept` is set when
 * an Array formal's buffer may be reallocated or freed through it. */
unsigned function_param_array_storage_unproved_in_program_points(
    ASTNode *const *statements, size_t statement_count, ASTNode *const *roots, size_t count,
    const SlotSummaryOrigin *origin, const SlotFunctionLookup *lookup, unsigned summary, bool *payload_read,
    bool *buffer_unkept);
/* No push or other reallocation or release reaches the buffer of the Array
 * formal through it, as the storage walk last settled; false when that is
 * not known. */
bool function_param_flow_array_buffer_kept(SemanticContext *ctx, ASTNode *function_decl, size_t param_index);
/* The same walk proving only the paths through one top-level `field` of a
 * struct formal; false when the formal is not proved through paths. */
bool function_param_array_storage_field_unproved_in_program_points(
    ASTNode *const *statements, size_t statement_count, ASTNode *const *roots, size_t count,
    const SlotSummaryOrigin *origin, const SlotFunctionLookup *lookup, unsigned summary, const char *field,
    unsigned *failures);
/* One entry's settled Array preservation failures, for the published snapshot. */
unsigned function_param_flow_array_preservation_settled(FunctionParamFlowSummaryStore *store, size_t index);

#endif
