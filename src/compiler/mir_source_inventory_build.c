#include "mir_source_inventory_build.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mir_base_helpers.h"

bool
mir_copy_ast_nodes(ASTNode ***dst, size_t *dst_count, ASTNode **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(ASTNode *));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(ASTNode *));
    *dst_count = src_count;
    return true;
}

bool
mir_copy_names(const char ***dst, size_t *dst_count, const char **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(const char *));
    if (*dst == NULL)
        return false;
    memcpy((void *)*dst, src, src_count * sizeof(const char *));
    *dst_count = src_count;
    return true;
}

bool
mir_copy_phi_nodes(MIRSourcePhiNode **dst, size_t *dst_count,
                   const HIRPhiNode *src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(MIRSourcePhiNode));
    if (*dst == NULL)
        return false;
    *dst_count = src_count;
    for (size_t i = 0; i < src_count; i++) {
        (*dst)[i].name = src[i].name;
        if (!copy_indices(&(*dst)[i].incoming_predecessors,
                          &(*dst)[i].incoming_predecessor_count,
                          src[i].incoming_predecessors,
                          src[i].incoming_predecessor_count)) {
            for (size_t j = 0; j < i; j++)
                free((*dst)[j].incoming_predecessors);
            free(*dst);
            *dst = NULL;
            *dst_count = 0;
            return false;
        }
    }
    return true;
}

void
mir_block_record_source_location(MIRBasicBlock *block, const ASTNode *source_node)
{
    if (block == NULL)
        return;
    block->has_source_location = source_node != NULL;
    block->source_line = source_node != NULL ? source_node->line : 0;
    block->source_column = source_node != NULL ? source_node->column : 0;
}

bool
mir_seed_non_cfg_block_source_inventory(MIRBasicBlock *block,
                                        ASTNode *func_decl)
{
    ASTNode *body;

    if (block == NULL || func_decl == NULL
        || func_decl->type != AST_FUNC_DECL) {
        return true;
    }
    body = ast_func_body(func_decl);
    if (body == NULL)
        return true;
    if (body->type == AST_BLOCK) {
        size_t statement_count = 0;
        ASTNode **statements = ast_block_statements(body, &statement_count);
        const ASTNode *source_node = statement_count > 0
            ? statements[0]
            : body;
        mir_block_record_source_location(block, source_node);
        return mir_copy_ast_nodes(&block->source_statement_inventory.items,
                                  &block->source_statement_inventory.count,
                                  statements,
                                  statement_count);
    }
    {
        ASTNode *single_statement = body;
        mir_block_record_source_location(block, body);
        return mir_copy_ast_nodes(&block->source_statement_inventory.items,
                                  &block->source_statement_inventory.count,
                                  &single_statement,
                                  1);
    }
}
