#include "parser_internal.h"

static bool
intent_defaults_append_name(char ***items, size_t *count, size_t *capacity,
                            const char *name)
{
    char **grown;
    char *owned_name;
    size_t next_capacity;

    if (items == NULL || count == NULL || capacity == NULL || name == NULL)
        return false;

    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity <= *count)
            return false;
        if (next_capacity > (size_t)-1 / sizeof(char *))
            return false;
        grown = realloc(*items, next_capacity * sizeof(char *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }

    owned_name = pergyra_strdup(name);
    if (owned_name == NULL)
        return false;

    (*items)[*count] = owned_name;
    *count += 1;
    return true;
}

void
parse_intent_apply_defaults(ASTNode *intent)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL)
        return;

    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        if (step->data.intent_step.who_count == 0
            && intent->data.intent_decl.default_who_count > 0) {
            bool copied_any = false;
            for (size_t j = 0; j < intent->data.intent_decl.default_who_count; j++) {
                if (intent_defaults_append_name(&step->data.intent_step.who_names,
                        &step->data.intent_step.who_count,
                        &step->data.intent_step.who_capacity,
                        intent->data.intent_decl.default_who_names[j])) {
                    copied_any = true;
                }
            }
            step->data.intent_step.inherited_who_from_intent = copied_any;
        }

        if (step->data.intent_step.where_type == NULL
            && intent->data.intent_decl.default_where_type != NULL) {
            step->data.intent_step.where_type =
                ast_clone(intent->data.intent_decl.default_where_type);
            step->data.intent_step.inherited_where_from_intent =
                step->data.intent_step.where_type != NULL;
        }
    }
}
