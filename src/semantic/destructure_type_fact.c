#include "destructure_type_fact.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker.h"
#include "type_checker_internal.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static bool
destructure_type_fact_reserve(SemanticContext *ctx, size_t needed)
{
    size_t capacity;
    PgyDestructureTypeFact *grown;

    if (ctx == NULL)
        return false;
    if (needed <= ctx->destructure_type_fact_capacity)
        return true;
    capacity = ctx->destructure_type_fact_capacity == 0
        ? 8
        : ctx->destructure_type_fact_capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->destructure_type_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    ctx->destructure_type_facts = grown;
    ctx->destructure_type_fact_capacity = capacity;
    return true;
}

bool
semantic_destructure_type_fact_record(SemanticContext *ctx,
                                      const ASTNode *destructure_node,
                                      size_t binding_index,
                                      size_t binding_count,
                                      const Type *binding_type)
{
    uint32_t function_id;
    uint32_t destructure_id;
    const char *type_name;
    PgyDestructureTypeFact *fact;

    if (ctx == NULL || destructure_node == NULL || binding_type == NULL
        || binding_count == 0 || binding_index >= binding_count)
        return false;
    function_id = semantic_current_routine_syntax_id(ctx);
    if (function_id == 0)
        return true;
    destructure_id = ast_node_stable_id(destructure_node);
    type_name = type_name_or_unknown(binding_type);
    if (destructure_id == 0 || type_name == NULL || type_name[0] == '\0'
        || strcmp(type_name, "Unknown") == 0
        || strcmp(type_name, "<unknown>") == 0)
        return false;

    for (size_t i = 0; i < ctx->destructure_type_fact_count; i++) {
        fact = &ctx->destructure_type_facts[i];
        if (fact->function_syntax_id != function_id
            || fact->destructure_syntax_id != destructure_id
            || fact->binding_index != binding_index)
            continue;
        return fact->binding_count == binding_count
            && strcmp(fact->binding_type_name, type_name) == 0;
    }

    if (!destructure_type_fact_reserve(ctx,
            ctx->destructure_type_fact_count + 1))
        return false;
    fact = &ctx->destructure_type_facts[ctx->destructure_type_fact_count];
    fact->function_syntax_id = function_id;
    fact->destructure_syntax_id = destructure_id;
    fact->binding_index = binding_index;
    fact->binding_count = binding_count;
    fact->binding_type_name = pergyra_strdup(type_name);
    if (fact->binding_type_name == NULL)
        return false;
    ctx->destructure_type_fact_count++;
    return true;
}

void
pgy_destructure_type_facts_destroy(PgyDestructureTypeFact *facts, size_t count)
{
    if (facts == NULL)
        return;
    for (size_t i = 0; i < count; i++)
        free(facts[i].binding_type_name);
    free(facts);
}
