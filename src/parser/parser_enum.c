#include "parser_internal.h"
#include "ast_constructors_internal.h"

static bool
parser_append_enum_method(Parser *parser, ASTNode *node, ASTNode *method)
{
    ASTNode **grown;
    size_t next_capacity;

    if (parser == NULL || node == NULL || method == NULL)
        return false;

    if (node->data.enum_decl.method_count
        >= node->data.enum_decl.method_capacity) {
        next_capacity = node->data.enum_decl.method_capacity == 0
            ? 4 : node->data.enum_decl.method_capacity * 2;
        if (next_capacity <= node->data.enum_decl.method_count
            || next_capacity > (size_t)-1 / sizeof(ASTNode *)) {
            parser_error(parser, "Too many enum methods");
            return false;
        }
        grown = realloc(node->data.enum_decl.methods,
                        next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing enum methods");
            return false;
        }
        node->data.enum_decl.methods = grown;
        node->data.enum_decl.method_capacity = next_capacity;
    }

    node->data.enum_decl.methods[node->data.enum_decl.method_count] = method;
    node->data.enum_decl.method_count += 1;
    return true;
}

static bool
parser_reserve_enum_variants(Parser *parser, ASTNode *node, size_t *capacity,
                             size_t needed)
{
    char **new_variants;
    ASTNode ***new_params;
    size_t *new_counts;
    size_t new_capacity;

    if (parser == NULL || node == NULL || capacity == NULL)
        return false;
    if (needed <= *capacity)
        return true;

    new_capacity = *capacity == 0 ? 4 : *capacity;
    while (new_capacity < needed) {
        if (new_capacity > (size_t)-1 / 2) {
            parser_error(parser, "Too many enum variants");
            return false;
        }
        new_capacity *= 2;
    }
    if (new_capacity > (size_t)-1 / sizeof(char *)
        || new_capacity > (size_t)-1 / sizeof(ASTNode **)
        || new_capacity > (size_t)-1 / sizeof(size_t)) {
        parser_error(parser, "Too many enum variants");
        return false;
    }

    new_variants = calloc(new_capacity, sizeof(char *));
    new_params = calloc(new_capacity, sizeof(ASTNode **));
    new_counts = calloc(new_capacity, sizeof(size_t));
    if (new_variants == NULL || new_params == NULL || new_counts == NULL) {
        free(new_variants);
        free(new_params);
        free(new_counts);
        parser_error(parser, "Out of memory while parsing enum variants");
        return false;
    }

    if (node->data.enum_decl.variant_count > 0) {
        memcpy(new_variants, node->data.enum_decl.variants,
               node->data.enum_decl.variant_count * sizeof(char *));
        memcpy(new_params, node->data.enum_decl.variant_params,
               node->data.enum_decl.variant_count * sizeof(ASTNode **));
        memcpy(new_counts, node->data.enum_decl.variant_param_counts,
               node->data.enum_decl.variant_count * sizeof(size_t));
    }

    free(node->data.enum_decl.variants);
    free(node->data.enum_decl.variant_params);
    free(node->data.enum_decl.variant_param_counts);
    node->data.enum_decl.variants = new_variants;
    node->data.enum_decl.variant_params = new_params;
    node->data.enum_decl.variant_param_counts = new_counts;
    *capacity = new_capacity;
    return true;
}

static bool
parser_append_enum_variant_param(Parser *parser, ASTNode *node, size_t variant,
                                 size_t *capacity, ASTNode *param_type)
{
    ASTNode **grown;
    size_t count;
    size_t new_capacity;

    if (parser == NULL || node == NULL || capacity == NULL || param_type == NULL)
        return false;

    count = node->data.enum_decl.variant_param_counts[variant];
    if (count < *capacity) {
        node->data.enum_decl.variant_params[variant][count] = param_type;
        node->data.enum_decl.variant_param_counts[variant] = count + 1;
        return true;
    }

    if (*capacity > (size_t)-1 / 2
        || (*capacity > 0 && *capacity * 2 > (size_t)-1 / sizeof(ASTNode *))) {
        parser_error(parser, "Too many enum variant parameters");
        return false;
    }
    new_capacity = *capacity == 0 ? 4 : *capacity * 2;
    grown = realloc(node->data.enum_decl.variant_params[variant],
                    new_capacity * sizeof(ASTNode *));
    if (grown == NULL) {
        parser_error(parser, "Out of memory while parsing enum variant parameters");
        return false;
    }

    node->data.enum_decl.variant_params[variant] = grown;
    *capacity = new_capacity;
    node->data.enum_decl.variant_params[variant][count] = param_type;
    node->data.enum_decl.variant_param_counts[variant] = count + 1;
    return true;
}

