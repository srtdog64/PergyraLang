#include "parser_internal.h"

static bool
parser_token_is_range_separator(Token token)
{
    return token.type == TOKEN_DOT
        && token.text != NULL
        && token.length == 2
        && strncmp(token.text, "..", 2) == 0;
}

static Token
parser_synthetic_minus_token(ASTNode *anchor)
{
    Token token;

    memset(&token, 0, sizeof(token));
    token.type = TOKEN_MINUS;
    token.length = 1;
    if (anchor != NULL) {
        token.line = anchor->line;
        token.column = anchor->column;
    }
    return token;
}

static ASTNode *
parser_desugar_slice_call(Parser *parser, ASTNode *receiver,
                          ASTNode *start, ASTNode *end)
{
    ASTNode *start_for_len = ast_clone(start);
    ASTNode *callee;
    ASTNode *call;
    ASTNode *len;

    if (start_for_len == NULL) {
        parser_error(parser, "Out of memory while parsing slice expression");
        ast_destroy(receiver);
        ast_destroy(start);
        ast_destroy(end);
        return NULL;
    }

    len = ast_create_binary(end, parser_synthetic_minus_token(start), start_for_len);
    callee = ast_create_member_access(receiver, "Slice");
    call = ast_create_call(callee);
    if (call == NULL || len == NULL || callee == NULL
        || !parser_append_call_argument(parser, call, NULL, start)
        || !parser_append_call_argument(parser, call, NULL, len)) {
        parser_error(parser, "Out of memory while parsing slice expression");
        ast_destroy(call);
        if (call == NULL) {
            ast_destroy(callee);
            ast_destroy(start);
            ast_destroy(len);
        }
        return NULL;
    }
    return call;
}

/* Two-token lookahead: current token is '{'; an object initializer body
 * begins with `identifier :`. Blocks (parallel/spawn/with/control bodies)
 * never do, so this distinguishes `Type { f: v }` from a statement block. */
static bool
parser_struct_literal_body_ahead(Parser *parser)
{
    Lexer saved = *parser->lexer;
    Token first = lexer_next_token(parser->lexer);
    Token second = lexer_next_token(parser->lexer);
    bool first_is_name = first.type == TOKEN_IDENTIFIER
        || (first.text != NULL
            && ((first.text[0] >= 'a' && first.text[0] <= 'z')
                || (first.text[0] >= 'A' && first.text[0] <= 'Z')
                || first.text[0] == '_'));
    *parser->lexer = saved;
    return first_is_name && second.type == TOKEN_COLON;
}

static ASTNode *
parser_parse_object_literal(Parser *parser, ASTNode *type_expr)
{
    ASTNode *call = ast_create_call(type_expr);
    bool saved_nsl = parser->no_struct_literal;
    if (!ast_call_mark_braced_initializer_syntax(call)) {
        parser_error(parser, "Out of memory while parsing object initializer");
        ast_destroy(call);
        return NULL;
    }
    if (call != NULL && type_expr != NULL) {
        call->line = type_expr->line;
        call->column = type_expr->column;
    }

    parser_consume(parser, TOKEN_LBRACE,
        "Expected '{' to begin object initializer");
    parser->no_struct_literal = false;
    if (!parser_check(parser, TOKEN_RBRACE)) {
        do {
            /* Field names may be keywords with an identifier-shaped spelling
             * (e.g. `type:`), so accept any name-like token here. */
            Token field;
            if (!parser_check(parser, TOKEN_IDENTIFIER)
                && parser->current_token.text != NULL
                && ((parser->current_token.text[0] >= 'a'
                        && parser->current_token.text[0] <= 'z')
                    || (parser->current_token.text[0] >= 'A'
                        && parser->current_token.text[0] <= 'Z')
                    || parser->current_token.text[0] == '_'))
                field = parser_advance(parser);
            else
                field = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected field name in object initializer");
            ASTNode *value;
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after field name in object initializer");
            value = parser_parse_expression(parser);
            if (!parser_append_call_argument(parser, call, field.text, value)) {
                ast_destroy(value);
                break;
            }
        } while (parser_match(parser, TOKEN_COMMA));
    }
    parser->no_struct_literal = saved_nsl;
    parser_consume(parser, TOKEN_RBRACE,
        "Expected '}' after object initializer");
    return call;
}

/* Open-ended slice bound: `ArrayLength(<clone of receiver>)`, used to fill the
 * omitted end of `xs[..]`, `xs[a..]`, and `xs[..]`. */
static ASTNode *
parser_slice_length_of(Parser *parser, ASTNode *receiver)
{
    ASTNode *receiver_clone = ast_clone(receiver);
    ASTNode *callee;
    ASTNode *call;

    if (receiver_clone == NULL) {
        parser_error(parser, "Out of memory while parsing open slice bound");
        return ast_create_number("0");
    }
    callee = ast_create_identifier("ArrayLength");
    call = ast_create_call(callee);
    if (call == NULL || callee == NULL
        || !parser_append_call_argument(parser, call, NULL, receiver_clone)) {
        parser_error(parser, "Out of memory while parsing open slice bound");
        ast_destroy(call);
        if (call == NULL) {
            ast_destroy(callee);
            ast_destroy(receiver_clone);
        }
        return ast_create_number("0");
    }
    return call;
}

