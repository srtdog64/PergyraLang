#ifndef PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_SUMMARY_INTERNAL_H
#define PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_SUMMARY_INTERNAL_H

#include "type_checker.h"
#include "slot_analyzer_internal.h"

typedef enum
{
    FUNCTION_PARAM_FLOW_UNSEEN = 0,
    FUNCTION_PARAM_FLOW_COMPUTING,
    FUNCTION_PARAM_FLOW_EVALUATED,
    FUNCTION_PARAM_FLOW_COMPLETE
} FunctionParamFlowSummaryState;

typedef struct
{
    uint32_t function_id;
    size_t param_index;
    ASTNode *function_decl;
    unsigned mask;
    /* Independent descriptor-retention facet; Slot access bits do not prove it. */
    bool array_storage_unproved;
    bool returned_backing_unproved;
    bool returned_backing_pending;
    bool *output_dependencies;
    size_t output_count;
    bool output_dependencies_ready;
    FunctionParamFlowSummaryState state;
} FunctionParamFlowSummaryEntry;

typedef struct {
    ASTNode *node;
    uint32_t id;
    ASTNodeType shape;
    uint64_t *origins;
    size_t origin_epoch, visit_epoch, work_epoch;
} FunctionParamFlowBackingNode;
typedef struct {
    uint32_t binding;
    uint64_t *origins, *destinations;
    bool local;
} FunctionParamFlowBackingBinding;
typedef struct FunctionParamFlowIndexClaims FunctionParamFlowIndexClaims;
typedef struct {
    size_t count, words, epoch, evaluation_pass;
    unsigned *effects;
    uint64_t *outputs;
    FunctionParamFlowBackingNode *nodes;
    size_t node_count;
    FunctionParamFlowBackingBinding *bindings;
    size_t binding_count;
    bool building, complete, pending, changed;
    bool private_stores_closed;
    FunctionParamFlowIndexClaims *claims;
} FunctionParamFlowBackingFact;
typedef struct {
    ASTNode *node;
    uint32_t id, binding;
    ASTNodeType shape;
    ASTNode **children;
    uint32_t *child_ids;
    size_t child_count;
    bool assignment_replacement, local_alias;
    bool private_array_push;
    bool execution_present, reachable, physical_value;
    bool numeric_cast;
    char *cast_target;
    bool result_relation, result_depends, external_retains;
    char *result_type_name;
} FunctionParamFlowIndexClaim;
struct FunctionParamFlowIndexClaims {
    FunctionParamFlowIndexClaim *rows;
    size_t count;
    bool failed;
};

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
    FunctionParamFlowBackingFact *backing;
    FunctionParamFlowIndexClaims *claims;
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
    bool settling_pending;
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
void function_param_flow_fail(FunctionParamFlowSummaryStore *store, ASTNode *site, const char *reason);
bool function_param_flow_program_point_index_build(FunctionParamFlowSummaryStore *store,
    ASTNode *function_decl, FunctionParamFlowProgramPointIndex **index_out);
void function_param_flow_program_point_indexes_destroy(FunctionParamFlowSummaryStore *store);
bool function_param_flow_returned_backing_effects(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs, size_t output_count, bool *pending,
    bool private_stores_closed);
bool function_param_flow_callee_relation_ready(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal);
bool function_param_flow_callee_relation_pending(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal);
void function_param_flow_callee_unfinished(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal, bool *pending, bool *complete);
bool function_param_flow_output_dependency_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t source_ordinal, size_t output_ordinal, bool *dependent);
bool function_param_flow_mutable_output_required(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node);
bool function_param_flow_no_store_call_append(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *call);
bool function_param_flow_no_store_graph(FunctionParamFlowSummaryStore *store, ASTNode *decl);
bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl);
bool function_param_flow_index_roots_current(const FunctionParamFlowProgramPointIndex *index);
bool function_param_flow_index_claim_record(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node);
bool function_param_flow_claim_record(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node);
bool function_param_flow_index_claims_current(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index);
FunctionParamFlowSummaryStore *function_param_flow_store_get(const SlotFunctionLookup *lookup);
unsigned function_param_flow_summary_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t ordinal);

bool function_param_array_storage_unproved_in_program_points(
    ASTNode *const *roots, size_t count, const SlotSummaryOrigin *origin,
    const SlotFunctionLookup *lookup);

#endif
