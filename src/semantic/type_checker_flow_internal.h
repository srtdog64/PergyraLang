#ifndef PERGYRA_TYPE_CHECKER_FLOW_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_FLOW_INTERNAL_H

#include <stdbool.h>

#include "type_checker_internal.h"
#include "type_checker_flow_resources.h"

typedef enum
{
    FLOW_NONE        = 0,
    FLOW_FALLTHROUGH = 1 << 0,
    FLOW_BREAK       = 1 << 1,
    FLOW_CONTINUE    = 1 << 2,
    FLOW_RETURN      = 1 << 3
} FlowFlags;

typedef struct
{
    ResourceConsumeSnapshot break_states;
    ResourceConsumeSnapshot continue_states;
    bool                 has_break_states;
    bool                 has_continue_states;
    Scope               *loop_scope;
} LoopFlowState;

Type *flow_normalize_type(Type *type);
bool flow_condition_is_static_bool(const ASTNode *node);
bool flow_static_bool_value(const ASTNode *node, bool *value_out);
bool flow_ast_contains_defer_stmt(const ASTNode *node);
void flow_reject_dynamic_defer_control(SemanticContext *ctx,
                                       ASTNode *site,
                                       const char *control_kind);
void merge_resource_snapshots_or(ResourceConsumeSnapshot *dst,
                                 bool *dst_initialized,
                                 const ResourceConsumeSnapshot *src);
void loop_flow_record(LoopFlowState *loop_flow,
                      bool is_break,
                      const ResourceConsumeSnapshot *state);
FlowFlags type_check_block_flow(ASTNode *node,
                                SemanticContext *ctx,
                                LoopFlowState *loop_flow);
FlowFlags type_check_statement_flow_boundary(ASTNode *node,
                                             SemanticContext *ctx);
FlowFlags type_check_loop_control_flow(ASTNode *node,
                                       SemanticContext *ctx,
                                       LoopFlowState *loop_flow,
                                       bool is_break);
FlowFlags type_check_for_loop_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_while_loop_flow(ASTNode *node, SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_INTERNAL_H */
