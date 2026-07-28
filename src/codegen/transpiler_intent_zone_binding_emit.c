#include "transpiler_intent_zone_binding_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_intent_context.h"
#include "transpiler_intent_participant.h"
#include "transpiler_intent_zone_slot.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_type_require.h"

bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                               ASTNode *type_node);
bool transpiler_can_forward_declare_type_name_early(TranspilerCtx *ctx,
                                                    const char *type_name);

static bool
transpiler_intent_binding_surface_desc(char *out, size_t out_size,
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
transpiler_intent_binding_surface_desc_too_long(TranspilerCtx *ctx,
                                                const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s diagnostic surface is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "intent binding");
}

void
emit_intent_step_restore_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                            ASTNode *intent, ASTNode *step,
                                            size_t step_index)
{
    const char *zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_where_type(step) == NULL
        || ast_intent_step_where_type(step)->type != AST_TYPE) {
        return;
    }

    zone_type = ast_type_name(ast_intent_step_where_type(step));
    if (zone_type == NULL)
        return;

    for (size_t i = 0; i < ast_intent_step_who_count(step); i++) {
        const char *alias = ast_intent_step_who_names(step, NULL)[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || ast_intent_involves_subject_type(involves) == NULL) {
            continue;
        }

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

void
emit_intent_step_restore_bound_zone_aliases_with_metadata(CodeBuf *out,
                                                          TranspilerCtx *ctx,
                                                          ASTNode *intent,
                                                          const char *zone_type,
                                                          const char **who_aliases,
                                                          size_t who_alias_count,
                                                          size_t step_index,
                                                          const IntentBindingMetadataView *bindings)
{
    if (out == NULL || ctx == NULL || intent == NULL || zone_type == NULL)
        return;

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name =
            resolve_intent_zone_slot_name_for_zone_with_bindings(
                ctx, intent, zone_type, alias, bindings);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

void
emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count = 0;
    size_t binding_count = 0;
    size_t explicit_binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    ASTNode **bindings = NULL;
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    const char *intent_name = NULL;
    size_t intent_step_count = 0;
    bool mir_requires_routine = false;
    bool mir_only_intent = false;
    const char *return_c_type = "bool";
    char return_c_type_buf[256];

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    intent_name = ast_intent_decl_name(node);
    intent_step_count = ast_intent_decl_step_count(node);
    mir_requires_routine = transpiler_active_has_mir(ctx) && intent_step_count > 0;
    explicit_binding_count = ast_intent_decl_binding_count(node);
    involve_count = ast_intent_decl_involve_count(node);
    value_count = ast_intent_decl_value_count(node);
    mir_routine = transpiler_find_mir_intent(ctx, node);
    if (mir_requires_routine && mir_routine == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing intent routine for forward declaration '%s'",
            intent_name != NULL ? intent_name : "(anonymous-intent)");
        goto cleanup;
    }
    mir_only_intent = mir_routine != NULL;
    if (mir_only_intent) {
        const char *return_type_name =
            mir_routine_return_type_name(mir_routine);
        if (return_type_name == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-backed C forward declaration has invalid return type metadata for '%s'",
                intent_name != NULL ? intent_name : "(anonymous-intent)");
            goto cleanup;
        }
        if (!transpiler_require_type_name_c_type_copy(
                ctx, return_type_name, "intent return type",
                return_c_type_buf, sizeof(return_c_type_buf))) {
            goto cleanup;
        }
        return_c_type = return_c_type_buf;
        mir_binding_count = transpiler_collect_mir_intent_bindings(
            mir_routine, &binding_metadata);
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    &binding_metadata, i)) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-backed C forward declaration has incomplete ordered intent binding metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                goto cleanup;
            }
            if (!intent_binding_metadata_kind_is_supported(
                    intent_binding_metadata_view_kind_at(
                        &binding_metadata, i))) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-backed C forward declaration has invalid ordered intent binding metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                goto cleanup;
            }
        }
    }
    if (!mir_only_intent) {
        bindings = ast_intent_decl_bindings(node, NULL);
        involves_nodes = ast_intent_decl_involves(node, NULL);
        values = ast_intent_decl_values(node, NULL);
    }
    binding_count = mir_only_intent
        ? mir_binding_count
        : (explicit_binding_count > 0
            ? explicit_binding_count
            : (involve_count + value_count));
    codebuf_write(buf, "\n%s\n%s(", return_c_type, intent_name);
    for (size_t i = 0; i < binding_count; i++) {
        ASTNode *binding = mir_only_intent
            ? NULL
            : (explicit_binding_count > 0
                ? bindings[i]
                : (i < involve_count
                    ? involves_nodes[i]
                    : values[i - involve_count]));
        const char *pt = NULL;
        const char *alias = "value";
        bool pointer_param = false;
        bool is_mir_participant = mir_only_intent
            && intent_binding_metadata_view_row_is_kind(
                &binding_metadata, i, "participant");
        bool is_mir_value = mir_only_intent
            && intent_binding_metadata_view_row_is_kind(
                &binding_metadata, i, "value");
        char surface_desc[256];

        if (i > 0)
            codebuf_write(buf, ", ");

        if (is_mir_participant) {
            const char *participant_type =
                intent_binding_metadata_view_type_at(
                    &binding_metadata, i);
            char participant_c_type_buf[256];
            alias = intent_binding_metadata_view_alias_at(
                &binding_metadata, i);
            if (alias == NULL)
                alias = "participant";
            if (!transpiler_intent_binding_surface_desc(surface_desc,
                    sizeof(surface_desc), "intent participant", alias,
                    intent_name)) {
                transpiler_intent_binding_surface_desc_too_long(
                    ctx, "intent participant");
                goto cleanup;
            }
            if (participant_type != NULL) {
                if (transpiler_require_type_name_c_type_copy(ctx,
                        participant_type, surface_desc, participant_c_type_buf,
                        sizeof(participant_c_type_buf))) {
                    pt = participant_c_type_buf;
                }
                pointer_param = intent_type_name_uses_pointer_self(
                    ctx, participant_type);
            }
            if (pt == NULL) {
                goto cleanup;
            }
            codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
            continue;
        }

        if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
            const char *participant_type = NULL;
            char participant_c_type_buf[256];
            ASTNode *subject_type = NULL;
            alias = ast_intent_involves_alias(binding) != NULL
                ? ast_intent_involves_alias(binding) : "participant";
            if (!transpiler_intent_binding_surface_desc(surface_desc,
                    sizeof(surface_desc), "intent participant", alias,
                    intent_name)) {
                transpiler_intent_binding_surface_desc_too_long(
                    ctx, "intent participant");
                goto cleanup;
            }
            if (participant_type != NULL) {
                if (transpiler_require_type_name_c_type_copy(ctx,
                        participant_type, surface_desc, participant_c_type_buf,
                        sizeof(participant_c_type_buf))) {
                    pt = participant_c_type_buf;
                }
                pointer_param = intent_type_name_uses_pointer_self(
                    ctx, participant_type);
            } else if ((subject_type = ast_intent_involves_subject_type(binding)) != NULL) {
                if (transpiler_require_ast_c_type_copy(ctx,
                        subject_type,
                        surface_desc,
                        participant_c_type_buf,
                        sizeof(participant_c_type_buf))) {
                    pt = participant_c_type_buf;
                }
                pointer_param = intent_involves_uses_pointer_self(ctx, binding);
            }
            if (pt == NULL) {
                goto cleanup;
            }
            codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
            continue;
        }

        if (mir_only_intent && !is_mir_value) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-backed C forward declaration has invalid ordered intent binding metadata for '%s'",
                intent_name != NULL ? intent_name : "(anonymous-intent)");
            goto cleanup;
        }
        /* Row 607: intent VALUE alias is MIR-carrier-owned; non-MIR AST alias
           fallback retired (defaults to "value"). */
        alias = mir_only_intent
            ? intent_binding_metadata_view_alias_at(&binding_metadata, i)
            : "value";
        if (alias == NULL)
            alias = "value";
        if (!transpiler_intent_binding_surface_desc(surface_desc,
                sizeof(surface_desc), "intent value", alias,
                intent_name)) {
            transpiler_intent_binding_surface_desc_too_long(
                ctx, "intent value");
            goto cleanup;
        }
        {
            const char *value_type_name = mir_only_intent
                ? intent_binding_metadata_view_type_at(&binding_metadata, i)
                : NULL;
            char value_c_type_buf[256];
            if (mir_only_intent && value_type_name == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-backed C forward declaration has missing value type metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                goto cleanup;
            }
            if (!mir_only_intent && value_type_name == NULL) {
                goto cleanup;
            }
            if (value_type_name != NULL
                && transpiler_require_type_name_c_type_copy(ctx,
                    value_type_name, surface_desc, value_c_type_buf,
                    sizeof(value_c_type_buf))) {
                pt = value_c_type_buf;
            }
            /* Row 607: intent VALUE type is MIR-carrier-owned; non-MIR AST
               value-type fallback retired, fails closed via guard below. */
            if (pt == NULL) {
                goto cleanup;
            }
            codebuf_write(buf, "%s %s", pt, alias);
        }
        if (!mir_only_intent)
        if (pt == NULL) {
            goto cleanup;
        }
    }
    codebuf_write(buf, ");\n");
