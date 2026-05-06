#ifndef PGY_TRANSPILER_INTENT_ZONE_BINDING_EMIT_H
#define PGY_TRANSPILER_INTENT_ZONE_BINDING_EMIT_H

#include "transpiler_intent_zone_slot.h"

static void
emit_intent_step_restore_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                            ASTNode *intent, ASTNode *step,
                                            size_t step_index)
{
    const char *zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_type == NULL)
        return;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}
static void
emit_intent_step_restore_bound_zone_aliases_with_metadata(CodeBuf *out,
                                                          TranspilerCtx *ctx,
                                                          ASTNode *intent,
                                                          const char *zone_type,
                                                          const char **who_aliases,
                                                          size_t who_alias_count,
                                                          size_t step_index)
{
    if (out == NULL || ctx == NULL || intent == NULL || zone_type == NULL)
        return;

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

static bool
intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *p = action_decl->data.func_decl.params[i];
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}
static void
emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;
    size_t binding_count = 0;
    size_t participant_index = 0;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    mir_routine = transpiler_find_mir_intent(ctx, node);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    binding_count = node->data.intent_decl.binding_count > 0
        ? node->data.intent_decl.binding_count
        : (node->data.intent_decl.involve_count + node->data.intent_decl.value_count);
    codebuf_write(buf, "\nbool\n%s(", node->data.intent_decl.name);
    for (size_t i = 0; i < binding_count; i++) {
        ASTNode *binding = node->data.intent_decl.binding_count > 0
            ? node->data.intent_decl.bindings[i]
            : (i < node->data.intent_decl.involve_count
                ? node->data.intent_decl.involves[i]
                : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
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
            alias = binding->data.intent_involves.alias != NULL
                ? binding->data.intent_involves.alias : "participant";
            snprintf(surface_desc, sizeof(surface_desc),
                "intent participant '%s' of '%s'",
                alias != NULL ? alias : "(anonymous)",
                node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
            if (participant_type != NULL) {
                pt = transpiler_require_type_name_c_type(ctx, participant_type, surface_desc);
                pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
            } else if (binding->data.intent_involves.subject_type != NULL) {
                pt = transpiler_require_ast_c_type(
                    ctx, binding->data.intent_involves.subject_type, surface_desc);
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

        alias = binding != NULL && binding->data.intent_value.alias != NULL
            ? binding->data.intent_value.alias : "value";
        snprintf(surface_desc, sizeof(surface_desc),
            "intent value '%s' of '%s'",
            alias,
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        pt = transpiler_require_ast_c_type(
            ctx,
            binding != NULL ? binding->data.intent_value.value_type : NULL,
            surface_desc);
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        codebuf_write(buf, "%s %s", pt, alias);
    }
    codebuf_write(buf, ");\n");
    free((void *)participant_aliases);
    free((void *)participant_types);
}

static bool
transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx, ASTNode *intent)
{
    if (ctx == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES
            || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, involves->data.intent_involves.subject_type)) {
            return false;
        }
    }
    for (size_t i = 0; i < intent->data.intent_decl.value_count; i++) {
        ASTNode *value = intent->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE
            || value->data.intent_value.value_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, value->data.intent_value.value_type)) {
            return false;
        }
    }
    return true;
}

#endif /* PGY_TRANSPILER_INTENT_ZONE_BINDING_EMIT_H */
