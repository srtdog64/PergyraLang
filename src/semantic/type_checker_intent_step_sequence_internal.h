#ifndef PGY_TYPE_CHECKER_INTENT_STEP_SEQUENCE_INTERNAL_H
#define PGY_TYPE_CHECKER_INTENT_STEP_SEQUENCE_INTERNAL_H

#include "type_checker.h"

void type_check_intent_step_sequence(
    ASTNode *node,
    SemanticContext *ctx,
    Type **typed_success_payload_types,
    Type **typed_failure_payload_types,
    size_t *typed_success_scope_count_out);

#endif /* PGY_TYPE_CHECKER_INTENT_STEP_SEQUENCE_INTERNAL_H */
