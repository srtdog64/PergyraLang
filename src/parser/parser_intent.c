#include "parser_internal.h"

static bool
parser_intent_match_keyword(Parser *parser, const char *keyword)
{
    if (parser == NULL || keyword == NULL) {
        return false;
    }

    if (strcmp(keyword, "intent") == 0)
        return parser_match(parser, TOKEN_INTENT);

    if (!parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || strcmp(parser->current_token.text, keyword) != 0) {
        return false;
    }

    parser_advance(parser);
    return true;
}

static void
intent_append_node(ASTNode ***items, size_t *count, ASTNode *node)
{
    ASTNode **grown;

    if (items == NULL || count == NULL || node == NULL)
        return;

    grown = realloc(*items, (*count + 1) * sizeof(ASTNode *));
    if (grown == NULL)
        return;
    grown[*count] = node;
    *items = grown;
    (*count)++;
}

static void
intent_append_binding(ASTNode *intent, ASTNode *node)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || node == NULL)
        return;
    intent_append_node(&intent->data.intent_decl.bindings,
        &intent->data.intent_decl.binding_count, node);
}

static void
intent_append_name(char ***items, size_t *count, const char *name)
{
    char **grown;

    if (items == NULL || count == NULL || name == NULL)
        return;

    grown = realloc(*items, (*count + 1) * sizeof(char *));
    if (grown == NULL)
        return;
    grown[*count] = pergyra_strdup(name);
    *items = grown;
    (*count)++;
}

static bool
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

static bool
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

static bool
intent_has_step_name(ASTNode *intent, const char *name)
{
    if (intent == NULL || name == NULL || intent->type != AST_INTENT_DECL)
        return false;

    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step != NULL
            && step->type == AST_INTENT_STEP
            && step->data.intent_step.name != NULL
            && strcmp(step->data.intent_step.name, name) == 0) {
            return true;
        }
    }

    return false;
}

static void
parse_intent_name_list(Parser *parser, char ***items, size_t *count,
                       const char *message)
{
    do {
        Token name = consume_binding_name_token(parser, message);
        intent_append_name(items, count, name.text);
    } while (parser_match(parser, TOKEN_COMMA));
}

static bool
intent_header_value_type_name(const char *type_name)
{
    static const char *value_types[] = {
        "Int", "Long", "Float", "Double", "Bool", "String", "Void",
        "Qubit", "QubitSlot", "Token",
        "Array", "Slice", "List", "Queue", "HashMap", "Set",
        "Box", "Rc", "Weak", "Channel", "Future", "RemoteFuture",
        "Result", "Option",
        "Slot", "SecureSlot", "DeviceSlot",
        "Allocator", "Timer", "Duration", "Instant",
        "Money", "Version", "IdempotencyKey"
    };

    if (type_name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(value_types) / sizeof(value_types[0]); i++) {
        if (strcmp(type_name, value_types[i]) == 0)
            return true;
    }

    return false;
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

static void
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
                    &intent->data.intent_decl.value_count, value);
                intent_append_binding(intent, value);
            } else {
                ASTNode *involves = ast_create_intent_involves(alias.text);
                involves->data.intent_involves.subject_type = binding_type;
                intent_append_node(&intent->data.intent_decl.involves,
                    &intent->data.intent_decl.involve_count, involves);
                intent_append_binding(intent, involves);
            }
        }
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after intent parameter list");
}

#include "parser_intent_step.h"