cleanup:
    intent_binding_metadata_view_dispose(&binding_metadata);
}

bool
transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx, ASTNode *intent)
{
    const MIRRoutine *mir_routine = NULL;
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count = 0;
    size_t intent_step_count = 0;
    bool mir_requires_routine = false;

    if (ctx == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    intent_step_count = ast_intent_decl_step_count(intent);
    mir_requires_routine = transpiler_active_has_mir(ctx) && intent_step_count > 0;
    mir_routine = transpiler_find_mir_intent(ctx, intent);
    if (mir_requires_routine && mir_routine == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing intent routine for early forward eligibility");
        return false;
    }
    if (mir_routine != NULL) {
        mir_binding_count = transpiler_collect_mir_intent_bindings(
            mir_routine, &binding_metadata);
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_supported_row(
                    &binding_metadata, i)
                || !transpiler_can_forward_declare_type_name_early(
                    ctx, intent_binding_metadata_view_type_at(
                        &binding_metadata, i))) {
                intent_binding_metadata_view_dispose(&binding_metadata);
                return false;
            }
        }
        intent_binding_metadata_view_dispose(&binding_metadata);
        return true;
    }
    /* Row 607 (SoT docs/125): intent participant/value shape is owned solely by
       the MIR routine binding carrier (handled in the mir_routine != NULL block
       above). Production intents are always routine-backed, so a missing routine
       here is a MIR-inventory defect, not an AST-fallback opportunity. Fail
       closed instead of reopening ast_intent_decl_involves/values. */
    transpiler_set_mir_inventory_missing(
        ctx,
        "MIR-only C path missing intent routine for early forward eligibility "
        "(non-MIR participant/value fallback retired)");
    return false;
}
