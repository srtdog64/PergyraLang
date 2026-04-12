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
        ASTNode *involves = ast_create_intent_involves(alias.text);
        parser_consume(parser, TOKEN_COLON, "Expected ':' after intent participant name");
        involves->data.intent_involves.subject_type = parse_type(parser);
        intent_append_node(&intent->data.intent_decl.involves,
            &intent->data.intent_decl.involve_count, involves);
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after intent parameter list");
}

static void
parse_intent_step_transfer_clause(Parser *parser, ASTNode *step, bool shorthand_move)
{
    Token from_alias;
    Token to_alias;

    if (step->data.intent_step.transfer_from_alias != NULL
        || step->data.intent_step.transfer_to_alias != NULL) {
        parser_error(parser, "Duplicate '%s' clause in intent step",
            shorthand_move ? "move" : "transfer");
        return;
    }

    if (!shorthand_move) {
        parser_consume(parser, TOKEN_COLON, "Expected ':' after 'transfer'");
    }

    from_alias = consume_binding_name_token(
        parser,
        shorthand_move
            ? "Expected source zone alias after 'move'"
            : "Expected source zone alias after 'transfer:'");

    if (shorthand_move) {
        if (!parser_intent_match_keyword(parser, "to")) {
            parser_error(parser, "Expected 'to' after transfer source in move clause");
            return;
        }
    } else if (!parser_match(parser, TOKEN_ARROW)) {
        parser_consume(parser, TOKEN_MINUS, "Expected '->' in transfer clause");
        parser_consume(parser, TOKEN_GREATER, "Expected '->' in transfer clause");
    }

    to_alias = consume_binding_name_token(
        parser,
        shorthand_move
            ? "Expected target zone alias after 'to'"
            : "Expected target zone alias after '->'");
    free(step->data.intent_step.transfer_from_alias);
    free(step->data.intent_step.transfer_to_alias);
    step->data.intent_step.transfer_from_alias = pergyra_strdup(from_alias.text);
    step->data.intent_step.transfer_to_alias = pergyra_strdup(to_alias.text);
    parser_consume(
        parser, TOKEN_SEMICOLON,
        shorthand_move
            ? "Expected ';' after move clause"
            : "Expected ';' after step transfer clause");
}

