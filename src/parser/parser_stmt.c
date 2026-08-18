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
static ASTNode *
parse_condition_expression(Parser *parser)
{
    ASTNode *expr;
    bool saved_nsl = parser->no_struct_literal;
    parser->no_struct_literal = true;
    expr = parser_parse_expression(parser);
    parser->no_struct_literal = saved_nsl;
    return expr;
}

ASTNode* parse_for_loop(Parser* parser) {
    ASTNode* for_loop = ast_create_for_loop();

    Token var = consume_binding_name_token(parser, "Expected loop variable");
    for_loop->data.for_loop.variable = pergyra_strdup(var.text);

    parser_consume(parser, TOKEN_IN, "Expected 'in' in for loop");

    ASTNode* first = parse_condition_expression(parser);

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

    while_loop->data.while_loop.condition = parse_condition_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after while condition");
    while_loop->data.while_loop.body = parser_parse_block(parser);

    return while_loop;
}

ASTNode* parse_match_statement(Parser* parser) {
    ASTNode* match = ast_create_match_statement();
    match->line = parser->previous_token.line;
    match->column = parser->previous_token.column;

    match->data.match_stmt.subject = parse_condition_expression(parser);

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

/*
 * Else-if chain safety. An `else if` tail used to recurse through this parser
 * directly, bypassing the expression/type/block/statement chokepoints, so a
 * 50,000-arm chain crashed the native parser with no diagnostic. The tail is
 * now stitched iteratively, and the arm count is capped like the
 * operator-chain cap: past the cap the remaining arms are still parsed but
 * discarded, so the whole chain is consumed and exactly one named diagnostic
 * surfaces. The cap equals the semantic analyzer's 512-depth statement
 * backstop, so the accept/reject boundary is exactly what semantic analysis
 * already enforced: over-cap chains now fail at parse with a chain-specific
 * diagnostic instead of crashing before semantic analysis could see them.
 */
#define PARSER_MAX_ELSE_IF_CHAIN 512

static ASTNode* parse_if_statement_arm(Parser* parser);

ASTNode* parse_if_statement(Parser* parser) {
    ASTNode* head = parse_if_statement_arm(parser);
    ASTNode* tail = head;
    int arm_count = 0;
    bool chain_capped = false;

    while (tail != NULL && tail->type == AST_IF_STMT &&
           parser_match(parser, TOKEN_ELSE)) {
        if (!parser_match(parser, TOKEN_IF)) {
            parser_consume(parser, TOKEN_LBRACE, "Expected '{' after else");
            tail->data.if_stmt.else_branch = parser_parse_block(parser);
            break;
        }
        ASTNode* arm = parse_if_statement_arm(parser);
        if (parser->has_error) {
            if (arm_count < PARSER_MAX_ELSE_IF_CHAIN && !chain_capped)
                tail->data.if_stmt.else_branch = arm;
            else
                ast_destroy(arm);
            break;
        }
        arm_count++;
        if (arm_count > PARSER_MAX_ELSE_IF_CHAIN) {
            chain_capped = true;
            ast_destroy(arm);
            continue;
        }
        tail->data.if_stmt.else_branch = arm;
        if (arm->type != AST_IF_STMT)
            break;  /* an if-let arm owns its own else clause */
        tail = arm;
    }
    if (chain_capped && !parser->has_error) {
        parser_error(parser,
            "If statement has too many chained else-if arms (limit is "
            "512); refactor into a match statement or a lookup table");
    }
    return head;
}

/* Parse one arm only: the driver loop above owns the else-if tail. */
static ASTNode* parse_if_statement_arm(Parser* parser) {
    /* if-let: `if let <pattern> = <expr> { then } [else { else }]` desugars
     * to a single-case match on <expr> with a default for the else branch. */
    if (parser_check(parser, TOKEN_LET)) {
        parser_advance(parser);  /* consume 'let' */
        /* Parse the pattern without assignment precedence so the `=` that
         * follows is not swallowed as an assignment expression. */
        ASTNode* pattern = parse_unary(parser);
        parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in if-let binding");
        ASTNode* subject = parse_condition_expression(parser);
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after if-let");
        ASTNode* then_block = parser_parse_block(parser);

        ASTNode* match = ast_create_match_statement();
        match->data.match_stmt.subject = subject;

        ASTNode* mc = ast_create_match_case();
        mc->data.match_case.pattern = pattern;
        mc->data.match_case.patterns = calloc(1, sizeof(ASTNode *));
        if (mc->data.match_case.patterns != NULL) {
            mc->data.match_case.patterns[0] = pattern;
            mc->data.match_case.pattern_count = 1;
            mc->data.match_case.pattern_capacity = 1;
        }
        mc->data.match_case.body = then_block;
        parser_append_match_case(parser, match, mc);

        if (parser_match(parser, TOKEN_ELSE)) {
            parser_consume(parser, TOKEN_LBRACE, "Expected '{' after else");
            match->data.match_stmt.default_body = parser_parse_block(parser);
        }
        return match;
    }

    ASTNode* if_stmt = ast_create_if_statement();

    if_stmt->data.if_stmt.condition = parse_condition_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after if condition");
    if_stmt->data.if_stmt.then_branch = parser_parse_block(parser);

    return if_stmt;
}

/* Scoped unsafe capability syntax lowers to the same unsafe-block model as
 * plain `unsafe { }`; the capability label is recorded on the node and the
 * body is lowered identically. Scoped unsafe capability label syntax accepts
 * `unsafe label { }`. This is not a universal mode bit: semantic raw escape
 * remains explicitly gated. */
static char* parse_unsafe_capability_label(Parser* parser) {
    char* label = NULL;

    if (parser_match(parser, TOKEN_LPAREN)) {
        if (parser_check(parser, TOKEN_IDENTIFIER)) {
            Token cap = parser_advance(parser);
            label = pergyra_strdup(cap.text);
        }
        while (parser_match(parser, TOKEN_COMMA)) {
            if (parser_check(parser, TOKEN_IDENTIFIER))
                parser_advance(parser);
        }
        parser_consume(parser, TOKEN_RPAREN,
            "Expected ')' after unsafe capability list");
        return label;
    }
    if (parser_check(parser, TOKEN_IDENTIFIER)
        && parser_peek_next(parser).type == TOKEN_LBRACE) {
        Token cap = parser_advance(parser);
        label = pergyra_strdup(cap.text);
    }
    return label;
}

ASTNode* parse_unsafe_block(Parser* parser) {
    char* capability = parse_unsafe_capability_label(parser);
    ASTNode* body;
    ASTNode* node;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after unsafe");
    body = parser_parse_block(parser);
    node = ast_create_unsafe_block(body);
    if (node != NULL)
        node->data.unsafe_block.capability = capability;
    else
        free(capability);
    return node;
}

static void transaction_add_compensation(ASTNode* node, ASTNode* handler) {
    size_t count = node->data.transaction_block.compensation_count;
    size_t cap = node->data.transaction_block.compensation_capacity;

    if (count == cap) {
        size_t grown_cap = (cap == 0) ? 4 : cap * 2;
        ASTNode** grown = realloc(node->data.transaction_block.compensations,
                                  grown_cap * sizeof(ASTNode*));
        if (grown == NULL)
            return;
        node->data.transaction_block.compensations = grown;
        node->data.transaction_block.compensation_capacity = grown_cap;
    }
    node->data.transaction_block.compensations[count] = handler;
    node->data.transaction_block.compensation_count = count + 1;
}

ASTNode* parse_transaction_block(Parser* parser) {
    ASTNode* body = ast_create_block();
    ASTNode* node;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after transaction");
    node = ast_create_transaction_block(body);
    if (node == NULL) {
        ast_destroy(body);
        return NULL;
    }

    /* `compensate <expr>;` is registered (top-level only, in source order) and
     * executed in reverse on `fail`; every other statement joins the body. */
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_COMPENSATE)) {
            ASTNode* handler = parser_parse_expression(parser);
            parser_consume_statement_terminator(parser,
                "Expected ';' after compensate handler");
            if (handler != NULL)
                transaction_add_compensation(node, handler);
            if (parser->has_error)
                parser_synchronize(parser);
            continue;
        }
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt != NULL)
            ast_add_statement(body, stmt);
        if (parser->has_error)
            parser_synchronize(parser);
    }
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' to close transaction");
    return node;
}