ASTNode *
parser_parse_call(Parser *parser)
{
    ASTNode *expr = parser_parse_primary(parser);

    while (true) {
        if (parser_check(parser, TOKEN_LBRACE)
            && !parser->no_struct_literal
            && expr != NULL && expr->type == AST_IDENTIFIER
            && parser_struct_literal_body_ahead(parser)) {
            expr = parser_parse_object_literal(parser, expr);
        } else if (parser_match(parser, TOKEN_LPAREN)) {
            expr = finish_call(parser, expr);
        } else if (parser_check(parser, TOKEN_DOT) &&
                   parser->current_token.length == 1 &&
                   strcmp(parser->current_token.text, ".") == 0) {
            Token name;
            parser_advance(parser);
            name = consume_member_name_token(parser,
                "Expected property name after '.'");
            expr = ast_create_member_access(expr, name.text);
            /* Generic method call `obj.Method<TypeArgs>(args)`: stash the type
             * arguments so the following call attaches them. The lookahead
             * keeps `obj.field < x` (comparison) from being misparsed. */
            if (parser_check(parser, TOKEN_LESS)
                && parser_generic_call_args_ahead(parser)) {
                parser->pending_call_generic_args =
                    parse_type_arguments(parser);
            }
        } else if (parser_match(parser, TOKEN_OPTIONAL_CHAIN)) {
            Token name = consume_member_name_token(parser,
                "Expected property name after '?.'");
            parser_error(parser,
                "Optional chaining '?.' is reserved but not implemented.\n"
                "Reason: optional member provenance is not frozen across semantic, AIR, MIR, and diagnostics.\n"
                "Fix: use explicit Option matching or helper functions.");
            expr = ast_create_member_access(expr, name.text);
        } else if (parser_match(parser, TOKEN_LBRACKET)) {
            ASTNode *index;
            if (parser_token_is_range_separator(parser->current_token)) {
                /* Leading range separator: `xs[..]` (full) or `xs[..end]`
                 * (prefix). The omitted start is 0; a full slice uses the
                 * receiver length for the end. */
                ASTNode *start = ast_create_number("0");
                ASTNode *end;
                parser_advance(parser);
                if (parser_check(parser, TOKEN_RBRACKET))
                    end = parser_slice_length_of(parser, expr);
                else
                    end = parser_parse_expression(parser);
                parser_consume(parser, TOKEN_RBRACKET,
                    "Expected ']' after slice expression");
                expr = parser_desugar_slice_call(parser, expr, start, end);
                continue;
            }
            index = parser_parse_expression(parser);
            if (parser_token_is_range_separator(parser->current_token)) {
                ASTNode *end;
                parser_advance(parser);
                /* `xs[a..]` (suffix) uses the receiver length for the end. */
                if (parser_check(parser, TOKEN_RBRACKET))
                    end = parser_slice_length_of(parser, expr);
                else
                    end = parser_parse_expression(parser);
                parser_consume(parser, TOKEN_RBRACKET,
                    "Expected ']' after slice expression");
                expr = parser_desugar_slice_call(parser, expr, index, end);
                continue;
            }
            parser_consume(parser, TOKEN_RBRACKET,
                "Expected ']' after array index");
            expr = ast_create_array_access(expr, index);
        } else if (parser_match(parser, TOKEN_QUESTION)) {
            ASTNode *try_node = calloc(1, sizeof(ASTNode));
            if (try_node != NULL) {
                try_node->type = AST_UNARY;
                try_node->line = parser->previous_token.line;
                try_node->data.unary.op = parser->previous_token;
                try_node->data.unary.op.text = NULL;
                try_node->data.unary.operand = expr;
            }
            expr = try_node;
        } else {
            break;
        }
    }

    return expr;
}

ASTNode *
finish_call(Parser *parser, ASTNode *callee)
{
    ASTNode *call = ast_create_call(callee);
    if (call != NULL && callee != NULL) {
        call->line = callee->line;
        call->column = callee->column;
    }

    if (parser->pending_call_generic_args != NULL) {
        call->data.call.generic_args = parser->pending_call_generic_args;
        parser->pending_call_generic_args = NULL;
    }

    bool saved_nsl = parser->no_struct_literal;
    parser->no_struct_literal = false;
    if (!parser_check(parser, TOKEN_RPAREN)) {
        do {
            const char *arg_name = NULL;
            ASTNode *arg;
            if (parser_check(parser, TOKEN_IDENTIFIER)
                && parser_peek_next(parser).type == TOKEN_COLON) {
                Token name = parser_advance(parser);
                parser_advance(parser);
                arg_name = name.text;
            }
            arg = parser_parse_expression(parser);
            if (!parser_append_call_argument(parser, call, arg_name, arg)) {
                ast_destroy(arg);
                break;
            }
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser->no_struct_literal = saved_nsl;
    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
    return call;
}
