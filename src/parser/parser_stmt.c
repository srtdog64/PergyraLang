#include "parser_internal.h"

/* Match case pattern list growth. */
static bool
parser_append_match_pattern(Parser *parser, ASTNode *match_case, ASTNode *pattern)
{
    ASTNode **grown;
    size_t next_capacity;

    if (parser == NULL || match_case == NULL || pattern == NULL)
        return false;

    if (match_case->data.match_case.pattern_count
        >= match_case->data.match_case.pattern_capacity) {
        next_capacity = match_case->data.match_case.pattern_capacity == 0
            ? 4 : match_case->data.match_case.pattern_capacity * 2;
        if (next_capacity <= match_case->data.match_case.pattern_count
            || next_capacity > (size_t)-1 / sizeof(ASTNode *)) {
            parser_error(parser, "Too many match OR patterns");
            return false;
        }
        grown = realloc(match_case->data.match_case.patterns,
                        next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing OR pattern");
            return false;
        }
        match_case->data.match_case.patterns = grown;
        match_case->data.match_case.pattern_capacity = next_capacity;
    }

    match_case->data.match_case.patterns[
        match_case->data.match_case.pattern_count++] = pattern;
    return true;
}

static bool
parser_append_match_case(Parser *parser, ASTNode *match, ASTNode *match_case)
{
    ASTNode **grown;
    size_t next_capacity;

    if (parser == NULL || match == NULL || match_case == NULL)
        return false;

    if (match->data.match_stmt.case_count >=
        match->data.match_stmt.case_capacity) {
        next_capacity = match->data.match_stmt.case_capacity == 0
            ? 4 : match->data.match_stmt.case_capacity * 2;
        if (next_capacity <= match->data.match_stmt.case_count
            || next_capacity > (size_t)-1 / sizeof(ASTNode *)) {
            parser_error(parser, "Too many match cases");
            return false;
        }
        grown = realloc(match->data.match_stmt.cases,
                        next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing match cases");
            return false;
        }
        match->data.match_stmt.cases = grown;
        match->data.match_stmt.case_capacity = next_capacity;
    }

    match->data.match_stmt.cases[match->data.match_stmt.case_count] = match_case;
    match->data.match_stmt.case_count += 1;
    return true;
}

/* Supports two forms:
 *   for x in start..end { }    range loop
 *   for item in collection { } for-in collection loop
 */
ASTNode* parse_for_loop(Parser* parser) {
    ASTNode* for_loop = ast_create_for_loop();

    Token var = consume_binding_name_token(parser, "Expected loop variable");
    for_loop->data.for_loop.variable = pergyra_strdup(var.text);

    parser_consume(parser, TOKEN_IN, "Expected 'in' in for loop");

    ASTNode* first = parser_parse_expression(parser);

    if (parser_check(parser, TOKEN_DOT)
        && parser->current_token.length == 2
        && parser->current_token.text != NULL
        && strncmp(parser->current_token.text, "..", 2) == 0) {
        parser_advance(parser);
        ASTNode* end = parser_parse_expression(parser);
        for_loop->data.for_loop.range_start = first;
        for_loop->data.for_loop.range_end = end;
    } else {
        for_loop->data.for_loop.iterable = first;
    }

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after for loop header");
    for_loop->data.for_loop.body = parser_parse_block(parser);

    return for_loop;
}

ASTNode* parse_while_statement(Parser* parser) {
    ASTNode* while_loop = ast_create_while_loop();
    while_loop->line = parser->previous_token.line;
    while_loop->column = parser->previous_token.column;

    while_loop->data.while_loop.condition = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after while condition");
    while_loop->data.while_loop.body = parser_parse_block(parser);

    return while_loop;
}

