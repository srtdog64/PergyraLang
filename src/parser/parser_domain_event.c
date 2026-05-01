#include "parser_domain_internal.h"

ASTNode* parse_event_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected event name");

    ASTNode* event_decl = ast_create_event_declaration(name.text);

    if (parser_match(parser, TOKEN_PUBLIC)) {
        event_decl->data.event_decl.access = ACCESS_PUBLIC;
        event_decl->is_exported = true;
    } else if (parser_match(parser, TOKEN_PRIVATE)) {
        event_decl->data.event_decl.access = ACCESS_PRIVATE;
        event_decl->is_exported = false;
    }

    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after event name");

    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");

        ASTNode* param_type = parse_type(parser);

        ASTNode* param = ast_create_let_declaration(param_name.text);
        param->data.let_decl.type = param_type;
        append_child_node(&event_decl->data.event_decl.params,
                          &event_decl->data.event_decl.param_count,
                          &event_decl->data.event_decl.param_capacity, param);

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after event parameters");

    if (parser_match(parser, TOKEN_ARROW)) {
        event_decl->data.event_decl.return_type = parse_type(parser);
    }

    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after event declaration");

    return event_decl;
}
