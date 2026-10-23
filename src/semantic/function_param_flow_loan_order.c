/* The order of a lent argument's loan in its routine: the claims that run
 * before the loan is made, after the statement that ends it, or after its
 * last read, from the program-point claims' parent rows. */
#include "function_param_flow_loan_order.h"
#include <stdint.h>
#include <stdlib.h>

/* A node of one of these runs once each time its parent runs, in source
 * order. Any other (a loop, a deferred, concurrent or intent body, a
 * closure) may run its code later than code after it. */
static bool claim_order_straight(ASTNodeType shape)
{
    switch (shape) {
    case AST_BLOCK: case AST_IF_STMT: case AST_MATCH_STMT: case AST_MATCH_CASE: case AST_WITH_STMT:
    case AST_UNSAFE_BLOCK: case AST_LET_DECL: case AST_LET_DESTRUCTURE: case AST_ASSIGNMENT: case AST_CALL:
    case AST_RETURN: case AST_BINARY: case AST_UNARY: case AST_CAST: case AST_TYPE_TEST: case AST_MEMBER_ACCESS:
    case AST_ARRAY_ACCESS: case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL: case AST_MAP_LITERAL:
    case AST_SET_LITERAL: case AST_IDENTIFIER: case AST_NUMBER: case AST_STRING: case AST_BOOLEAN:
        return true;
    default:
        return false;
    }
}

/* Each row's parent row, built once per claims generation. */
static bool claim_order_parents(FunctionParamFlowIndexClaims *claims)
{
    if (claims->order_parent != NULL && claims->order_generation == claims->generation) return true;
    size_t *parent = realloc(claims->order_parent, (claims->count == 0 ? 1 : claims->count) * sizeof(*parent));
    if (parent == NULL) return false;
    for (size_t r = 0; r < claims->count; r++) parent[r] = SIZE_MAX;
    for (size_t r = 0; r < claims->count; r++)
        for (size_t c = 0; c < claims->rows[r].child_count; c++) {
            size_t child = claims->rows[r].child_claim_ordinals[c];
            if (child < claims->count) parent[child] = r;
        }
    claims->order_parent = parent;
    claims->order_generation = claims->generation;
    claims->order_site = NULL;
    return true;
}

static size_t claim_row(FunctionParamFlowIndexClaims *claims, const ASTNode *site)
{
    if (claims->order_site != site) {
        claims->order_site_row = SIZE_MAX;
        for (size_t r = 0; r < claims->count; r++) if (claims->rows[r].node == site) claims->order_site_row = r;
        claims->order_site = site;
    }
    return claims->order_site_row;
}

/* An assignment starts at its target, whose root identifier carries the
 * position the parser leaves off the assignment. A node with no source
 * position, as a desugaring makes, is not ordered. */
static const ASTNode *claim_position(const ASTNode *node)
{
    while (node != NULL && node->line == 0 && (node->type == AST_ASSIGNMENT || node->type == AST_MEMBER_ACCESS ||
           node->type == AST_ARRAY_ACCESS))
        node = node->type == AST_ASSIGNMENT ? ast_assignment_target(node) :
            node->type == AST_MEMBER_ACCESS ? ast_member_object(node) : ast_array_access_array(node);
    return node == NULL || node->line == 0 ? NULL : node;
}

static bool claim_starts_before(const ASTNode *first, const ASTNode *second)
{
    first = claim_position(first);
    second = claim_position(second);
    return first != NULL && second != NULL &&
        (first->line < second->line || (first->line == second->line && first->column < second->column));
}

/* `row` is `scope` or a part of it. */
static bool claim_within(const FunctionParamFlowIndexClaims *claims, size_t row, size_t scope)
{
    for (size_t at = row, steps = 0; at != SIZE_MAX && steps <= claims->count; at = claims->order_parent[at], steps++)
        if (at == scope) return true;
    return false;
}

bool function_param_flow_claim_in_match_pattern(FunctionParamFlowIndexClaims *claims, size_t claim)
{
    if (claims == NULL || claims->failed || claim >= claims->count || !claim_order_parents(claims)) return false;
    /* The nearest case above the claim decides: only its patterns evaluate
     * nothing, its guard and body run. */
    for (size_t at = claim, steps = 0; steps <= claims->count; steps++) {
        size_t parent = claims->order_parent[at];
        if (parent == SIZE_MAX) return false;
        const ASTNode *arm = claims->rows[parent].node;
        if (claims->rows[parent].shape == AST_MATCH_CASE && arm != NULL && arm->type == AST_MATCH_CASE) {
            const ASTNode *node = claims->rows[at].node;
            if (node == ast_match_case_pattern(arm)) return true;
            for (size_t p = 0; p < ast_match_case_pattern_count(arm); p++)
                if (node == ast_match_case_pattern_at(arm, p)) return true;
            return false;
        }
        at = parent;
    }
    return false;
}

