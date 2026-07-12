/*
 * Copyright (c) 2026 Pergyra Language Project
 * Parallel-surface parsing owner (docs/181): the executable statement
 * form (`parallel { arms }`), the rung-0 join form
 * (`parallel (x in xs) [join with all] { body }`), and the role reactive
 * form (declared vision surface, fail-closed). Split out of parser.c /
 * parser_domain.c under the 550-line responsibility rule.
 */

#include "parser_internal.h"
#include "../common/string_compat.h"

#include <string.h>

/* Rungs 0+1 of the join form (docs/181 SS1.4): statement form, all-join,
 * mandatory binding. Two modes share the parse:
 *   parallel (x in xs)      element mode (rung 0)
 *   parallel (i in lo..hi)  index mode (R1) -- same `..` shape as the
 *                           range for-loop, zero new surface.
 * `join`/`with`/`all`/`any` are contextual identifiers, not keywords. */
static ASTNode*
parser_parse_parallel_join_form(Parser* parser, ASTNode* parallel)
{
    Token elem = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected element binding name in parallel (x in xs)");
    char* elem_name = elem.text != NULL ? pergyra_strdup(elem.text) : NULL;
    ASTNode* range_end = NULL;

    parser_consume(parser, TOKEN_IN, "Expected 'in' in parallel (x in xs)");
    ASTNode* collection = parser_parse_expression(parser);
    if (parser_check(parser, TOKEN_DOT)
        && parser->current_token.length == 2
        && parser->current_token.text != NULL
        && strncmp(parser->current_token.text, "..", 2) == 0) {
        parser_advance(parser);
        range_end = parser_parse_expression(parser);
    }
    parser_consume(parser, TOKEN_RPAREN,
        "Expected ')' after parallel collection");

    /* `join`/`with`/`all`/`any` are contextual words; `with` in
     * particular lexes as the with-statement keyword, so all four are
     * matched by token text, never token type. */
    if (parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "join") == 0) {
        parser_advance(parser);  /* join */
        if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "with") == 0) {
            parser_advance(parser);  /* with */
        } else {
            parser_error(parser, "Expected 'with' after 'join'");
        }
        if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "any") == 0) {
            /* R3 (docs/181): first give wins; the checker narrows the
             * admitted shape (element mode, expression form). */
            if (!ast_parallel_set_join_any(parallel)) {
                parser_error(parser,
                    "Out of memory while recording parallel join any mode");
            }
            parser_advance(parser);
        } else if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "all") == 0) {
            parser_advance(parser);  /* all (the rung-0 default) */
        } else if (parser->current_token.text != NULL
            && (strcmp(parser->current_token.text, "sum") == 0
                || strcmp(parser->current_token.text, "product") == 0
                || strcmp(parser->current_token.text, "min") == 0
                || strcmp(parser->current_token.text, "max") == 0)) {
            /* R4 reduce combinators (docs/181 R4): a closed contextual
             * set, sealed on the node for checker and both emitters. */
            if (!ast_parallel_set_join_reduce_op(parallel,
                    parser->current_token.text)) {
                parser_error(parser,
                    "Out of memory while recording parallel join reduce combinator");
            }
            parser_advance(parser);
        } else {
            parser_error(parser,
                "Expected join mode 'all', 'sum', 'product', 'min', or 'max' after 'join with'");
        }
    }

    parser_consume(parser, TOKEN_LBRACE,
        "Expected '{' for parallel join body");
    parser->in_parallel_block = true;
    {
        ASTNode* body = parser_parse_block(parser);
        parser->in_parallel_block = false;
        if (body != NULL)
            ast_add_parallel_task(parallel, body);
    }

    if (!ast_parallel_set_join_form(parallel, elem_name, collection)) {
        parser_error(parser,
            "Out of memory while recording parallel join form");
        ast_destroy(collection);
        ast_destroy(range_end);
    } else {
        ast_parallel_set_join_range_end(parallel, range_end);
    }
    free(elem_name);
    return parallel;
}

