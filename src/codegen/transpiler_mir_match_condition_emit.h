#ifndef PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H
#define PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H

#include "transpiler_mir_expr_ssa.h"

static ASTNode *
transpiler_mir_find_match_subject_for_case(ASTNode *node, ASTNode *case_node)
{
    if (node == NULL || case_node == NULL)
        return NULL;

    if (node->type == AST_MATCH_STMT) {
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (node->data.match_stmt.cases[i] == case_node)
                return node->data.match_stmt.subject;
        }
        if (node->data.match_stmt.default_body != NULL) {
            ASTNode *found = transpiler_mir_find_match_subject_for_case(
                node->data.match_stmt.default_body, case_node);
            if (found != NULL)
                return found;
        }
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            ASTNode *found = transpiler_mir_find_match_subject_for_case(
                node->data.block.statements[i], case_node);
            if (found != NULL)
                return found;
        }
        break;
    case AST_IF_STMT: {
        ASTNode *found = transpiler_mir_find_match_subject_for_case(
            node->data.if_stmt.then_branch, case_node);
        if (found != NULL)
            return found;
        return transpiler_mir_find_match_subject_for_case(
            node->data.if_stmt.else_branch, case_node);
    }
    case AST_FOR_LOOP:
        return transpiler_mir_find_match_subject_for_case(
            node->data.for_loop.body, case_node);
    case AST_WHILE_LOOP:
        return transpiler_mir_find_match_subject_for_case(
            node->data.while_loop.body, case_node);
    case AST_WITH_STMT:
        return transpiler_mir_find_match_subject_for_case(
            node->data.with_stmt.body, case_node);
    default:
        break;
    }
    return NULL;
}

static bool
transpiler_mir_is_option_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            if (kind != NULL)
                *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        if (kind != NULL)
            *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        if (kind != NULL)
            *kind = "Some";
        if (binding != NULL
            && pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }
    return false;
}

static bool
transpiler_mir_is_result_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL || pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;
    if ((strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
        && pat->data.call.arg_count == 1) {
        if (kind != NULL)
            *kind = name;
        if (binding != NULL
            && pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }
    return false;
}

static const char *
transpiler_mir_match_payload_field(const char *kind)
{
    if (kind == NULL)
        return NULL;
    if (strcmp(kind, "Some") == 0)
        return "value";
    if (strcmp(kind, "Ok") == 0)
        return "ok";
    if (strcmp(kind, "Err") == 0)
        return "err";
    return NULL;
}

static bool
transpiler_mir_match_payload_type_name(TranspilerCtx *ctx,
                                       ASTNode *subject_node,
                                       const char *kind,
                                       char *buf,
                                       size_t buf_size)
{
    const char *subject_type;

    if (buf == NULL || buf_size == 0)
        return false;
    buf[0] = '\0';
    if (ctx == NULL || subject_node == NULL || kind == NULL)
        return false;

    subject_type = infer_expression_type_name(ctx, subject_node);
    if (subject_type == NULL || subject_type[0] == '\0')
        return false;

    if (strcmp(kind, "Some") == 0) {
        if (strncmp(subject_type, "Option<", 7) != 0)
            return false;
        return slot_inner_type_name_copy(subject_type, buf, buf_size)
            && buf[0] != '\0';
    }
    if (strcmp(kind, "Ok") == 0) {
        if (strncmp(subject_type, "Result<", 7) != 0)
            return false;
        copy_constructed_arg_name_at(subject_type, 0, buf, buf_size);
        return buf[0] != '\0' && strcmp(buf, "Unknown") != 0;
    }
    if (strcmp(kind, "Err") == 0) {
        if (strncmp(subject_type, "Result<", 7) != 0)
            return false;
        copy_constructed_arg_name_at(subject_type, 1, buf, buf_size);
        return buf[0] != '\0' && strcmp(buf, "Unknown") != 0;
    }

    return false;
}

static void
transpiler_mir_emit_match_payload_binding(CodeBuf *buf,
                                          TranspilerCtx *ctx,
                                          ASTNode *subject_node,
                                          const char *subject,
                                          const char *kind,
                                          const char *binding)
{
    const char *field;
    const char *payload_c_type;
    char payload_type[128];

    if (buf == NULL || ctx == NULL || subject == NULL
        || kind == NULL || binding == NULL) {
        return;
    }

    field = transpiler_mir_match_payload_field(kind);
    if (field == NULL)
        return;
    if (!transpiler_mir_match_payload_type_name(ctx, subject_node, kind,
                                                payload_type,
                                                sizeof(payload_type))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C MIR match lowering cannot derive payload type for %s(%s); explicit Option<T>/Result<T,E> subject type is required",
            kind,
            binding);
        return;
    }

    {
        char payload_c_type_buf[256];
        if (!pergyra_type_to_c_copy(payload_type, payload_c_type_buf,
                sizeof(payload_c_type_buf))) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C MIR match lowering cannot render payload type '%s' for %s(%s)",
                payload_type,
                kind,
                binding);
            return;
        }
        payload_c_type = payload_c_type_buf;

        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s %s = (%s).%s;\n",
                      payload_c_type, binding, subject, field);
    }
}

