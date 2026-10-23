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
    bool uses_listed;
} FunctionParamFlowLoanSpan;
FunctionParamFlowLoanSpan function_param_flow_loan_span(SemanticContext *ctx, FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, const uint32_t *carriers, size_t carrier_count, const ASTNode *body);
void function_param_flow_loan_span_release(FunctionParamFlowLoanSpan *span);
/* `claim` runs, each time the routine runs, before the loan is made, after
 * its scope ended or after its last read; `body` is the routine's body. */
bool function_param_flow_claim_outside_loan(FunctionParamFlowIndexClaims *claims, size_t claim,
    const FunctionParamFlowLoanSpan *span, const ASTNode *body);
/* The loan owner proof (function_param_flow_loan_owner_proof.c): the
 * storage root of a place through local aliases, and whether `caller`
 * changes `binding` while a loan of it, or one it carries, lives. */
bool function_param_flow_loan_storage_root(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, const ASTNode *place, const ASTNode **root);
bool function_param_flow_loan_owner_unchanged(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, uint32_t binding, bool lends_on,
    bool own_descriptor, bool carrier, const FunctionParamFlowLoanSpan *span);
/* `claim` is a match case pattern or a part of one: it binds parts of the
 * subject and evaluates nothing. */
bool function_param_flow_claim_in_match_pattern(FunctionParamFlowIndexClaims *claims, size_t claim);

#endif
