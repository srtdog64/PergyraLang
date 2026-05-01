#include "parser_internal.h"

static bool
parser_append_func_param(Parser *parser, ASTNode *func, FuncParam *param)
{
    FuncParam **grown;

    if (parser == NULL || func == NULL || param == NULL)
        return false;

    if (func->data.func_decl.param_count == func->data.func_decl.param_capacity) {
        size_t next_capacity = func->data.func_decl.param_capacity == 0
            ? 4
            : func->data.func_decl.param_capacity * 2;
        grown = realloc(func->data.func_decl.params, next_capacity * sizeof(FuncParam *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing function parameters");
            return false;
        }
        func->data.func_decl.params = grown;
        func->data.func_decl.param_capacity = next_capacity;
    }

    func->data.func_decl.params[func->data.func_decl.param_count++] = param;
    return true;
}

static bool
parser_append_class_field(Parser *parser, ASTNode *class_decl, ClassField *field)
{
    ClassField **grown;

    if (parser == NULL || class_decl == NULL || field == NULL)
        return false;

    if (class_decl->data.class_decl.field_count == class_decl->data.class_decl.field_capacity) {
        size_t next_capacity = class_decl->data.class_decl.field_capacity == 0
            ? 4
            : class_decl->data.class_decl.field_capacity * 2;
        grown = realloc(class_decl->data.class_decl.fields, next_capacity * sizeof(ClassField *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing nominal fields");
            return false;
        }
        class_decl->data.class_decl.fields = grown;
        class_decl->data.class_decl.field_capacity = next_capacity;
    }

    class_decl->data.class_decl.fields[class_decl->data.class_decl.field_count++] = field;
    return true;
}

static bool
parser_append_class_method(Parser *parser, ASTNode *class_decl, ASTNode *method)
{
    ASTNode **grown;

    if (parser == NULL || class_decl == NULL || method == NULL)
        return false;

    if (class_decl->data.class_decl.method_count == class_decl->data.class_decl.method_capacity) {
        size_t next_capacity = class_decl->data.class_decl.method_capacity == 0
            ? 4
            : class_decl->data.class_decl.method_capacity * 2;
        grown = realloc(class_decl->data.class_decl.methods, next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing nominal methods");
            return false;
        }
        class_decl->data.class_decl.methods = grown;
        class_decl->data.class_decl.method_capacity = next_capacity;
    }

    class_decl->data.class_decl.methods[class_decl->data.class_decl.method_count++] = method;
    return true;
}

// 함수 선언 파싱
static ASTNode* parse_function_like_declaration(Parser* parser, bool is_action) {
    // 함수 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected function name");

    ASTNode* func = ast_create_function(name.text);
    parser->last_func_decl_async = false;
    func->data.func_decl.doc_comment = parser_take_pending_doc_comment(parser);
    func->data.func_decl.is_action = is_action;

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
        Token param_name = consume_binding_name_token(parser, "Expected parameter name");

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

        parser_append_func_param(parser, func, param);

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after parameters");

    // 반환 타입
    if (parser_match(parser, TOKEN_ARROW)) {
        func->data.func_decl.return_type = parse_type(parser);
    }

    while (!parser_is_at_end(parser)) {
        bool matched = false;

        if (!parser_decl_parse_next_function_clause(parser, func, is_action,
                                                    &matched)
            || parser_has_error(parser)) {
            return func;
        }
        if (!matched) {
            break;
        }
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

ASTNode* parse_function_declaration(Parser* parser) {
    return parse_function_like_declaration(parser, false);
}

ASTNode* parse_action_declaration(Parser* parser) {
    return parse_function_like_declaration(parser, true);
}

// 클래스 선언 파싱
ASTNode* parse_class_declaration(Parser* parser) {
    return parse_type_declaration(parser, NOMINAL_DECL_CLASS);
}

ASTNode* parse_subject_declaration(Parser* parser) {
    return parse_type_declaration(parser, NOMINAL_DECL_SUBJECT);
}

ASTNode* parse_vessel_declaration(Parser* parser) {
    return parse_type_declaration(parser, NOMINAL_DECL_VESSEL);
}

// 구조체 선언 파싱
ASTNode* parse_struct_declaration(Parser* parser) {
    return parse_type_declaration(parser, NOMINAL_DECL_STRUCT);
}

ASTNode* parse_object_declaration(Parser* parser) {
    return parse_type_declaration(parser, NOMINAL_DECL_OBJECT);
}

ASTNode* parse_tobject_declaration(Parser* parser) {
    return parse_type_declaration(parser, NOMINAL_DECL_TOBJECT);
}

// 클래스/구조체 선언 공통 파싱
ASTNode* parse_type_declaration(Parser* parser, NominalDeclKind decl_kind) {
    bool is_struct = (decl_kind == NOMINAL_DECL_STRUCT
        || decl_kind == NOMINAL_DECL_VESSEL
        || decl_kind == NOMINAL_DECL_OBJECT
        || decl_kind == NOMINAL_DECL_TOBJECT);
    const char *kind_name = "class";
    ASTNode *class_decl = NULL;

    if (decl_kind == NOMINAL_DECL_SUBJECT)
        kind_name = "subject";
    else if (decl_kind == NOMINAL_DECL_VESSEL)
        kind_name = "vessel";
    else if (decl_kind == NOMINAL_DECL_STRUCT)
        kind_name = "struct";
    else if (decl_kind == NOMINAL_DECL_OBJECT)
        kind_name = "object";
    else if (decl_kind == NOMINAL_DECL_TOBJECT)
        kind_name = "tobject";

    // 클래스 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER,
        is_struct ? "Expected value/projection type name" : "Expected nominal type name");

    switch (decl_kind) {
    case NOMINAL_DECL_SUBJECT:
        class_decl = ast_create_subject(name.text);
        break;
    case NOMINAL_DECL_VESSEL:
        class_decl = ast_create_vessel(name.text);
        break;
    case NOMINAL_DECL_STRUCT:
        class_decl = ast_create_struct(name.text);
        break;
    case NOMINAL_DECL_OBJECT:
        class_decl = ast_create_object(name.text);
        break;
    case NOMINAL_DECL_TOBJECT:
        class_decl = ast_create_tobject(name.text);
        break;
    case NOMINAL_DECL_CLASS:
    default:
        class_decl = ast_create_class(name.text);
        break;
    }
    class_decl->data.class_decl.doc_comment = parser_take_pending_doc_comment(parser);

    // 제네릭 파라미터
    class_decl->data.class_decl.generic_params = parse_generic_params(parser);

    // where 절
    class_decl->data.class_decl.where_clause = parse_where_clause(parser);

    {
        char brace_msg[96];
        snprintf(brace_msg, sizeof(brace_msg), "Expected '{' after %s name", kind_name);
        parser_consume(parser, TOKEN_LBRACE, brace_msg);
    }

    // 클래스 멤버들
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        // 접근 제어자
        AccessModifier access = is_struct ? ACCESS_PUBLIC : ACCESS_PRIVATE;
        bool explicit_access = false;
        if (parser_match(parser, TOKEN_PUBLIC)) {
            access = ACCESS_PUBLIC;
            explicit_access = true;
        } else if (parser_match(parser, TOKEN_PRIVATE)) {
            access = ACCESS_PRIVATE;
            explicit_access = true;
        }

        // 클래스 필드 또는 구조체 bare field / let field
        if (parser_check(parser, TOKEN_LET) ||
            (decl_kind == NOMINAL_DECL_SUBJECT
             && parser_check(parser, TOKEN_VESSEL)) ||
            (is_struct && parser_check(parser, TOKEN_IDENTIFIER))) {
            bool has_let = parser_match(parser, TOKEN_LET);
            bool is_vessel_field = false;
            if (!has_let && decl_kind == NOMINAL_DECL_SUBJECT) {
                is_vessel_field = parser_match(parser, TOKEN_VESSEL);
            }
            if (!has_let && !is_struct) {
                if (is_vessel_field) {
                    /* handled below */
                } else {
                    parser_error(parser, "Expected field or method declaration");
                    return class_decl;
                }
            }

            if (!has_let && !is_struct && !is_vessel_field) {
                parser_error(parser, "Expected field or method declaration");
                return class_decl;
            }

            /* Destructuring field declarations like
             *     private let (slot, token) = ClaimSecureSlot<Int>(lvl);
             * are not yet supported. Emit a targeted error with a fix hint
             * instead of the generic "Expected field name". */
            if (has_let && parser_check(parser, TOKEN_LPAREN)) {
                parser_error(parser,
                    "class-body destructuring field is not yet supported; "
                    "declare each field separately and initialize them in a "
                    "constructor/init method");
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
            field->has_explicit_access = explicit_access;
            field->is_vessel_field = is_vessel_field;

            parser_append_class_field(parser, class_decl, field);

            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after field declaration");
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_FUNC)) {
            // 메서드
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            method->data.func_decl.access = access;
            method->data.func_decl.has_explicit_access = explicit_access;

            parser_append_class_method(parser, class_decl, method);
        } else if (decl_kind == NOMINAL_DECL_SUBJECT
            && parser_decl_match_contextual_keyword(parser, "action")) {
            ASTNode* method = parser_finalize_statement(parser, parse_action_declaration(parser));
            method->data.func_decl.access = access;
            method->data.func_decl.has_explicit_access = explicit_access;

            parser_append_class_method(parser, class_decl, method);
        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser, "Expected %s member declaration",
                kind_name);
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
