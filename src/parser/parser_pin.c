#include "parser_internal.h"

static bool
parser_check_contextual_keyword(Parser *parser, const char *keyword)
{
    return parser != NULL && keyword != NULL
        && parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, keyword) == 0;
}

bool
parser_check_pin_block_start(Parser *parser)
{
    Token next;

    if (!parser_check_contextual_keyword(parser, "pin"))
        return false;

    next = parser_peek_next(parser);
    return next.type != TOKEN_LPAREN && next.type != TOKEN_SEMICOLON
        && next.type != TOKEN_EOF;
}

static const char *
parser_pin_view_callee(ASTNode *view_type)
{
    if (view_type == NULL)
        return "ViewRead";
    if (view_type->type == AST_TYPE && view_type->data.type.name != NULL) {
        if (strcmp(view_type->data.type.name, "WriteView") == 0)
            return "ViewWrite";
        if (strcmp(view_type->data.type.name, "ReadView") == 0)
            return "ViewRead";
    }
    return NULL;
}

static ASTNode *
parser_create_pin_view_decl(Parser *parser, Token pin_token,
                            ASTNode *slot_expr, Token alias,
                            ASTNode *view_type)
{
    const char *callee_name = parser_pin_view_callee(view_type);
    ASTNode *callee;
    ASTNode *call;
    ASTNode *decl;

    if (callee_name == NULL) {
        parser_error(parser,
            "Pin/Lease view type must be ReadView<T> or WriteView<T>");
        return NULL;
    }

    callee = ast_create_identifier(callee_name);
    call = ast_create_call(callee);
    ast_add_argument(call, slot_expr);
    call->line = pin_token.line;
    call->column = pin_token.column;

    decl = ast_create_let_declaration(alias.text);
    decl->line = alias.line;
    decl->column = alias.column;
    decl->data.let_decl.type = view_type;
    decl->data.let_decl.initializer = call;
    decl->data.let_decl.is_alias = true;
    return decl;
}

ASTNode *
parser_parse_pin_block(Parser *parser)
{
    Token pin_token = parser_advance(parser); /* contextual 'pin' */
    ASTNode *slot_expr;
    ASTNode *view_type = NULL;
    Token alias;
    ASTNode *wrapper;
    ASTNode *view_decl;

    if (parser->scope_depth == 0) {
        parser_error(parser, "Pin/Lease block must appear inside a function, action, or local block");
        return NULL;
    }

    slot_expr = parser_parse_expression(parser);
    parser_consume(parser, TOKEN_AS, "Expected 'as' after pinned slot expression");
    alias = consume_binding_name_token(parser, "Expected view name after 'as'");

    if (parser_match(parser, TOKEN_COLON))
        view_type = parse_type(parser);

    view_decl = parser_create_pin_view_decl(parser, pin_token, slot_expr,
                                           alias, view_type);
    if (view_decl == NULL)
        return NULL;

    wrapper = ast_create_block();
    wrapper->line = pin_token.line;
    wrapper->column = pin_token.column;
    wrapper->data.block.is_pin_block = true;
    wrapper->data.block.pin_view_is_write =
        view_type != NULL
        && view_type->type == AST_TYPE
        && view_type->data.type.name != NULL
        && strcmp(view_type->data.type.name, "WriteView") == 0;
    wrapper->data.block.pin_view_name = pergyra_strdup(alias.text);
    if (slot_expr != NULL && slot_expr->type == AST_IDENTIFIER)
        wrapper->data.block.pin_source_name =
            pergyra_strdup(slot_expr->data.identifier.name);
    ast_add_statement(wrapper, view_decl);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after pin view declaration");
    parser->scope_depth++;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode *stmt = parser_parse_statement(parser);
        if (stmt != NULL)
            ast_add_statement(wrapper, stmt);
        if (parser->has_error)
            parser_synchronize(parser);
    }
    parser->scope_depth--;
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after pin block");

    return wrapper;
}
