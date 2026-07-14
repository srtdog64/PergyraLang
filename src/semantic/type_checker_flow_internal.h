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
    FLOW_RETURN      = 1 << 3,
    FLOW_HAS_DEFER   = 1 << 4
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
Type *flow_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
bool flow_condition_is_static_bool(const ASTNode *node);
bool flow_static_bool_value(const ASTNode *node, bool *value_out);
FlowFlags flow_terminating_flags(FlowFlags flags);
FlowFlags flow_record_statement_result(FlowFlags current,
                                       FlowFlags statement);
bool flow_has_fallthrough(FlowFlags flags);
void flow_reject_dynamic_defer_control(SemanticContext *ctx,
                                       ASTNode *site,
                                       const char *control_kind);
bool resource_snapshots_equal(const ResourceConsumeSnapshot *a,
                              const ResourceConsumeSnapshot *b);
bool resource_snapshot_availability_equal(const ResourceConsumeSnapshot *a,
                                          const ResourceConsumeSnapshot *b);
ResourceConsumeSnapshot copy_resource_snapshot(
    const ResourceConsumeSnapshot *src);
void merge_resource_snapshots_or(ResourceConsumeSnapshot *dst,
                                 bool *dst_initialized,
                                 const ResourceConsumeSnapshot *src);
void loop_flow_record(LoopFlowState *loop_flow,
                      bool is_break,
                      const ResourceConsumeSnapshot *state);
FlowFlags type_check_block_flow(ASTNode *node,
                                SemanticContext *ctx,
                                LoopFlowState *loop_flow);
FlowFlags type_check_statement_flow(ASTNode *node,
                                    SemanticContext *ctx,
                                    LoopFlowState *loop_flow);
FlowFlags type_check_if_stmt_flow(ASTNode *node,
                                  SemanticContext *ctx,
                                  LoopFlowState *loop_flow);
FlowFlags type_check_match_stmt_flow(ASTNode *node,
                                     SemanticContext *ctx,
                                     LoopFlowState *loop_flow);
FlowFlags type_check_statement_flow_boundary(ASTNode *node,
                                             SemanticContext *ctx);
FlowFlags type_check_loop_control_flow(ASTNode *node,
                                       SemanticContext *ctx,
                                       LoopFlowState *loop_flow,
                                       bool is_break);
FlowFlags type_check_unsafe_block_flow(ASTNode *node,
                                       SemanticContext *ctx,
                                       LoopFlowState *loop_flow);
FlowFlags type_check_defer_stmt_flow(ASTNode *node,
                                     SemanticContext *ctx);
FlowFlags type_check_namespace_flow(ASTNode *node,
                                    SemanticContext *ctx,
                                    LoopFlowState *loop_flow);
FlowFlags type_check_with_stmt_flow(ASTNode *node,
                                    SemanticContext *ctx,
                                    LoopFlowState *loop_flow);
FlowFlags type_check_parallel_stmt_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_async_stmt_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_select_stmt_flow_kind(ASTNode *node,
                                           SemanticContext *ctx);
FlowFlags type_check_let_stmt_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_destructure_stmt_flow(ASTNode *node,
                                           SemanticContext *ctx);
FlowFlags type_check_return_stmt_flow(ASTNode *node, SemanticContext *ctx);
/* docs/181 R2 (type_checker_flow_parallel_join.c) */
FlowFlags type_check_give_stmt_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_event_stmt_flow(ASTNode *node,
                                     SemanticContext *ctx,
                                     const char *event_kind);
FlowFlags type_check_event_invoke_stmt_flow(ASTNode *node,
                                            SemanticContext *ctx);
FlowFlags type_check_use_stmt_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_for_loop_flow(ASTNode *node, SemanticContext *ctx);
FlowFlags type_check_while_loop_flow(ASTNode *node, SemanticContext *ctx);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_INTERNAL_H */
