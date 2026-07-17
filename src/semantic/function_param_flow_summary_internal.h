#ifndef PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_SUMMARY_INTERNAL_H
#define PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_SUMMARY_INTERNAL_H

#include "type_checker.h"

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
    FunctionParamFlowSummaryState state;
} FunctionParamFlowSummaryEntry;

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
    size_t indexed_statement_visits;
    size_t indexed_program_points;
};

void function_param_flow_program_point_index_destroy(
    FunctionParamFlowProgramPointIndex *index);

#endif
