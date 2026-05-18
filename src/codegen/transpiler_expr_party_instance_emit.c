/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend party instance expression lowering.
 */

#include "transpiler_expr_party_instance_emit.h"

#include <stdlib.h>

#include "../parser/ast_api.h"
#include "transpiler_format.h"

char *
emit_party_instance_expr(ASTNode *node, TranspilerCtx *ctx)
{
    CodeBuf *assignments = codebuf_create();

    for (size_t i = 0; i < ast_party_instance_assignment_count(node); i++) {
        char *value = emit_expression(
            ast_party_instance_assignment_value(node, i), ctx);
        if (i > 0)
            codebuf_write(assignments, ", ");
        codebuf_write(assignments, ".%s = %s",
                      ast_party_instance_assignment_slot_name(node, i),
                      value);
        free(value);
    }

    {
        char *result = strdup_fmt("(%s){%s}",
                                  ast_party_instance_party_type(node),
                                  assignments->data);
        codebuf_destroy(assignments);
        return result;
    }
}
