#include "transpiler_intent_prologue_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_intent_participant.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static bool
transpiler_intent_prologue_surface_desc(char *out, size_t out_size,
                                        const char *surface_kind,
                                        const char *alias,
                                        const char *intent_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    written = snprintf(out, out_size, "%s '%s' of '%s'",
        surface_kind,
        alias != NULL ? alias : "(anonymous)",
        intent_name != NULL ? intent_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_intent_prologue_surface_desc_too_long(TranspilerCtx *ctx,
                                                 const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s diagnostic surface is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "intent binding");
}

bool
transpiler_emit_intent_signature_and_entry(ASTNode *node,
                                           TranspilerCtx *ctx,
                                           bool mir_only_intent,
                                           bool has_compensate_steps,
                                           size_t step_count,
                                           const char **participant_aliases,
                                           const char **participant_types,
                                           size_t participant_count,
                                           const char **value_aliases,
                                           const char **value_types,
                                           size_t mir_value_count,
                                           bool emit_cleanup_from_mir,
                                           const MIRRoutine *mir_routine)
{
    size_t subject_count = 0;
    const char *intent_name;
    size_t explicit_binding_count;
    size_t involve_count;
    size_t value_count;
    ASTNode **bindings;
    ASTNode **involves_nodes;
    ASTNode **values;
    ASTNode *priority_expr;

    if (node == NULL || ctx == NULL)
        return false;

    intent_name = ast_intent_decl_name(node);
    explicit_binding_count = ast_intent_decl_binding_count(node);
    involve_count = ast_intent_decl_involve_count(node);
    value_count = ast_intent_decl_value_count(node);
    bindings = ast_intent_decl_bindings(node, NULL);
    involves_nodes = ast_intent_decl_involves(node, NULL);
    values = ast_intent_decl_values(node, NULL);
    priority_expr = ast_intent_decl_priority_expr(node);

    codebuf_write(ctx->out, "\nbool\n%s(", intent_name);
    {
        size_t binding_count = explicit_binding_count > 0
            ? explicit_binding_count
            : (involve_count + value_count);
        size_t participant_index = 0;
        size_t value_index = 0;
        for (size_t i = 0; i < binding_count; i++) {
            ASTNode *binding = explicit_binding_count > 0
                ? bindings[i]
                : (i < involve_count
                    ? involves_nodes[i]
                    : values[i - involve_count]);
            const char *pt = NULL;
            const char *alias = "value";
            char *type_name = NULL;
            bool pointer_param = false;
            char c_type_buf[256];
            char surface_desc[256];

            if (i > 0)
                codebuf_write(ctx->out, ", ");

            if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *participant_type = participant_types != NULL && participant_index < participant_count
                    ? participant_types[participant_index]
                    : NULL;
                alias = (mir_only_intent && participant_aliases != NULL && participant_index < participant_count
                    && participant_aliases[participant_index] != NULL)
                    ? participant_aliases[participant_index]
                    : (ast_intent_involves_alias(binding) != NULL
                        ? ast_intent_involves_alias(binding) : "participant");
                if (!transpiler_intent_prologue_surface_desc(surface_desc,
                        sizeof(surface_desc), "intent participant", alias,
                        intent_name)) {
                    transpiler_intent_prologue_surface_desc_too_long(
                        ctx, "intent participant");
                    return false;
                }
                if (participant_type != NULL) {
                    if (transpiler_require_type_name_c_type_copy(ctx,
                            participant_type, surface_desc,
                            c_type_buf,
                            sizeof(c_type_buf))) {
                        pt = c_type_buf;
                    }
                    type_name = pergyra_strdup(participant_type);
                    pointer_param = intent_type_name_uses_pointer_self(
                        ctx, participant_type);
                } else if (!mir_only_intent
                    && ast_intent_involves_subject_type(binding) != NULL) {
                    if (transpiler_require_ast_c_type_copy(ctx,
                            ast_intent_involves_subject_type(binding),
                            surface_desc,
                            c_type_buf,
                            sizeof(c_type_buf))) {
                        pt = c_type_buf;
                    }
                    type_name = render_type_name_in_ctx(
                        ctx, ast_intent_involves_subject_type(binding));
                    pointer_param = intent_involves_uses_pointer_self(ctx, binding);
                }
                participant_index++;
            } else {
                const char *value_type_name = value_types != NULL
                        && value_index < mir_value_count
                    ? value_types[value_index]
                    : NULL;
                ASTNode *value_type = binding != NULL
                    ? ast_intent_value_type(binding) : NULL;
                alias = (value_aliases != NULL && value_index < mir_value_count
                         && value_aliases[value_index] != NULL)
                    ? value_aliases[value_index]
                    : (binding != NULL && ast_intent_value_alias(binding) != NULL)
                    ? ast_intent_value_alias(binding) : "value";
                if (!transpiler_intent_prologue_surface_desc(surface_desc,
                        sizeof(surface_desc), "intent value", alias,
                        intent_name)) {
                    transpiler_intent_prologue_surface_desc_too_long(
                        ctx, "intent value");
                    return false;
                }
                if (value_type_name != NULL
                    && transpiler_require_type_name_c_type_copy(ctx,
                        value_type_name, surface_desc, c_type_buf,
                        sizeof(c_type_buf))) {
                    pt = c_type_buf;
                    type_name = pergyra_strdup(value_type_name);
                } else if (value_type_name == NULL
                           && transpiler_require_ast_c_type_copy(ctx,
                               value_type, surface_desc, c_type_buf,
                               sizeof(c_type_buf))) {
                    pt = c_type_buf;
                    if (value_type != NULL)
                        type_name = render_type_name_in_ctx(ctx, value_type);
                }
                value_index++;
            }
            if (pt == NULL)
                return false;
            codebuf_write(ctx->out, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
            if (type_name != NULL) {
                register_typed_var(ctx, alias, type_name);
                if (pointer_param) {
                    TypedVarEntry *entry = lookup_typed_entry(ctx, alias);
                    if (entry != NULL)
                        entry->is_subject_ref = true;
                }
                free(type_name);
            }
        }
    }
    codebuf_write(ctx->out, ")\n{\n");
    ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "bool __intent_result = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "bool __intent_failed = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "(void)__intent_failed;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "int32_t __intent_handle = 0;\n");
    if (has_compensate_steps && step_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "bool __intent_step_completed[%zu] = { false };\n",
            step_count);
    }
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        const char *participant_type = participant_types != NULL && i < participant_count
            ? participant_types[i]
            : NULL;
        bool is_subject = participant_type != NULL
            ? intent_type_name_is_subject_participant(ctx, participant_type)
            : intent_involves_is_subject_participant(ctx, involves);
        if (is_subject)
            subject_count++;
    }
    if (subject_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "void *__intent_subjects[%zu];\n",
            subject_count);
        {
            size_t subject_index = 0;
            for (size_t i = 0; i < involve_count; i++) {
                ASTNode *involves = involves_nodes[i];
                const char *alias = (mir_only_intent && participant_aliases != NULL && i < participant_count
                    && participant_aliases[i] != NULL)
                    ? participant_aliases[i]
                    : ((involves != NULL && ast_intent_involves_alias(involves) != NULL)
                        ? ast_intent_involves_alias(involves) : "participant");
                const char *participant_type = participant_types != NULL && i < participant_count
                    ? participant_types[i]
                    : NULL;
                bool is_subject = participant_type != NULL
                    ? intent_type_name_is_subject_participant(ctx, participant_type)
                    : intent_involves_is_subject_participant(ctx, involves);
                if (!is_subject)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "__intent_subjects[%zu] = (void *)%s;\n",
                    subject_index++, alias);
            }
        }
    }
    {
        char *priority = NULL;
        bool owns_priority = false;
        if (priority_expr != NULL) {
            priority = emit_expression(priority_expr, ctx);
            owns_priority = (priority != NULL);
        } else {
            priority = (char *)transpiler_scratch_strdup(ctx, "0");
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "__intent_handle = pgy_intent_enter_export(\"%s\", %s, %zu, %s, %s);\n",
            intent_name,
            subject_count > 0 ? "__intent_subjects" : "NULL",
            subject_count,
            ast_intent_decl_is_concurrent(node) ? "true" : "false",
            priority != NULL ? priority : "0");
        if (owns_priority)
            free(priority);
    }
    {
        write_indent(ctx);
        codebuf_write(ctx->out, "if (__intent_handle == 0) {\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_failed = true;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_result = false;\n");
        write_indent(ctx);
        if (emit_cleanup_from_mir) {
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                intent_name, mir_routine->cleanup_block);
        } else {
            codebuf_write(ctx->out, "goto __intent_cleanup;\n");
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    return true;
}
