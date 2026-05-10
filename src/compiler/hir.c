#include "hir.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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
append_index_unique(size_t **items, size_t *count, size_t *capacity, size_t value)
{
    if (items == NULL || count == NULL || capacity == NULL)
        return false;
    if (*count > 0 && *items == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }

    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_next_capacity(&next_capacity, 4, sizeof(size_t)))
            return false;
        size_t *grown = realloc(*items, next_capacity * sizeof(size_t));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = value;
    (*count)++;
    return true;
}

static const char *
hir_node_name(ASTNode *node)
{
    if (node == NULL)
        return "(null)";

    switch (node->type) {
        case AST_FUNC_DECL:
            return node->data.func_decl.name;
        case AST_CLASS_DECL:
            return node->data.class_decl.name;
        case AST_TYPE_ALIAS:
            return node->data.type_alias.name;
        case AST_EXTERN_BLOCK:
            return node->data.extern_block.abi;
        case AST_ABILITY_DECL:
            return node->data.ability_decl.name;
        case AST_ROLE_DECL:
            return node->data.role_decl.name;
        case AST_PARTY_DECL:
            return node->data.party_decl.name;
        case AST_ROSTER_DECL:
            return node->data.roster_decl.name;
        case AST_WORLD_DECL:
            return node->data.world_decl.name;
        case AST_INTENT_DECL:
            return node->data.intent_decl.name;
        case AST_RELATION_DECL:
            return node->data.relation_decl.name;
        case AST_EFFECT_DECL:
            return node->data.effect_decl.name;
        case AST_ZONE_DECL:
            return node->data.zone_decl.name;
        case AST_EVENT_DECL:
            return node->data.event_decl.name;
        case AST_LET_DECL:
            return node->data.let_decl.name;
        default:
            return NULL;
    }
}

typedef struct
{
    const char *name;
    size_t      index;
} HIRRoutineNameIndex;

static int
hir_routine_name_index_compare(const void *lhs, const void *rhs)
{
    const HIRRoutineNameIndex *a = (const HIRRoutineNameIndex *)lhs;
    const HIRRoutineNameIndex *b = (const HIRRoutineNameIndex *)rhs;
    int name_cmp = strcmp(a->name, b->name);

    if (name_cmp != 0)
        return name_cmp;
    return (a->index > b->index) - (a->index < b->index);
}

static HIRRoutineNameIndex *
hir_build_routine_name_index(const HIRProgram *hir, size_t *out_count)
{
    HIRRoutineNameIndex *index;
    size_t count = 0;

    if (out_count == NULL)
        return NULL;
    *out_count = 0;
    if (hir == NULL || hir->routine_count == 0)
        return NULL;
    if (hir->routine_count > SIZE_MAX / sizeof(HIRRoutineNameIndex))
        return NULL;

    index = calloc(hir->routine_count, sizeof(HIRRoutineNameIndex));
    if (index == NULL)
        return NULL;

    for (size_t i = 0; i < hir->routine_count; i++) {
        if (hir->routines[i].name == NULL)
            continue;
        index[count].name = hir->routines[i].name;
        index[count].index = i;
        count++;
    }

    qsort(index, count, sizeof(HIRRoutineNameIndex),
          hir_routine_name_index_compare);
    *out_count = count;
    return index;
}

static ssize_t
hir_lookup_routine_index_by_name(const HIRRoutineNameIndex *index,
                                 size_t count,
                                 const char *name)
{
    HIRRoutineNameIndex key;
    HIRRoutineNameIndex *found;
    size_t pos;
    size_t best;

    if (index == NULL || count == 0 || name == NULL)
        return -1;

    key.name = name;
    key.index = 0;
    found = bsearch(&key, index, count, sizeof(HIRRoutineNameIndex),
                    hir_routine_name_index_compare);
    if (found == NULL)
        return -1;

    pos = (size_t)(found - index);
    while (pos > 0 && strcmp(index[pos - 1].name, name) == 0)
        pos--;
    best = index[pos].index;
    while (pos < count && strcmp(index[pos].name, name) == 0) {
        if (index[pos].index < best)
            best = index[pos].index;
        pos++;
    }
    return (ssize_t)best;
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
            if (node->data.func_decl.name != NULL
                && strcmp(node->data.func_decl.name, "Main") == 0) {
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
        case AST_TASK_GROUP:
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
            /* Already resolved by driver ??skip */
            break;
        case AST_UNSAFE_BLOCK:
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

    for (size_t i = 0; i < hir->executable_count; i++)
        ast_add_statement(body, hir->executables[i]);
    func->data.func_decl.body = body;
    hir->synthetic_executable_func = func;

    memset(&item, 0, sizeof(item));
    item.kind = HIR_TOPLEVEL_EXECUTABLE;
    item.ast = func;
    item.name = func->data.func_decl.name;

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
hir_lower(ASTNode *annotated_ast, char **error_message)
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

    for (size_t i = 0; i < annotated_ast->data.program.count; i++) {
        if (!hir_classify_top_level(hir,
                                    annotated_ast->data.program.statements[i],
                                    error_message)) {
            hir_destroy(hir);
            return NULL;
        }
    }

    if (!hir_append_synthetic_executable_routine(hir, error_message)) {
        hir_destroy(hir);
        return NULL;
    }

    size_t routine_index_count = 0;
    HIRRoutineNameIndex *routine_index =
        hir_build_routine_name_index(hir, &routine_index_count);
    if (hir->routine_count > 0 && routine_index == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        hir_destroy(hir);
        return NULL;
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < routine->direct_call_count; j++) {
            ssize_t callee = hir_lookup_routine_index_by_name(
                routine_index, routine_index_count, routine->direct_calls[j]);
            if (callee >= 0
                && !append_index_unique(&routine->callee_routine_ids,
                                        &routine->callee_routine_count,
                                        &routine->callee_routine_capacity,
                                        (size_t)callee)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup("Out of memory");
                free(routine_index);
                hir_destroy(hir);
                return NULL;
            }
        }
    }
    free(routine_index);

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < hir->routine_count; i++) {
            HIRRoutine *routine = &hir->routines[i];
            bool is_root = (routine->kind == HIR_TOPLEVEL_INTENT)
                           || (routine->kind == HIR_TOPLEVEL_EXECUTABLE)
                           || routine->is_exported
                           || (routine->name != NULL
                               && strcmp(routine->name, "Main") == 0);
            if (!routine->is_entry_reachable && is_root) {
                routine->is_entry_reachable = true;
                changed = true;
            }
            if (!routine->is_entry_reachable)
                continue;
            for (size_t j = 0; j < routine->callee_routine_count; j++) {
                size_t callee = routine->callee_routine_ids[j];
                if (callee < hir->routine_count && !hir->routines[callee].is_entry_reachable) {
                    hir->routines[callee].is_entry_reachable = true;
                    changed = true;
                }
            }
        }
    }

    return hir;
}
