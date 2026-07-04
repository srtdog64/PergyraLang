/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend match statement lowering.
 */

#include "transpiler_match_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_block_emit.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_match_bindings.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_require.h"

static char *
transpiler_match_emit_part(TranspilerCtx *ctx,
                           ASTNode *expr,
                           const char *role)
{
    char *rendered = emit_expression(expr, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C match lowering could not lower %s expression",
        role != NULL ? role : "operand");
    return NULL;
}

void
emit_match_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *subject_node = ast_match_subject(node);
    char *subj = transpiler_match_emit_part(ctx, subject_node, "subject");
    int saved_indent = ctx->indent;
    int tmp_id = ctx->tmp_counter++;
    const char *subject_type = transpiler_expr_infer_type_name(ctx,
        subject_node);
    char subject_c_type_buf[256];
    const char *subject_c_type = NULL;
    bool subject_is_option = transpiler_type_name_is_option(subject_type);

    if (subj == NULL)
        return;

    if (subject_type == NULL || subject_type[0] == '\0'
        || strcmp(subject_type, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C match lowering requires a concrete subject type; implicit Int match fallback is disabled");
        free(subj);
        return;
    }
    if (transpiler_require_type_name_c_type_copy(ctx, subject_type,
            "match subject", subject_c_type_buf,
            sizeof(subject_c_type_buf))) {
        subject_c_type = subject_c_type_buf;
    }
    if (subject_c_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C match lowering requires a stable C rendering for subject type '%s'",
            subject_type);
        free(subj);
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "%s __match_%d = %s;\n",
        subject_c_type, tmp_id, subj);
    free(subj);

    for (size_t i = 0; i < ast_match_case_count(node); i++) {
        ASTNode *mc = ast_match_case_at(node, i);
        const char *kind = NULL, *binding = NULL;
        ASTNode *pattern_node = ast_match_case_pattern(mc);
        bool option_case = subject_is_option
            && transpiler_match_is_option_destructor(pattern_node, &kind, &binding);

        write_indent(ctx);

        if (ast_match_case_patterns(mc, NULL) != NULL
            && ast_match_case_pattern_count(mc) > 1) {
            codebuf_write(ctx->out, i == 0 ? "if (" : "else if (");
            for (size_t p = 0; p < ast_match_case_pattern_count(mc); p++) {
                char *pat = transpiler_match_emit_part(ctx,
                    ast_match_case_pattern_at(mc, p), "pattern");
                if (pat == NULL) {
                    ctx->indent = saved_indent;
                    return;
                }
                if (p > 0)
                    codebuf_write(ctx->out, " || ");
                codebuf_write(ctx->out, "__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        } else if (option_case) {
            const char *tag_val = pgy_codegen_match_variant_c_option_tag(
                pgy_codegen_match_variant_lookup(kind));
            codebuf_write(ctx->out,
                i == 0 ? "if (__match_%d.tag == %s"
                       : "else if (__match_%d.tag == %s",
                tmp_id, tag_val);
        } else if (transpiler_match_is_result_destructor(pattern_node, &kind, &binding)) {
            const char *tag_val = pgy_codegen_match_variant_c_result_tag(
                pgy_codegen_match_variant_lookup(kind));
            codebuf_write(ctx->out,
                i == 0 ? "if (__match_%d.tag == %s"
                       : "else if (__match_%d.tag == %s",
                tmp_id, tag_val);
        } else {
            const char *vname = NULL, *ename = NULL;
            const char **bindings = NULL;
            const char **binding_type_names = NULL;
            size_t bind_count = 0;
            const char *bindings_buf[8];
            const char *binding_type_names_buf[8];
            if (transpiler_match_is_enum_variant_destructor(pattern_node, ctx,
                    &vname, &ename, &bindings,
                    &binding_type_names, &bind_count,
                    bindings_buf, binding_type_names_buf,
                    sizeof(bindings_buf) / sizeof(bindings_buf[0]))) {
                codebuf_write(ctx->out,
                    i == 0 ? "if (__match_%d.tag == %s_TAG_%s"
                           : "else if (__match_%d.tag == %s_TAG_%s",
                    tmp_id, ename, vname);
                kind = vname;
            } else {
                char *pat = transpiler_match_emit_part(ctx,
                    pattern_node, "pattern");
                if (pat == NULL) {
                    ctx->indent = saved_indent;
                    return;
                }
                codebuf_write(ctx->out,
                    i == 0 ? "if (__match_%d == %s"
                           : "else if (__match_%d == %s",
                    tmp_id, pat);
                free(pat);
            }
        }

        if (ast_match_case_guard(mc) != NULL) {
            char *guard = transpiler_match_emit_part(ctx,
                ast_match_case_guard(mc), "guard");
            if (guard == NULL) {
                ctx->indent = saved_indent;
                return;
            }
            codebuf_write(ctx->out, " && %s", guard);
            free(guard);
        }
        codebuf_write(ctx->out, ")\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;

        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        int saved_alias_count = ctx->alias_var_count;
        if (binding != NULL && kind != NULL) {
            transpiler_emit_builtin_match_binding(pattern_node, kind, binding,
                subject_type, subject_is_option, tmp_id, ctx);
        }
        transpiler_emit_enum_match_bindings(pattern_node, kind, tmp_id, ctx);

        emit_block(ast_match_case_body(mc), ctx);
        transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                      saved_typed_count,
                                                      saved_alias_count);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    if (ast_match_default_body(node) != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(ast_match_default_body(node), ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}
