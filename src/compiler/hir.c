#include "hir.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

#include "hir_internal.h"

static bool
hir_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    size_t next;

    if (capacity == NULL || elem_size == 0)
        return false;
    if (*capacity == 0) {
        next = initial;
    } else {
        if (*capacity > SIZE_MAX / 2)
            return false;
        next = *capacity * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *capacity = next;
    return true;
}

static bool
append_ast(ASTNode ***items, size_t *count, size_t *capacity, ASTNode *node)
{
    if (items == NULL || count == NULL || capacity == NULL)
        return false;
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_next_capacity(&next_capacity, 8, sizeof(ASTNode *)))
            return false;
        ASTNode **grown = realloc(*items, next_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = node;
    (*count)++;
    return true;
}

static bool
append_item(HIRTopLevelItem **items,
            size_t *count,
            size_t *capacity,
            HIRTopLevelItem item)
{
    if (items == NULL || count == NULL || capacity == NULL)
        return false;
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_next_capacity(&next_capacity, 16, sizeof(HIRTopLevelItem)))
            return false;
        HIRTopLevelItem *grown = realloc(*items, next_capacity * sizeof(HIRTopLevelItem));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = item;
    (*count)++;
    return true;
}

static bool
hir_append_resource_flow_symbol(HIRRoutine *routine,
                                const PgyResourceFlowFact *fact)
{
    HIRResourceFlowSymbol *grown;
    size_t next_capacity;

    if (routine == NULL || fact == NULL)
        return false;
    if (routine->resource_flow_symbol_count
            == routine->resource_flow_symbol_capacity) {
        next_capacity = routine->resource_flow_symbol_capacity;
        if (!hir_next_capacity(&next_capacity, 8,
                               sizeof(HIRResourceFlowSymbol)))
            return false;
        grown = realloc(routine->resource_flow_symbols,
                        next_capacity * sizeof(HIRResourceFlowSymbol));
        if (grown == NULL)
            return false;
        routine->resource_flow_symbols = grown;
        routine->resource_flow_symbol_capacity = next_capacity;
    }
    HIRResourceFlowSymbol *symbol =
        &routine->resource_flow_symbols[routine->resource_flow_symbol_count];
    memset(symbol, 0, sizeof(*symbol));
    symbol->stable_index = fact->stable_index;
    symbol->declaration_syntax_id = fact->declaration_syntax_id;
    symbol->line = fact->line;
    symbol->column = fact->column;
    symbol->symbol_kind = fact->symbol_kind;
    symbol->is_parameter = fact->is_parameter;
    symbol->parameter_index = fact->parameter_index;
    if (fact->name != NULL) {
        symbol->name = pergyra_strdup(fact->name);
        if (symbol->name == NULL)
            return false;
    }
    routine->resource_flow_symbol_count++;
    return true;
}

static bool
hir_attach_resource_flow_facts(HIRProgram *hir,
                               const PgyResourceFlowFact *facts,
                               size_t fact_count,
                               char **error_message)
{
    if (hir == NULL)
        return false;
    if (facts == NULL && fact_count != 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR ResourceFlowUniverse fact storage is missing");
        return false;
    }
    for (size_t i = 0; i < fact_count; i++) {
        bool matched = false;
        for (size_t r = 0; r < hir->routine_count; r++) {
            HIRRoutine *routine = &hir->routines[r];
            if (routine->source_syntax_id != facts[i].function_syntax_id)
                continue;
            if (!hir_append_resource_flow_symbol(routine, &facts[i])) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "Out of memory while attaching resource-flow facts");
                return false;
            }
            matched = true;
            break;
        }
        if (!matched) {
            if (error_message != NULL) {
                char detail[128];
                (void) snprintf(
                    detail, sizeof(detail),
                    "ResourceFlowUniverse fact references an unknown HIR routine (function_syntax_id=%u)",
                    facts[i].function_syntax_id);
                *error_message = pergyra_strdup(detail);
            }
            return false;
        }
    }
    return true;
}

static bool
hir_append_function_param_flow_summary(
    HIRRoutine *routine,
    const PgyFunctionParamFlowFact *fact,
    char **error_message)
{
    HIRFunctionParamFlowSummary *grown;
    size_t next_capacity;

    if (routine == NULL || fact == NULL)
        return false;
    if (routine->function_param_flow_summary_count
        == routine->function_param_flow_summary_capacity) {
        next_capacity = routine->function_param_flow_summary_capacity;
        if (!hir_next_capacity(&next_capacity, 4,
                               sizeof(HIRFunctionParamFlowSummary)))
            return false;
        grown = realloc(routine->function_param_flow_summaries,
                        next_capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        routine->function_param_flow_summaries = grown;
        routine->function_param_flow_summary_capacity = next_capacity;
    }
    for (size_t i = 0; i < routine->function_param_flow_summary_count; i++) {
        if (routine->function_param_flow_summaries[i].parameter_index
            == fact->parameter_index) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "duplicate HIR function parameter flow summary identity");
            return false;
        }
    }
    routine->function_param_flow_summaries[
        routine->function_param_flow_summary_count].parameter_index =
        fact->parameter_index;
    routine->function_param_flow_summaries[
        routine->function_param_flow_summary_count].mask = fact->mask;
    routine->function_param_flow_summary_count++;
    return true;
}

