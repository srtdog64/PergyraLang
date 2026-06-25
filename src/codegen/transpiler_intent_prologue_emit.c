#include "transpiler_intent_prologue_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_intent_participant.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_intent_query.h"
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
                                           bool has_compensate_steps,
                                           size_t step_count,
                                           const IntentBindingMetadataView *bindings_view,
                                           bool emit_cleanup_from_mir,
                                           const MIRRoutine *mir_routine)
{
    size_t subject_count = 0;
    const char *intent_name;
    size_t explicit_binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    size_t mir_binding_count = 0;
    ASTNode **bindings = NULL;
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    ASTNode *priority_expr;

    if (node == NULL || ctx == NULL)
        return false;

    intent_name = ast_intent_decl_name(node);
    if (mir_routine != NULL) {
        priority_expr = transpiler_find_mir_intent_eval_expr(
            mir_routine, intent_name, "priority");
        if (transpiler_mir_intent_has_stmt(
                mir_routine, intent_name, "IntentEval", "priority")
            && priority_expr == NULL) {
            transpiler_set_mir_intent_carrier_missing(ctx,
                "MIR-backed C intent prologue missing priority eval carrier");
            return false;
        }
        mir_binding_count = bindings_view != NULL ? bindings_view->count : 0;
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    bindings_view, i)) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-backed C intent prologue has incomplete ordered binding metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                return false;
            }
            if (!intent_binding_metadata_view_has_supported_row(
                    bindings_view, i)) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-backed C intent prologue has invalid ordered binding metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                return false;
            }
        }
    } else {
        priority_expr = ast_intent_decl_priority_expr(node);
        explicit_binding_count = ast_intent_decl_binding_count(node);
        involve_count = ast_intent_decl_involve_count(node);
        value_count = ast_intent_decl_value_count(node);
        bindings = ast_intent_decl_bindings(node, NULL);
        involves_nodes = ast_intent_decl_involves(node, NULL);
        values = ast_intent_decl_values(node, NULL);
    }

    codebuf_write(ctx->out, "\nbool\n%s(", intent_name);
    {
        size_t binding_count = mir_routine != NULL
            ? mir_binding_count
            : (explicit_binding_count > 0
                ? explicit_binding_count
                : (involve_count + value_count));
        size_t participant_index = 0;
        size_t value_index = 0;
        for (size_t i = 0; i < binding_count; i++) {
            ASTNode *binding = mir_routine != NULL
                ? NULL
                : (explicit_binding_count > 0
                    ? bindings[i]
                    : (i < involve_count
                        ? involves_nodes[i]
                        : values[i - involve_count]));
            const char *pt = NULL;
            const char *alias = "value";
            char *type_name = NULL;
            bool pointer_param = false;
            char c_type_buf[256];
            char surface_desc[256];

            if (i > 0)
                codebuf_write(ctx->out, ", ");

            if (mir_routine != NULL
                && intent_binding_metadata_view_row_is_kind(
                    bindings_view, i, "participant")) {
                const char *participant_type =
                    intent_binding_metadata_view_type_at(bindings_view, i);
                alias = intent_binding_metadata_view_alias_at(
                    bindings_view, i);
                if (alias == NULL)
                    alias = "participant";
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
                }
            } else if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *participant_type = NULL;
                (void)participant_index;
                alias = ast_intent_involves_alias(binding) != NULL
                    ? ast_intent_involves_alias(binding) : "participant";
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
                } else if (ast_intent_involves_subject_type(binding) != NULL) {
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
                const char *value_type_name = mir_routine != NULL
                    ? intent_binding_metadata_view_type_at(bindings_view, i)
                    : NULL;
                (void)value_index;
                if (mir_routine != NULL
                    && !intent_binding_metadata_view_row_is_kind(
                        bindings_view, i, "value")) {
                    transpiler_set_mir_inventory_missing(
                        ctx,
                        "MIR-backed C intent prologue has invalid ordered binding metadata for '%s'",
                        intent_name != NULL ? intent_name : "(anonymous-intent)");
                    return false;
                }
                /* Row 607: intent VALUE alias is MIR-carrier-owned; non-MIR AST
                   alias fallback retired (defaults to "value"). */
                alias = mir_routine != NULL
                    ? intent_binding_metadata_view_alias_at(bindings_view, i)
                    : "value";
                if (alias == NULL)
                    alias = "value";
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
                }
                /* Row 607: intent VALUE type is MIR-carrier-owned; the non-MIR
                   AST value-type fallback is retired and fails closed via the
                   pt==NULL guard below. */
                if (mir_routine == NULL)
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
    if (mir_routine != NULL) {
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (intent_binding_metadata_view_row_is_kind(
                    bindings_view, i, "participant")
                && intent_type_name_is_subject_participant(ctx,
                    intent_binding_metadata_view_type_at(
                        bindings_view, i))) {
                subject_count++;
            }
        }
    } else {
        for (size_t i = 0; i < involve_count; i++) {
            ASTNode *involves = involves_nodes[i];
            bool is_subject = intent_involves_is_subject_participant(ctx, involves);
            if (is_subject)
                subject_count++;
        }
    }
    if (subject_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "void *__intent_subjects[%zu];\n",
            subject_count);
        {
            size_t subject_index = 0;
            size_t participant_loop_count = mir_routine != NULL
                ? mir_binding_count : involve_count;
            for (size_t i = 0; i < participant_loop_count; i++) {
                ASTNode *involves = mir_routine != NULL
                    ? NULL : involves_nodes[i];
                const char *alias = mir_routine != NULL
                    ? intent_binding_metadata_view_alias_at(
                        bindings_view, i)
                    : ((involves != NULL
                        && ast_intent_involves_alias(involves) != NULL)
                        ? ast_intent_involves_alias(involves) : "participant");
                bool is_subject = mir_routine != NULL
                    ? (intent_binding_metadata_view_row_is_kind(
                            bindings_view, i, "participant")
                        && intent_type_name_is_subject_participant(ctx,
                            intent_binding_metadata_view_type_at(
                                bindings_view, i)))
                    : intent_involves_is_subject_participant(ctx, involves);
                if (alias == NULL)
                    alias = "participant";
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
