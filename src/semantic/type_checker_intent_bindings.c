#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "../common/string_compat.h"

ASTNode *
find_intent_involves_local(ASTNode *intent, const char *alias)
{
    ASTNode **involves_nodes;
    size_t involve_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && ast_intent_involves_alias(involves) != NULL
            && strcmp(ast_intent_involves_alias(involves), alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

ASTNode *
find_intent_value_local(ASTNode *intent, const char *alias)
{
    ASTNode **values;
    size_t value_count;

    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    values = ast_intent_decl_values(intent, &value_count);
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        if (value != NULL && value->type == AST_INTENT_VALUE
            && ast_intent_value_alias(value) != NULL
            && strcmp(ast_intent_value_alias(value), alias) == 0) {
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
    ASTNode **involves_nodes;
    size_t involve_count;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent == NULL || intent->type != AST_INTENT_DECL || type_name == NULL)
        return NULL;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
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
        *alias_out = ast_intent_involves_alias(matched);
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

    to_name = ast_intent_step_transfer_to_alias(step);
    if (to_name == NULL)
        return NULL;

    to_involves = find_intent_involves_local(intent_decl, to_name);
    if (to_involves == NULL) {
        to_involves = find_unique_intent_involves_by_type_name(
            intent_decl, to_name, &resolved_alias);
        if (to_involves != NULL && resolved_alias != NULL) {
            (void)ast_intent_step_replace_transfer_to_alias_copy(
                step, resolved_alias);
        }
    } else {
        resolved_alias = to_name;
    }

    if (resolved_alias_out != NULL)
        *resolved_alias_out = resolved_alias;
    return to_involves;
}