static bool
hir_attach_function_param_flow_facts(
    HIRProgram *hir,
    const PgyFunctionParamFlowFact *facts,
    size_t fact_count,
    char **error_message)
{
    if (hir == NULL || (facts == NULL && fact_count != 0))
        return false;
    for (size_t i = 0; i < fact_count; i++) {
        bool matched = false;
        for (size_t r = 0; r < hir->routine_count; r++) {
            HIRRoutine *routine = &hir->routines[r];
            if (routine->source_syntax_id != facts[i].function_syntax_id)
                continue;
            if (!hir_append_function_param_flow_summary(
                    routine, &facts[i], error_message)) {
                if (error_message != NULL && *error_message == NULL)
                    *error_message = pergyra_strdup(
                        "Out of memory while attaching function parameter flow summaries");
                return false;
            }
            matched = true;
            break;
        }
        if (!matched) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "Function parameter flow fact references an unknown HIR routine");
            return false;
        }
    }
    return true;
}

static const char *
hir_node_name(ASTNode *node)
{
    if (node == NULL)
        return "(null)";

    switch (node->type) {
        case AST_FUNC_DECL:
            return ast_declaration_name(node);
        case AST_CLASS_DECL:
            return ast_class_name(node);
        case AST_TYPE_ALIAS:
            return ast_type_alias_name(node);
        case AST_EXTERN_BLOCK:
            return ast_extern_block_abi(node);
        case AST_ABILITY_DECL:
            return ast_ability_name(node);
        case AST_ROLE_DECL:
            return ast_role_name(node);
        case AST_PARTY_DECL:
            return ast_party_name(node);
        case AST_ROSTER_DECL:
            return ast_roster_name(node);
        case AST_WORLD_DECL:
            return ast_world_name(node);
        case AST_INTENT_DECL:
            return ast_intent_decl_name(node);
        case AST_RELATION_DECL:
            return ast_relation_name(node);
        case AST_EFFECT_DECL:
            return ast_effect_name(node);
        case AST_ZONE_DECL:
            return ast_zone_name(node);
        case AST_EVENT_DECL:
            return ast_event_name(node);
        case AST_LET_DECL:
            return ast_let_name(node);
        default:
            return NULL;
    }
}

const char *
hir_top_level_kind_name(HIRTopLevelKind kind)
{
    switch (kind) {
        case HIR_TOPLEVEL_EXTERN: return "extern";
        case HIR_TOPLEVEL_TYPE: return "type";
        case HIR_TOPLEVEL_ABILITY: return "ability";
        case HIR_TOPLEVEL_ROLE: return "role";
        case HIR_TOPLEVEL_PARTY: return "party";
        case HIR_TOPLEVEL_SYSTEMIC: return "roster";
        case HIR_TOPLEVEL_WORLD: return "world";
        case HIR_TOPLEVEL_RELATION: return "relation";
        case HIR_TOPLEVEL_EFFECT: return "effect";
        case HIR_TOPLEVEL_ZONE: return "zone";
        case HIR_TOPLEVEL_EVENT: return "event";
        case HIR_TOPLEVEL_INTENT: return "intent";
        case HIR_TOPLEVEL_FUNCTION: return "function";
        case HIR_TOPLEVEL_EXECUTABLE: return "executable";
        default: return "unknown";
    }
}

const char *
hir_phase_name(HIRPhase phase)
{
    switch (phase) {
        case HIR_PHASE_EXTERN: return "extern";
        case HIR_PHASE_TYPE: return "type";
        case HIR_PHASE_CAPABILITY: return "capability";
        case HIR_PHASE_DOMAIN: return "domain";
        case HIR_PHASE_ROUTINE: return "routine";
        case HIR_PHASE_EXECUTABLE: return "executable";
        default: return "unknown";
    }
}