ASTNode* parse_match_statement(Parser* parser) {
    ASTNode* match = ast_create_match_statement();
    match->line = parser->previous_token.line;
    match->column = parser->previous_token.column;

    match->data.match_stmt.subject = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after match expression");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_CASE)) {
            ASTNode* mc = ast_create_match_case();
            mc->line = parser->previous_token.line;
            mc->column = parser->previous_token.column;

            mc->data.match_case.pattern = parser_parse_expression(parser);
            mc->data.match_case.patterns = calloc(1, sizeof(ASTNode *));
            if (mc->data.match_case.patterns != NULL) {
                mc->data.match_case.patterns[0] = mc->data.match_case.pattern;
                mc->data.match_case.pattern_count = 1;
                mc->data.match_case.pattern_capacity = 1;
            }
            while (parser_match(parser, TOKEN_PATTERN_OR)) {
                ASTNode *alt = parser_parse_expression(parser);
                if (!parser_append_match_pattern(parser, mc, alt))
                    break;
            }

            if (parser_match(parser, TOKEN_IF))
                mc->data.match_case.guard = parser_parse_expression(parser);

            parser_consume(parser, TOKEN_COLON, "Expected ':' after case pattern");

            ASTNode* body = ast_create_block();
            while (!parser_check(parser, TOKEN_CASE) &&
                   !parser_check(parser, TOKEN_DEFAULT) &&
                   !parser_check(parser, TOKEN_RBRACE) &&
                   !parser_is_at_end(parser)) {
                ASTNode* stmt = parser_parse_statement(parser);
                if (stmt) ast_add_statement(body, stmt);
                if (parser->has_error)
                    parser_synchronize(parser);
            }
            mc->data.match_case.body = body;

            parser_append_match_case(parser, match, mc);

        } else if (parser_match(parser, TOKEN_DEFAULT)) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after default");

            ASTNode* body = ast_create_block();
            while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
                ASTNode* stmt = parser_parse_statement(parser);
                if (stmt) ast_add_statement(body, stmt);
                if (parser->has_error)
                    parser_synchronize(parser);
            }
            match->data.match_stmt.default_body = body;
        } else {
            parser_error(parser, "Expected 'case' or 'default' in match");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after match body");
    return match;
}

ASTNode* parse_if_statement(Parser* parser) {
    ASTNode* if_stmt = ast_create_if_statement();

    if_stmt->data.if_stmt.condition = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after if condition");
    if_stmt->data.if_stmt.then_branch = parser_parse_block(parser);

    if (parser_match(parser, TOKEN_ELSE)) {
        if (parser_match(parser, TOKEN_IF)) {
            if_stmt->data.if_stmt.else_branch = parse_if_statement(parser);
        } else {
            parser_consume(parser, TOKEN_LBRACE, "Expected '{' after else");
            if_stmt->data.if_stmt.else_branch = parser_parse_block(parser);
        }
    }

    return if_stmt;
}

ASTNode* parse_unsafe_block(Parser* parser) {
    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_error(parser,
            "Scoped unsafe capability syntax 'unsafe(...) { ... }' is reserved but not implemented.\n"
            "Reason: unsafe must be a named lexical capability scope, not a universal mode bit.\n"
            "Fix: use plain 'unsafe { ... }' only as today's boundary marker, or wait for scoped unsafe(raw)/unsafe(ffi) gates.");
        return ast_create_unsafe_block(ast_create_block());
    }
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser,
            "Scoped unsafe capability label syntax 'unsafe raw { ... }' is reserved but not implemented.\n"
            "Reason: unsafe labels must lower to the same capability-scope model as unsafe(raw), not to a loose parser shortcut.\n"
            "Fix: use plain 'unsafe { ... }' only as today's boundary marker, or wait for scoped unsafe capability gates.");
        return ast_create_unsafe_block(ast_create_block());
    }
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after unsafe");
    ASTNode* body = parser_parse_block(parser);
    return ast_create_unsafe_block(body);
}

ASTNode* parse_defer_statement(Parser* parser) {
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after defer");
    ASTNode* body = parser_parse_block(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after defer block");
    return ast_create_defer_statement(body);
}

ASTNode* parse_return_statement(Parser* parser) {
    ASTNode* return_stmt = ast_create_return_statement();

    if (!parser_check(parser, TOKEN_SEMICOLON))
        return_stmt->data.return_stmt.value = parser_parse_expression(parser);

    parser_reject_reserved_cast_after_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after return statement");

    return return_stmt;
}
