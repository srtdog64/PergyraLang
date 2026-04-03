#include "parser_internal.h"
#include "../semantic/type_system.h"

static bool
parser_effect_mask_from_token(Token tok, uint32_t *mask_out)
{
    if (mask_out == NULL)
        return false;

    if (tok.type == TOKEN_SECURE) {
        *mask_out = EFFECT_SECURE;
        return true;
    }

    if (tok.text == NULL)
        return false;
    if (strcmp(tok.text, "remote") == 0) {
        *mask_out = EFFECT_REMOTE;
        return true;
    }
    if (strcmp(tok.text, "nondeterministic") == 0) {
        *mask_out = EFFECT_NONDETERMINISTIC;
        return true;
    }
    if (strcmp(tok.text, "collapse") == 0) {
        *mask_out = EFFECT_COLLAPSE;
        return true;
    }
    if (strcmp(tok.text, "local") == 0) {
        *mask_out = EFFECT_NONE;
        return true;
    }

    return false;
}

void
parse_optional_effect_clause(Parser *parser, bool *has_clause_out,
                             uint32_t *mask_out)
{
    uint32_t mask = 0;

    if (has_clause_out == NULL || mask_out == NULL || !parser_match(parser, TOKEN_WITH))
        return;

    *has_clause_out = true;

    if (!parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || strcmp(parser->current_token.text, "effects") != 0) {
        parser_error(parser, "Expected 'effects' after 'with'");
        return;
    }
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        uint32_t effect = EFFECT_NONE;
        Token tok;

        if (parser_check(parser, TOKEN_IDENTIFIER) || parser_check(parser, TOKEN_SECURE))
            tok = parser_advance(parser);
        else {
            parser_error(parser, "Expected effect name after 'with effects'");
            return;
        }

        if (!parser_effect_mask_from_token(tok, &effect)) {
            parser_error(parser, "Unknown effect '%s' in effect clause",
                         tok.text != NULL ? tok.text : "<token>");
            return;
        }

        mask |= effect;
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    *mask_out = mask;
}