static char *
transpiler_mir_render_match_case_condition(ASTNode *func_decl,
                                           ASTNode *case_node,
                                           TranspilerCtx *ctx,
                                           const TranspilerSSANameMap *ssa_map)
{
    ASTNode *subject_node;
    char *subject;
    char *cond = NULL;
    char *guard = NULL;

    if (func_decl == NULL || case_node == NULL || ctx == NULL
        || func_decl->type != AST_FUNC_DECL || case_node->type != AST_MATCH_CASE) {
        return NULL;
    }

    subject_node = transpiler_mir_find_match_subject_for_case(
        func_decl->data.func_decl.body, case_node);
    if (subject_node == NULL)
        return NULL;

    subject = emit_expression_with_ssa_map(subject_node, ctx, ssa_map);
    if (subject == NULL)
        return NULL;

    if (case_node->data.match_case.patterns != NULL
        && case_node->data.match_case.pattern_count > 1) {
        for (size_t i = 0; i < case_node->data.match_case.pattern_count; i++) {
            char *pat = emit_expression_with_ssa_map(
                case_node->data.match_case.patterns[i], ctx, ssa_map);
            char *next = NULL;
            if (pat == NULL)
                continue;
            next = cond == NULL
                ? strdup_fmt("(%s == %s)", subject, pat)
                : strdup_fmt("(%s || (%s == %s))", cond, subject, pat);
            free(cond);
            free(pat);
            cond = next;
        }
    } else if (case_node->data.match_case.pattern != NULL) {
        const char *kind = NULL;
        const char *binding = NULL;
        if (transpiler_mir_is_option_destructor(
                case_node->data.match_case.pattern, &kind, &binding)) {
            const char *tag = strcmp(kind, "Some") == 0
                ? "PgyOptionSome" : "PgyOptionNone";
            transpiler_mir_emit_match_payload_binding(ctx->out, ctx,
                                                      subject_node,
                                                      subject, kind, binding);
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
        } else if (transpiler_mir_is_result_destructor(
                       case_node->data.match_case.pattern, &kind, &binding)) {
            const char *tag = strcmp(kind, "Ok") == 0
                ? "PgyResultOk" : "PgyResultErr";
            transpiler_mir_emit_match_payload_binding(ctx->out, ctx,
                                                      subject_node,
                                                      subject, kind, binding);
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
        } else {
            char *pat = emit_expression_with_ssa_map(
                case_node->data.match_case.pattern, ctx, ssa_map);
            if (pat != NULL)
                cond = strdup_fmt("%s == %s", subject, pat);
            free(pat);
        }
    }

    if (case_node->data.match_case.guard != NULL) {
        guard = emit_expression_with_ssa_map(case_node->data.match_case.guard,
                                             ctx, ssa_map);
        if (guard != NULL && cond != NULL) {
            char *with_guard = strdup_fmt("(%s) && (%s)", cond, guard);
            free(cond);
            cond = with_guard;
        }
    }

    free(guard);
    free(subject);
    return cond;
}

#endif /* PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H */
