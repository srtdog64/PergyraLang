static void
append_overlay_method_projection_invalidations(CodeBuf *buf,
                                               TranspilerCtx *ctx,
                                               const char *source_slot_name,
                                               const char *host_type_name,
                                               ASTNode *node,
                                               int depth)
{
    if (buf == NULL || ctx == NULL || source_slot_name == NULL
        || host_type_name == NULL || node == NULL || depth > 8) {
        return;
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.block.statements[i], depth + 1);
        }
        break;
    case AST_IF_STMT:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.if_stmt.then_branch, depth + 1);
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.if_stmt.else_branch, depth + 1);
        break;
    case AST_FOR_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.for_loop.body, depth + 1);
        break;
    case AST_WHILE_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.while_loop.body, depth + 1);
        break;
    case AST_MATCH_STMT:
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.match_stmt.cases[i], depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.match_stmt.default_body, depth + 1);
        break;
    case AST_MATCH_CASE:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.match_case.body, depth + 1);
        break;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.select_stmt.cases[i], depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.select_stmt.default_case, depth + 1);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.async_block.statements[i], depth + 1);
        }
        break;
    case AST_ASSIGNMENT: {
        const char *field_name = method_assignment_projection_field_name(
            ctx, host_type_name, node->data.assignment.target);
        if (field_name != NULL) {
            char *invalidation = emit_current_overlay_projection_invalidation(
                ctx, source_slot_name, field_name);
            if (invalidation != NULL) {
                codebuf_write(buf, "%s", invalidation);
                free(invalidation);
            }
        }
        break;
    }
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_MEMBER_ACCESS
            && node->data.call.callee->data.member.object != NULL
            && node->data.call.callee->data.member.object->type == AST_IDENTIFIER
            && node->data.call.callee->data.member.object->data.identifier.name != NULL
            && node->data.call.callee->data.member.name != NULL) {
            ASTNode *host_decl = find_class_decl(ctx, host_type_name);
            ClassField *field = NULL;

            if (host_decl != NULL && host_decl->type == AST_CLASS_DECL) {
                field = find_host_field_by_name_local(host_decl,
                    node->data.call.callee->data.member.object->data.identifier.name);
            }
            if (field != NULL && field->is_vessel_field
                && field->type != NULL
                && field->type->type == AST_TYPE
                && field->type->data.type.name != NULL) {
                ASTNode *method_decl = find_nominal_host_method_decl(
                    ctx, field->type->data.type.name,
                    node->data.call.callee->data.member.name);
                if (method_decl != NULL) {
                    append_overlay_method_projection_invalidations(
                        buf, ctx, source_slot_name,
                        field->type->data.type.name,
                        method_decl->data.func_decl.body, depth + 1);
                }
            }
        }
        break;
    default:
        break;
    }
}

static char *
emit_current_overlay_method_projection_invalidation(TranspilerCtx *ctx,
                                                    const char *source_slot_name,
                                                    const char *host_type_name,
                                                    ASTNode *method_decl)
{
    CodeBuf *buf;

    if (ctx == NULL || source_slot_name == NULL || host_type_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    buf = codebuf_create();
    append_overlay_method_projection_invalidations(
        buf, ctx, source_slot_name, host_type_name,
        method_decl->data.func_decl.body, 0);

    if (buf->len == 0) {
        codebuf_destroy(buf);
        return NULL;
    }

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}
