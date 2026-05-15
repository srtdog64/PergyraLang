#include "parser_internal.h"

Token
consume_name_token(Parser* parser, const char* message)
{
    if (parser_check_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

Token
consume_decl_name_token(Parser* parser, const char* message)
{
    if (parser_check_decl_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

Token
consume_binding_name_token(Parser* parser, const char* message)
{
    if (parser_check_binding_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

bool
parser_check_name_token(Parser *parser)
{
    return parser_check_decl_name_token(parser);
}

bool
parser_check_decl_name_token(Parser *parser)
{
    if (parser == NULL)
        return false;

    switch (parser->current_token.type) {
    case TOKEN_IDENTIFIER:
        return true;
    default:
        return false;
    }
}

bool
parser_check_binding_name_token(Parser *parser)
{
    if (parser == NULL)
        return false;

    switch (parser->current_token.type) {
    case TOKEN_IDENTIFIER:
    case TOKEN_SLOT:
    case TOKEN_EVENT:
    case TOKEN_WORLD:
    case TOKEN_ZONE:
    case TOKEN_ROSTER:
    case TOKEN_RELATION:
    case TOKEN_EFFECT:
        return true;
    default:
        return false;
    }
}

bool
parser_match_name_token(Parser *parser)
{
    return parser_match_expr_name_token(parser);
}

bool
parser_check_expr_name_token(Parser *parser)
{
    return parser_check_binding_name_token(parser);
}

bool
parser_match_expr_name_token(Parser *parser)
{
    if (!parser_check_expr_name_token(parser))
        return false;
    parser_advance(parser);
    return true;
}

Token
consume_member_name_token(Parser* parser, const char* message)
{
    if (parser_check_expr_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

static bool
parser_append_generic_param(Parser *parser, GenericParams *params, GenericParam *param)
{
    GenericParam **grown;

    if (params == NULL)
        return false;

    if (params->count == params->capacity) {
        size_t next_capacity = params->capacity == 0 ? 4 : params->capacity * 2;
        grown = realloc(params->params, next_capacity * sizeof(GenericParam *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while appending generic parameter");
            return false;
        }
        params->params = grown;
        params->capacity = next_capacity;
    }

    params->params[params->count++] = param;
    return true;
}

static void
parser_free_generic_param(GenericParam *param)
{
    if (param == NULL)
        return;

    free(param->name);
    ast_destroy(param->constraint);
    ast_destroy(param->default_type);
    free(param);
}

static bool
parser_append_type_bound(Parser *parser, TypeConstraint *constraint, ASTNode *bound)
{
    ASTNode **grown;

    if (constraint == NULL)
        return false;

    if (constraint->bound_count == constraint->bound_capacity) {
        size_t next_capacity = constraint->bound_capacity == 0
            ? 4
            : constraint->bound_capacity * 2;
        grown = realloc(constraint->bounds, next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while appending type bound");
            return false;
        }
        constraint->bounds = grown;
        constraint->bound_capacity = next_capacity;
    }

    constraint->bounds[constraint->bound_count++] = bound;
    return true;
}

static bool
parser_append_where_constraint(Parser *parser, WhereClause *where, TypeConstraint *constraint)
{
    TypeConstraint **grown;

    if (where == NULL)
        return false;

    if (where->count == where->capacity) {
        size_t next_capacity = where->capacity == 0 ? 4 : where->capacity * 2;
        grown = realloc(where->constraints, next_capacity * sizeof(TypeConstraint *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while appending where constraint");
            return false;
        }
        where->constraints = grown;
        where->capacity = next_capacity;
    }

    where->constraints[where->count++] = constraint;
    return true;
}

static void
parser_free_type_constraint(TypeConstraint *constraint)
{
    size_t i;

    if (constraint == NULL)
        return;

    free(constraint->type_param);
    for (i = 0; i < constraint->bound_count; i++)
        ast_destroy(constraint->bounds[i]);
    free(constraint->bounds);
    free(constraint);
}

static bool
parser_append_type_node_with_capacity(Parser *parser,
                                      ASTNode ***items,
                                      size_t *count,
                                      size_t *capacity,
                                      ASTNode *item)
{
    ASTNode **grown;
    size_t next_capacity;

    if (items == NULL || count == NULL || capacity == NULL)
        return false;

    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        grown = realloc(*items, next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while growing type node list");
            return false;
        }
        *items = grown;
        *capacity = next_capacity;
    }

    (*items)[*count] = item;
    (*count)++;
    return true;
}

static bool
parser_token_is_placeholder_type(Token token)
{
    return token.type == TOKEN_IDENTIFIER && token.text != NULL
        && strcmp(token.text, "_") == 0;
}

// ============= 제네릭 파싱 =============

// 제네릭 파라미터 파싱: <T, U: Trait, V = Default>
GenericParams* parse_generic_params(Parser* parser) {
    if (!parser_match(parser, TOKEN_LESS)) return NULL;

    GenericParams* params = calloc(1, sizeof(GenericParams));
    params->count = 0;
    params->capacity = 0;
    params->params = NULL;

    while (!parser_check(parser, TOKEN_GREATER) && !parser_is_at_end(parser)) {
        GenericParam* param = calloc(1, sizeof(GenericParam));

        // 타입 파라미터 이름
        Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type parameter name");
        if (parser_token_is_placeholder_type(name)) {
            parser_error(parser,
                "Generic parameter placeholder '_' is reserved but not implemented.\n"
                "Reason: generic parameter elision needs DAG-owned ambiguity diagnostics before it can be beta-stable.\n"
                "Fix: write an explicit type parameter name.");
        }
        param->name = pergyra_strdup(name.text);

        // 제약조건 ': Trait'
        if (parser_match(parser, TOKEN_COLON)) {
            param->constraint = parse_type_constraint(parser);
        }

        // 기본값 '= Type'
        if (parser_match(parser, TOKEN_ASSIGN)) {
            param->default_type = parse_type(parser);
        }

        // 파라미터 추가
        if (!parser_append_generic_param(parser, params, param)) {
            parser_free_generic_param(param);
            break;
        }

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_GREATER, "Expected '>' after generic parameters");

    return params;
}

// 타입 인자 파싱: <String, List<Event>, HashMap<String, Int>>
GenericParams* parse_type_arguments(Parser* parser) {
    if (!parser_match(parser, TOKEN_LESS)) return NULL;

    GenericParams* params = calloc(1, sizeof(GenericParams));
    params->count = 0;
    params->capacity = 0;
    params->params = NULL;

    while (!parser_check(parser, TOKEN_GREATER) && !parser_is_at_end(parser)) {
        GenericParam* param = calloc(1, sizeof(GenericParam));
        ASTNode* arg_type = parse_type(parser);

        if (arg_type != NULL && arg_type->type == AST_TYPE
            && arg_type->data.type.name != NULL) {
            param->name = pergyra_strdup(arg_type->data.type.name);
        } else if (arg_type != NULL && arg_type->type == AST_EVENT_HANDLER_TYPE) {
            param->name = pergyra_strdup("func");
        } else {
            param->name = pergyra_strdup("TypeArg");
        }
        param->constraint = arg_type;

        if (!parser_append_generic_param(parser, params, param)) {
            parser_free_generic_param(param);
            break;
        }

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_GREATER, "Expected '>' after type arguments");

    return params;
}

// where 절 파싱: where T: Comparable + Clone
WhereClause* parse_where_clause(Parser* parser) {
    if (!parser_match(parser, TOKEN_WHERE)) return NULL;

    WhereClause* where = calloc(1, sizeof(WhereClause));
    where->count = 0;
    where->capacity = 0;
    where->constraints = NULL;

    do {
        TypeConstraint* constraint = calloc(1, sizeof(TypeConstraint));
        bool constraint_ok = true;

        // 타입 파라미터
        Token param = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type parameter");
        if (parser_token_is_placeholder_type(param)) {
            parser_error(parser,
                "Generic parameter placeholder '_' is reserved but not implemented.\n"
                "Reason: generic parameter elision needs DAG-owned ambiguity diagnostics before it can be beta-stable.\n"
                "Fix: write an explicit type parameter name.");
        }
        constraint->type_param = pergyra_strdup(param.text);

        parser_consume(parser, TOKEN_COLON, "Expected ':' after type parameter");

        // Trait 바운드 (Trait1 + Trait2 + ...)
        constraint->bound_count = 0;
        constraint->bound_capacity = 0;
        constraint->bounds = NULL;

        do {
            ASTNode* trait = parse_type(parser);
            if (!parser_append_type_bound(parser, constraint, trait)) {
                ast_destroy(trait);
                constraint_ok = false;
                break;
            }
        } while (parser_match(parser, TOKEN_PLUS));

        // 제약조건 추가
        if (!constraint_ok || !parser_append_where_constraint(parser, where, constraint)) {
            parser_free_type_constraint(constraint);
            break;
        }

    } while (parser_match(parser, TOKEN_COMMA));

    return where;
}

// 타입 파싱: Type<T, U>
ASTNode* parse_type(Parser* parser) {
    if (parser_match(parser, TOKEN_FUNC)) {
        ASTNode *handler_type = ast_create_event_handler_type();

        parser_consume(parser, TOKEN_LPAREN,
            "Expected '(' after 'func' in function type");

        while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
            ASTNode *param_type = parse_type(parser);
            if (!parser_append_type_node_with_capacity(parser,
                    &handler_type->data.event_handler_type.param_types,
                    &handler_type->data.event_handler_type.param_count,
                    &handler_type->data.event_handler_type.param_capacity,
                    param_type)) {
                ast_destroy(param_type);
                break;
            }

            if (!parser_match(parser, TOKEN_COMMA))
                break;
        }

        parser_consume(parser, TOKEN_RPAREN,
            "Expected ')' after function type parameters");

        if (parser_match(parser, TOKEN_ARROW))
            handler_type->data.event_handler_type.return_type = parse_type(parser);
        else
            handler_type->data.event_handler_type.return_type = ast_create_type("Void");

        return handler_type;
    }

    /* Tuple type: (T, U, ...) — requires at least one comma.
     * Single parenthesized (T) is treated as just T (no tuple). */
    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_advance(parser); /* consume '(' */
        ASTNode **elements = NULL;
        size_t    element_count = 0;
        size_t    element_cap   = 0;

        while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
            ASTNode *elem = parse_type(parser);
            if (!parser_append_type_node_with_capacity(parser,
                    &elements,
                    &element_count,
                    &element_cap,
                    elem)) {
                ast_destroy(elem);
                break;
            }
            if (!parser_match(parser, TOKEN_COMMA))
                break;
        }
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' in tuple type");

        if (element_count >= 2) {
            ASTNode *tuple_node = ast_create_type("Tuple");
            tuple_node->data.type.tuple_elements = elements;
            tuple_node->data.type.tuple_element_count = element_count;
            return tuple_node;
        }
        /* Single element: treat (T) as just T */
        if (element_count == 1) {
            ASTNode *single = elements[0];
            free(elements);
            return single;
        }
        /* Empty (): treat as Void */
        free(elements);
        return ast_create_type("Void");
    }

    Token type_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type name");
    if (parser_token_is_placeholder_type(type_name)) {
        parser_error(parser,
            "Generic/type argument elision '_' is reserved but not implemented.\n"
            "Reason: type-argument elision must be backed by DAG evidence and stable ambiguity diagnostics.\n"
            "Fix: write the type explicitly.");
    }
    char *qualified_name = pergyra_strdup(type_name.text);

    while (parser_check(parser, TOKEN_DOT)
           && parser->current_token.length == 1
           && strcmp(parser->current_token.text, ".") == 0) {
        parser_advance(parser);
        Token part = parser_consume(parser, TOKEN_IDENTIFIER,
                                    "Expected type name after '.'");
        if (parser_token_is_placeholder_type(part)) {
            parser_error(parser,
                "Generic/type argument elision '_' is reserved but not implemented.\n"
                "Reason: type-argument elision must be backed by DAG evidence and stable ambiguity diagnostics.\n"
                "Fix: write the type explicitly.");
        }
        size_t prefix_len = strlen(qualified_name);
        size_t part_len = strlen(part.text);
        char *next = malloc(prefix_len + 1 + part_len + 1);
        if (next == NULL) {
            free(qualified_name);
            return ast_create_type(type_name.text);
        }
        memcpy(next, qualified_name, prefix_len);
        next[prefix_len] = '_';
        memcpy(next + prefix_len + 1, part.text, part_len);
        next[prefix_len + 1 + part_len] = '\0';
        free(qualified_name);
        qualified_name = next;
    }

    ASTNode* type_node = ast_create_type(qualified_name);
    free(qualified_name);

    // 제네릭 인자
    if (parser_check(parser, TOKEN_LESS))
        type_node->data.type.generic_args = parse_type_arguments(parser);

    return type_node;
}

ASTNode* parse_type_alias_declaration(Parser *parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected alias name after 'type'");
    parser_consume(parser, TOKEN_ASSIGN, "Expected '=' after alias name");
    ASTNode *target_type = parse_type(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after type alias");

    ASTNode *alias = ast_create_type_alias(name.text, target_type);
    if (alias != NULL) {
        alias->line = name.line;
        alias->column = name.column;
    }
    return alias;
}

// 타입 제약조건 파싱
ASTNode* parse_type_constraint(Parser* parser) {
    // 단순 버전 - 향후 확장 필요
    return parse_type(parser);
}

void skip_generic_arguments(Parser* parser) {
    int depth = 0;

    if (!parser_match(parser, TOKEN_LESS)) {
        return;
    }

    depth = 1;
    while (depth > 0 && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_LESS)) {
            depth++;
        } else if (parser_match(parser, TOKEN_GREATER)) {
            depth--;
        } else {
            parser_advance(parser);
        }
    }
}
