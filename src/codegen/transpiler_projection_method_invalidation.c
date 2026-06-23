/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend projection invalidation for hosted method calls.
 */

#include "transpiler_projection_method_invalidation.h"

#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_format.h"
#include "transpiler_overlay_projection.h"
#include "transpiler_projection_field_path.h"

static void
append_overlay_method_projection_invalidations_from_metadata(
    CodeBuf *buf,
    TranspilerCtx *ctx,
    const char *source_slot_name,
    const char *host_type_name,
    const MIRDeclMethod *method_meta,
    int depth)
{
    if (buf == NULL || ctx == NULL || source_slot_name == NULL
        || host_type_name == NULL || method_meta == NULL || depth > 8) {
        return;
    }

    for (size_t i = 0;
         i < transpiler_mir_decl_method_projection_write_count(method_meta);
         i++) {
        const char *field_name = method_projection_write_field_name(
            ctx,
            host_type_name,
            transpiler_mir_decl_method_projection_write_root_name(
                method_meta, i),
            transpiler_mir_decl_method_projection_write_member_name(
                method_meta, i));
        if (field_name != NULL) {
            char *invalidation = emit_current_overlay_projection_invalidation(
                ctx, source_slot_name, field_name);
            if (invalidation != NULL) {
                codebuf_write(buf, "%s", invalidation);
                free(invalidation);
            }
        }
    }

    for (size_t i = 0;
         i < transpiler_mir_decl_method_projection_call_count(method_meta);
         i++) {
        const char *receiver_name =
            transpiler_mir_decl_method_projection_call_receiver_name(
                method_meta, i);
        const char *method_name =
            transpiler_mir_decl_method_projection_call_method_name(
                method_meta, i);
        const char *field_type_name =
            host_projection_subject_field_type_name(
                ctx, host_type_name, receiver_name);
        const MIRDeclMethod *nested_meta = field_type_name != NULL
            ? transpiler_find_host_method_metadata_in_context(
                ctx, field_type_name, method_name)
            : NULL;

        if (field_type_name != NULL && nested_meta == NULL
            && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing projection invalidation method metadata for '%s.%s'",
                field_type_name,
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }
        if (nested_meta != NULL) {
            append_overlay_method_projection_invalidations_from_metadata(
                buf, ctx, source_slot_name, field_type_name,
                nested_meta, depth + 1);
        }
    }
}

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
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                ast_block_statement(node, i), depth + 1);
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
        for (size_t i = 0; i < ast_match_case_count(node); i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                ast_match_case_at(node, i), depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            ast_match_default_body(node), depth + 1);
        break;
    case AST_MATCH_CASE:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            ast_match_case_body(node), depth + 1);
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
            && ast_identifier_name(ast_member_object(ast_call_callee(node))) != NULL
            && ast_member_name(ast_call_callee(node)) != NULL) {
            ASTNode *host_decl =
                transpiler_find_projection_nominal_decl_local(
                    ctx, host_type_name);
            const char *field_type_name = NULL;

            if (host_decl != NULL && host_decl->type == AST_CLASS_DECL) {
                field_type_name = host_projection_subject_field_type_name(
                    ctx, host_type_name,
                    ast_identifier_name(ast_member_object(ast_call_callee(node))));
            }
            if (field_type_name != NULL) {
                const char *method_name = ast_member_name(ast_call_callee(node));
                const MIRDeclMethod *method_meta =
                    transpiler_find_host_method_metadata_in_context(
                        ctx, field_type_name, method_name);
                if (method_meta != NULL) {
                    append_overlay_method_projection_invalidations_from_metadata(
                        buf, ctx, source_slot_name,
                        field_type_name, method_meta, depth + 1);
                } else {
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR-only C path missing projection invalidation method metadata for '%s.%s'",
                        field_type_name != NULL ? field_type_name : "(anonymous)",
                        method_name != NULL ? method_name : "(anonymous)");
                    return;
                }
            }
        }
        break;
    default:
        break;
    }
}

char *
emit_current_overlay_method_projection_invalidation(TranspilerCtx *ctx,
                                                    const char *source_slot_name,
                                                    const char *host_type_name,
                                                    const MIRDeclMethod *method_meta,
                                                    ASTNode *method_decl)
{
    CodeBuf *buf;
    ASTNode *body;

    if (ctx == NULL || source_slot_name == NULL || host_type_name == NULL)
        return NULL;

    if (transpiler_active_has_mir(ctx) && method_meta != NULL) {
        buf = codebuf_create();
        append_overlay_method_projection_invalidations_from_metadata(
            buf, ctx, source_slot_name, host_type_name, method_meta, 0);
        if (ctx->backend_error != NULL || buf->len == 0) {
            codebuf_destroy(buf);
            return NULL;
        }
        {
            char *result = pergyra_strdup(buf->data);
            codebuf_destroy(buf);
            return result;
        }
    }

    body = ast_func_body(method_decl);
    if (body == NULL)
        return NULL;

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
