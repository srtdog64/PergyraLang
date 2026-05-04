#include "parser_internal.h"

bool
parser_append_expr_node_with_capacity(Parser *parser,
                                      ASTNode ***items,
                                      size_t *count,
                                      size_t *capacity,
                                      ASTNode *item)
{
    ASTNode **grown;
    size_t next_capacity;

    if (items == NULL || count == NULL || capacity == NULL)
        return false;
    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        grown = realloc(*items, next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while growing expression node list");
            return false;
        }
        *items = grown;
        *capacity = next_capacity;
    }

    (*items)[*count] = item;
    (*count)++;
    return true;
}