static bool
hir_classify_top_level(HIRProgram *hir, ASTNode *node, char **error_message)
{
    HIRTopLevelItem item;
    memset(&item, 0, sizeof(item));
    item.ast = node;
    item.name = hir_node_name(node);

    switch (node->type) {
        case AST_EXTERN_BLOCK:
            item.kind = HIR_TOPLEVEL_EXTERN;
            if (!append_ast(&hir->externs, &hir->extern_count, &hir->extern_capacity, node))
                goto oom;
            break;
        case AST_CLASS_DECL:
        case AST_TYPE_ALIAS:
        case AST_ENUM_DECL:
            item.kind = HIR_TOPLEVEL_TYPE;
            if (!append_ast(&hir->types, &hir->type_count, &hir->type_capacity, node))
                goto oom;
            break;
        case AST_ABILITY_DECL:
            item.kind = HIR_TOPLEVEL_ABILITY;
            if (!append_ast(&hir->abilities, &hir->ability_count, &hir->ability_capacity, node))
                goto oom;
            break;
        case AST_ROLE_DECL:
            item.kind = HIR_TOPLEVEL_ROLE;
            if (!append_ast(&hir->roles, &hir->role_count, &hir->role_capacity, node))
                goto oom;
            break;
        case AST_PARTY_DECL:
            item.kind = HIR_TOPLEVEL_PARTY;
            if (!append_ast(&hir->parties, &hir->party_count, &hir->party_capacity, node))
                goto oom;
            break;
        case AST_ROSTER_DECL:
            item.kind = HIR_TOPLEVEL_SYSTEMIC;
            if (!append_ast(&hir->rosters, &hir->roster_count, &hir->roster_capacity, node))
                goto oom;
            break;
        case AST_WORLD_DECL:
            item.kind = HIR_TOPLEVEL_WORLD;
            if (!append_ast(&hir->worlds, &hir->world_count, &hir->world_capacity, node))
                goto oom;
            break;
        case AST_INTENT_DECL:
            item.kind = HIR_TOPLEVEL_INTENT;
            if (!append_ast(&hir->intents, &hir->intent_count, &hir->intent_capacity, node))
                goto oom;
            break;
        case AST_RELATION_DECL:
            item.kind = HIR_TOPLEVEL_RELATION;
            if (!append_ast(&hir->relations, &hir->relation_count, &hir->relation_capacity, node))
                goto oom;
            break;
        case AST_EFFECT_DECL:
            item.kind = HIR_TOPLEVEL_EFFECT;
            if (!append_ast(&hir->effects, &hir->effect_count, &hir->effect_capacity, node))
                goto oom;
            break;
        case AST_ZONE_DECL:
            item.kind = HIR_TOPLEVEL_ZONE;
            if (!append_ast(&hir->zones, &hir->zone_count, &hir->zone_capacity, node))
                goto oom;
            break;
        case AST_EVENT_DECL:
            item.kind = HIR_TOPLEVEL_EVENT;
            if (!append_ast(&hir->events, &hir->event_count, &hir->event_capacity, node))
                goto oom;
            break;
        case AST_FUNC_DECL:
            item.kind = HIR_TOPLEVEL_FUNCTION;
            if (!append_ast(&hir->functions, &hir->function_count, &hir->function_capacity, node))
                goto oom;
            if (ast_declaration_name(node) != NULL
                && strcmp(ast_declaration_name(node), "Main") == 0) {
                hir->has_main_function = true;
            }
            break;

        case AST_LET_DECL:
        case AST_WITH_STMT:
        case AST_PARALLEL_BLOCK:
        case AST_FOR_LOOP:
        case AST_WHILE_LOOP:
        case AST_IF_STMT:
        case AST_RETURN:
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_SELECT_STMT:
        case AST_MATCH_STMT:
        case AST_BINARY:
        case AST_UNARY:
        case AST_CALL:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
        case AST_ASSIGNMENT:
        case AST_AWAIT_EXPR:
        case AST_CHANNEL_SEND:
        case AST_CHANNEL_RECV:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_IDENTIFIER:
        case AST_ASYNC_BLOCK:
        case AST_SPAWN_EXPR:
        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_LAMBDA_EXPR:
        case AST_BLOCK:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables,
                            &hir->executable_count,
                            &hir->executable_capacity,
                            node))
                goto oom;
            break;

        case AST_IMPORT_DECL:
        case AST_USE_DECL:
            /* Already resolved by driver; skip. */
            break;
        case AST_LIFECYCLE_DECL:
            /* Semantic-only domain annotation: consumed by the lifecycle
             * analysis pass (static rejects + runtime-guard annotations);
             * lowers to no code of its own. */
            break;
        case AST_UNSAFE_BLOCK:
        case AST_TRANSACTION_BLOCK:
        case AST_DEFER_STMT:
        case AST_BIND_STMT:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables,
                            &hir->executable_count,
                            &hir->executable_capacity,
                            node))
                goto oom;
            break;

        default:
            if (error_message != NULL) {
                char message[128];
                snprintf(message, sizeof(message),
                         "Unsupported top-level AST node for HIR lowering: %d",
                         (int)node->type);
                *error_message = pergyra_strdup(message);
            }
            return false;
    }

    if (!append_item(&hir->items, &hir->item_count, &hir->item_capacity, item))
        goto oom;
    if (!hir_append_decl_and_routine(hir, item, error_message))
        goto oom;

    return true;

