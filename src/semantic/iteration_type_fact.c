#include "iteration_type_fact.h"

#include <stdlib.h>
#include <string.h>

#include "type_checker.h"
#include "type_checker_internal.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static bool
iteration_type_fact_reserve(SemanticContext *ctx, size_t needed)
{
    size_t capacity;
    PgyIterationTypeFact *grown;

    if (ctx == NULL)
        return false;
    if (needed <= ctx->iteration_type_fact_capacity)
        return true;
    capacity = ctx->iteration_type_fact_capacity == 0
        ? 8
        : ctx->iteration_type_fact_capacity;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->iteration_type_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    ctx->iteration_type_facts = grown;
    ctx->iteration_type_fact_capacity = capacity;
    return true;
}

bool
semantic_iteration_type_fact_record(SemanticContext *ctx,
                                    const ASTNode *iteration_node,
                                    const Type *binding_type,
                                    const Type *iterable_type,
                                    bool collection_hoisted)
{
    uint32_t function_id;
    uint32_t iteration_id;
    const char *binding_name;
    const char *iterable_name;
    PgyIterationTypeFact *fact;

    if (ctx == NULL || iteration_node == NULL || binding_type == NULL
        || iterable_type == NULL)
        return false;
    function_id = ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl)
        : ast_node_stable_id(ctx->program_root);
    iteration_id = ast_node_stable_id(iteration_node);
    binding_name = type_name_or_unknown(binding_type);
    iterable_name = type_name_or_unknown(iterable_type);
    if (function_id == 0 || iteration_id == 0
        || binding_name == NULL || iterable_name == NULL
        || binding_name[0] == '\0' || iterable_name[0] == '\0'
        || strcmp(binding_name, "Unknown") == 0
        || strcmp(iterable_name, "Unknown") == 0)
        return false;

    for (size_t i = 0; i < ctx->iteration_type_fact_count; i++) {
        fact = &ctx->iteration_type_facts[i];
        if (fact->function_syntax_id != function_id
            || fact->iteration_syntax_id != iteration_id)
            continue;
        if (strcmp(fact->binding_type_name, binding_name) != 0
            || strcmp(fact->iterable_type_name, iterable_name) != 0
            || fact->collection_hoisted != collection_hoisted)
            return false;
        return true;
    }

    if (!iteration_type_fact_reserve(ctx,
            ctx->iteration_type_fact_count + 1))
        return false;
    fact = &ctx->iteration_type_facts[ctx->iteration_type_fact_count];
    fact->function_syntax_id = function_id;
    fact->iteration_syntax_id = iteration_id;
    fact->binding_type_name = pergyra_strdup(binding_name);
    fact->iterable_type_name = pergyra_strdup(iterable_name);
    fact->collection_hoisted = collection_hoisted;
    if (fact->binding_type_name == NULL || fact->iterable_type_name == NULL) {
        free(fact->binding_type_name);
        free(fact->iterable_type_name);
        fact->binding_type_name = NULL;
        fact->iterable_type_name = NULL;
        return false;
    }
    ctx->iteration_type_fact_count++;
    return true;
}

void
pgy_iteration_type_facts_destroy(PgyIterationTypeFact *facts, size_t count)
{
    if (facts == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        free(facts[i].binding_type_name);
        free(facts[i].iterable_type_name);
    }
    free(facts);
}
