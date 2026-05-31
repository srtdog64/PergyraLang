#include "transpiler_mir_match_condition_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_match_bindings.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static bool
transpiler_mir_is_option_destructor(ASTNode *pat,
                                    const char **kind,
                                    const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(pat);
        PgyCodegenMatchVariantKind variant =
            pgy_codegen_match_variant_lookup(name);
        if (variant == PGY_MATCH_VARIANT_NONE_CTOR) {
            if (kind != NULL)
                *kind = pgy_codegen_match_variant_name(variant);
            return true;
        }
        return false;
    }

    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;

    if (variant == PGY_MATCH_VARIANT_NONE_CTOR && arg_count == 0) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        return true;
    }
    if (variant == PGY_MATCH_VARIANT_SOME && arg_count == 1) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (binding != NULL
            && payload != NULL
            && payload->type == AST_IDENTIFIER) {
            *binding = ast_identifier_name(payload);
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
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat == NULL || pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;
    if (pgy_codegen_match_variant_is_result(variant)
        && arg_count == 1) {
        if (kind != NULL)
            *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (binding != NULL
            && payload != NULL
            && payload->type == AST_IDENTIFIER) {
            *binding = ast_identifier_name(payload);
        }
        return true;
    }
    return false;
}

static const char *
transpiler_mir_match_payload_field(const char *kind)
{
    return pgy_codegen_match_variant_c_payload_field(
        pgy_codegen_match_variant_lookup(kind));
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

    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(kind);

    if (variant == PGY_MATCH_VARIANT_SOME) {
        if (!transpiler_type_name_is_option(subject_type))
            return false;
        return slot_inner_type_name_copy(subject_type, buf, buf_size)
            && buf[0] != '\0';
    }
    if (variant == PGY_MATCH_VARIANT_OK) {
        if (!transpiler_type_name_is_result(subject_type))
            return false;
        copy_constructed_arg_name_at(subject_type, 0, buf, buf_size);
        return buf[0] != '\0' && strcmp(buf, "Unknown") != 0;
    }
    if (variant == PGY_MATCH_VARIANT_ERR) {
        if (!transpiler_type_name_is_result(subject_type))
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
        if (!transpiler_require_type_name_c_type_copy(ctx, payload_type,
                "MIR match payload", payload_c_type_buf,
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
        register_typed_var(ctx, binding, payload_type);
    }
}

char *
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

    subject_node = ast_find_match_subject_for_case(ast_func_body(func_decl),
                                                   case_node);
    if (subject_node == NULL)
        return NULL;

    subject = emit_expression_with_ssa_map(subject_node, ctx, ssa_map);
    if (subject == NULL)
        return NULL;

    if (ast_match_case_patterns(case_node, NULL) != NULL
        && ast_match_case_pattern_count(case_node) > 1) {
        for (size_t i = 0; i < ast_match_case_pattern_count(case_node); i++) {
            char *pat = emit_expression_with_ssa_map(
                ast_match_case_pattern_at(case_node, i), ctx, ssa_map);
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
    } else if (ast_match_case_pattern(case_node) != NULL) {
        const char *kind = NULL;
        const char *binding = NULL;
        ASTNode *pattern_node = ast_match_case_pattern(case_node);
        if (transpiler_mir_is_option_destructor(
                pattern_node, &kind, &binding)) {
            const char *tag = pgy_codegen_match_variant_c_option_tag(
                pgy_codegen_match_variant_lookup(kind));
            transpiler_mir_emit_match_payload_binding(ctx->out, ctx,
                                                      subject_node,
                                                      subject, kind, binding);
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
        } else if (transpiler_mir_is_result_destructor(
                       pattern_node, &kind, &binding)) {
            const char *tag = pgy_codegen_match_variant_c_result_tag(
                pgy_codegen_match_variant_lookup(kind));
            transpiler_mir_emit_match_payload_binding(ctx->out, ctx,
                                                      subject_node,
                                                      subject, kind, binding);
            cond = strdup_fmt("(%s).tag == %s", subject, tag);
        } else {
            const char *enum_vname = NULL;
            const char *enum_ename = NULL;
            const char **enum_bindings = NULL;
            ASTNode **enum_binding_types = NULL;
            size_t enum_bind_count = 0;
            const char *enum_bindings_buf[8];
            ASTNode *enum_binding_types_buf[8];
            if (transpiler_match_is_enum_variant_destructor(pattern_node, ctx,
                    &enum_vname, &enum_ename, &enum_bindings,
                    &enum_binding_types, &enum_bind_count,
                    enum_bindings_buf, enum_binding_types_buf,
                    sizeof(enum_bindings_buf)
                        / sizeof(enum_bindings_buf[0]))) {
                for (size_t b = 0; b < enum_bind_count; b++) {
                    if (enum_bindings == NULL || enum_bindings[b] == NULL)
                        continue;
                    char bt_buf[256];
                    const char *bt_c_type = "int32_t";
                    if (enum_binding_types != NULL
                        && enum_binding_types[b] != NULL) {
                        if (!transpiler_require_ast_c_type_copy(
                                ctx, enum_binding_types[b],
                                "MIR enum match payload binding",
                                bt_buf, sizeof(bt_buf))) {
                            free(subject);
                            free(cond);
                            free(guard);
                            return NULL;
                        }
                        bt_c_type = bt_buf;
                    }
                    /*
                     * Wildcard `_` bindings are discarded — body never
                     * references them. Rename per-(variant, slot) to avoid
                     * C function-scope redefinition when multiple cases
                     * use `_`. Non-wildcard names keep their identity so
                     * the SSA-map-driven body resolution still works.
                     */
                    const char *emitted_name = enum_bindings[b];
                    char wildcard_buf[64];
                    if (emitted_name != NULL
                        && strcmp(emitted_name, "_") == 0) {
                        int wn = snprintf(wildcard_buf, sizeof(wildcard_buf),
                            "_pgy_match_discard_%s_%zu",
                            enum_vname, b);
                        if (wn > 0
                            && (size_t)wn < sizeof(wildcard_buf)) {
                            emitted_name = wildcard_buf;
                        }
                    }
                    write_indent_to(ctx->out, ctx->indent);
                    codebuf_write(ctx->out,
                        "%s %s = (%s).%s._%zu;\n",
                        bt_c_type, emitted_name, subject, enum_vname, b);
                    if (emitted_name != enum_bindings[b]) {
                        write_indent_to(ctx->out, ctx->indent);
                        codebuf_write(ctx->out, "(void)%s;\n", emitted_name);
                    }
                }
                cond = strdup_fmt("(%s).tag == %s_TAG_%s",
                    subject, enum_ename, enum_vname);
            } else {
                char *pat = emit_expression_with_ssa_map(
                    pattern_node, ctx, ssa_map);
                if (pat != NULL)
                    cond = strdup_fmt("%s == %s", subject, pat);
                free(pat);
            }
        }
    }

    if (ast_match_case_guard(case_node) != NULL) {
        guard = emit_expression_with_ssa_map(ast_match_case_guard(case_node),
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
