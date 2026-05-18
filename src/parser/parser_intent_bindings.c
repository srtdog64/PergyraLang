#include "parser_internal.h"

#include <stdlib.h>

static int
parser_intent_name_table_compare(const void *key, const void *entry)
{
    return strcmp((const char *)key, *(const char * const *)entry);
}

bool
intent_append_node(ASTNode ***items, size_t *count, size_t *capacity,
                   ASTNode *node)
{
    ASTNode **grown;
    size_t next_capacity;

    if (items == NULL || count == NULL || capacity == NULL || node == NULL)
        return false;

    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity <= *count)
            return false;
        if (next_capacity > (size_t)-1 / sizeof(ASTNode *))
            return false;
        grown = realloc(*items, next_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }

    (*items)[*count] = node;
    *count += 1;
    return true;
}

void
intent_append_binding(ASTNode *intent, ASTNode *node)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || node == NULL)
        return;
    intent_append_node(&intent->data.intent_decl.bindings,
        &intent->data.intent_decl.binding_count,
        &intent->data.intent_decl.binding_capacity, node);
}

bool
intent_append_name(char ***items, size_t *count, size_t *capacity,
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

bool
intent_has_involves_alias(ASTNode *intent, const char *alias)
{
    if (intent == NULL || alias == NULL || intent->type != AST_INTENT_DECL)
        return false;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL
            && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return true;
        }
    }

    return false;
}

bool
intent_has_value_alias(ASTNode *intent, const char *alias)
{
    if (intent == NULL || alias == NULL || intent->type != AST_INTENT_DECL)
        return false;

    for (size_t i = 0; i < intent->data.intent_decl.value_count; i++) {
        ASTNode *value = intent->data.intent_decl.values[i];
        if (value != NULL
            && value->type == AST_INTENT_VALUE
            && value->data.intent_value.alias != NULL
            && strcmp(value->data.intent_value.alias, alias) == 0) {
            return true;
        }
    }

    return false;
}

void
parse_intent_name_list(Parser *parser, char ***items, size_t *count,
                       size_t *capacity, const char *message)
{
    do {
        Token name = consume_binding_name_token(parser, message);
        intent_append_name(items, count, capacity, name.text);
    } while (parser_match(parser, TOKEN_COMMA));
}

static bool
intent_header_value_type_name(const char *type_name)
{
    static const char *value_types[] = {
        "Allocator",
        "Array",
        "Bool",
        "Box",
        "Channel",
        "DeviceSlot",
        "Double",
        "Duration",
        "Float",
        "Future",
        "HashMap",
        "IdempotencyKey",
        "Instant",
        "Int",
        "List",
        "Long",
        "Money",
        "Option",
        "Qubit",
        "QubitSlot",
        "Queue",
        "Rc",
        "RemoteFuture",
        "Result",
        "SecureSlot",
        "Set",
        "Slice",
        "Slot",
        "String",
        "Timer",
        "Token",
        "Version",
        "Void",
        "Weak",
    };

    if (type_name == NULL)
        return false;

    return bsearch(type_name,
                   value_types,
                   sizeof(value_types) / sizeof(value_types[0]),
                   sizeof(value_types[0]),
                   parser_intent_name_table_compare) != NULL;
}

static bool
intent_param_type_is_value_binding(Parser *parser, ASTNode *type_node)
{
    ASTNodeType decl_type = AST_PROGRAM;
    NominalDeclKind nominal_kind = NOMINAL_DECL_CLASS;

    if (type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return false;
    }

    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        return true;
    }

    if (parser_lookup_decl_hint(parser, type_node->data.type.name,
                                &decl_type, &nominal_kind)) {
        if (decl_type == AST_CLASS_DECL) {
            return nominal_kind != NOMINAL_DECL_SUBJECT;
        }
        if (decl_type != AST_ZONE_DECL
            && decl_type != AST_WORLD_DECL
            && decl_type != AST_RELATION_DECL
            && decl_type != AST_EFFECT_DECL
            && decl_type != AST_PARTY_DECL
            && decl_type != AST_ROSTER_DECL
            && decl_type != AST_INTENT_DECL
            && decl_type != AST_EVENT_DECL) {
            return true;
        }
    }

    return intent_header_value_type_name(type_node->data.type.name);
}

void
parse_intent_param_list(Parser *parser, ASTNode *intent)
{
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after intent name");
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token alias = consume_binding_name_token(parser, "Expected intent participant name");
        if (intent_has_involves_alias(intent, alias.text)
            || intent_has_value_alias(intent, alias.text)) {
            parser_error(parser,
                "Duplicate intent binding alias '%s' in parameter list", alias.text);
            return;
        }
        parser_consume(parser, TOKEN_COLON, "Expected ':' after intent participant name");
        {
            ASTNode *binding_type = parse_type(parser);
            if (intent_param_type_is_value_binding(parser, binding_type)) {
                ASTNode *value = ast_create_intent_value(alias.text);
                value->data.intent_value.value_type = binding_type;
                intent_append_node(&intent->data.intent_decl.values,
                    &intent->data.intent_decl.value_count,
                    &intent->data.intent_decl.value_capacity, value);
                intent_append_binding(intent, value);
            } else {
                ASTNode *involves = ast_create_intent_involves(alias.text);
                involves->data.intent_involves.subject_type = binding_type;
                intent_append_node(&intent->data.intent_decl.involves,
                    &intent->data.intent_decl.involve_count,
                    &intent->data.intent_decl.involve_capacity, involves);
                intent_append_binding(intent, involves);
            }
        }
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after intent parameter list");
}
