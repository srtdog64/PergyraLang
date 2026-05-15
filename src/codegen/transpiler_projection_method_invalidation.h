#ifndef PGY_TRANSPILER_PROJECTION_METHOD_INVALIDATION_H
#define PGY_TRANSPILER_PROJECTION_METHOD_INVALIDATION_H

#include "../parser/ast_api.h"

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
            ast_if_then_branch(node), depth + 1);
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            ast_if_else_branch(node), depth + 1);
        break;
    case AST_FOR_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            ast_for_body(node), depth + 1);
        break;
    case AST_WHILE_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            ast_while_body(node), depth + 1);
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
        for (size_t i = 0; i < ast_select_case_count(node); i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                ast_select_case(node, i), depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            ast_select_default_case(node), depth + 1);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                ast_async_block_statement(node, i), depth + 1);
        }
        break;
    case AST_ASSIGNMENT: {
        const char *field_name = method_assignment_projection_field_name(
            ctx, host_type_name, ast_assignment_target(node));
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
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_MEMBER_ACCESS
            && ast_member_object(ast_call_callee(node)) != NULL
            && ast_member_object(ast_call_callee(node))->type == AST_IDENTIFIER
            && ast_member_object(ast_call_callee(node))->data.identifier.name != NULL
            && ast_member_name(ast_call_callee(node)) != NULL) {
            ASTNode *host_decl = find_class_decl(ctx, host_type_name);
            ClassField *field = NULL;

            if (host_decl != NULL && host_decl->type == AST_CLASS_DECL) {
                field = find_host_field_by_name_local(host_decl,
                    ast_member_object(ast_call_callee(node))->data.identifier.name);
            }
            if (field != NULL && field->is_vessel_field
                && field->type != NULL
                && field->type->type == AST_TYPE
                && ast_type_name(field->type) != NULL) {
                ASTNode *method_decl = find_nominal_host_method_decl(
                    ctx, ast_type_name(field->type),
                    ast_member_name(ast_call_callee(node)));
                if (method_decl != NULL) {
                    append_overlay_method_projection_invalidations(
                        buf, ctx, source_slot_name,
                        ast_type_name(field->type),
                        ast_func_body(method_decl), depth + 1);
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

    ASTNode *body = ast_func_body(method_decl);
    if (ctx == NULL || source_slot_name == NULL || host_type_name == NULL
        || body == NULL) {
        return NULL;
    }

    buf = codebuf_create();
    append_overlay_method_projection_invalidations(
        buf, ctx, source_slot_name, host_type_name,
        body, 0);

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
#endif /* PGY_TRANSPILER_PROJECTION_METHOD_INVALIDATION_H */
