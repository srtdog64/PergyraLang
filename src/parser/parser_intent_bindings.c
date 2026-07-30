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

static bool
intent_param_final_value_role(Parser *parser, ASTNode *type_node,
                              bool *is_value_out)
{
    ASTNodeType decl_type = AST_PROGRAM;
    NominalDeclKind nominal_kind = NOMINAL_DECL_CLASS;

    if (is_value_out != NULL)
        *is_value_out = false;
    if (parser == NULL || type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL || is_value_out == NULL) {
        return false;
    }
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        *is_value_out = true;
        return true;
    }
    if (parser_lookup_decl_hint(parser, type_node->data.type.name,
                                &decl_type, &nominal_kind)) {
        if (decl_type == AST_CLASS_DECL) {
            *is_value_out = nominal_kind != NOMINAL_DECL_SUBJECT;
            return true;
        }
        *is_value_out = decl_type != AST_ZONE_DECL
            && decl_type != AST_WORLD_DECL
            && decl_type != AST_RELATION_DECL
            && decl_type != AST_EFFECT_DECL
            && decl_type != AST_PARTY_DECL
            && decl_type != AST_ROSTER_DECL
            && decl_type != AST_INTENT_DECL
            && decl_type != AST_EVENT_DECL;
        return true;
    }
    if (intent_header_value_type_name(type_node->data.type.name)) {
        *is_value_out = true;
        return true;
    }
    parser_error(parser,
        "Intent parameter type '%s' is unresolved after declaration composition",
        type_node->data.type.name);
    return false;
}

static void
intent_binding_reclassify(ASTNode *binding, bool is_value)
{
    char *alias;
    ASTNode *type_node;

    if (binding == NULL)
        return;
    if (binding->type == AST_INTENT_VALUE) {
        if (is_value)
            return;
        alias = binding->data.intent_value.alias;
        type_node = binding->data.intent_value.value_type;
        binding->data.intent_value.alias = NULL;
        binding->data.intent_value.value_type = NULL;
        binding->type = AST_INTENT_INVOLVES;
        binding->data.intent_involves.alias = alias;
        binding->data.intent_involves.subject_type = type_node;
        return;
    }
    if (binding->type == AST_INTENT_INVOLVES) {
        if (!is_value)
            return;
        alias = binding->data.intent_involves.alias;
        type_node = binding->data.intent_involves.subject_type;
        binding->data.intent_involves.alias = NULL;
        binding->data.intent_involves.subject_type = NULL;
        binding->type = AST_INTENT_VALUE;
        binding->data.intent_value.alias = alias;
        binding->data.intent_value.value_type = type_node;
    }
}

