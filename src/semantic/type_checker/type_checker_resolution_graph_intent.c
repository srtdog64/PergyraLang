#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
resolution_intent_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void
semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                     SemanticContext *ctx)
{
    ASTNode **involves_nodes;
    size_t involve_count;
    ASTNode **values;
    size_t value_count;
    ASTNode **steps;
    size_t step_count;
    const char *intent_name;

    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL || ctx == NULL)
        return;
    intent_name = ast_intent_decl_name(intent_decl);

    involves_nodes = ast_intent_decl_involves(intent_decl, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        semantic_type_resolution_collect_type_refs(
            ast_intent_involves_subject_type(involves),
            ctx,
            involves,
            ast_intent_involves_alias(involves) != NULL
                ? ast_intent_involves_alias(involves) : "<intent-binding>",
            "intent involves type lookup");
    }

    values = ast_intent_decl_values(intent_decl, &value_count);
    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        semantic_type_resolution_collect_type_refs(
            ast_intent_value_type(value),
            ctx,
            value,
            ast_intent_value_alias(value) != NULL
                ? ast_intent_value_alias(value) : "<intent-value>",
            "intent value type lookup");
    }

    semantic_type_resolution_collect_type_refs(
        ast_intent_decl_return_type(intent_decl),
        ctx,
        intent_decl,
        intent_name != NULL ? intent_name : "<intent>",
        "intent return-type lookup");

    semantic_type_resolution_collect_type_refs(
        ast_intent_decl_default_where_type(intent_decl),
        ctx,
        intent_decl,
        intent_name != NULL ? intent_name : "<intent>",
        "intent default where-type lookup");

    steps = ast_intent_decl_steps(intent_decl, &step_count);
    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        char *step_consumer_name;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        step_consumer_name = resolution_intent_strdup_fmt(
            "intent %s.%s",
            intent_name != NULL ? intent_name : "<intent>",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>");
        if (step_consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            ast_intent_step_where_type(step),
            ctx,
            step,
            step_consumer_name,
            "intent step where-type lookup");
        semantic_type_resolution_precollect_required_abilities(
            ast_intent_step_required_abilities(step, NULL),
            ast_intent_step_required_ability_count(step),
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        semantic_type_resolution_record_string_dependency(
            ctx,
            step,
            step_consumer_name,
            ast_intent_step_causes_effect(step),
            "intent step causes-effect lookup");
        free(step_consumer_name);
    }
}
