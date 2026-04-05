#include "hir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static bool
append_ast(ASTNode ***items, size_t *count, ASTNode *node)
{
    ASTNode **grown = realloc(*items, (*count + 1) * sizeof(ASTNode *));
    if (grown == NULL)
        return false;
    grown[*count] = node;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_item(HIRTopLevelItem **items, size_t *count, HIRTopLevelItem item)
{
    HIRTopLevelItem *grown = realloc(*items, (*count + 1) * sizeof(HIRTopLevelItem));
    if (grown == NULL)
        return false;
    grown[*count] = item;
    *items = grown;
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
        case AST_EXTERN_BLOCK:
            return node->data.extern_block.abi;
        case AST_ABILITY_DECL:
            return node->data.ability_decl.name;
        case AST_ROLE_DECL:
            return node->data.role_decl.name;
        case AST_PARTY_DECL:
            return node->data.party_decl.name;
        case AST_SYSTEMIC_DECL:
            return node->data.systemic_decl.name;
        case AST_WORLD_DECL:
            return node->data.world_decl.name;
        case AST_RELATION_DECL:
            return node->data.relation_decl.name;
        case AST_EFFECT_DECL:
            return node->data.effect_decl.name;
        case AST_ZONE_DECL:
            return node->data.zone_decl.name;
        case AST_ACTOR_DECL:
            return node->data.actor_decl.name;
        case AST_EVENT_DECL:
            return node->data.event_decl.name;
        case AST_LET_DECL:
            return node->data.let_decl.name;
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
        case HIR_TOPLEVEL_SYSTEMIC: return "systemic";
        case HIR_TOPLEVEL_WORLD: return "world";
        case HIR_TOPLEVEL_RELATION: return "relation";
        case HIR_TOPLEVEL_EFFECT: return "effect";
        case HIR_TOPLEVEL_ZONE: return "zone";
        case HIR_TOPLEVEL_ACTOR: return "actor";
        case HIR_TOPLEVEL_EVENT: return "event";
        case HIR_TOPLEVEL_FUNCTION: return "function";
        case HIR_TOPLEVEL_EXECUTABLE: return "executable";
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
            if (!append_ast(&hir->externs, &hir->extern_count, node))
                goto oom;
            break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:
            item.kind = HIR_TOPLEVEL_TYPE;
            if (!append_ast(&hir->types, &hir->type_count, node))
                goto oom;
            break;
        case AST_ABILITY_DECL:
            item.kind = HIR_TOPLEVEL_ABILITY;
            if (!append_ast(&hir->abilities, &hir->ability_count, node))
                goto oom;
            break;
        case AST_ROLE_DECL:
            item.kind = HIR_TOPLEVEL_ROLE;
            if (!append_ast(&hir->roles, &hir->role_count, node))
                goto oom;
            break;
        case AST_PARTY_DECL:
            item.kind = HIR_TOPLEVEL_PARTY;
            if (!append_ast(&hir->parties, &hir->party_count, node))
                goto oom;
            break;
        case AST_SYSTEMIC_DECL:
            item.kind = HIR_TOPLEVEL_SYSTEMIC;
            if (!append_ast(&hir->systemics, &hir->systemic_count, node))
                goto oom;
            break;
        case AST_WORLD_DECL:
            item.kind = HIR_TOPLEVEL_WORLD;
            if (!append_ast(&hir->worlds, &hir->world_count, node))
                goto oom;
            break;
        case AST_RELATION_DECL:
            item.kind = HIR_TOPLEVEL_RELATION;
            if (!append_ast(&hir->relations, &hir->relation_count, node))
                goto oom;
            break;
        case AST_EFFECT_DECL:
            item.kind = HIR_TOPLEVEL_EFFECT;
            if (!append_ast(&hir->effects, &hir->effect_count, node))
                goto oom;
            break;
        case AST_ZONE_DECL:
            item.kind = HIR_TOPLEVEL_ZONE;
            if (!append_ast(&hir->zones, &hir->zone_count, node))
                goto oom;
            break;
        case AST_ACTOR_DECL:
            item.kind = HIR_TOPLEVEL_ACTOR;
            if (!append_ast(&hir->actors, &hir->actor_count, node))
                goto oom;
            break;
        case AST_EVENT_DECL:
            item.kind = HIR_TOPLEVEL_EVENT;
            if (!append_ast(&hir->events, &hir->event_count, node))
                goto oom;
            break;
        case AST_FUNC_DECL:
            item.kind = HIR_TOPLEVEL_FUNCTION;
            if (!append_ast(&hir->functions, &hir->function_count, node))
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
            if (!append_ast(&hir->executables, &hir->executable_count, node))
                goto oom;
            break;

        case AST_IMPORT_DECL:
        case AST_USE_DECL:
            /* Already resolved by driver — skip */
            break;
        case AST_UNSAFE_BLOCK:
        case AST_DEFER_STMT:
        case AST_BIND_STMT:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables, &hir->executable_count, node))
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

    if (!append_item(&hir->items, &hir->item_count, item))
        goto oom;

    return true;

oom:
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
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

    return hir;
}

void
hir_destroy(HIRProgram *hir)
{
    if (hir == NULL)
        return;

    free(hir->items);
    free(hir->externs);
    free(hir->types);
    free(hir->abilities);
    free(hir->roles);
    free(hir->parties);
    free(hir->systemics);
    free(hir->worlds);
    free(hir->relations);
    free(hir->effects);
    free(hir->zones);
    free(hir->actors);
    free(hir->events);
    free(hir->functions);
    free(hir->executables);
    free(hir);
}

void
hir_dump(const HIRProgram *hir, FILE *out)
{
    if (out == NULL)
        out = stdout;

    if (hir == NULL) {
        fprintf(out, "HIR: (null)\n");
        return;
    }

    fprintf(out,
            "HIR Program\n"
            "  items: %zu\n"
            "  externs: %zu\n"
            "  types: %zu\n"
            "  abilities: %zu\n"
            "  roles: %zu\n"
            "  parties: %zu\n"
            "  systemics: %zu\n"
            "  worlds: %zu\n"
            "  actors: %zu\n"
            "  events: %zu\n"
            "  functions: %zu\n"
            "  executables: %zu\n"
            "  has_main: %s\n",
            hir->item_count,
            hir->extern_count,
            hir->type_count,
            hir->ability_count,
            hir->role_count,
            hir->party_count,
            hir->systemic_count,
            hir->world_count,
            hir->actor_count,
            hir->event_count,
            hir->function_count,
            hir->executable_count,
            hir->has_main_function ? "true" : "false");

    for (size_t i = 0; i < hir->item_count; i++) {
        const HIRTopLevelItem *item = &hir->items[i];
        fprintf(out, "  [%02zu] %-10s", i, hir_top_level_kind_name(item->kind));
        if (item->name != NULL)
            fprintf(out, " %s", item->name);
        fprintf(out, "\n");
    }
}
