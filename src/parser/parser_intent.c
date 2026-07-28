#include "parser_internal.h"
#include "../common/numeric_parse.h"

bool
parser_intent_match_keyword(Parser *parser, const char *keyword)
{
    if (parser == NULL || keyword == NULL) {
        return false;
    }

    if (strcmp(keyword, "intent") == 0)
        return parser_match(parser, TOKEN_INTENT);

    if (strcmp(keyword, "compensate") == 0)
        return parser_match(parser, TOKEN_COMPENSATE);

    if (!parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || strcmp(parser->current_token.text, keyword) != 0) {
        return false;
    }

    parser_advance(parser);
    return true;
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

static bool
intent_has_failure_terminal_step(const ASTNode *intent, const char *step_name)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL
        || step_name == NULL) {
        return false;
    }
    for (size_t i = 0;
         i < intent->data.intent_decl.failure_terminal_count;
         i++) {
        const char *existing =
            intent->data.intent_decl.failure_terminals[i].step_name;
        if (existing != NULL && strcmp(existing, step_name) == 0)
            return true;
    }
    return false;
}

static bool
intent_append_failure_terminal(Parser *parser,
                               ASTNode *intent,
                               Token step,
                               ASTNode *expr)
{
    ASTIntentTerminalData *grown;
    ASTIntentTerminalData *terminal;
    size_t next_capacity;

    if (intent->data.intent_decl.failure_terminal_count
        >= intent->data.intent_decl.failure_terminal_capacity) {
        next_capacity =
            intent->data.intent_decl.failure_terminal_capacity == 0
                ? 4
                : intent->data.intent_decl.failure_terminal_capacity * 2;
        if (next_capacity
            > (size_t)-1 / sizeof(ASTIntentTerminalData)) {
            parser_error(parser,
                         "Too many typed intent failure terminals");
            return false;
        }
        grown = realloc(intent->data.intent_decl.failure_terminals,
                        next_capacity * sizeof(ASTIntentTerminalData));
        if (grown == NULL) {
            parser_error(parser,
                         "Out of memory while recording typed intent failure terminal");
            return false;
        }
        intent->data.intent_decl.failure_terminals = grown;
        intent->data.intent_decl.failure_terminal_capacity = next_capacity;
    }

    terminal = &intent->data.intent_decl.failure_terminals[
        intent->data.intent_decl.failure_terminal_count];
    memset(terminal, 0, sizeof(*terminal));
    terminal->step_name = pergyra_strdup(step.text);
    if (terminal->step_name == NULL) {
        parser_error(parser,
                     "Out of memory while recording typed intent failure terminal step");
        return false;
    }
    terminal->expr = expr;
    intent->data.intent_decl.failure_terminal_count++;
    return true;
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

    if (parser_match(parser, TOKEN_ARROW))
        intent->data.intent_decl.return_type = parse_type(parser);

    /* Optional resilience modifiers before the body:
     *   intent X with retry(n) { ... }
     * retry(n) is parsed and carried as declaration metadata. Backend
     * execution lowering is fail-closed in semantic until C/LLVM wrappers land.
     * timeout/backoff are reserved so they cannot become silent no-ops. */
    if (parser_match(parser, TOKEN_WITH)) {
        do {
            if (!parser_check(parser, TOKEN_IDENTIFIER)
                || parser->current_token.text == NULL) {
                parser_error(parser,
                    "Expected a resilience modifier after 'with' (retry)");
                break;
            }
            if (strcmp(parser->current_token.text, "retry") == 0) {
                parser_advance(parser);
                parser_consume(parser, TOKEN_LPAREN,
                    "Expected '(' after 'retry'");
                Token attempts = parser_consume(parser, TOKEN_NUMBER,
                    "Expected an attempt count in retry(n)");
                int count = 0;
                if (attempts.text == NULL
                    || !pgy_parse_positive_int_strict(attempts.text, &count))
                    parser_error(parser,
                        "retry(n) requires a positive integer attempt count");
                else
                    intent->data.intent_decl.retry_count = count;
                parser_consume(parser, TOKEN_RPAREN,
                    "Expected ')' after retry attempt count");
            } else if (strcmp(parser->current_token.text, "timeout") == 0
                       || strcmp(parser->current_token.text, "backoff") == 0) {
                parser_error(parser,
                    "'timeout'/'backoff' resilience modifiers are reserved "
                    "but not implemented.\n"
                    "Reason: per-attempt cancellation and delay shaping need "
                    "runtime cancellation support.\n"
                    "Fix: remove the modifier until backend lowering lands.");
                break;
            } else {
                parser_error(parser,
                    "Unknown resilience modifier '%s'; expected 'retry'",
                    parser->current_token.text);
                break;
            }
        } while (parser_match(parser, TOKEN_COMMA));
    }

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
                    &intent->data.intent_decl.default_who_capacity,
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
                    &intent->data.intent_decl.involve_count,
                    &intent->data.intent_decl.involve_capacity, involves);
                intent_append_binding(intent, involves);
                intent_append_name(&intent->data.intent_decl.default_who_names,
                    &intent->data.intent_decl.default_who_count,
                    &intent->data.intent_decl.default_who_capacity, alias.text);
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
                &intent->data.intent_decl.involve_count,
                &intent->data.intent_decl.involve_capacity, involves);
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
                &intent->data.intent_decl.value_count,
                &intent->data.intent_decl.value_capacity, value);
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
                &intent->data.intent_decl.step_count,
                &intent->data.intent_decl.step_capacity, step);
            continue;
        }

        if (parser_intent_match_keyword(parser, "success")) {
            if (seen_success_clause) {
                parser_error(parser, "Duplicate 'success' clause in intent declaration");
                return intent;
            }
            seen_success_clause = true;
            if (intent->data.intent_decl.return_type != NULL) {
                Token step = consume_decl_name_token(
                    parser,
                    "Expected terminal step name after typed intent 'success'");
                parser_consume(parser, TOKEN_COLON,
                               "Expected ':' after typed intent success step");
                intent->data.intent_decl.success_terminal.step_name =
                    pergyra_strdup(step.text);
                if (intent->data.intent_decl.success_terminal.step_name == NULL) {
                    parser_error(parser,
                                 "Out of memory while recording typed intent success terminal");
                    return intent;
                }
                intent->data.intent_decl.success_terminal.expr =
                    parser_parse_expression(parser);
                parser_consume(
                    parser, TOKEN_SEMICOLON,
                    "Expected ';' after typed intent success terminal");
                continue;
            }
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'success'");
            ast_destroy(intent->data.intent_decl.success_expr);
            intent->data.intent_decl.success_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after success clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "failure")) {
            if (intent->data.intent_decl.return_type == NULL
                && seen_failure_clause) {
                parser_error(parser, "Duplicate 'failure' clause in intent declaration");
                return intent;
            }
            seen_failure_clause = true;
            if (intent->data.intent_decl.return_type != NULL) {
                Token step = consume_decl_name_token(
                    parser,
                    "Expected source step name after typed intent 'failure'");
                ASTNode *expr;
                if (intent_has_failure_terminal_step(intent, step.text)) {
                    parser_error(parser,
                                 "Duplicate typed intent failure terminal for step '%s'",
                                 step.text);
                    return intent;
                }
                parser_consume(parser, TOKEN_COLON,
                               "Expected ':' after typed intent failure step");
                expr = parser_parse_expression(parser);
                parser_consume(
                    parser, TOKEN_SEMICOLON,
                    "Expected ';' after typed intent failure terminal");
                if (!intent_append_failure_terminal(
                        parser, intent, step, expr)) {
                    ast_destroy(expr);
                    return intent;
                }
                continue;
            }
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
            "step <Name> [after <Step>] { ... }, success [<Step>]:, failure [<Step>]:");
        return intent;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after intent body");

    parse_intent_apply_defaults(intent);
    return intent;
}
