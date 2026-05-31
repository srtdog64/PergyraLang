#include "transpiler_intent_zone_binding_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_intent_context.h"
#include "transpiler_intent_participant.h"
#include "transpiler_intent_zone_slot.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_type_require.h"

bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                               ASTNode *type_node);

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
                                                          const char **participant_aliases,
                                                          const char **participant_types,
                                                          size_t participant_count)
{
    if (out == NULL || ctx == NULL || intent == NULL || zone_type == NULL)
        return;

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name =
            resolve_intent_zone_slot_name_for_zone_with_metadata(
                ctx, intent, zone_type, alias,
                participant_aliases, participant_types, participant_count);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

bool
intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < ast_func_param_count(action_decl); i++) {
        FuncParam *p = ast_func_param(action_decl, i);
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}

void
emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;
    size_t binding_count = 0;
    size_t participant_index = 0;
    size_t explicit_binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    ASTNode **bindings = NULL;
    ASTNode **involves_nodes = NULL;
    ASTNode **values = NULL;
    const char *intent_name = NULL;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    intent_name = ast_intent_decl_name(node);
    explicit_binding_count = ast_intent_decl_binding_count(node);
    involve_count = ast_intent_decl_involve_count(node);
    value_count = ast_intent_decl_value_count(node);
    bindings = ast_intent_decl_bindings(node, NULL);
    involves_nodes = ast_intent_decl_involves(node, NULL);
    values = ast_intent_decl_values(node, NULL);
    mir_routine = transpiler_find_mir_intent(ctx, node);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    binding_count = explicit_binding_count > 0
        ? explicit_binding_count
        : (involve_count + value_count);
    codebuf_write(buf, "\nbool\n%s(", intent_name);
    for (size_t i = 0; i < binding_count; i++) {
        ASTNode *binding = explicit_binding_count > 0
            ? bindings[i]
            : (i < involve_count
                ? involves_nodes[i]
                : values[i - involve_count]);
        const char *pt = NULL;
        const char *alias = "value";
        bool pointer_param = false;
        char surface_desc[256];

        if (i > 0)
            codebuf_write(buf, ", ");

        if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
            const char *participant_type = participant_types != NULL
                    && participant_index < participant_count
                ? participant_types[participant_index]
                : NULL;
            char participant_c_type_buf[256];
            ASTNode *subject_type = ast_intent_involves_subject_type(binding);
            alias = ast_intent_involves_alias(binding) != NULL
                ? ast_intent_involves_alias(binding) : "participant";
            if (!transpiler_intent_binding_surface_desc(surface_desc,
                    sizeof(surface_desc), "intent participant", alias,
                    intent_name)) {
                transpiler_intent_binding_surface_desc_too_long(
                    ctx, "intent participant");
                free((void *)participant_aliases);
                free((void *)participant_types);
                return;
            }
            if (participant_type != NULL) {
                if (transpiler_require_type_name_c_type_copy(ctx,
                        participant_type, surface_desc, participant_c_type_buf,
                        sizeof(participant_c_type_buf))) {
                    pt = participant_c_type_buf;
                }
                pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
            } else if (subject_type != NULL) {
                if (transpiler_require_ast_c_type_copy(ctx,
                        subject_type,
                        surface_desc,
                        participant_c_type_buf,
                        sizeof(participant_c_type_buf))) {
                    pt = participant_c_type_buf;
                }
                pointer_param = intent_involves_uses_pointer_self(ctx, binding);
            }
            if (participant_aliases != NULL && participant_index < participant_count
                && participant_aliases[participant_index] != NULL) {
                alias = participant_aliases[participant_index];
            }
            participant_index++;
            if (pt == NULL) {
                free((void *)participant_aliases);
                free((void *)participant_types);
                return;
            }
            codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
            continue;
        }

        alias = binding != NULL && ast_intent_value_alias(binding) != NULL
            ? ast_intent_value_alias(binding) : "value";
        if (!transpiler_intent_binding_surface_desc(surface_desc,
                sizeof(surface_desc), "intent value", alias,
                intent_name)) {
            transpiler_intent_binding_surface_desc_too_long(
                ctx, "intent value");
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        {
            char value_c_type_buf[256];
            if (transpiler_require_ast_c_type_copy(ctx,
                    binding != NULL ? ast_intent_value_type(binding) : NULL,
                    surface_desc,
                    value_c_type_buf,
                    sizeof(value_c_type_buf))) {
                pt = value_c_type_buf;
            }
            if (pt == NULL) {
                free((void *)participant_aliases);
                free((void *)participant_types);
                return;
            }
            codebuf_write(buf, "%s %s", pt, alias);
        }
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
    }
    codebuf_write(buf, ");\n");
    free((void *)participant_aliases);
    free((void *)participant_types);
}

bool
transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx, ASTNode *intent)
{
    ASTNode **involves_nodes;
    ASTNode **values;
    size_t involve_count;
    size_t value_count;

    if (ctx == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    values = ast_intent_decl_values(intent, &value_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        ASTNode *subject_type = NULL;
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES) {
            continue;
        }
        subject_type = ast_intent_involves_subject_type(involves);
        if (subject_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, subject_type)) {
            return false;
        }
    }
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        ASTNode *value_type = NULL;
        if (value == NULL || value->type != AST_INTENT_VALUE) {
            continue;
        }
        value_type = ast_intent_value_type(value);
        if (value_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, value_type)) {
            return false;
        }
    }
    return true;
}