ASTNode* parse_defer_statement(Parser* parser) {
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after defer");
    ASTNode* body = parser_parse_block(parser);
    parser_consume_statement_terminator(parser, "Expected ';' after defer block");
    return ast_create_defer_statement(body);
}

ASTNode* parse_fail_statement(Parser* parser) {
    ASTNode* reason = NULL;

    /* `fail` rolls back the enclosing transaction; an optional reason follows on
     * the same line, mirroring the bare-vs-valued shape of `return`. */
    if (!parser_check(parser, TOKEN_SEMICOLON)
        && !parser_check(parser, TOKEN_RBRACE)
        && parser->current_token.line == parser->previous_token.line)
        reason = parser_parse_expression(parser);

    parser_consume_statement_terminator(parser, "Expected ';' after fail statement");
    return ast_create_fail_statement(reason);
}

ASTNode* parse_return_statement(Parser* parser) {
    ASTNode* return_stmt = ast_create_return_statement();

    /* A return value is present only when it follows on the same line; a bare
     * `return` may end at a newline or '}' (newline-terminated style). */
    if (!parser_check(parser, TOKEN_SEMICOLON)
        && !parser_check(parser, TOKEN_RBRACE)
        && parser->current_token.line == parser->previous_token.line)
        return_stmt->data.return_stmt.value = parser_parse_expression(parser);

    parser_reject_reserved_cast_after_expression(parser);
    parser_consume_statement_terminator(parser, "Expected ';' after return statement");

    return return_stmt;
}
