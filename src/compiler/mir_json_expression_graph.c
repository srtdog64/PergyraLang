#include "mir_json_expression_graph.h"
#include "mir_json_expression_graph_materialize.h"
#include "mir.h"
#include "mir_call_fact.h"
#include "mir_json_dump_internal.h"
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

static ASTNode *
mir_json_instruction_expression(const MIRInstruction *inst, int lane)
{
    ASTNode *expr;

    if (inst == NULL || lane < 0 || lane > 1)
        return NULL;
    if (lane == 0 && inst->kind == MIR_INST_BRANCH
        && inst->branch_shape == MIR_BRANCH_FOR_RANGE) {
        return inst->expr1;
    }
    if (lane == 0 && mir_instruction_source_is_defer_stmt(inst)) {
        if (inst->arg0 != NULL && strcmp(inst->arg0, "Log") == 0)
            return mir_defer_log_expression_fact(inst);
        if (inst->arg0 != NULL && strcmp(inst->arg0, "Call") == 0)
            return mir_defer_call_expression_fact(inst);
        return NULL;
    }
    if (lane == 1) {
        if ((inst->kind == MIR_INST_ASSIGN || inst->kind == MIR_INST_DEF)
            && mir_instruction_source_node_type_or(inst, AST_PROGRAM)
                == AST_ASSIGNMENT) {
            return inst->expr1;
        }
        return NULL;
    }
    expr = inst->expr0;
    if (inst->kind != MIR_INST_STMT || expr == NULL
        || expr->type != AST_CALL || inst->arg0 == NULL) {
        return expr;
    }
    if (strcmp(inst->arg0, "Log") == 0)
        return ast_call_arg_count(expr) > 0
            ? ast_call_argument(expr, 0)
            : NULL;
    if (strcmp(inst->arg0, "ArrayPush") == 0)
        return ast_call_arg_count(expr) > 1
            ? ast_call_argument(expr, 1)
            : NULL;
    if (strcmp(inst->arg0, "ArraySet") == 0)
        return ast_call_arg_count(expr) > 2
            ? ast_call_argument(expr, 2)
            : NULL;
    return expr;
}

static uint32_t
mir_expression_graph_hash_string(uint32_t hash, const char *value)
{
    const unsigned char *cursor =
        (const unsigned char *)(value != NULL ? value : "");
    size_t length = value != NULL ? strlen(value) : 0;

    hash = (uint32_t)(((uint64_t)hash * 131u + length) % 268435456u);
    while (*cursor != '\0') {
        hash = (uint32_t)(((uint64_t)hash * 131u + *cursor)
                          % 268435456u);
        cursor++;
    }
    return hash;
}

static uint32_t
mir_expression_graph_hash_int(uint32_t hash, int value)
{
    return (uint32_t)(((uint64_t)hash * 131u
                       + (uint32_t)(value + 2)) % 268435456u);
}

static uint32_t
mir_expression_graph_digest(const MIRJsonExpressionGraph *graph, int root)
{
    uint32_t hash = 71u;

    hash = mir_expression_graph_hash_int(hash, root);
    hash = mir_expression_graph_hash_int(
        hash, graph != NULL ? (int)graph->count : -1);
    if (graph != NULL) {
        for (size_t i = 0; i < graph->count; i++) {
            const MIRJsonExpressionGraphNode *node = &graph->nodes[i];
            hash = mir_expression_graph_hash_string(hash, node->kind);
            hash = mir_expression_graph_hash_string(hash, node->text);
            hash = mir_expression_graph_hash_string(
                hash, node->call_target_kind);
            hash = mir_expression_graph_hash_string(
                hash, node->call_target_name);
            hash = mir_expression_graph_hash_int(
                hash, (int)node->runtime_call_abi_id);
            hash = mir_expression_graph_hash_int(hash, node->left);
            hash = mir_expression_graph_hash_int(hash, node->right);
        }
    }
    return 1073741824u + hash;
}

bool
mir_expression_graph_identity(ASTNode *expression,
                              size_t *root_id_out,
                              uint32_t *digest_out)
{
    MIRJsonExpressionGraph graph = {0};
    int root = mir_json_expression_graph_build(&graph, expression);

    if (root_id_out != NULL)
        *root_id_out = SIZE_MAX;
    if (digest_out != NULL)
        *digest_out = 0;
    if (root < 0) {
        mir_json_expression_graph_dispose(&graph);
        return false;
    }
    if (root_id_out != NULL)
        *root_id_out = (size_t)root;
    if (digest_out != NULL)
        *digest_out = mir_expression_graph_digest(&graph, root);
    mir_json_expression_graph_dispose(&graph);
    return true;
}

void
mir_json_emit_instruction_expression_graph(FILE *out,
                                           const MIRInstruction *inst,
                                           int lane)
{
    MIRJsonExpressionGraph graph = {0};
    ASTNode *expr = mir_json_instruction_expression(inst, lane);
    int root;

    if (out == NULL)
        return;
    root = mir_json_expression_graph_build(&graph, expr);
    if (root < 0) {
        fputs("null", out);
        mir_json_expression_graph_dispose(&graph);
        return;
    }
    fprintf(out, "{\"root\":%d,\"digest\":%u,\"nodes\":[",
            root, mir_expression_graph_digest(&graph, root));
    for (size_t i = 0; i < graph.count; i++) {
        const MIRJsonExpressionGraphNode *node = &graph.nodes[i];
        if (i > 0)
            fputc(',', out);
        fputs("{\"kind\":", out);
        mir_json_emit_str(out, node->kind);
        fputs(",\"text\":", out);
        mir_json_emit_str(out, node->text);
        fputs(",\"call_target_kind\":", out);
        mir_json_emit_str(out, node->call_target_kind);
        fputs(",\"call_target_name\":", out);
        mir_json_emit_str(out, node->call_target_name);
        fprintf(out, ",\"runtime_call_abi_id\":%u",
                node->runtime_call_abi_id);
        fputs(",\"left\":", out);
        if (node->left < 0)
            fputs("null", out);
        else
            fprintf(out, "%d", node->left);
        fputs(",\"right\":", out);
        if (node->right < 0)
            fputs("null", out);
        else
            fprintf(out, "%d", node->right);
        fputc('}', out);
    }
    fputs("]}", out);
    mir_json_expression_graph_dispose(&graph);
}