static bool
intent_finalize_header_parameter_roles(Parser *parser, ASTNode *intent)
{
    ASTNode **involves;
    ASTNode **values;
    bool *header_roles;
    size_t involve_count = 0;
    size_t value_count = 0;

    if (intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    if (intent->data.intent_decl.header_binding_count
        > intent->data.intent_decl.binding_count) {
        return false;
    }
    header_roles = calloc(
        intent->data.intent_decl.header_binding_count > 0
            ? intent->data.intent_decl.header_binding_count : 1,
        sizeof(bool));
    involves = calloc(intent->data.intent_decl.binding_count > 0
        ? intent->data.intent_decl.binding_count : 1, sizeof(ASTNode *));
    values = calloc(intent->data.intent_decl.binding_count > 0
        ? intent->data.intent_decl.binding_count : 1, sizeof(ASTNode *));
    if (header_roles == NULL || involves == NULL || values == NULL) {
        free(header_roles);
        free(involves);
        free(values);
        parser_error(parser,
            "Out of memory while finalizing intent parameter roles");
        return false;
    }
    for (size_t i = 0;
         i < intent->data.intent_decl.header_binding_count; i++) {
        ASTNode *binding = intent->data.intent_decl.bindings[i];
        ASTNode *type_node = binding != NULL
            && binding->type == AST_INTENT_VALUE
            ? binding->data.intent_value.value_type
            : (binding != NULL && binding->type == AST_INTENT_INVOLVES
                ? binding->data.intent_involves.subject_type : NULL);
        if (!intent_param_final_value_role(
                parser, type_node, &header_roles[i])) {
            free(header_roles);
            free(involves);
            free(values);
            return false;
        }
    }
    for (size_t i = 0; i < intent->data.intent_decl.binding_count; i++) {
        ASTNode *binding = intent->data.intent_decl.bindings[i];
        if (i < intent->data.intent_decl.header_binding_count)
            intent_binding_reclassify(binding, header_roles[i]);
        if (binding != NULL && binding->type == AST_INTENT_INVOLVES)
            involves[involve_count++] = binding;
        else if (binding != NULL && binding->type == AST_INTENT_VALUE)
            values[value_count++] = binding;
        else {
            free(header_roles);
            free(involves);
            free(values);
            parser_error(parser,
                "Intent binding role is invalid after declaration composition");
            return false;
        }
    }
    free(header_roles);
    free(intent->data.intent_decl.involves);
    free(intent->data.intent_decl.values);
    intent->data.intent_decl.involves = involves;
    intent->data.intent_decl.involve_count = involve_count;
    intent->data.intent_decl.involve_capacity =
        intent->data.intent_decl.binding_count;
    intent->data.intent_decl.values = values;
    intent->data.intent_decl.value_count = value_count;
    intent->data.intent_decl.value_capacity =
        intent->data.intent_decl.binding_count;
    return true;
}

bool
parser_finalize_intent_parameter_roles(Parser *parser, ASTNode *node)
{
    if (parser == NULL || node == NULL)
        return false;
    if (node->type == AST_INTENT_DECL)
        return intent_finalize_header_parameter_roles(parser, node);
    if (node->type == AST_PROGRAM) {
        for (size_t i = 0; i < node->data.program.count; i++) {
            if (!parser_finalize_intent_parameter_roles(
                    parser, node->data.program.statements[i]))
                return false;
        }
    } else if (node->type == AST_NAMESPACE_DECL) {
        for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
            if (!parser_finalize_intent_parameter_roles(
                    parser, node->data.namespace_decl.statements[i]))
                return false;
        }
    }
    return true;
}

static bool
intent_register_composed_declarations(Parser *resolver, ASTNode *node)
{
    if (resolver == NULL || node == NULL)
        return false;
    if (node->type == AST_PROGRAM) {
        for (size_t i = 0; i < node->data.program.count; i++) {
            if (!intent_register_composed_declarations(
                    resolver, node->data.program.statements[i])) {
                return false;
            }
        }
        return true;
    }
    if (node->type == AST_NAMESPACE_DECL) {
        for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
            if (!intent_register_composed_declarations(
                    resolver, node->data.namespace_decl.statements[i])) {
                return false;
            }
        }
        return true;
    }
    return parser_register_composed_decl_hint(resolver, node);
}

bool
parser_finalize_composed_intent_parameter_roles(ASTNode *program,
                                                 char **error_message)
{
    Parser *resolver;
    bool ok;

    if (error_message != NULL)
        *error_message = NULL;
    if (program == NULL || program->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "intent parameter finalization requires a composed Program");
        return false;
    }

    resolver = calloc(1, sizeof(Parser));
    if (resolver == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "out of memory while finalizing composed declarations");
        return false;
    }
    resolver->emit_recovered_errors = false;
    ok = intent_register_composed_declarations(resolver, program)
        && parser_finalize_intent_parameter_roles(resolver, program);
    if (!ok && error_message != NULL) {
        const char *diagnostic = parser_get_error(resolver);
        if (diagnostic == NULL || diagnostic[0] == '\0') {
            diagnostic =
                "intent parameter roles could not be finalized after declaration composition";
        }
        *error_message = pergyra_strdup(diagnostic);
    }
    parser_destroy(resolver);
    return ok;
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
    intent->data.intent_decl.header_binding_count =
        intent->data.intent_decl.binding_count;
}