const ASTNode *function_param_flow_claim_match_subject(FunctionParamFlowIndexClaims *claims, size_t claim)
{
    if (!function_param_flow_claim_in_match_pattern(claims, claim)) return NULL;
    for (size_t at = claim, steps = 0; at != SIZE_MAX && steps <= claims->count; at = claims->order_parent[at], steps++)
        if (claims->rows[at].shape == AST_MATCH_STMT && claims->rows[at].node != NULL &&
            claims->rows[at].node->type == AST_MATCH_STMT) return ast_match_subject(claims->rows[at].node);
    return NULL;
}

/* Only blocks, branches, `with` scopes and plain statements or expressions
 * enclose `row` up to the routine's body, never a loop, a deferred or
 * concurrent body or a closure: `row` runs once, in source order, each time
 * the routine runs. The chain must reach the body: a node no claim records
 * could be a loop the chain would miss. */
static bool claim_runs_once(const FunctionParamFlowIndexClaims *claims, size_t row, const ASTNode *body)
{
    for (size_t at = claims->order_parent[row], steps = 0; at != SIZE_MAX; at = claims->order_parent[at], steps++) {
        if (claims->rows[at].node == body) return true;
        if (steps > claims->count || !claim_order_straight(claims->rows[at].shape)) return false;
    }
    return false;
}

/* A row of these runs where it is written, a `while` body once per pass; a
 * type runs nothing. No deferred, concurrent or closure body is one. */
static bool claim_shape_in_place(ASTNodeType shape)
{
    return claim_order_straight(shape) || shape == AST_TYPE || shape == AST_WHILE_LOOP ||
        shape == AST_BREAK || shape == AST_CONTINUE;
}

/* A statement that ends a loan made in it: it runs once, holds the lending
 * call and the `let` of every local the loan reached, and holds only rows
 * that run in place, so no deferred, concurrent or closure body keeps one
 * of those locals past its end. */
static bool claim_loan_scope(const FunctionParamFlowIndexClaims *claims, size_t scope,
    const uint32_t *carriers, size_t carrier_count, const ASTNode *body)
{
    size_t parent = claims->order_parent[scope];
    if (parent == SIZE_MAX || claims->rows[parent].shape != AST_BLOCK || !claim_runs_once(claims, scope, body))
        return false;
    for (size_t k = 0; k < carrier_count; k++) {
        bool inside = false;
        for (size_t r = 0; r < claims->count && !inside; r++)
            inside = r != scope && claims->rows[r].id == carriers[k] && claims->rows[r].shape == AST_LET_DECL &&
                claim_within(claims, r, scope);
        if (!inside) return false;
    }
    for (size_t r = 0; r < claims->count; r++)
        if (claim_within(claims, r, scope) && !claim_shape_in_place(claims->rows[r].shape)) return false;
    return true;
}

bool function_param_flow_claims_in_place(const FunctionParamFlowIndexClaims *claims)
{
    if (claims == NULL || claims->failed) return false;
    for (size_t r = 0; r < claims->count; r++)
        if (!claim_shape_in_place(claims->rows[r].shape)) return false;
    return true;
}

size_t function_param_flow_claim_parent_row(FunctionParamFlowIndexClaims *claims, size_t row)
{
    if (claims == NULL || claims->failed || row >= claims->count || !claim_order_parents(claims)) return SIZE_MAX;
    return claims->order_parent[row];
}

