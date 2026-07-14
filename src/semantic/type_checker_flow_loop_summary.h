#ifndef PERGYRA_TYPE_CHECKER_FLOW_LOOP_SUMMARY_H
#define PERGYRA_TYPE_CHECKER_FLOW_LOOP_SUMMARY_H

#include "type_checker_flow_internal.h"

void loop_flow_summary_begin_function(SemanticContext *ctx);
void loop_flow_summary_end_function(SemanticContext *ctx);
void loop_flow_summary_note_body_check(SemanticContext *ctx,
                                       const ASTNode *node,
                                       const char *kind);
bool loop_flow_summary_try_apply(SemanticContext *ctx,
                                 const ASTNode *node,
                                 const ResourceConsumeSnapshot *entry,
                                 uint32_t effect_base,
                                 FlowFlags *flags_out);
void loop_flow_summary_record(SemanticContext *ctx,
                              const ASTNode *node,
                              const ResourceConsumeSnapshot *entry,
                              const ResourceConsumeSnapshot *exit,
                              uint32_t effect_base,
                              uint32_t effect_delta,
                              FlowFlags flags);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_LOOP_SUMMARY_H */
