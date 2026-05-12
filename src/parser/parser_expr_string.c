#include "parser_internal.h"

static ASTNode *
parse_interpolated_expression_fragment(const char *expr_src, bool *ok_out)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *expr;

    if (ok_out != NULL)
        *ok_out = false;

    if (expr_src == NULL)
        return ast_create_string("\"\"");

    lexer = lexer_create(expr_src);
    if (lexer == NULL)
        return NULL;

    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return NULL;
    }

    expr = parser_parse_expression(parser);
    if (parser_has_error(parser) || !parser_is_at_end(parser)) {
        ast_destroy(expr);
        expr = NULL;
    }
    parser_destroy(parser);
    lexer_destroy(lexer);

    if (expr != NULL && ok_out != NULL)
        *ok_out = true;
    return expr;
}

bool
is_multiline_string_token(const char *value)
{
    size_t len;

    if (value == NULL)
        return false;

    len = strlen(value);
    return len >= 6
           && strncmp(value, "\"\"\"", 3) == 0
           && strncmp(value + len - 3, "\"\"\"", 3) == 0;
}

static bool
interpolation_opener_is_escaped(const char *start, const char *opener)
{
    size_t slash_count = 0;
    const char *p;

    if (start == NULL || opener == NULL || opener <= start)
        return false;

    p = opener;
    while (p > start && *(p - 1) == '\\') {
        slash_count++;
        p--;
    }

    return (slash_count % 2) == 1;
}

ASTNode *
parse_interpolation_body(const char *raw, bool is_fstring)
{
    size_t len = strlen(raw);
    const char *s = raw + 1;
    const char *end = raw + len - 1;
    ASTNode *result = NULL;

    while (s < end) {
        const char *interp = NULL;
        int delim_len = 0;

        if (is_fstring) {
            interp = strchr(s, '{');
            while (interp != NULL && interpolation_opener_is_escaped(s, interp))
                interp = strchr(interp + 1, '{');
            delim_len = 1;
        } else {
            interp = strstr(s, "${");
            while (interp != NULL && interpolation_opener_is_escaped(s, interp))
                interp = strstr(interp + 2, "${");
            delim_len = 2;
        }

        if (interp == NULL || interp >= end) {
            if (s < end) {
                size_t part_len = (size_t)(end - s);
                char *part = malloc(part_len + 3);
                ASTNode *str_node;
                if (part == NULL)
                    break;
                part[0] = '"';
                memcpy(part + 1, s, part_len);
                part[part_len + 1] = '"';
                part[part_len + 2] = '\0';
                str_node = ast_create_string(part);
                free(part);
                if (result == NULL) {
                    result = str_node;
                } else {
                    Token plus_tok = { .type = TOKEN_PLUS, .text = "+", .length = 1 };
                    result = ast_create_binary(result, plus_tok, str_node);
                }
            }
            break;
        }

        if (interp > s) {
            size_t part_len = (size_t)(interp - s);
            char *part = malloc(part_len + 3);
            ASTNode *str_node;
            if (part == NULL)
                break;
            part[0] = '"';
            memcpy(part + 1, s, part_len);
            part[part_len + 1] = '"';
            part[part_len + 2] = '\0';
            str_node = ast_create_string(part);
            free(part);
            if (result == NULL) {
                result = str_node;
            } else {
                Token plus_tok = { .type = TOKEN_PLUS, .text = "+", .length = 1 };
                result = ast_create_binary(result, plus_tok, str_node);
            }
        }

        const char *expr_start = interp + delim_len;
        const char *expr_end = strchr(expr_start, '}');
        if (expr_end == NULL || expr_end >= end) {
            if (result != NULL)
                ast_destroy(result);
            return ast_create_string(raw);
        }

        size_t expr_len = (size_t)(expr_end - expr_start);
        char *expr_str = malloc(expr_len + 1);
        ASTNode *inner_expr;
        ASTNode *to_string;
        bool parsed = false;
        if (expr_str == NULL)
            break;
        memcpy(expr_str, expr_start, expr_len);
        expr_str[expr_len] = '\0';

        inner_expr = parse_interpolated_expression_fragment(expr_str, &parsed);
        if (!parsed || inner_expr == NULL) {
            free(expr_str);
            if (result != NULL)
                ast_destroy(result);
            return ast_create_string(raw);
        }
        to_string = ast_create_call(ast_create_identifier("ToString"));
        ast_add_argument(to_string, inner_expr);
        free(expr_str);

        if (result == NULL) {
            result = to_string;
        } else {
            Token plus_tok = { .type = TOKEN_PLUS, .text = "+", .length = 1 };
            result = ast_create_binary(result, plus_tok, to_string);
        }

        s = expr_end + 1;
    }

    return result != NULL ? result : ast_create_string(raw);
}
