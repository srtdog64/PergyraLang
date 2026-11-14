#ifndef PERGYRA_TYPE_CHECKER_FLOW_EFFECTS_H
#define PERGYRA_TYPE_CHECKER_FLOW_EFFECTS_H

#include <stdbool.h>
#include <stdint.h>

#include "type_checker_internal.h"

uint32_t effect_delta_from_baseline(uint32_t baseline, uint32_t after);

void flow_record_branch_effect_conflict_labeled(SemanticContext *ctx,
                                                const ASTNode *node,
                                                uint32_t left_delta,
                                                const char *left_label,
                                                uint32_t right_delta,
                                                const char *right_label);

void flow_record_branch_effect_conflict(SemanticContext *ctx,
                                        const ASTNode *node,
                                        uint32_t left_delta,
                                        uint32_t right_delta);

void flow_record_unreachable_statement(SemanticContext *ctx,
                                       const ASTNode *node);

void flow_merge_effect_delta(SemanticContext *ctx,
                             const ASTNode *node,
                             uint32_t *merged_delta,
                             uint32_t *previous_delta,
                             bool *have_previous_delta,
                             uint32_t current_delta);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_EFFECTS_H */
