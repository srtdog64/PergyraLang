#include "parser_internal.h"

static bool
parser_intent_match_keyword(Parser *parser, const char *keyword)
{
    if (parser == NULL || keyword == NULL
        || !parser_check(parser, TOKEN_IDENTIFIER)
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

static void
parse_intent_name_list(Parser *parser, char ***items, size_t *count,
                       const char *message)
{
    do {
        Token name = consume_name_token(parser, message);
        intent_append_name(items, count, name.text);
    } while (parser_match(parser, TOKEN_COMMA));
}

static void
parse_intent_param_list(Parser *parser, ASTNode *intent)
{
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after intent name");
    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token alias = consume_name_token(parser, "Expected intent actor name");
        ASTNode *involves = ast_create_intent_involves(alias.text);
        parser_consume(parser, TOKEN_COLON, "Expected ':' after intent actor name");
        involves->data.intent_involves.subject_type = parse_type(parser);
        intent_append_node(&intent->data.intent_decl.involves,
            &intent->data.intent_decl.involve_count, involves);
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after intent parameter list");
}

static ASTNode *
parse_intent_step(Parser *parser)
{
    Token name_tok = consume_name_token(parser, "Expected step name");
    ASTNode *step = ast_create_intent_step(name_tok.text);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after step name");
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_WHERE)) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'where'");
            ast_destroy(step->data.intent_step.where_type);
            step->data.intent_step.where_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step where clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "who")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'who'");
            parse_intent_name_list(parser,
                &step->data.intent_step.who_names,
                &step->data.intent_step.who_count,
                "Expected involves alias after 'who:'");
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step who clause");
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

        if (parser_intent_match_keyword(parser, "pre")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'pre'");
            ast_destroy(step->data.intent_step.pre_expr);
            step->data.intent_step.pre_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step pre clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "guard")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'guard'");
            ast_destroy(step->data.intent_step.guard_expr);
            step->data.intent_step.guard_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step guard clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "post")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'post'");
            ast_destroy(step->data.intent_step.post_expr);
            step->data.intent_step.post_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step post clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "invariant")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'invariant'");
            ast_destroy(step->data.intent_step.invariant_expr);
            step->data.intent_step.invariant_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step invariant clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "requires")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'requires'");
            parse_intent_name_list(parser,
                &step->data.intent_step.required_abilities,
                &step->data.intent_step.required_ability_count,
                "Expected ability name after 'requires:'");
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step requires clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "authorized")) {
            parser_intent_match_keyword(parser, "by");
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
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'causes'");
            effect_name = consume_name_token(parser, "Expected effect name after 'causes:'");
            free(step->data.intent_step.causes_effect);
            step->data.intent_step.causes_effect = pergyra_strdup(effect_name.text);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step causes clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "expect")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'expect'");
            ast_destroy(step->data.intent_step.expect_expr);
            step->data.intent_step.expect_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after step expect clause");
            continue;
        }

        parser_error(parser, "Unsupported intent step clause");
        return step;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after step body");
    return step;
}

ASTNode *
parse_intent_declaration(Parser *parser)
{
    Token name_tok = consume_name_token(parser, "Expected intent name");
    ASTNode *intent = ast_create_intent_declaration(name_tok.text);

    if (parser_check(parser, TOKEN_LPAREN))
        parse_intent_param_list(parser, intent);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after intent name");
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_intent_match_keyword(parser, "exclusive")) {
            intent->data.intent_decl.is_concurrent = false;
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after 'exclusive'");
            continue;
        }

        if (parser_intent_match_keyword(parser, "concurrent")) {
            intent->data.intent_decl.is_concurrent = true;
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after 'concurrent'");
            continue;
        }

        if (parser_intent_match_keyword(parser, "priority")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'priority'");
            ast_destroy(intent->data.intent_decl.priority_expr);
            intent->data.intent_decl.priority_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after priority clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "involves")) {
            Token alias = consume_name_token(parser, "Expected involves alias");
            ASTNode *involves = ast_create_intent_involves(alias.text);
            parser_consume(parser, TOKEN_COLON, "Expected ':' after involves alias");
            involves->data.intent_involves.subject_type = parse_type(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after involves clause");
            intent_append_node(&intent->data.intent_decl.involves,
                &intent->data.intent_decl.involve_count, involves);
            continue;
        }

        if (parser_intent_match_keyword(parser, "step")) {
            ASTNode *step = parse_intent_step(parser);
            intent_append_node(&intent->data.intent_decl.steps,
                &intent->data.intent_decl.step_count, step);
            continue;
        }

        if (parser_intent_match_keyword(parser, "success")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'success'");
            ast_destroy(intent->data.intent_decl.success_expr);
            intent->data.intent_decl.success_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after success clause");
            continue;
        }

        if (parser_intent_match_keyword(parser, "failure")) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after 'failure'");
            ast_destroy(intent->data.intent_decl.failure_expr);
            intent->data.intent_decl.failure_expr = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after failure clause");
            continue;
        }

        parser_error(parser, "Unsupported intent declaration item");
        return intent;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after intent body");
    return intent;
}
