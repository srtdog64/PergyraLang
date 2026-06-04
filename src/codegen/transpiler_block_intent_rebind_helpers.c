#include "transpiler_block_intent_rebind_helpers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_intent_context.h"
#include "transpiler_intent_participant.h"
#include "transpiler_intent_zone_slot.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"

bool
emit_intent_step_rebind_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                           ASTNode *intent, ASTNode *step,
                                           size_t step_index)
{
    const char *zone_alias;
    const char *zone_type;
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || ast_intent_step_where_type(step) == NULL
        || ast_intent_step_where_type(step)->type != AST_TYPE) {
        return false;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = ast_type_name(ast_intent_step_where_type(step));
    if (zone_alias == NULL || zone_type == NULL)
        return false;

    for (size_t i = 0; i < ast_intent_step_who_count(step); i++) {
        const char *alias = ast_intent_step_who_names(step, NULL)[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);
        char participant_c_type_buf[256];
        const char *participant_c_type = NULL;
        char surface_desc[256];

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || ast_intent_involves_subject_type(involves) == NULL) {
            continue;
        }

        snprintf(surface_desc, sizeof(surface_desc),
            "intent step participant '%s'", alias);
        if (transpiler_require_ast_c_type_copy(ctx,
                ast_intent_involves_subject_type(involves),
                surface_desc,
                participant_c_type_buf,
                sizeof(participant_c_type_buf))) {
            participant_c_type = participant_c_type_buf;
        }
        if (participant_c_type == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

bool
emit_intent_step_rebind_bound_zone_aliases_with_metadata(CodeBuf *out,
                                                         TranspilerCtx *ctx,
                                                         ASTNode *intent,
                                                         const char *zone_type,
                                                         const char *zone_alias,
                                                         const char **who_aliases,
                                                         size_t who_alias_count,
                                                         size_t step_index,
                                                         const IntentBindingMetadataView *bindings)
{
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL
        || zone_alias == NULL || zone_type == NULL) {
        return false;
    }

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name =
            resolve_intent_zone_slot_name_for_zone_with_bindings(
                ctx, intent, zone_type, alias, bindings);
        const char *participant_type = intent_zone_binding_type_name_with_bindings(
            intent, alias, bindings);
        char participant_c_type_buf[256];
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || participant_type == NULL) {
            continue;
        }

        participant_c_type = NULL;
        if (transpiler_require_type_name_c_type_copy(ctx, participant_type,
                "intent step participant metadata",
                participant_c_type_buf,
                sizeof(participant_c_type_buf))) {
            participant_c_type = participant_c_type_buf;
        }
        if (participant_c_type == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}
