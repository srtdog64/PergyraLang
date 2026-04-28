#include "parser_internal.h"

static bool
parser_check_contextual_keyword(Parser *parser, const char *keyword)
{
    return parser != NULL && keyword != NULL
        && parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, keyword) == 0;
}

static void
parser_apply_lexical_zone_context(ASTNode *node, const char *zone_name)
{
    if (node == NULL || zone_name == NULL)
        return;

    switch (node->type) {
    case AST_FUNC_DECL:
        if (node->data.func_decl.within_zone == NULL)
            node->data.func_decl.within_zone = pergyra_strdup(zone_name);
        break;
    case AST_CLASS_DECL:
        for (size_t i = 0; i < node->data.class_decl.method_count; i++)
            parser_apply_lexical_zone_context(
                node->data.class_decl.methods[i], zone_name);
        break;
    case AST_NAMESPACE_DECL:
        for (size_t i = 0; i < node->data.namespace_decl.count; i++)
            parser_apply_lexical_zone_context(
                node->data.namespace_decl.statements[i], zone_name);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            parser_apply_lexical_zone_context(
                node->data.block.statements[i], zone_name);
        break;
    default:
        break;
    }
}

bool
parser_check_within_context_block_start(Parser *parser)
{
    return parser_check_contextual_keyword(parser, "within");
}

ASTNode *
parser_parse_within_context_block(Parser *parser)
{
    Token zone_name;
    ASTNode *block;

    parser_advance(parser); /* consume contextual 'within' */
    zone_name = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected zone name after 'within'");
    parser_consume(parser, TOKEN_LBRACE,
        "Expected '{' after lexical zone context");

    block = ast_create_block();
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode *stmt;

        if (parser_check_contextual_keyword(parser, "within")) {
            parser_error(parser,
                "Nested lexical zone context is not supported in this surface");
            break;
        }

        stmt = parser_parse_statement(parser);
        if (stmt != NULL) {
            parser_apply_lexical_zone_context(stmt, zone_name.text);
            ast_add_statement(block, stmt);
        }

        if (parser->has_error)
            parser_synchronize(parser);
    }

    parser_consume(parser, TOKEN_RBRACE,
        "Expected '}' after lexical zone context");
    return block;
}
