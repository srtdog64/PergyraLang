#include "parser_internal.h"

ASTNode *
parser_parse_enum_declaration_after_keyword(Parser *parser)
{
    Token name_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected enum name");
    size_t cap = 0;
    ASTNode *node;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after enum name");

    node = calloc(1, sizeof(ASTNode));
    node->type = AST_ENUM_DECL;
    node->line = name_tok.line;
    node->data.enum_decl.name = pergyra_strndup(name_tok.text, name_tok.length);
    node->data.enum_decl.variants = NULL;
    node->data.enum_decl.variant_params = NULL;
    node->data.enum_decl.variant_param_counts = NULL;
    node->data.enum_decl.variant_count = 0;
    node->data.enum_decl.methods = NULL;
    node->data.enum_decl.method_count = 0;

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        Token var_tok;
        size_t idx;

        if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode *method = parser_finalize_statement(parser,
                parse_function_declaration(parser));
            node->data.enum_decl.method_count++;
            node->data.enum_decl.methods = realloc(
                node->data.enum_decl.methods,
                node->data.enum_decl.method_count * sizeof(ASTNode *));
            node->data.enum_decl.methods[node->data.enum_decl.method_count - 1] = method;
            continue;
        }

        var_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variant name");
        idx = node->data.enum_decl.variant_count;
        if (idx >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            node->data.enum_decl.variants = realloc(
                node->data.enum_decl.variants, cap * sizeof(char *));
            node->data.enum_decl.variant_params = realloc(
                node->data.enum_decl.variant_params, cap * sizeof(ASTNode **));
            node->data.enum_decl.variant_param_counts = realloc(
                node->data.enum_decl.variant_param_counts, cap * sizeof(size_t));
        }
        node->data.enum_decl.variants[idx] =
            pergyra_strndup(var_tok.text, var_tok.length);
        node->data.enum_decl.variant_params[idx] = NULL;
        node->data.enum_decl.variant_param_counts[idx] = 0;

        if (parser_match(parser, TOKEN_LPAREN)) {
            size_t pcap = 0;
            while (!parser_check(parser, TOKEN_RPAREN)
                   && !parser_is_at_end(parser)) {
                ASTNode *ptype = parse_type(parser);
                size_t pc = node->data.enum_decl.variant_param_counts[idx];
                if (pc >= pcap) {
                    pcap = pcap == 0 ? 4 : pcap * 2;
                    node->data.enum_decl.variant_params[idx] = realloc(
                        node->data.enum_decl.variant_params[idx],
                        pcap * sizeof(ASTNode *));
                }
                node->data.enum_decl.variant_params[idx][pc] = ptype;
                node->data.enum_decl.variant_param_counts[idx]++;
                if (!parser_match(parser, TOKEN_COMMA))
                    break;
            }
            parser_consume(parser, TOKEN_RPAREN,
                "Expected ')' after variant parameters");
        }

        node->data.enum_decl.variant_count++;
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after enum variants");
    return node;
}