oom:
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
}

static bool
hir_append_synthetic_executable_routine(HIRProgram *hir, char **error_message)
{
    ASTNode *func;
    ASTNode *body;
    HIRTopLevelItem item;

    if (hir == NULL || hir->executable_count == 0 || hir->synthetic_executable_func != NULL)
        return true;

    func = ast_create_function("__pgy_top_level_exec");
    body = ast_create_block();
    if (func == NULL || body == NULL) {
        ast_destroy(func);
        ast_destroy(body);
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }

    /* The synthetic entry returns Void. Without an explicit return type the
     * backends disagree on the signature (the forward declaration normalizes
     * to Void while body emission defaults elsewhere), so set it here. */
    {
        ASTNode *void_type = ast_create_type("Void");
        if (void_type == NULL || !ast_func_set_return_type(func, void_type)) {
            ast_destroy(void_type);
            ast_destroy(func);
            ast_destroy(body);
            if (error_message != NULL)
                *error_message = pergyra_strdup("Out of memory");
            return false;
        }
    }

    for (size_t i = 0; i < hir->executable_count; i++)
        ast_add_statement(body, hir->executables[i]);
    if (!ast_func_attach_body(func, body)) {
        ast_destroy(func);
        ast_destroy(body);
        if (error_message != NULL)
            *error_message = pergyra_strdup("Internal HIR synthetic body error");
        return false;
    }
    hir->synthetic_executable_func = func;
    /* Top-level statements were type-checked before a synthetic executable
     * existed. Reuse the program root's stable identity so their semantic
     * rows still bind to exactly one HIR routine. */
    if (hir->source_program_syntax_id != 0)
        func->stable_id = hir->source_program_syntax_id;

    /* Also register the synthetic entry among the regular functions so the
     * downstream MIR program exposes it via the function inventory: this is
     * what sets has_top_level_exec and lets the backends find the routine to
     * call from the generated main() wrapper. */
    if (!append_ast(&hir->functions, &hir->function_count,
            &hir->function_capacity, func)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }

    memset(&item, 0, sizeof(item));
    item.kind = HIR_TOPLEVEL_EXECUTABLE;
    item.ast = func;
    item.name = ast_declaration_name(func);

    if (!append_item(&hir->items, &hir->item_count, &hir->item_capacity, item)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }
    if (!hir_append_decl_and_routine(hir, item, error_message))
        return false;

    return true;
}

HIRProgram *
hir_lower_with_resource_and_param_flow_facts(
    ASTNode *annotated_ast,
    const PgyResourceFlowFact *facts,
    size_t fact_count,
    const PgyFunctionParamFlowFact *param_facts,
    size_t param_fact_count,
    char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (annotated_ast == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Cannot lower null AST");
        return NULL;
    }
    if (annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR lowering requires AST_PROGRAM root");
        return NULL;
    }

    HIRProgram *hir = calloc(1, sizeof(HIRProgram));
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }
    hir->has_resource_flow_facts = facts != NULL || fact_count != 0;
    hir->has_function_param_flow_facts =
        param_facts != NULL || param_fact_count != 0;
    hir->source_program_syntax_id = ast_node_stable_id(annotated_ast);

    for (size_t i = 0; i < ast_program_statement_count(annotated_ast); i++) {
        if (!hir_classify_top_level(hir,
                                    ast_program_statement(annotated_ast, i),
                                    error_message)) {
            hir_destroy(hir);
            return NULL;
        }
    }

    if (!hir_append_synthetic_executable_routine(hir, error_message)) {
        hir_destroy(hir);
        return NULL;
    }

    if (!hir_finish_callgraph(hir, error_message)) {
        hir_destroy(hir);
        return NULL;
    }

    if (!hir_attach_resource_flow_facts(hir, facts, fact_count,
                                        error_message)) {
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_function_param_flow_facts(
            hir, param_facts, param_fact_count, error_message)) {
        hir_destroy(hir);
        return NULL;
    }

    return hir;
}

HIRProgram *
hir_lower_with_resource_flow_facts(ASTNode *annotated_ast,
                                   const PgyResourceFlowFact *facts,
                                   size_t fact_count,
                                   char **error_message)
{
    return hir_lower_with_resource_and_param_flow_facts(
        annotated_ast, facts, fact_count, NULL, 0, error_message);
}

HIRProgram *
hir_lower(ASTNode *annotated_ast, char **error_message)
{
    return hir_lower_with_resource_flow_facts(
        annotated_ast, NULL, 0, error_message);
}
