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
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        semantic_type_resolution_collect_type_refs(
            involves->data.intent_involves.subject_type,
            ctx,
            involves,
            involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "<intent-binding>",
            "intent involves type lookup");
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.value_count; i++) {
        ASTNode *value = intent_decl->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        semantic_type_resolution_collect_type_refs(
            value->data.intent_value.value_type,
            ctx,
            value,
            value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "<intent-value>",
            "intent value type lookup");
    }

    semantic_type_resolution_collect_type_refs(
        intent_decl->data.intent_decl.default_where_type,
        ctx,
        intent_decl,
        intent_decl->data.intent_decl.name != NULL
            ? intent_decl->data.intent_decl.name : "<intent>",
        "intent default where-type lookup");

    for (size_t i = 0; i < intent_decl->data.intent_decl.step_count; i++) {
        ASTNode *step = intent_decl->data.intent_decl.steps[i];
        char *step_consumer_name;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        step_consumer_name = resolution_intent_strdup_fmt(
            "intent %s.%s",
            intent_decl->data.intent_decl.name != NULL
                ? intent_decl->data.intent_decl.name : "<intent>",
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>");
        if (step_consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            step->data.intent_step.where_type,
            ctx,
            step,
            step_consumer_name,
            "intent step where-type lookup");
        semantic_type_resolution_precollect_required_abilities(
            step->data.intent_step.required_abilities,
            step->data.intent_step.required_ability_count,
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        semantic_type_resolution_record_string_dependency(
            ctx,
            step,
            step_consumer_name,
            step->data.intent_step.causes_effect,
            "intent step causes-effect lookup");
        free(step_consumer_name);
    }
}
