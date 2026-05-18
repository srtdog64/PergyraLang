#include "parser_internal.h"

static bool
parser_token_is_range_separator(Token token)
{
    return token.type == TOKEN_DOT
        && token.text != NULL
        && token.length == 2
        && strncmp(token.text, "..", 2) == 0;
}

ASTNode *
parser_parse_call(Parser *parser)
{
    ASTNode *expr = parser_parse_primary(parser);

    while (true) {
        if (parser_match(parser, TOKEN_LPAREN)) {
            expr = finish_call(parser, expr);
        } else if (parser_check(parser, TOKEN_DOT) &&
                   parser->current_token.length == 1 &&
                   strcmp(parser->current_token.text, ".") == 0) {
            Token name;
            parser_advance(parser);
            name = consume_member_name_token(parser,
                "Expected property name after '.'");
            expr = ast_create_member_access(expr, name.text);
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
                parser_error(parser,
                    "Slicing 'xs[..]' is reserved but not implemented.\n"
                    "Reason: public slice ABI and ownership policy are not frozen for beta.\n"
                    "Fix: use explicit slice helper functions.");
                parser_advance(parser);
                if (!parser_check(parser, TOKEN_RBRACKET))
                    (void)parser_parse_expression(parser);
                parser_consume(parser, TOKEN_RBRACKET,
                    "Expected ']' after slice expression");
                continue;
            }
            index = parser_parse_expression(parser);
            if (parser_token_is_range_separator(parser->current_token)) {
                parser_error(parser,
                    "Slicing 'xs[a..b]' is reserved but not implemented.\n"
                    "Reason: public slice ABI and ownership policy are not frozen for beta.\n"
                    "Fix: use explicit slice helper functions.");
                parser_advance(parser);
                if (!parser_check(parser, TOKEN_RBRACKET))
                    (void)parser_parse_expression(parser);
                parser_consume(parser, TOKEN_RBRACKET,
                    "Expected ']' after slice expression");
                ast_destroy(index);
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

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");
    return call;
}