Token
consume_name_token(Parser* parser, const char* message)
{
    if (parser_check(parser, TOKEN_IDENTIFIER) || parser_check(parser, TOKEN_SLOT))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

// ============= 제네릭 파싱 =============

// 제네릭 파라미터 파싱: <T, U: Trait, V = Default>
GenericParams* parse_generic_params(Parser* parser) {
    if (!parser_match(parser, TOKEN_LESS)) return NULL;

    GenericParams* params = calloc(1, sizeof(GenericParams));
    params->count = 0;
    params->params = NULL;

    while (!parser_check(parser, TOKEN_GREATER) && !parser_is_at_end(parser)) {
        GenericParam* param = calloc(1, sizeof(GenericParam));

        // 타입 파라미터 이름
        Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type parameter name");
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
        params->count++;
        params->params = realloc(params->params, params->count * sizeof(GenericParam*));
        params->params[params->count - 1] = param;

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_GREATER, "Expected '>' after generic parameters");

    return params;
}

// where 절 파싱: where T: Comparable + Clone
WhereClause* parse_where_clause(Parser* parser) {
    if (!parser_match(parser, TOKEN_WHERE)) return NULL;

    WhereClause* where = calloc(1, sizeof(WhereClause));
    where->count = 0;
    where->constraints = NULL;

    do {
        TypeConstraint* constraint = calloc(1, sizeof(TypeConstraint));

        // 타입 파라미터
        Token param = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type parameter");
        constraint->type_param = pergyra_strdup(param.text);

        parser_consume(parser, TOKEN_COLON, "Expected ':' after type parameter");

        // Trait 바운드 (Trait1 + Trait2 + ...)
        constraint->bound_count = 0;
        constraint->bounds = NULL;

        do {
            ASTNode* trait = parse_type(parser);
            constraint->bound_count++;
            constraint->bounds = realloc(constraint->bounds,
                                       constraint->bound_count * sizeof(ASTNode*));
            constraint->bounds[constraint->bound_count - 1] = trait;
        } while (parser_match(parser, TOKEN_PLUS));

        // 제약조건 추가
        where->count++;
        where->constraints = realloc(where->constraints,
                                   where->count * sizeof(TypeConstraint*));
        where->constraints[where->count - 1] = constraint;

    } while (parser_match(parser, TOKEN_COMMA));

    return where;
}

// 타입 파싱: Type<T, U>
ASTNode* parse_type(Parser* parser) {
    Token type_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected type name");
    char *qualified_name = pergyra_strdup(type_name.text);

    while (parser_check(parser, TOKEN_DOT)
           && parser->current_token.length == 1
           && strcmp(parser->current_token.text, ".") == 0) {
        parser_advance(parser);
        Token part = parser_consume(parser, TOKEN_IDENTIFIER,
                                    "Expected type name after '.'");
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
    if (parser_check(parser, TOKEN_LESS)) {
        type_node->data.type.generic_args = parse_generic_params(parser);
    }

    return type_node;
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

// 함수 선언 파싱
ASTNode* parse_function_declaration(Parser* parser) {
    // 함수 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected function name");

    ASTNode* func = ast_create_function(name.text);
    parser->last_func_decl_async = false;
    func->data.func_decl.doc_comment = parser_take_pending_doc_comment(parser);

    // 제네릭 파라미터
    func->data.func_decl.generic_params = parse_generic_params(parser);

    // 함수 파라미터
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after function name");

    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        // 소유권 수식자: own / ref
        ParamMode mode = PARAM_MODE_DEFAULT;
        if (parser_match(parser, TOKEN_OWN))
            mode = PARAM_MODE_OWN;
        else if (parser_match(parser, TOKEN_REF))
            mode = PARAM_MODE_REF;

        // 파라미터 이름
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");

        FuncParam* param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup(param_name.text);
        param->mode = mode;

        // self 파라미터: no type annotation needed
        if (strcmp(param_name.text, "self") == 0
            && !parser_check(parser, TOKEN_COLON)) {
            param->type = NULL; // self type resolved by codegen
        } else {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");
            ASTNode* param_type = parse_type(parser);
            param->type = param_type;
        }

        // 파라미터 추가
        func->data.func_decl.param_count++;
        func->data.func_decl.params = realloc(func->data.func_decl.params,
                                             func->data.func_decl.param_count * sizeof(FuncParam*));
        func->data.func_decl.params[func->data.func_decl.param_count - 1] = param;

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");

    // 반환 타입
    if (parser_match(parser, TOKEN_ARROW)) {
        func->data.func_decl.return_type = parse_type(parser);
    }

    while (!parser_is_at_end(parser)) {
        if (func->data.func_decl.where_clause == NULL
            && parser_check(parser, TOKEN_WHERE)) {
            func->data.func_decl.where_clause = parse_where_clause(parser);
            continue;
        }
        if (!func->data.func_decl.has_effects_clause
            && parser_check(parser, TOKEN_WITH)) {
            parse_optional_effect_clause(parser,
                &func->data.func_decl.has_effects_clause,
                &func->data.func_decl.declared_effects);
            continue;
        }
        break;
    }

    if (parser->in_extern_block) {
        parser_consume(parser, TOKEN_SEMICOLON,
            "Expected ';' after extern function declaration");
        return func;
    }

    // Abstract/declaration-only method (ends with ';' instead of '{')
    if (parser_match(parser, TOKEN_SEMICOLON)) {
        func->data.func_decl.body = NULL;
        return func;
    }

    // 함수 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' before function body");
    func->data.func_decl.body = parser_parse_block(parser);

    return func;
}

// 클래스 선언 파싱
ASTNode* parse_class_declaration(Parser* parser) {
    return parse_type_declaration(parser, false);
}

// 구조체 선언 파싱
ASTNode* parse_struct_declaration(Parser* parser) {
    return parse_type_declaration(parser, true);
}

// 클래스/구조체 선언 공통 파싱
ASTNode* parse_type_declaration(Parser* parser, bool is_struct) {
    // 클래스 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER,
        is_struct ? "Expected struct name" : "Expected class name");

    ASTNode* class_decl = is_struct ? ast_create_struct(name.text)
                                    : ast_create_class(name.text);
    class_decl->data.class_decl.doc_comment = parser_take_pending_doc_comment(parser);

    // 제네릭 파라미터
    class_decl->data.class_decl.generic_params = parse_generic_params(parser);

    // where 절
    class_decl->data.class_decl.where_clause = parse_where_clause(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after class name");

    // 클래스 멤버들
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        // 접근 제어자
        AccessModifier access = is_struct ? ACCESS_PUBLIC : ACCESS_PRIVATE;
        if (parser_match(parser, TOKEN_PUBLIC)) {
            access = ACCESS_PUBLIC;
        } else if (parser_match(parser, TOKEN_PRIVATE)) {
            access = ACCESS_PRIVATE;
        }

        // 클래스 필드 또는 구조체 bare field / let field
        if (parser_check(parser, TOKEN_LET) ||
            (is_struct && parser_check(parser, TOKEN_IDENTIFIER))) {
            bool has_let = parser_match(parser, TOKEN_LET);
            if (!has_let && !is_struct) {
                parser_error(parser, "Expected field or method declaration");
                return class_decl;
            }

            // 필드
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected field name");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after field name");
            ASTNode* field_type = parse_type(parser);

            ClassField* field = calloc(1, sizeof(ClassField));
            field->name = pergyra_strdup(field_name.text);
            field->type = field_type;
            field->access = access;

            // 필드 추가
            class_decl->data.class_decl.field_count++;
            class_decl->data.class_decl.fields = realloc(
                class_decl->data.class_decl.fields,
                class_decl->data.class_decl.field_count * sizeof(ClassField*)
            );
            class_decl->data.class_decl.fields[class_decl->data.class_decl.field_count - 1] = field;

            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after field declaration");
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_FUNC)) {
            // 메서드
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            method->data.func_decl.access = access;

            // 메서드 추가
            class_decl->data.class_decl.method_count++;
            class_decl->data.class_decl.methods = realloc(
                class_decl->data.class_decl.methods,
                class_decl->data.class_decl.method_count * sizeof(ASTNode*)
            );
            class_decl->data.class_decl.methods[class_decl->data.class_decl.method_count - 1] = method;
        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser, "Expected %s member declaration",
                is_struct ? "struct" : "class");
            return class_decl;
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after class body");

    return class_decl;
}

ASTNode* parse_extern_block(Parser* parser) {
    Token abi = parser_consume(parser, TOKEN_STRING,
        "Expected ABI string after extern");
    if (abi.length < 2) {
        parser_error(parser, "Invalid ABI string");
        return NULL;
    }

    char* abi_name = pergyra_strndup(abi.text + 1, abi.length - 2);
    ASTNode* block = ast_create_extern_block(abi_name);
    free(abi_name);
    if (!block) return NULL;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after extern ABI");

    bool prev_extern = parser->in_extern_block;
    parser->in_extern_block = true;

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        parser_consume(parser, TOKEN_FUNC,
            "Expected 'func' declaration inside extern block");
        ASTNode* decl = parser_finalize_statement(parser, parse_function_declaration(parser));
        if (decl) {
            ast_add_statement(block, decl);
        }
    }

    parser->in_extern_block = prev_extern;
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after extern block");
    return block;
}