// parallel 블록 파싱
ASTNode* parser_parse_parallel_block(Parser* parser) {
    ASTNode* parallel = ast_create_parallel_block();

    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_advance(parser);  /* '(' */

        /* Executable rung 0 (docs/181 SS1): element binding present. */
        if (parser_check(parser, TOKEN_IDENTIFIER)
            && parser_peek_next(parser).type == TOKEN_IN) {
            return parser_parse_parallel_join_form(parser, parallel);
        }

        /* Binding-less sketch form: fail closed (never a silent no-op),
         * consuming the tokens for clean recovery. */
        parser_error(parser,
            "parallel join requires an element binding (docs/181 SS1): parallel (x in xs) join with all { ... }");
        ast_destroy(parser_parse_expression(parser));
        parser_consume(parser, TOKEN_RPAREN,
            "Expected ')' after parallel target");
        if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "join") == 0) {
            parser_advance(parser);  /* join */
            if (parser->current_token.text != NULL
                && strcmp(parser->current_token.text, "with") == 0)
                parser_advance(parser);  /* with */
            if (parser_check(parser, TOKEN_IDENTIFIER))
                parser_advance(parser);  /* mode: all / any / ... */
        }
        if (parser_match(parser, TOKEN_LBRACE)) {
            parser->in_parallel_block = true;
            ast_destroy(parser_parse_block(parser));
            parser->in_parallel_block = false;
        }
        return parallel;
    }

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'parallel'");

    parser->in_parallel_block = true;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt;
        /* docs/177 F3(a) (BDFL 2026-07-09): a bare `{ ... }` inside a
         * parallel block is one multi-statement arm. The statement grammar
         * outside parallel is unchanged. */
        if (parser_check(parser, TOKEN_LBRACE)) {
            parser_advance(parser);
            stmt = parser_parse_block(parser);
        } else {
            stmt = parser_parse_statement(parser);
        }
        if (stmt != NULL) {
            ast_add_parallel_task(parallel, stmt);
        }
        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }
    parser->in_parallel_block = false;

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after parallel block");

    return parallel;
}

/* Role reactive parallel (`on (lane)` / `every (d)` / `continuous`) is a
 * declared vision surface (docs/181 SS2): pinned as the SEA lane surface,
 * gated on duration literals + virtual clock + cooperative cancellation.
 * No rung executes yet, so the form fails closed instead of parsing into
 * a block no checker or emitter consumes. Tokens are still consumed for
 * clean recovery. */
ASTNode*
parser_parse_reactive_parallel_block(Parser* parser)
{
    ASTNode* blk = ast_create_parallel_block();

    parser_error(parser,
        "role reactive parallel (on/every/continuous) is a declared vision surface (docs/181): not yet executable");
    parser_consume(parser, TOKEN_PARALLEL, "Expected 'parallel'");
    if (parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "on") == 0) {
        parser_advance(parser);
        parser_consume(parser, TOKEN_LPAREN, "Expected '(' after 'on'");
        ast_destroy(parser_parse_expression(parser));
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after 'on' target");
    }

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' for parallel block");
    bool saved_async = parser->in_async_context;
    parser->in_parallel_block = true;
    parser->in_async_context = true;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "every") == 0) {
            parser_advance(parser);
            parser_consume(parser, TOKEN_LPAREN, "Expected '(' after 'every'");
            ast_destroy(parser_parse_expression(parser));  /* duration count */
            if (parser_check(parser, TOKEN_IDENTIFIER))
                parser_advance(parser);  /* unit suffix: ms / s / ... */
            parser_consume(parser, TOKEN_RPAREN, "Expected ')' after duration");
            parser_consume(parser, TOKEN_LBRACE, "Expected '{' for every block");
            ASTNode* body = parser_parse_block(parser);
            if (body != NULL)
                ast_add_parallel_task(blk, body);
        } else if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "continuous") == 0
            && parser_peek_next(parser).type == TOKEN_LBRACE) {
            parser_advance(parser);
            parser_consume(parser, TOKEN_LBRACE, "Expected '{' for continuous block");
            ASTNode* body = parser_parse_block(parser);
            if (body != NULL)
                ast_add_parallel_task(blk, body);
        } else {
            ASTNode* stmt = parser_parse_statement(parser);
            if (stmt != NULL)
                ast_add_parallel_task(blk, stmt);
        }
        if (parser->has_error)
            parser_synchronize(parser);
    }
    parser->in_parallel_block = false;
    parser->in_async_context = saved_async;
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after parallel block");
    return blk;
}