ASTNode *
parse_intent_declaration(Parser *parser)
{
    Token name_tok = consume_decl_name_token(parser, "Expected intent name");
    ASTNode *intent = ast_create_intent_declaration(name_tok.text);
    bool seen_mode_clause = false;
    bool seen_priority_clause = false;
    bool seen_rollback_clause = false;
    bool seen_success_clause = false;
    bool seen_failure_clause = false;

    if (parser_check(parser, TOKEN_LPAREN))
        parse_intent_param_list(parser, intent);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after intent name");
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_intent_match_keyword(parser, "exclusive")) {
            if (seen_mode_clause) {
                parser_error(parser, "Duplicate intent mode clause; use only one of 'exclusive' or 'concurrent'");
                return intent;
            }
            seen_mode_clause = true;
            intent->data.intent_decl.is_concurrent = false;
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after 'exclusive'");
            continue;
        }

        if (parser_intent_match_keyword(parser, "concurrent")) {
            if (seen_mode_clause) {
                parser_error(parser, "Duplicate intent mode clause; use only one of 'exclusive' or 'concurrent'");
                return intent;
            }
            seen_mode_clause = true;
            intent->data.intent_decl.is_concurrent = true;
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after 'concurrent'");
            continue;
        }

        if (parser_intent_match_keyword(parser, "priority")) {
            if (seen_priority_clause) {
                parser_error(parser, "Duplicate 'priority' clause in intent declaration");
                return intent;
            }
            seen_priority_clause = true;
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'priority'");
            ast_destroy(intent->data.intent_decl.priority_expr);
            intent->data.intent_decl.priority_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after priority clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "rollback")) {
            Token mode;
            if (seen_rollback_clause) {
                parser_error(parser, "Duplicate 'rollback' clause in intent declaration");
                return intent;
            }
            seen_rollback_clause = true;

            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'rollback'");
            mode = consume_name_token(parser, "Expected rollback policy after 'rollback:'");
            if (mode.text != NULL && strcmp(mode.text, "none") == 0) {
                intent->data.intent_decl.rollback_policy = INTENT_ROLLBACK_NONE;
            } else if (mode.text != NULL && strcmp(mode.text, "current") == 0) {
                intent->data.intent_decl.rollback_policy = INTENT_ROLLBACK_CURRENT;
            } else if (mode.text != NULL && strcmp(mode.text, "full") == 0) {
                intent->data.intent_decl.rollback_policy = INTENT_ROLLBACK_FULL;
            } else {
                parser_error(parser, "Intent rollback policy must be one of: full, current, none");
                return intent;
            }
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after rollback clause");
            continue;
        }

        /* Intent-level who: / where: — propagated to steps that omit them */
        if (parser_intent_match_keyword(parser, "who")) {
            if (intent->data.intent_decl.default_who_count > 0) {
                parser_error(parser, "Duplicate intent-level 'who' clause");
                return intent;
            }
            if (parser_match(parser, TOKEN_COLON)) {
                parse_intent_name_list(parser,
                    &intent->data.intent_decl.default_who_names,
                    &intent->data.intent_decl.default_who_count,
                    "Expected involves alias after 'who:'");
                parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after intent-level who clause");
                continue;
            }

            {
                Token alias = consume_binding_name_token(
                    parser, "Expected intent participant alias after 'who'");
                if (intent_has_involves_alias(intent, alias.text)
                    || intent_has_value_alias(intent, alias.text)) {
                    parser_error(parser,
                        "Duplicate intent binding alias '%s' in intent-level who declaration",
                        alias.text);
                    return intent;
                }
                ASTNode *involves = ast_create_intent_involves(alias.text);
                parser_consume(parser, TOKEN_COLON, "Expected ':' after intent participant alias");
                involves->data.intent_involves.subject_type = parse_type(parser);
                parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after intent-level who declaration");
                intent_append_node(&intent->data.intent_decl.involves,
                    &intent->data.intent_decl.involve_count, involves);
                intent_append_binding(intent, involves);
                intent_append_name(&intent->data.intent_decl.default_who_names,
                    &intent->data.intent_decl.default_who_count, alias.text);
                continue;
            }
        }

        if (parser_match(parser, TOKEN_WHERE)) {
            if (intent->data.intent_decl.default_where_type != NULL) {
                parser_error(parser, "Duplicate intent-level 'where' clause");
                return intent;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'where'");
            ast_destroy(intent->data.intent_decl.default_where_type);
            intent->data.intent_decl.default_where_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after intent-level where clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "involves")) {
            Token alias = consume_binding_name_token(parser, "Expected involves alias");
            if (intent_has_involves_alias(intent, alias.text)
                || intent_has_value_alias(intent, alias.text)) {
                parser_error(parser, "Duplicate intent binding alias '%s' in involves clause",
                    alias.text);
                return intent;
            }
            ASTNode *involves = ast_create_intent_involves(alias.text);
            parser_consume(parser, TOKEN_COLON, "Expected ':' after involves alias");
            involves->data.intent_involves.subject_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after involves clause");
            intent_append_node(&intent->data.intent_decl.involves,
                &intent->data.intent_decl.involve_count, involves);
            intent_append_binding(intent, involves);
            continue;
        }

        if (parser_match(parser, TOKEN_WITH)) {
            Token alias = consume_binding_name_token(
                parser, "Expected intent value binding after 'with'");
            if (intent_has_value_alias(intent, alias.text)
                || intent_has_involves_alias(intent, alias.text)) {
                parser_error(parser, "Duplicate intent binding alias '%s' in value clause",
                    alias.text);
                return intent;
            }
            ASTNode *value = ast_create_intent_value(alias.text);
            parser_consume(parser, TOKEN_COLON, "Expected ':' after intent value binding");
            value->data.intent_value.value_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after intent value clause");
            intent_append_node(&intent->data.intent_decl.values,
                &intent->data.intent_decl.value_count, value);
            intent_append_binding(intent, value);
            continue;
        }

        if (parser_intent_match_keyword(parser, "step")) {
            ASTNode *step = parse_intent_step(parser);
            if (step != NULL
                && step->type == AST_INTENT_STEP
                && step->data.intent_step.name != NULL
                && intent_has_step_name(intent, step->data.intent_step.name)) {
                parser_error(parser, "Duplicate intent step '%s'",
                    step->data.intent_step.name);
                ast_destroy(step);
                return intent;
            }
            intent_append_node(&intent->data.intent_decl.steps,
                &intent->data.intent_decl.step_count, step);
            continue;
        }

        if (parser_intent_match_keyword(parser, "success")) {
            if (seen_success_clause) {
                parser_error(parser, "Duplicate 'success' clause in intent declaration");
                return intent;
            }
            seen_success_clause = true;
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'success'");
            ast_destroy(intent->data.intent_decl.success_expr);
            intent->data.intent_decl.success_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after success clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "failure")) {
            if (seen_failure_clause) {
                parser_error(parser, "Duplicate 'failure' clause in intent declaration");
                return intent;
            }
            seen_failure_clause = true;
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'failure'");
            ast_destroy(intent->data.intent_decl.failure_expr);
            intent->data.intent_decl.failure_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after failure clause");
            continue;
        }

        parser_error(parser,
            "Unsupported intent declaration item; expected one of: "
            "exclusive;, concurrent;, priority:, rollback:, who:, who <alias>: <Type>;, "
            "where:, involves <alias>: <Type>;, with <alias>: <Type>;, "
            "step <Name> { ... }, success:, failure:");
        return intent;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after intent body");

    /* Propagate intent-level who/where to steps that omit them */
    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL)
            continue;

        /* If step has no who, copy intent-level default */
        if (step->data.intent_step.who_count == 0
            && intent->data.intent_decl.default_who_count > 0) {
            for (size_t j = 0; j < intent->data.intent_decl.default_who_count; j++) {
                intent_append_name(&step->data.intent_step.who_names,
                    &step->data.intent_step.who_count,
                    intent->data.intent_decl.default_who_names[j]);
            }
        }

        /* If step has no where, copy intent-level default */
        if (step->data.intent_step.where_type == NULL
            && intent->data.intent_decl.default_where_type != NULL) {
            step->data.intent_step.where_type =
                ast_clone(intent->data.intent_decl.default_where_type);
        }
    }

    return intent;
}
