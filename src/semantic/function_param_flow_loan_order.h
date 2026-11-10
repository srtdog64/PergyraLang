#ifndef PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_LOAN_ORDER_H
#define PERGYRA_SEMANTIC_FUNCTION_PARAM_FLOW_LOAN_ORDER_H

/* When a lent argument's loan lives, read from the program-point claims
 * (function_param_flow_claim_publication.c), and the proof that a loan's
 * owner stays unchanged while it does (function_param_flow_loan_owner_proof.c). */
#include "function_param_flow_summary_internal.h"

/* A lent argument's loan: the lending call and its row, the row of the
 * statement after which no local the loan reached is in scope (SIZE_MAX if
 * none), and the rows that read such a local, when `uses_listed` (freed by
 * function_param_flow_loan_span_release). Rows belong to the claims
 * generation `generation`. */
typedef struct {
    const ASTNode *call;
    size_t generation, call_row, scope;
    size_t *uses, use_count;
    const ASTNode *const *preserving_assignments;
    size_t preserving_assignment_count;
    bool uses_listed;
} FunctionParamFlowLoanSpan;
FunctionParamFlowLoanSpan function_param_flow_loan_span(SemanticContext *ctx, FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, const uint32_t *carriers, size_t carrier_count, const ASTNode *body);
void function_param_flow_loan_span_release(FunctionParamFlowLoanSpan *span);
/* `claim` runs, each time the routine runs, before the loan is made, after
 * its scope ended or after its last read; `body` is the routine's body. */
bool function_param_flow_claim_outside_loan(FunctionParamFlowIndexClaims *claims, size_t claim,
    const FunctionParamFlowLoanSpan *span, const ASTNode *body);
/* The exact local binding is read in a later, separately completed statement
 * after `call`.  This consumes the sealed program-point claims and their
 * existing order relation; it does not rescan the routine or infer from line
 * numbers alone.  Assignment-target-only occurrences are not reads. */
bool function_param_flow_binding_read_after_call(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, const ASTNode *call,
    uint32_t binding, const ASTNode *body);
/* Currency-bearing form of the same query.  A true return means the sealed
 * order proof was complete; `*read_after` then owns the yes/no verdict. */
bool function_param_flow_binding_read_after_call_current(
    SemanticContext *ctx, FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, uint32_t binding, const ASTNode *body,
    bool *read_after);
/* The statement containing `first` is guaranteed to finish before the
 * statement containing `second` begins in the same routine body.  This is
 * the sealed claim-order relation used by loan consumers; it is not a source
 * line comparison and fails closed across delayed or concurrent bodies. */
bool function_param_flow_claim_completed_before(
    FunctionParamFlowIndexClaims *claims, const ASTNode *first,
    const ASTNode *second, const ASTNode *body);
/* The exact local has no reachable executable use before `call`, and at
 * least one occurrence of the binding is an argument inside that call.  A
 * consumer that has already validated the local declaration may therefore
 * treat its initializer as the reaching definition at this call. */
bool function_param_flow_binding_still_initializer_at_call(
    SemanticContext *ctx, FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, uint32_t binding, const ASTNode *body);
/* The loan owner proof (function_param_flow_loan_owner_proof.c): the
 * storage root of a place through local aliases, and whether `caller`
 * changes `binding` while a loan of it, or one it carries, lives. */
bool function_param_flow_loan_storage_root(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, const ASTNode *place, const ASTNode **root);
bool function_param_flow_loan_owner_unchanged(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, uint32_t binding,
    const ASTNode *owner_place, const FunctionParamFlowResultPath *owner_path,
    bool lends_on, bool own_descriptor,
    bool carrier, const FunctionParamFlowLoanSpan *span);
/* Every row of `claims` runs where it is written: plain code, types and
 * `while` loops, no deferred, concurrent or closure body. */
bool function_param_flow_claims_in_place(const FunctionParamFlowIndexClaims *claims);
/* The row that holds `row`, or SIZE_MAX. */
size_t function_param_flow_claim_parent_row(FunctionParamFlowIndexClaims *claims, size_t row);
/* `claim` is a match case pattern or a part of one: it binds parts of the
 * subject and evaluates nothing. */
bool function_param_flow_claim_in_match_pattern(FunctionParamFlowIndexClaims *claims, size_t claim);
/* The subject of the match whose case pattern holds `claim`, or NULL. */
const ASTNode *function_param_flow_claim_match_subject(FunctionParamFlowIndexClaims *claims, size_t claim);

#endif
