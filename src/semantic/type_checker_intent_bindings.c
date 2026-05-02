#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "../common/string_compat.h"

ASTNode *
find_intent_involves_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

ASTNode *
find_intent_value_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.value_count; i++) {
        ASTNode *value = intent->data.intent_decl.values[i];
        if (value != NULL && value->type == AST_INTENT_VALUE
            && value->data.intent_value.alias != NULL
            && strcmp(value->data.intent_value.alias, alias) == 0) {
            return value;
        }
    }
    return NULL;
}

static ASTNode *
find_unique_intent_involves_by_type_name(ASTNode *intent,
                                         const char *type_name,
                                         const char **alias_out)
{
    ASTNode *matched = NULL;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent == NULL || intent->type != AST_INTENT_DECL || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        const char *participant_type_name = intent_involves_type_name(involves);

        if (participant_type_name == NULL
            || strcmp(participant_type_name, type_name) != 0) {
            continue;
        }

        if (matched != NULL)
            return NULL;
        matched = involves;
    }

    if (matched != NULL && alias_out != NULL)
        *alias_out = matched->data.intent_involves.alias;
    return matched;
}

ASTNode *
intent_step_resolve_transfer_target_involves(ASTNode *intent_decl,
                                             ASTNode *step,
                                             const char **resolved_alias_out)
{
    const char *to_name;
    ASTNode *to_involves;
    const char *resolved_alias = NULL;

    if (resolved_alias_out != NULL)
        *resolved_alias_out = NULL;
    if (intent_decl == NULL || step == NULL || step->type != AST_INTENT_STEP)
        return NULL;

    to_name = step->data.intent_step.transfer_to_alias;
    if (to_name == NULL)
        return NULL;

    to_involves = find_intent_involves_local(intent_decl, to_name);
    if (to_involves == NULL) {
        to_involves = find_unique_intent_involves_by_type_name(
            intent_decl, to_name, &resolved_alias);
        if (to_involves != NULL && resolved_alias != NULL) {
            free(step->data.intent_step.transfer_to_alias);
            step->data.intent_step.transfer_to_alias =
                pergyra_strdup(resolved_alias);
        }
    } else {
        resolved_alias = to_name;
    }

    if (resolved_alias_out != NULL)
        *resolved_alias_out = resolved_alias;
    return to_involves;
}
