#include "region_escape_fact.h"

#include <stdlib.h>

#include "../lexer/lexer.h" /* TOKEN_PLUS */
#include "../parser/ast.h"
#include "../parser/ast_api.h"
#include "builtin_kind.h"
#include "region_retention_summary.h"

typedef struct {
    PgyRegionEscapeFact *facts;
    size_t count;
    size_t capacity;
    uint32_t next_scope;
    PgyRegionRetentionSummaryLookup retention_lookup;
    void *retention_userdata;
    bool failed;
} RegionEscapeCollector;

static bool
region_escape_is_string_concat(const ASTNode *expr)
{
    const ASTNode *left;
    const ASTNode *right;

    if (expr == NULL || expr->type != AST_BINARY
        || expr->data.binary.op.type != TOKEN_PLUS) {
        return false;
    }
    left = expr->data.binary.left;
    right = expr->data.binary.right;
    if ((left != NULL && left->type == AST_STRING)
        || (right != NULL && right->type == AST_STRING)) {
        return true;
    }
    return region_escape_is_string_concat(left);
}

static bool
region_escape_argument_is_borrowed(
    const ASTNode *call,
    size_t argument_index,
    PgyRegionRetentionSummaryLookup retention_lookup,
    void *retention_userdata)
{
    uint32_t builtin_kind = 0;
    PgyRegionRetentionKind retention = PGY_REGION_RETENTION_UNKNOWN;

    if (call == NULL || call->type != AST_CALL)
        return false;
    if (ast_call_semantic_callee_builtin_kind(call, &builtin_kind)
        && builtin_kind != (uint32_t)BUILTIN_NOT_BUILTIN)
        return semantic_region_retention_summary_for_builtin(
                   builtin_kind, argument_index, &retention)
            && retention == PGY_REGION_RETENTION_BORROWED_FOR_CALL;
    return retention_lookup != NULL
        && retention_lookup(call, argument_index, &retention,
                            retention_userdata)
        && retention == PGY_REGION_RETENTION_BORROWED_FOR_CALL;
}

static bool
region_escape_append(RegionEscapeCollector *collector,
                     const ASTNode *site,
                     uint32_t scope_id,
                     uint32_t function_syntax_id)
{
    PgyRegionEscapeFact *grown;
    size_t next_capacity;

    if (collector == NULL || site == NULL) {
        return false;
    }
    if (ast_node_stable_id(site) == 0) {
        collector->failed = true;
        return false;
    }
    if (collector->count == collector->capacity) {
        next_capacity = collector->capacity == 0
            ? 8 : collector->capacity * 2;
        if (next_capacity < collector->capacity
            || next_capacity > SIZE_MAX / sizeof(*grown)) {
            collector->failed = true;
            return false;
        }
        grown = realloc(collector->facts,
                        next_capacity * sizeof(*collector->facts));
        if (grown == NULL) {
            collector->failed = true;
            return false;
        }
        collector->facts = grown;
        collector->capacity = next_capacity;
    }
    collector->facts[collector->count].allocation_site_id =
        ast_node_stable_id(site);
    collector->facts[collector->count].scope_id = scope_id;
    collector->facts[collector->count].function_syntax_id =
        function_syntax_id;
    collector->count++;
    return true;
}

static void
region_escape_append_concat_spine(RegionEscapeCollector *collector,
                                  const ASTNode *expr,
                                  uint32_t scope_id,
                                  uint32_t function_syntax_id)
{
    while (!collector->failed && region_escape_is_string_concat(expr)) {
        if (!region_escape_append(collector, expr, scope_id,
                                  function_syntax_id)) {
            return;
        }
        expr = expr->data.binary.left;
    }
}

static void
region_escape_walk(RegionEscapeCollector *collector,
                   const ASTNode *node,
                   uint32_t scope_id,
                   uint32_t function_syntax_id)
{
    if (collector == NULL || collector->failed || node == NULL)
        return;

    switch (node->type) {
    case AST_PROGRAM:
        for (size_t i = 0; i < node->data.program.count; i++)
            region_escape_walk(collector, node->data.program.statements[i],
                               scope_id, function_syntax_id);
        break;
    case AST_FUNC_DECL: {
        uint32_t function_scope = ++collector->next_scope;
        uint32_t function_id = ast_node_stable_id(node);
        region_escape_walk(collector, node->data.func_decl.body,
                           function_scope, function_id);
        break;
    }
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            region_escape_walk(collector, node->data.block.statements[i],
                               scope_id, function_syntax_id);
        break;
    case AST_CALL:
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            if (region_escape_argument_is_borrowed(
                    node, i, collector->retention_lookup,
                    collector->retention_userdata)) {
                const ASTNode *arg = node->data.call.arguments[i];
                if (region_escape_is_string_concat(arg))
                    region_escape_append_concat_spine(
                        collector, arg, scope_id, function_syntax_id);
            }
        }
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            region_escape_walk(collector, node->data.call.arguments[i],
                               scope_id, function_syntax_id);
        break;
    default:
        break;
    }
}

bool
semantic_region_escape_collect(const struct ASTNode *root,
                               PgyRegionRetentionSummaryLookup retention_lookup,
                               void *retention_userdata,
                               PgyRegionEscapeFact **facts_out,
                               size_t *count_out)
{
    RegionEscapeCollector collector = {
        .retention_lookup = retention_lookup,
        .retention_userdata = retention_userdata
    };

    if (facts_out != NULL)
        *facts_out = NULL;
    if (count_out != NULL)
        *count_out = 0;
    if (root == NULL)
        return false;

    region_escape_walk(&collector, root, 0, 0);
    if (collector.failed) {
        free(collector.facts);
        return false;
    }
    if (facts_out != NULL)
        *facts_out = collector.facts;
    else
        free(collector.facts);
    if (count_out != NULL)
        *count_out = collector.count;
    return true;
}

void
semantic_region_escape_facts_free(PgyRegionEscapeFact *facts)
{
    free(facts);
}