static void
parser_commit_enum_variant(ASTNode *node)
{
    if (node != NULL)
        node->data.enum_decl.variant_count++;
}

ASTNode *
parser_parse_enum_declaration_after_keyword(Parser *parser)
{
    Token name_tok;
    size_t cap = 0;
    ASTNode *node;

    /* parser_consume reports a mismatch without advancing. Required enum
     * header tokens must therefore fail before body-loop entry, otherwise a
     * malformed delimiter can be observed forever by the variant loop. */
    if (!parser_check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected enum name");
        return NULL;
    }
    name_tok = parser_advance(parser);
    if (!parser_match(parser, TOKEN_LBRACE)) {
        parser_error(parser, "Expected '{' after enum name");
        return NULL;
    }

    node = ast_create_node(AST_ENUM_DECL);
    node->line = name_tok.line;
    node->data.enum_decl.name = pergyra_strndup(name_tok.text, name_tok.length);
    node->data.enum_decl.variants = NULL;
    node->data.enum_decl.variant_params = NULL;
    node->data.enum_decl.variant_param_counts = NULL;
    node->data.enum_decl.variant_count = 0;
    node->data.enum_decl.methods = NULL;
    node->data.enum_decl.method_count = 0;
    node->data.enum_decl.method_capacity = 0;

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        Token var_tok;
        size_t idx;

        if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode *method = parser_finalize_statement(parser,
                parse_function_declaration(parser));
            if (!parser_append_enum_method(parser, node, method))
                return node;
            continue;
        }

        /* A failed variant-name consume would leave the same token at the
         * loop head. Discard the partial declaration; the program parser's
         * existing error synchronization owns the recovery step. */
        if (!parser_check(parser, TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected variant name");
            ast_destroy(node);
            return NULL;
        }
        var_tok = parser_advance(parser);
        idx = node->data.enum_decl.variant_count;
        if (!parser_reserve_enum_variants(parser, node, &cap, idx + 1))
            return node;
        node->data.enum_decl.variants[idx] =
            pergyra_strndup(var_tok.text, var_tok.length);
        if (node->data.enum_decl.variants[idx] == NULL) {
            parser_error(parser, "Out of memory while parsing enum variant name");
            return node;
        }
        node->data.enum_decl.variant_params[idx] = NULL;
        node->data.enum_decl.variant_param_counts[idx] = 0;

        if (parser_match(parser, TOKEN_LPAREN)) {
            size_t pcap = 0;
            while (!parser_check(parser, TOKEN_RPAREN)
                   && !parser_is_at_end(parser)) {
                /* Optional `name:` label on a variant parameter; the field
                 * name is erased and only the type is retained. */
                if (parser_check(parser, TOKEN_IDENTIFIER)
                    && parser_peek_next(parser).type == TOKEN_COLON) {
                    parser_advance(parser);
                    parser_advance(parser);
                }
                ASTNode *ptype = parse_type(parser);
                if (!parser_append_enum_variant_param(parser, node, idx, &pcap, ptype)) {
                    ast_destroy(ptype);
                    parser_commit_enum_variant(node);
                    return node;
                }
                if (!parser_match(parser, TOKEN_COMMA))
                    break;
            }
            parser_consume(parser, TOKEN_RPAREN,
                "Expected ')' after variant parameters");
        }

        parser_commit_enum_variant(node);
        /* Variants may be separated by commas or by newlines; the loop
         * condition terminates the list at '}'. */
        parser_match(parser, TOKEN_COMMA);
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after enum variants");
    return node;
}
