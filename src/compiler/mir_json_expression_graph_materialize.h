#ifndef PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_MATERIALIZE_H
#define PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_MATERIALIZE_H

#include "mir_types.h"

typedef struct {
    const char *kind;
    char *text;
    const char *call_target_kind;
    const char *call_target_name;
    uint32_t call_target_syntax_id;
    uint32_t runtime_call_abi_id;
    uint32_t binding_syntax_id;
    const char *binding_kind;
    int binding_ordinal;
    int left;
    int right;
} MIRJsonExpressionGraphNode;

typedef struct {
    MIRJsonExpressionGraphNode *nodes;
    size_t count;
    size_t capacity;
    const MIRRoutine *binding_routine;
} MIRJsonExpressionGraph;

void mir_json_expression_graph_dispose(
    MIRJsonExpressionGraph *graph);
int mir_json_expression_graph_build(
    MIRJsonExpressionGraph *graph,
    ASTNode *expr);
int mir_json_expression_graph_build_for_routine(
    MIRJsonExpressionGraph *graph,
    ASTNode *expr,
    const MIRRoutine *routine);

#endif