static ASTNode *
parse_intent_step(Parser *parser)
{
    Token name_tok = consume_decl_name_token(parser, "Expected step name");
    ASTNode *step = ast_create_intent_step(name_tok.text);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after step name");
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_WHERE)) {
            if (step->data.intent_step.where_type != NULL) {
                parser_error(parser, "Duplicate 'where' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'where'");
            ast_destroy(step->data.intent_step.where_type);
            step->data.intent_step.where_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step where clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "who")) {
            if (step->data.intent_step.who_count > 0) {
                parser_error(parser, "Duplicate 'who' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'who'");
            parse_intent_name_list(parser,
                &step->data.intent_step.who_names,
                &step->data.intent_step.who_count,
                "Expected involves alias after 'who:'");
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step who clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "using")) {
            if (step->data.intent_step.using_expr != NULL) {
                parser_error(parser, "Duplicate 'using' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'using'");
            ast_destroy(step->data.intent_step.using_expr);
            step->data.intent_step.using_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step using clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "intent")) {
            if (step->data.intent_step.intent_expr != NULL) {
                parser_error(parser, "Duplicate 'intent' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'intent'");
            ast_destroy(step->data.intent_step.intent_expr);
            step->data.intent_step.intent_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step intent clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "transfer")) {
            parse_intent_step_transfer_clause(parser, step, false);
            continue;
        }

        if (parser_intent_match_keyword(parser, "move")) {
            parse_intent_step_transfer_clause(parser, step, true);
            continue;
        }

        if (parser_intent_match_keyword(parser, "on")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'on'");
            intent_append_node(&step->data.intent_step.on_exprs,
                &step->data.intent_step.on_expr_count,
                parser_parse_expression(parser));
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step on clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "compensate")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'compensate'");
            intent_append_node(&step->data.intent_step.compensate_exprs,
                &step->data.intent_step.compensate_expr_count,
                parser_parse_expression(parser));
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step compensate clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "pre")) {
            if (step->data.intent_step.pre_expr != NULL) {
                parser_error(parser, "Duplicate 'pre' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'pre'");
            ast_destroy(step->data.intent_step.pre_expr);
            step->data.intent_step.pre_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step pre clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "guard")) {
            if (step->data.intent_step.guard_expr != NULL) {
                parser_error(parser, "Duplicate 'guard' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'guard'");
            ast_destroy(step->data.intent_step.guard_expr);
            step->data.intent_step.guard_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step guard clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "post")) {
            if (step->data.intent_step.post_expr != NULL) {
                parser_error(parser, "Duplicate 'post' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'post'");
            ast_destroy(step->data.intent_step.post_expr);
            step->data.intent_step.post_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step post clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "invariant")) {
            if (step->data.intent_step.invariant_expr != NULL) {
                parser_error(parser, "Duplicate 'invariant' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'invariant'");
            ast_destroy(step->data.intent_step.invariant_expr);
            step->data.intent_step.invariant_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step invariant clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "requires")) {
            if (step->data.intent_step.required_ability_count > 0) {
                parser_error(parser, "Duplicate 'requires' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'requires'");
            do {
                ASTNode *ability = parse_type(parser);
                size_t next = step->data.intent_step.required_ability_count + 1;
                step->data.intent_step.required_abilities = realloc(
                    step->data.intent_step.required_abilities,
                    next * sizeof(ASTNode *));
                step->data.intent_step.required_abilities[next - 1] = ability;
                step->data.intent_step.required_ability_count = next;
            } while (parser_match(parser, TOKEN_COMMA));
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step requires clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "authorized")) {
            if (step->data.intent_step.authorized_by_count > 0) {
                parser_error(parser, "Duplicate 'authorized by' clause in intent step");
                return step;
            }
            if (!parser_intent_match_keyword(parser, "by")) {
                parser_error(parser,
                    "Expected 'by' after 'authorized' in intent step clause; use 'authorized by: <participant>'");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'authorized by'");
            parse_intent_name_list(parser,
                &step->data.intent_step.authorized_by,
                &step->data.intent_step.authorized_by_count,
                "Expected involves alias after 'authorized by:'");
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step authorization clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "causes")) {
            Token effect_name;
            if (step->data.intent_step.causes_effect != NULL) {
                parser_error(parser, "Duplicate 'causes' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'causes'");
            effect_name = consume_name_token(parser, "Expected effect name after 'causes:'");
            free(step->data.intent_step.causes_effect);
            step->data.intent_step.causes_effect = pergyra_strdup(effect_name.text);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step causes clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "within")) {
            parser_error(parser,
                "'within' is an action clause, not an intent step clause; "
                "use 'where: <Zone>;' on the step or omit it to inherit the zone from the matching action contract");
            return step;
        }

        if (parser_match(parser, TOKEN_WITH)) {
            parser_error(parser,
                "'with effects ...' is not a valid intent step clause; "
                "use 'causes: <Effect>;' on the step or declare effects on the matching action contract");
            return step;
        }

        if (parser_intent_match_keyword(parser, "intent")) {
            if (step->data.intent_step.intent_expr != NULL) {
                parser_error(parser, "Duplicate 'intent' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'intent'");
            ast_destroy(step->data.intent_step.intent_expr);
            step->data.intent_step.intent_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step intent clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "expect")) {
            if (step->data.intent_step.expect_expr != NULL) {
                parser_error(parser, "Duplicate 'expect' clause in intent step");
                return step;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'expect'");
            ast_destroy(step->data.intent_step.expect_expr);
            step->data.intent_step.expect_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step expect clause");
            continue;
        }

        parser_error(parser,
            "Unsupported intent step clause; expected one of: "
            "where:, who:, using:, intent:, transfer:, move, on:, compensate:, "
            "pre:, guard:, post:, invariant:, requires:, authorized by:, causes:, expect:. "
            "Action-only clauses such as 'within' and 'with effects' belong on the matching action contract.");
        return step;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after step body");
    return step;
}

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
