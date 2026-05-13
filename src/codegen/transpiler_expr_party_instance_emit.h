#ifndef PGY_TRANSPILER_EXPR_PARTY_INSTANCE_EMIT_H
#define PGY_TRANSPILER_EXPR_PARTY_INSTANCE_EMIT_H

static char *
emit_party_instance_expr(ASTNode *node, TranspilerCtx *ctx)
{
    CodeBuf *assignments = codebuf_create();

    for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
        char *value = emit_expression(
            node->data.party_instance.assignments[i].value, ctx);
        if (i > 0)
            codebuf_write(assignments, ", ");
        codebuf_write(assignments, ".%s = %s",
                      node->data.party_instance.assignments[i].slot_name,
                      value);
        free(value);
    }

    char *result = strdup_fmt("(%s){%s}",
                              node->data.party_instance.party_type,
                              assignments->data);
    codebuf_destroy(assignments);
    return result;
}

#endif /* PGY_TRANSPILER_EXPR_PARTY_INSTANCE_EMIT_H */
