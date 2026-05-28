#include "parser_internal.h"

static void
free_lookahead_token(Token *token, bool owned)
{
    if (owned && token != NULL) {
        free(token->text);
        token->text = NULL;
    }
}

bool
parser_is_lambda_start(Parser *parser)
{
    Lexer lookahead;
    Token token;
    bool token_owned = false;
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
                free(next.text);
                free_lookahead_token(&token, token_owned);
                return is_lambda;
            }
        } else if (token.type == TOKEN_EOF) {
            free_lookahead_token(&token, token_owned);
            return false;
        }

        free_lookahead_token(&token, token_owned);
        token = lexer_next_token(&lookahead);
        token_owned = true;
    }
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
