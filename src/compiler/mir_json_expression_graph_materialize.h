#ifndef PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_MATERIALIZE_H
#define PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_MATERIALIZE_H

#include "mir_types.h"

typedef struct {
    const char *kind;
    char *text;
    const char *call_target_kind;
    const char *call_target_name;
    int left;
    int right;
} MIRJsonExpressionGraphNode;

typedef struct {
    MIRJsonExpressionGraphNode *nodes;
    size_t count;
    size_t capacity;
} MIRJsonExpressionGraph;

void mir_json_expression_graph_dispose(
    MIRJsonExpressionGraph *graph);
int mir_json_expression_graph_build(
    MIRJsonExpressionGraph *graph,
    ASTNode *expr);

#endif
