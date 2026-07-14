#include "parser_internal.h"

bool
parser_is_lambda_start(Parser *parser)
{
    Lexer lookahead;
    Token token;
    int paren_depth = 0;

    if (!parser_check(parser, TOKEN_LPAREN))
        return false;

    lookahead = *parser->lexer;
    token = parser->current_token;

    while (true) {
        if (token.type == TOKEN_LPAREN) {
            paren_depth++;
        } else if (token.type == TOKEN_RPAREN) {
            paren_depth--;
            if (paren_depth == 0) {
                Token next = lexer_next_token(&lookahead);
                bool is_lambda = next.type == TOKEN_LAMBDA
                    || next.type == TOKEN_ARROW;
                return is_lambda;
            }
        } else if (token.type == TOKEN_EOF) {
            return false;
        }

        token = lexer_next_token(&lookahead);
    }
}

/* Parse one pipe-closure parameter: either a bare binding name or a tuple
 * pattern `(a, b, _)`. A tuple pattern is represented as an AST_TUPLE_LITERAL
 * of identifier nodes so the binding names are preserved. */
static ASTNode*
parse_pipe_lambda_param(Parser* parser)
{
    if (parser_match(parser, TOKEN_LPAREN)) {
        ASTNode* tuple = calloc(1, sizeof(ASTNode));
        size_t cap = 4;
        if (tuple == NULL)
            return NULL;
        tuple->type = AST_TUPLE_LITERAL;
        tuple->line = parser->previous_token.line;
        tuple->data.tuple_literal.elements = calloc(cap, sizeof(ASTNode *));
        tuple->data.tuple_literal.count = 0;
        while (!parser_check(parser, TOKEN_RPAREN)
               && !parser_is_at_end(parser)) {
            Token nm = consume_binding_name_token(parser,
                "Expected name in lambda tuple pattern");
            ASTNode* id = ast_create_identifier(nm.text);
            if (!parser_append_expr_node_with_capacity(parser,
                    &tuple->data.tuple_literal.elements,
                    &tuple->data.tuple_literal.count, &cap, id)) {
                ast_destroy(id);
                break;
            }
            if (!parser_match(parser, TOKEN_COMMA))
                break;
        }
        parser_consume(parser, TOKEN_RPAREN,
            "Expected ')' after lambda tuple pattern");
        return tuple;
    }

    Token name = consume_binding_name_token(parser,
        "Expected lambda parameter name");
    return ast_create_identifier(name.text);
}

/* Pipe-closure form `|p1, p2| body` (params may be tuple patterns). */
ASTNode*
parse_pipe_lambda_expression(Parser* parser)
{
    ASTNode* lambda = ast_create_lambda_expression();

    parser_consume(parser, TOKEN_PATTERN_OR,
        "Expected '|' before lambda parameters");

    while (!parser_check(parser, TOKEN_PATTERN_OR) && !parser_is_at_end(parser)) {
        ASTNode* param = parse_pipe_lambda_param(parser);
        if (param == NULL)
            break;
        if (!parser_append_expr_node_with_capacity(parser,
                &lambda->data.lambda_expr.params,
                &lambda->data.lambda_expr.param_count,
                &lambda->data.lambda_expr.param_capacity,
                param)) {
            ast_destroy(param);
            break;
        }
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    parser_consume(parser, TOKEN_PATTERN_OR,
        "Expected '|' after lambda parameters");
    if (parser_match(parser, TOKEN_ARROW))
        lambda->data.lambda_expr.return_type = parse_type(parser);

    if (parser_check(parser, TOKEN_LBRACE)) {
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' for lambda body");
        lambda->data.lambda_expr.body = parser_parse_block(parser);
    } else {
        lambda->data.lambda_expr.body = parser_parse_expression(parser);
    }

    return lambda;
}

ASTNode*
parse_lambda_expression(Parser* parser)
{
    ASTNode* lambda = ast_create_lambda_expression();

    parser_consume(parser, TOKEN_LPAREN, "Expected '(' before lambda parameters");

    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token param_name = consume_binding_name_token(parser, "Expected parameter name");

        ASTNode* param = ast_create_identifier(param_name.text);

        if (parser_match(parser, TOKEN_COLON)) {
            ASTNode* param_type = parse_type(parser);
            ASTNode* typed_param = ast_create_let_declaration(param_name.text);
            typed_param->data.let_decl.type = param_type;
            ast_destroy(param);
            param = typed_param;
        }
        if (parser_match(parser, TOKEN_ASSIGN)) {
            parser_error(parser,
                "Default value arguments are reserved but not implemented.\n"
                "Reason: value defaults need call ABI, overload/dispatch, and named-argument interaction policy.\n"
                "Fix: use an overload or wrapper function.");
            ast_destroy(parser_parse_expression(parser));
        }

        if (!parser_append_expr_node_with_capacity(parser,
                &lambda->data.lambda_expr.params,
                &lambda->data.lambda_expr.param_count,
                &lambda->data.lambda_expr.param_capacity,
                param)) {
            ast_destroy(param);
            break;
        }

        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after lambda parameters");
    if (parser_match(parser, TOKEN_ARROW))
        lambda->data.lambda_expr.return_type = parse_type(parser);
    parser_consume(parser, TOKEN_LAMBDA, "Expected '=>' in lambda expression");

    if (parser_check(parser, TOKEN_LBRACE)) {
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' for lambda body");
        lambda->data.lambda_expr.body = parser_parse_block(parser);
    } else {
        lambda->data.lambda_expr.body = parser_parse_expression(parser);
    }

    return lambda;
}
