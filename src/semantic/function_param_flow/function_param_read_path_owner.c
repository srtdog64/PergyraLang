/* Formal read paths, from each callee's program-point claims.
 *
 * A routine reads of its formal every access path the formal appears at: an
 * identifier, a member chain on it, or the Array an element access names. A
 * path handed to a declared callee as an argument is read only as far as
 * that callee reads its own formal, followed to a fixed depth; a member path
 * a method is called on reads its whole receiver. Anything the claims cannot
 * name reads everything. */

#include "function_param_read_path_owner.h"
#include "function_param_flow_loan_order.h"
#include "../default_aggregate_move_internal.h"
#include "function_param_publication_owner.h"

#include <string.h>

enum { READ_PATH_DEPTH = 8 };

static bool paths_overlap(const char *a, const char *b)
{
    return move_walk_path_contains(a, b) || move_walk_path_contains(b, a);
}

static bool formal_reads(SemanticContext *ctx, ASTNode *callee, size_t formal, const char *rest, size_t depth);

/* The read at `at`, a path of the formal, reaches storage at or below `rest`. */
static bool path_reads(SemanticContext *ctx, FunctionParamFlowIndexClaims *claims, size_t at, const char *rest,
    size_t depth)
{
    const ASTNode *node = claims->rows[at].node;
    size_t parent = function_param_flow_claim_parent_row(claims, at);
    ASTNode *call = parent != SIZE_MAX ? claims->rows[parent].node : NULL;
    if (call != NULL && call->type == AST_CALL && node == ast_call_callee(call) && node->type == AST_MEMBER_ACCESS)
        node = ast_member_object(node);
    char path[512];
    uint32_t binding = 0;
    if (!move_walk_access_path(node, &binding, path, sizeof(path))) return true;
    if (call == NULL || call->type != AST_CALL || node == ast_call_callee(call)) return paths_overlap(path, rest);
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        if (ast_call_argument(call, p) != node) continue;
        ASTNode *next = semantic_param_publication_callee(ctx, call, p);
        size_t next_formal = 0;
        if (next == NULL || !semantic_param_publication_formal_for_actual(ctx, call, p, &next_formal) ||
            !move_walk_path_contains(path, rest)) return paths_overlap(path, rest);
        return formal_reads(ctx, next, next_formal, rest + strlen(path), depth + 1);
    }
    return paths_overlap(path, rest);
}

static bool formal_reads(SemanticContext *ctx, ASTNode *callee, size_t formal, const char *rest, size_t depth)
{
    if (depth > READ_PATH_DEPTH || callee == NULL || formal >= ast_func_param_count(callee) ||
        ctx->function_param_publications == NULL) return true;
    FuncParam *param = ast_func_param(callee, formal);
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (param == NULL || store == NULL || store->failed ||
        !function_param_flow_program_point_index_build(store, callee, &index) ||
        !function_param_flow_claims_in_place(index->claims)) return true;
    FunctionParamFlowIndexClaims *claims = index->claims;
    uint32_t formal_id = ast_func_param_stable_id(param);
    for (size_t r = 0; r < claims->count; r++) {
        ASTNode *node = claims->rows[r].node;
        if (claims->rows[r].shape != AST_IDENTIFIER || node == NULL ||
            ast_identifier_binding_syntax_id(node) != formal_id) continue;
        /* Climb to the whole access path this appearance names. */
        size_t at = r;
        for (size_t steps = 0; steps <= claims->count; steps++) {
            size_t parent = function_param_flow_claim_parent_row(claims, at);
            ASTNode *up = parent != SIZE_MAX ? claims->rows[parent].node : NULL;
            if (up == NULL || !((up->type == AST_MEMBER_ACCESS && ast_member_object(up) == claims->rows[at].node) ||
                    (up->type == AST_ARRAY_ACCESS && ast_array_access_array(up) == claims->rows[at].node))) break;
            at = parent;
        }
        if (path_reads(ctx, claims, at, rest, depth)) return true;
    }
    return false;
}

bool function_param_read_paths_miss(SemanticContext *ctx, ASTNode *call, size_t argument, const char *rest)
{
    ASTNode *callee = ctx != NULL && call != NULL && call->type == AST_CALL && argument < ast_call_arg_count(call) ?
        semantic_param_publication_callee(ctx, call, argument) : NULL;
    size_t formal = 0;
    if (callee == NULL || rest == NULL || rest[0] != '.' ||
        !semantic_param_publication_formal_for_actual(ctx, call, argument, &formal)) return false;
    return !formal_reads(ctx, callee, formal, rest, 0);
}
