#ifndef PGY_TYPE_CHECKER_INTENT_STEP_SEQUENCE_INTERNAL_H
#define PGY_TYPE_CHECKER_INTENT_STEP_SEQUENCE_INTERNAL_H

#include "type_checker.h"

/* Success opens the outcome scope; the sequence caller still closes it. */
bool type_check_intent_step_bind_outcome(ASTNode *intent_decl, ASTNode *step,
    Type *outcome_type, ASTNode *child_intent, SemanticContext *ctx,
    bool *scope_entered_out);

void type_check_intent_step_sequence(
    ASTNode *node,
    SemanticContext *ctx,
    Type **typed_success_payload_types,
    Type **typed_failure_payload_types,
    size_t *typed_success_scope_count_out);

#endif /* PGY_TYPE_CHECKER_INTENT_STEP_SEQUENCE_INTERNAL_H */