FunctionParamFlowLoanSpan function_param_flow_loan_span(SemanticContext *ctx, FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, const uint32_t *carriers, size_t carrier_count, const ASTNode *body)
{
    FunctionParamFlowLoanSpan span = {call, SIZE_MAX, SIZE_MAX, SIZE_MAX, NULL, 0, false};
    if (ctx == NULL || claims == NULL || claims->failed || call == NULL || body == NULL ||
        !claim_order_parents(claims)) return span;
    span.generation = claims->generation;
    span.call_row = claim_row(claims, call);
    for (size_t at = span.call_row, steps = 0; at != SIZE_MAX && steps <= claims->count;
         at = claims->order_parent[at], steps++) {
        if (claims->rows[at].node == body) break;
        if (claim_loan_scope(claims, at, carriers, carrier_count, body)) { span.scope = at; break; }
    }
    /* Every read of a local the loan reached, listed only when every row of
     * the routine runs in place: a closure or deferred body could read one
     * after the code written after it. */
    bool listed = span.call_row != SIZE_MAX && function_param_flow_claims_in_place(claims);
    for (size_t r = 0; listed && r < claims->count; r++) {
        uint32_t binding = claims->rows[r].shape == AST_IDENTIFIER ?
            semantic_param_publication_binding(ctx, claims->rows[r].node) : 0;
        bool carrier = false;
        for (size_t k = 0; binding != 0 && k < carrier_count && !carrier; k++) carrier = binding == carriers[k];
        if (!carrier) continue;
        size_t *grown = realloc(span.uses, (span.use_count + 1) * sizeof(*grown));
        if (grown == NULL) { listed = false; break; }
        span.uses = grown;
        span.uses[span.use_count++] = r;
    }
    span.uses_listed = listed;
    return span;
}

void function_param_flow_loan_span_release(FunctionParamFlowLoanSpan *span)
{
    if (span == NULL) return;
    free(span->uses);
    span->uses = NULL;
    span->use_count = 0;
    span->uses_listed = false;
}

/* The statement holding `row`: the value an expression yields lives until
 * its statement ends. */
static size_t claim_statement(const FunctionParamFlowIndexClaims *claims, size_t row)
{
    for (size_t at = row, steps = 0; at != SIZE_MAX && steps <= claims->count; at = claims->order_parent[at], steps++) {
        size_t parent = claims->order_parent[at];
        if (parent != SIZE_MAX && claims->rows[parent].shape == AST_BLOCK) return at;
    }
    return SIZE_MAX;
}

/* The statement holding `row` has finished when `claim` starts and does not
 * run again after it: it holds no part of the claim, `row` starts first, and
 * the two meet in a statement that runs once. */
static bool claim_statement_finished_before(const FunctionParamFlowIndexClaims *claims, size_t row, size_t claim,
    const ASTNode *body)
{
    size_t at = claim_statement(claims, row);
    if (at == SIZE_MAX || claim_within(claims, claim, at) ||
        !claim_starts_before(claims->rows[row].node, claims->rows[claim].node)) return false;
    for (size_t steps = 0; at != SIZE_MAX && !claim_within(claims, claim, at) && steps <= claims->count; steps++)
        at = claims->order_parent[at];
    return at != SIZE_MAX && (claims->rows[at].node == body ||
        (claim_order_straight(claims->rows[at].shape) && claim_runs_once(claims, at, body)));
}

/* `claim` starts after the statement that made the loan and after the
 * statement of every read of a local the loan reached, each finished and
 * not run again: when the claim changes the owner, nothing reads the loan. */
static bool claim_after_last_use(const FunctionParamFlowIndexClaims *claims, size_t claim,
    const FunctionParamFlowLoanSpan *span, const ASTNode *body)
{
    if (!span->uses_listed || span->call_row >= claims->count ||
        !claim_statement_finished_before(claims, span->call_row, claim, body)) return false;
    for (size_t u = 0; u < span->use_count; u++)
        if (!claim_statement_finished_before(claims, span->uses[u], claim, body)) return false;
    return true;
}

/* A claim that runs once and starts before the lending call, holding no part
 * of it, has finished when the loan is made and does not run again. One
 * that starts after the statement that ends the loan, outside it, runs
 * after that statement finished, when no local the loan reached is in
 * scope; one after the loan's last read runs when nothing reads it. */
bool function_param_flow_claim_outside_loan(FunctionParamFlowIndexClaims *claims, size_t claim,
    const FunctionParamFlowLoanSpan *span, const ASTNode *body)
{
    if (claims == NULL || claims->failed || span == NULL || span->call == NULL || body == NULL ||
        claim >= claims->count || span->generation != claims->generation || !claim_order_parents(claims)) return false;
    if (span->scope < claims->count && claim_starts_before(claims->rows[span->scope].node, claims->rows[claim].node) &&
        !claim_within(claims, claim, span->scope)) return true;
    if (claim_after_last_use(claims, claim, span, body)) return true;
    size_t site = claim_row(claims, span->call);
    return site != SIZE_MAX && claim_starts_before(claims->rows[claim].node, span->call) &&
        claim_runs_once(claims, claim, body) && !claim_within(claims, site, claim);
}
