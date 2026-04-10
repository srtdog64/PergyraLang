/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser_internal.h"
#include <ctype.h>

// 파서 생성
Parser* parser_create(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!parser) return NULL;

    parser->lexer = lexer;
    parser->has_error = false;
    parser->scope_depth = 0;
    parser->in_parallel_block = false;
    parser->in_with_statement = false;
    parser->in_extern_block = false;
    parser->next_decl_exported = false;
    parser->last_func_decl_async = false;
    parser->pending_doc_comment = NULL;
    parser->source_path = NULL;

    // 첫 번째 토큰 읽기
    parser->current_token = lexer_next_token(lexer);

    return parser;
}

// 파서 소멸
void parser_destroy(Parser* parser) {
    if (parser) {
        ast_destroy_structured_comment(parser->pending_doc_comment);
        free(parser);
    }
}

// 토큰 진행
Token parser_advance(Parser* parser) {
    parser->previous_token = parser->current_token;
    parser->current_token = lexer_next_token(parser->lexer);
    return parser->previous_token;
}

// 다음 토큰 미리보기 (non-destructive lookahead)
Token parser_peek_next(Parser* parser) {
    /* Save lexer state */
    const char *saved_current = parser->lexer->current;
    size_t saved_pos = parser->lexer->position;
    uint32_t saved_line = parser->lexer->line;
    uint32_t saved_col = parser->lexer->column;

    Token next = lexer_next_token(parser->lexer);

    /* Restore lexer state */
    parser->lexer->current = saved_current;
    parser->lexer->position = saved_pos;
    parser->lexer->line = saved_line;
    parser->lexer->column = saved_col;

    return next;
}

// 토큰 타입 확인
bool parser_check(Parser* parser, PgyTokenType type) {
    return parser->current_token.type == type;
}

// 토큰 매칭 및 진행
bool parser_match(Parser* parser, PgyTokenType type) {
    if (!parser_check(parser, type)) return false;
    parser_advance(parser);
    return true;
}

static bool
parser_match_contextual_keyword(Parser *parser, const char *keyword)
{
    if (!parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || keyword == NULL) {
        return false;
    }

    if (strcmp(parser->current_token.text, keyword) != 0)
        return false;

    parser_advance(parser);
    return true;
}

static bool
parser_check_contextual_keyword(Parser *parser, const char *keyword)
{
    return parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && keyword != NULL
        && strcmp(parser->current_token.text, keyword) == 0;
}

static bool
parser_starts_contextual_declaration(Parser *parser, const char *keyword)
{
    Token next;

    if (!parser_check_contextual_keyword(parser, keyword))
        return false;

    next = parser_peek_next(parser);
    return next.type == TOKEN_IDENTIFIER;
}

// 토큰 소비 (필수)
Token parser_consume(Parser* parser, PgyTokenType type, const char* message) {
    if (parser_check(parser, type)) {
        return parser_advance(parser);
    }

    parser_error(parser, message);
    return parser->current_token;
}

// 에러 처리
void parser_error(Parser* parser, const char* format, ...) {
    parser->has_error = true;

    va_list args;
    va_start(args, format);
    vsnprintf(parser->error_msg, sizeof(parser->error_msg), format, args);
    va_end(args);

    // 에러 위치 정보 추가
    char location[256];
    snprintf(location, sizeof(location), " at line %d, column %d",
             parser->current_token.line, parser->current_token.column);
    strncat(parser->error_msg, location,
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
}

// 에러 복구 - 다음 문장까지 건너뛰기
void parser_synchronize(Parser* parser) {
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        if (parser->previous_token.type == TOKEN_SEMICOLON) return;
        if (parser_check_contextual_keyword(parser, "object")
            || parser_check_contextual_keyword(parser, "vessel")
            || parser_check_contextual_keyword(parser, "intent")
            || parser_check_contextual_keyword(parser, "tobject")
            || parser_check_contextual_keyword(parser, "world")
            || parser_check_contextual_keyword(parser, "roster")
            || parser_check_contextual_keyword(parser, "roster")  /* deprecated */
            || parser_check_contextual_keyword(parser, "relation")
            || parser_check_contextual_keyword(parser, "effect")
            || parser_check_contextual_keyword(parser, "zone")) {
            return;
        }

        switch (parser->current_token.type) {
            case TOKEN_CLASS:
            case TOKEN_STRUCT:
            case TOKEN_EXTERN:
            case TOKEN_FUNC:
            case PGY_TOKEN_TYPE:
            case TOKEN_LET:
            case TOKEN_INNATE:
            case TOKEN_ABILITY:
            case TOKEN_ROLE:
            case TOKEN_PARTY:
            case TOKEN_NAMESPACE:
            case TOKEN_EXPORT:
            case TOKEN_WITH:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
                return;
            default:
                break;
        }

        parser_advance(parser);
    }
}

// 파싱 종료 확인
bool parser_is_at_end(const Parser* parser) {
    return parser->current_token.type == TOKEN_EOF;
}

// 에러 확인
bool parser_has_error(const Parser* parser) {
    return parser->has_error;
}

// 에러 메시지 가져오기
const char* parser_get_error(const Parser* parser) {
    return parser->error_msg;
}

static char *
parser_trimmed_copy(const char *text)
{
    const char *start = text;
    const char *end;
    size_t length;

    if (text == NULL)
        return pergyra_strdup("");

    while (*start != '\0' && isspace((unsigned char)*start))
        start++;

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    length = (size_t)(end - start);
    return pergyra_strndup(start, length);
}

static bool
parser_doc_tag_name_equals(const char *name, const char *expected)
{
    size_t i;

    if (name == NULL || expected == NULL)
        return false;

    for (i = 0; name[i] != '\0' && expected[i] != '\0'; i++) {
        if (tolower((unsigned char)name[i]) != tolower((unsigned char)expected[i]))
            return false;
    }

    return name[i] == '\0' && expected[i] == '\0';
}

static bool
parser_doc_tag_type_from_name(const char *name, DocTagType *out_type)
{
    if (parser_doc_tag_name_equals(name, "what")) {
        *out_type = DOC_TAG_WHAT;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "why")) {
        *out_type = DOC_TAG_WHY;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "alt")) {
        *out_type = DOC_TAG_ALT;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "next")) {
        *out_type = DOC_TAG_NEXT;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "effects")) {
        *out_type = DOC_TAG_EFFECTS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "params")) {
        *out_type = DOC_TAG_PARAMS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "returns")) {
        *out_type = DOC_TAG_RETURNS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "throws")) {
        *out_type = DOC_TAG_THROWS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "complexity")) {
        *out_type = DOC_TAG_COMPLEXITY;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "invariants")) {
        *out_type = DOC_TAG_INVARIANTS;
        return true;
    }
    if (parser_doc_tag_name_equals(name, "example")) {
        *out_type = DOC_TAG_EXAMPLE;
        return true;
    }

    return false;
}

static StructuredComment *
parser_ensure_pending_doc_comment(Parser *parser)
{
    if (parser->pending_doc_comment == NULL)
        parser->pending_doc_comment = calloc(1, sizeof(StructuredComment));
    return parser->pending_doc_comment;
}

static void
parser_add_doc_tag(Parser *parser, DocTagType type, const char *content)
{
    StructuredComment *comment;
    DocTag *tag;
    DocTag **new_tags;

    if (content == NULL)
        return;

    comment = parser_ensure_pending_doc_comment(parser);
    if (comment == NULL)
        return;

    tag = calloc(1, sizeof(DocTag));
    if (tag == NULL)
        return;

    tag->type = type;
    tag->content = parser_trimmed_copy(content);

    new_tags = realloc(comment->tags, (comment->tag_count + 1) * sizeof(DocTag *));
    if (new_tags == NULL) {
        free(tag->content);
        free(tag);
        return;
    }

    comment->tags = new_tags;
    comment->tags[comment->tag_count++] = tag;
}

static void
parser_parse_doc_comment_line(Parser *parser, const char *line)
{
    char *trimmed;
    char *colon;

    if (line == NULL)
        return;

    trimmed = parser_trimmed_copy(line);
    if (trimmed == NULL || trimmed[0] == '\0') {
        free(trimmed);
        return;
    }

    if (trimmed[0] == '@') {
        char *name_start = trimmed + 1;
        char *cursor = name_start;
        DocTagType type;

        while (*cursor != '\0' && !isspace((unsigned char)*cursor) && *cursor != ':')
            cursor++;

        if (*cursor != '\0') {
            *cursor++ = '\0';
            while (*cursor != '\0' && (isspace((unsigned char)*cursor) || *cursor == ':'))
                cursor++;
        }

        if (parser_doc_tag_type_from_name(name_start, &type))
            parser_add_doc_tag(parser, type, cursor);

        free(trimmed);
        return;
    }

    if (trimmed[0] == '[') {
        char *close = strchr(trimmed, ']');
        DocTagType type;

        if (close != NULL) {
            char *content_start = close + 1;
            *close = '\0';
            while (*content_start != '\0' &&
                   (isspace((unsigned char)*content_start) || *content_start == ':'))
                content_start++;

            if (parser_doc_tag_type_from_name(trimmed + 1, &type))
                parser_add_doc_tag(parser, type, content_start);
        }

        free(trimmed);
        return;
    }

    colon = strchr(trimmed, ':');
    if (colon != NULL) {
        DocTagType type;
        *colon = '\0';
        if (parser_doc_tag_type_from_name(trimmed, &type))
            parser_add_doc_tag(parser, type, colon + 1);
    }

    free(trimmed);
}

void
parser_collect_doc_comments(Parser *parser)
{
    while (parser_check(parser, TOKEN_DOC_COMMENT)) {
        Token doc = parser_advance(parser);
        parser_parse_doc_comment_line(parser, doc.text);
    }
}

void
parser_discard_pending_doc_comment(Parser *parser)
{
    if (parser == NULL)
        return;

    ast_destroy_structured_comment(parser->pending_doc_comment);
    parser->pending_doc_comment = NULL;
}

StructuredComment *
parser_take_pending_doc_comment(Parser *parser)
{
    StructuredComment *comment;

    if (parser == NULL)
        return NULL;

    comment = parser->pending_doc_comment;
    parser->pending_doc_comment = NULL;
    return comment;
}

static bool
parser_attach_pending_doc_comment(Parser *parser, ASTNode *node)
{
    if (parser == NULL || node == NULL || parser->pending_doc_comment == NULL)
        return false;

    switch (node->type) {
        case AST_FUNC_DECL:
            if (parser->last_func_decl_async) {
                node->data.async_func_decl.doc_comment = parser->pending_doc_comment;
            } else {
                node->data.func_decl.doc_comment = parser->pending_doc_comment;
            }
            parser->pending_doc_comment = NULL;
            return true;
        case AST_CLASS_DECL:
            node->data.class_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ABILITY_DECL:
            node->data.ability_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ROLE_DECL:
            node->data.role_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_PARTY_DECL:
            node->data.party_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ROSTER_DECL:
            node->data.roster_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_WORLD_DECL:
            node->data.world_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_INTENT_DECL:
            node->data.intent_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_RELATION_DECL:
            node->data.relation_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_EFFECT_DECL:
            node->data.effect_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        case AST_ZONE_DECL:
            node->data.zone_decl.doc_comment = parser->pending_doc_comment;
            parser->pending_doc_comment = NULL;
            return true;
        default:
            return false;
    }
}

static bool
parser_is_exportable_decl(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_FUNC_DECL:
        case AST_CLASS_DECL:
        case AST_EXTERN_BLOCK:
        case AST_LET_DECL:
        case AST_TYPE_ALIAS:
        case AST_ABILITY_DECL:
        case AST_ROLE_DECL:
        case AST_PARTY_DECL:
        case AST_ROSTER_DECL:
        case AST_WORLD_DECL:
        case AST_INTENT_DECL:
        case AST_RELATION_DECL:
        case AST_EFFECT_DECL:
        case AST_ZONE_DECL:
        case AST_EVENT_DECL:
        case AST_ENUM_DECL:
        case AST_IMPORT_DECL:
        case AST_NAMESPACE_DECL:
            return true;
        default:
            return false;
    }
}

static ASTNode *
parser_parse_export_declaration(Parser *parser)
{
    ASTNode *node = NULL;

    if (parser_match(parser, TOKEN_ASYNC))
        node = parser_parse_async_function(parser);
    else if (parser_match(parser, TOKEN_FUNC))
        node = parse_function_declaration(parser);
    else if (parser_match(parser, TOKEN_IMPORT)) {
        Token path = parser_consume(parser, TOKEN_STRING,
            "Expected string path after 'import'");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after import");
        char *raw = pergyra_strndup(path.text + 1, path.length - 2);
        node = ast_create_import_declaration(raw);
        free(raw);
    } else if (parser_match(parser, TOKEN_NAMESPACE)) {
        Token name_tok = parser_consume(parser, TOKEN_IDENTIFIER,
            "Expected namespace name");
        node = ast_create_namespace_declaration(name_tok.text);
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after namespace name");
        while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
            ASTNode *stmt = parser_parse_statement(parser);
            if (stmt != NULL)
                ast_add_statement(node, stmt);
            if (parser->has_error)
                parser_synchronize(parser);
        }
        parser_consume(parser, TOKEN_RBRACE, "Expected '}' after namespace body");
    } else if (parser_match(parser, TOKEN_EXTERN))
        node = parse_extern_block(parser);
    else if (parser_match(parser, PGY_TOKEN_TYPE))
        node = parse_type_alias_declaration(parser);
    else if (parser_check(parser, TOKEN_CLASS)) {
        bool is_subject = parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "subject") == 0;
        parser_advance(parser);
        node = is_subject ? parse_subject_declaration(parser)
                          : parse_class_declaration(parser);
    } else if (parser_match_contextual_keyword(parser, "object"))
        node = parse_object_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "vessel"))
        node = parse_vessel_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "intent"))
        node = parse_intent_declaration(parser);
    else if (parser_check(parser, TOKEN_STRUCT)) {
        bool is_tobject = parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "tobject") == 0;
        parser_advance(parser);
        node = is_tobject ? parse_tobject_declaration(parser)
                      : parse_struct_declaration(parser);
    }
    else if (parser_match(parser, TOKEN_LET))
        node = parser_parse_let_declaration(parser);
    else if (parser_match(parser, TOKEN_ENUM)) {
        Token name_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected enum name");
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
        size_t cap = 0;

        while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
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
            Token var_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variant name");
            size_t idx = node->data.enum_decl.variant_count;
            if (idx >= cap) {
                cap = cap == 0 ? 4 : cap * 2;
                node->data.enum_decl.variants = realloc(
                    node->data.enum_decl.variants, cap * sizeof(char*));
                node->data.enum_decl.variant_params = realloc(
                    node->data.enum_decl.variant_params, cap * sizeof(ASTNode**));
                node->data.enum_decl.variant_param_counts = realloc(
                    node->data.enum_decl.variant_param_counts, cap * sizeof(size_t));
            }
            node->data.enum_decl.variants[idx] =
                pergyra_strndup(var_tok.text, var_tok.length);
            node->data.enum_decl.variant_params[idx] = NULL;
            node->data.enum_decl.variant_param_counts[idx] = 0;

            /* Tagged union: parse variant parameters — Circle(Int, Int) */
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
                            pcap * sizeof(ASTNode*));
                    }
                    node->data.enum_decl.variant_params[idx][pc] = ptype;
                    node->data.enum_decl.variant_param_counts[idx]++;
                    if (!parser_match(parser, TOKEN_COMMA)) break;
                }
                parser_consume(parser, TOKEN_RPAREN,
                    "Expected ')' after variant parameters");
            }

            node->data.enum_decl.variant_count++;
            if (!parser_match(parser, TOKEN_COMMA)) break;
        }
        parser_consume(parser, TOKEN_RBRACE, "Expected '}' after enum variants");
    } else if (parser_match_contextual_keyword(parser, "roster")
               || parser_match_contextual_keyword(parser, "roster"))  /* roster legacy alias */
        node = parse_roster_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "world"))
        node = parse_world_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "relation"))
        node = parse_relation_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "effect"))
        node = parse_effect_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "zone"))
        node = parse_zone_declaration(parser);
    else if (parser_match(parser, TOKEN_PARTY))
        node = parse_party_declaration(parser);
    else if (parser_match(parser, TOKEN_INNATE)) {
        parser_consume(parser, TOKEN_ABILITY,
            "Expected 'ability' after 'innate'");
        node = parse_ability_declaration(parser, true);
    }
    else if (parser_match(parser, TOKEN_ABILITY))
        node = parse_ability_declaration(parser, false);
    else if (parser_match(parser, TOKEN_ROLE))
        node = parse_role_declaration(parser);
    else if (parser_match_contextual_keyword(parser, "event"))
        node = parse_event_declaration(parser);

    if (node == NULL) {
        parser_error(parser, "'export' can only apply to declarations");
        return NULL;
    }

    node->is_exported = true;
    return parser_finalize_statement(parser, node);
}

ASTNode *
parser_finalize_statement(Parser *parser, ASTNode *node)
{
    if (node != NULL && parser->next_decl_exported) {
        if (parser_is_exportable_decl(node)) {
            node->is_exported = true;
        } else {
            parser_error(parser, "'export' can only apply to declarations");
        }
    }
    if (node != NULL && node->origin_path == NULL && parser->source_path != NULL)
        node->origin_path = pergyra_strdup(parser->source_path);
    if (!parser_attach_pending_doc_comment(parser, node))
        parser_discard_pending_doc_comment(parser);
    parser->next_decl_exported = false;
    parser->last_func_decl_async = false;
    return node;
}

// ============= 문장 파싱 =============

// 프로그램 파싱
ASTNode* parser_parse_program(Parser* parser) {
    ASTNode* program = ast_create_program();

    while (!parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_statement(program, stmt);
        }

        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }

    return program;
}

// 문장 파싱
ASTNode* parser_parse_statement(Parser* parser) {
    parser_collect_doc_comments(parser);

    // async 함수 선언
    if (parser_match(parser, TOKEN_ASYNC)) {
        if (parser_check(parser, TOKEN_FUNC))
            return parser_finalize_statement(parser, parser_parse_async_function(parser));
        if (parser_check(parser, TOKEN_LBRACE))
            return parser_finalize_statement(parser, parser_parse_async_block(parser));
        parser_error(parser, "Expected 'func' or '{' after 'async'");
        return NULL;
    }

    // select 문
    if (parser_match(parser, TOKEN_SELECT)) {
        return parser_finalize_statement(parser, parser_parse_select_statement(parser));
    }

    // export 수식어 — declaration only
    if (parser_match(parser, TOKEN_EXPORT)) {
        return parser_parse_export_declaration(parser);
    }

    // top-level access modifier for ability declarations
    if (parser_check(parser, TOKEN_PUBLIC)
        || parser_check(parser, TOKEN_PRIVATE)) {
        AccessModifier access = ACCESS_PUBLIC;
        bool explicit_access = true;
        PgyTokenType access_tok = parser->current_token.type;
        ASTNode *node = NULL;

        parser_advance(parser);
        if (access_tok == TOKEN_PRIVATE)
            access = ACCESS_PRIVATE;

        if (parser_match(parser, TOKEN_INNATE)) {
            parser_consume(parser, TOKEN_ABILITY,
                "Expected 'ability' after 'innate'");
            node = parse_ability_declaration(parser, true);
        } else if (parser_match(parser, TOKEN_ABILITY)) {
            node = parse_ability_declaration(parser, false);
        } else {
            parser_error(parser,
                "Top-level access modifiers currently apply only to ability declarations");
            return NULL;
        }

        if (node != NULL && node->type == AST_ABILITY_DECL) {
            node->data.ability_decl.access = access;
            node->data.ability_decl.has_explicit_access = explicit_access;
            if (access == ACCESS_PRIVATE || access == ACCESS_PROTECTED)
                node->is_exported = false;
            else
                node->is_exported = true;
        }
        return parser_finalize_statement(parser, node);
    }

    // 함수 선언
    if (parser_match(parser, TOKEN_FUNC)) {
        return parser_finalize_statement(parser, parse_function_declaration(parser));
    }

    // use 선언 (standard library)
    if (parser_match(parser, TOKEN_USE)) {
        Token mod_name = parser_consume(parser, TOKEN_IDENTIFIER,
            "Expected module name after 'use'");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after use");
        ASTNode *use_node = calloc(1, sizeof(ASTNode));
        use_node->type = AST_USE_DECL;
        use_node->line = mod_name.line;
        use_node->column = mod_name.column;
        use_node->data.use_decl.module_name = pergyra_strndup(mod_name.text, mod_name.length);
        return parser_finalize_statement(parser, use_node);
    }

    // import 선언
    if (parser_match(parser, TOKEN_IMPORT)) {
        Token path = parser_consume(parser, TOKEN_STRING,
            "Expected string path after 'import'");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after import");
        /* Strip quotes from string literal */
        char *raw = pergyra_strndup(path.text + 1, path.length - 2);
        ASTNode *imp = ast_create_import_declaration(raw);
        free(raw);
        return parser_finalize_statement(parser, imp);
    }

    // namespace 선언
    if (parser_match(parser, TOKEN_NAMESPACE)) {
        Token name_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected namespace name");
        ASTNode *ns = ast_create_namespace_declaration(name_tok.text);
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after namespace name");
        while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
            ASTNode *stmt = parser_parse_statement(parser);
            if (stmt != NULL)
                ast_add_statement(ns, stmt);
            if (parser->has_error)
                parser_synchronize(parser);
        }
        parser_consume(parser, TOKEN_RBRACE, "Expected '}' after namespace body");
        return parser_finalize_statement(parser, ns);
    }

    // extern 블록
    if (parser_match(parser, TOKEN_EXTERN)) {
        return parser_finalize_statement(parser, parse_extern_block(parser));
    }

    if (parser_match(parser, PGY_TOKEN_TYPE)) {
        return parser_finalize_statement(parser, parse_type_alias_declaration(parser));
    }

    // 클래스 / subject 선언
    if (parser_check(parser, TOKEN_CLASS)) {
        bool is_subject = parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "subject") == 0;
        parser_advance(parser);
        return parser_finalize_statement(parser,
            is_subject ? parse_subject_declaration(parser)
                       : parse_class_declaration(parser));
    }

    // object declaration: declaration-context keyword for local/internal projection
    if (parser_starts_contextual_declaration(parser, "object")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_object_declaration(parser));
    }

    if (parser_starts_contextual_declaration(parser, "vessel")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_vessel_declaration(parser));
    }

    if (parser_starts_contextual_declaration(parser, "intent")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_intent_declaration(parser));
    }

    // tobject / struct declarations share a token family today, but not a language contract
    if (parser_check(parser, TOKEN_STRUCT)) {
        bool is_tobject = parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "tobject") == 0;
        parser_advance(parser);
        return parser_finalize_statement(parser,
            is_tobject ? parse_tobject_declaration(parser)
                   : parse_struct_declaration(parser));
    }

    // let 선언
    if (parser_match(parser, TOKEN_LET)) {
        return parser_finalize_statement(parser, parser_parse_let_declaration(parser));
    }

    // := 단축 선언: name := expr
    // Consume identifier speculatively, check for :=, rewind if not
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        Token saved = parser->current_token;
        Token saved_prev = parser->previous_token;
        const char *lx_saved = parser->lexer->current;
        size_t lx_pos = parser->lexer->position;
        uint32_t lx_line = parser->lexer->line;
        uint32_t lx_col = parser->lexer->column;

        parser_advance(parser);  // consume identifier
        if (parser_check(parser, TOKEN_COLON_ASSIGN)) {
            parser_advance(parser);  // consume :=
            ASTNode *init = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after := declaration");
            ASTNode *let_node = ast_create_let_declaration(saved.text);
            let_node->data.let_decl.initializer = init;
            return let_node;
        }
        // Rewind — not a := declaration
        parser->current_token = saved;
        parser->previous_token = saved_prev;
        parser->lexer->current = lx_saved;
        parser->lexer->position = lx_pos;
        parser->lexer->line = lx_line;
        parser->lexer->column = lx_col;
    }

    /* labeled loop: label: while ... / label: for ... */
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        Token saved = parser->current_token;
        Token saved_prev = parser->previous_token;
        const char *lx_saved = parser->lexer->current;
        size_t lx_pos = parser->lexer->position;
        uint32_t lx_line = parser->lexer->line;
        uint32_t lx_col = parser->lexer->column;
        Token next = parser_peek_next(parser);

        if (next.type == TOKEN_COLON) {
            parser_advance(parser);
            parser_consume(parser, TOKEN_COLON, "Expected ':' after loop label");
            if (parser_match(parser, TOKEN_FOR)) {
                ASTNode *loop = parse_for_loop(parser);
                loop->data.for_loop.label = pergyra_strdup(saved.text);
                return parser_finalize_statement(parser, loop);
            }
            if (parser_match(parser, TOKEN_WHILE)) {
                ASTNode *loop = parse_while_statement(parser);
                loop->data.while_loop.label = pergyra_strdup(saved.text);
                return parser_finalize_statement(parser, loop);
            }

            parser->current_token = saved;
            parser->previous_token = saved_prev;
            parser->lexer->current = lx_saved;
            parser->lexer->position = lx_pos;
            parser->lexer->line = lx_line;
            parser->lexer->column = lx_col;
        }
    }

    // with 문
    if (parser_match(parser, TOKEN_WITH)) {
        return parser_finalize_statement(parser, parser_parse_with_statement(parser));
    }

    // parallel 블록
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_finalize_statement(parser, parser_parse_parallel_block(parser));
    }

    // for 루프
    if (parser_match(parser, TOKEN_FOR)) {
        return parser_finalize_statement(parser, parse_for_loop(parser));
    }

    // while 루프
    if (parser_match(parser, TOKEN_WHILE)) {
        return parser_finalize_statement(parser, parse_while_statement(parser));
    }

    // match 문
    if (parser_match(parser, TOKEN_MATCH)) {
        return parser_finalize_statement(parser, parse_match_statement(parser));
    }

    // if 문
    if (parser_match(parser, TOKEN_IF)) {
        return parser_finalize_statement(parser, parse_if_statement(parser));
    }

    // return 문
    if (parser_match(parser, TOKEN_RETURN)) {
        return parser_finalize_statement(parser, parse_return_statement(parser));
    }

    // break
    if (parser_match(parser, TOKEN_BREAK)) {
        char *label = NULL;
        if (parser_check(parser, TOKEN_IDENTIFIER)) {
            Token label_tok = parser_advance(parser);
            label = pergyra_strndup(label_tok.text, label_tok.length);
        }
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after break");
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_BREAK;
        node->line = parser->previous_token.line;
        node->data.break_stmt.label = label;
        return parser_finalize_statement(parser, node);
    }

    // continue
    if (parser_match(parser, TOKEN_CONTINUE)) {
        char *label = NULL;
        if (parser_check(parser, TOKEN_IDENTIFIER)) {
            Token label_tok = parser_advance(parser);
            label = pergyra_strndup(label_tok.text, label_tok.length);
        }
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after continue");
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_CONTINUE;
        node->line = parser->previous_token.line;
        node->data.continue_stmt.label = label;
        return parser_finalize_statement(parser, node);
    }

    // enum 선언
    if (parser_match(parser, TOKEN_ENUM)) {
        Token name_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected enum name");
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after enum name");

        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_ENUM_DECL;
        node->line = name_tok.line;
        node->data.enum_decl.name = pergyra_strndup(name_tok.text, name_tok.length);
        node->data.enum_decl.variants = NULL;
        node->data.enum_decl.variant_params = NULL;
        node->data.enum_decl.variant_param_counts = NULL;
        node->data.enum_decl.variant_count = 0;
        node->data.enum_decl.methods = NULL;
        node->data.enum_decl.method_count = 0;
        size_t cap = 0;

        while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
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
            Token var_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variant name");
            size_t idx = node->data.enum_decl.variant_count;
            if (idx >= cap) {
                cap = cap == 0 ? 4 : cap * 2;
                node->data.enum_decl.variants = realloc(
                    node->data.enum_decl.variants, cap * sizeof(char*));
                node->data.enum_decl.variant_params = realloc(
                    node->data.enum_decl.variant_params, cap * sizeof(ASTNode**));
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
                            pcap * sizeof(ASTNode*));
                    }
                    node->data.enum_decl.variant_params[idx][pc] = ptype;
                    node->data.enum_decl.variant_param_counts[idx]++;
                    if (!parser_match(parser, TOKEN_COMMA)) break;
                }
                parser_consume(parser, TOKEN_RPAREN,
                    "Expected ')' after variant parameters");
            }

            node->data.enum_decl.variant_count++;
            if (!parser_match(parser, TOKEN_COMMA)) break;
        }
        parser_consume(parser, TOKEN_RBRACE, "Expected '}' after enum variants");
        return parser_finalize_statement(parser, node);
    }

    // unsafe 블록
    if (parser_match(parser, TOKEN_UNSAFE)) {
        return parser_finalize_statement(parser, parse_unsafe_block(parser));
    }

    // defer 문
    if (parser_match(parser, TOKEN_DEFER)) {
        return parser_finalize_statement(parser, parse_defer_statement(parser));
    }

    // bind party.slot = Role;
    if (parser_match(parser, TOKEN_BIND)) {
        Token party_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected party variable after 'bind'");
        parser_consume(parser, TOKEN_DOT, "Expected '.' after party variable");
        Token slot_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected slot name after '.'");
        parser_consume(parser, TOKEN_ASSIGN, "Expected '=' after slot name");
        Token role_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected role name after '='");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after bind statement");
        return parser_finalize_statement(parser,
            ast_create_bind_statement(party_tok.text, slot_tok.text, role_tok.text));
    }

    // roster declaration (roster kept as legacy alias)
    if (parser_starts_contextual_declaration(parser, "roster")
        || parser_starts_contextual_declaration(parser, "roster")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_roster_declaration(parser));
    }

    // world 선언
    if (parser_starts_contextual_declaration(parser, "world")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_world_declaration(parser));
    }

    // relation 선언
    if (parser_starts_contextual_declaration(parser, "relation")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_relation_declaration(parser));
    }

    // effect 선언
    if (parser_starts_contextual_declaration(parser, "effect")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_effect_declaration(parser));
    }

    // zone 선언
    if (parser_starts_contextual_declaration(parser, "zone")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_zone_declaration(parser));
    }

    // party 선언
    if (parser_match(parser, TOKEN_PARTY)) {
        return parser_finalize_statement(parser, parse_party_declaration(parser));
    }

    if (parser_match(parser, TOKEN_INNATE)) {
        parser_consume(parser, TOKEN_ABILITY,
            "Expected 'ability' after 'innate'");
        return parser_finalize_statement(parser,
            parse_ability_declaration(parser, true));
    }

    // ability 선언
    if (parser_match(parser, TOKEN_ABILITY)) {
        return parser_finalize_statement(parser,
            parse_ability_declaration(parser, false));
    }

    // role 선언
    if (parser_match(parser, TOKEN_ROLE)) {
        return parser_finalize_statement(parser, parse_role_declaration(parser));
    }

    // event 선언 (contextual keyword)
    if (parser_starts_contextual_declaration(parser, "event")) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_event_declaration(parser));
    }

    // 표현식 문장
    return parser_finalize_statement(parser, parser_parse_expression_statement(parser));
}

// let 선언 파싱
ASTNode* parser_parse_let_declaration(Parser* parser) {
    /* Destructuring: let (a, b, c) = expr; */
    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_advance(parser);  /* consume '(' */
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_LET_DESTRUCTURE;
        node->line = parser->previous_token.line;
        node->column = parser->previous_token.column;
        node->data.let_destructure.names = NULL;
        node->data.let_destructure.name_count = 0;
        node->data.let_destructure.initializer = NULL;

        while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
            Token var = consume_name_token(parser, "Expected variable name in destructuring");
            size_t n = ++node->data.let_destructure.name_count;
            node->data.let_destructure.names = realloc(
                node->data.let_destructure.names, n * sizeof(char *));
            node->data.let_destructure.names[n - 1] = pergyra_strdup(var.text);
            if (!parser_match(parser, TOKEN_COMMA))
                break;
        }
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after destructuring names");
        parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let destructuring");
        node->data.let_destructure.initializer = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after let destructuring");
        return node;
    }

    // 변수 이름
    Token name = consume_name_token(parser, "Expected variable name");

    ASTNode* let_decl = ast_create_let_declaration(name.text);

    // 타입 어노테이션 (선택적)
    if (parser_match(parser, TOKEN_COLON)) {
        let_decl->data.let_decl.type = parse_type(parser);
    }

    // 초기화 표현식
    parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let declaration");
    let_decl->data.let_decl.initializer = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after let declaration");

    return let_decl;
}

// with 문 파싱
ASTNode* parser_parse_with_statement(Parser* parser) {
    ASTNode* with_stmt = ast_create_with_statement();

    // 슬롯 타입
    if (parser_match(parser, TOKEN_SLOT)) {
        with_stmt->data.with_stmt.is_secure = false;
    } else {
        Token slot_kind = parser_consume(
            parser,
            TOKEN_IDENTIFIER,
            "Expected 'slot' or 'SecureSlot' after 'with'"
        );

        if (strcmp(slot_kind.text, "SecureSlot") == 0) {
            with_stmt->data.with_stmt.is_secure = true;
        } else if (strcmp(slot_kind.text, "slot") != 0) {
            parser_error(parser, "Expected 'slot' or 'SecureSlot' after 'with'");
        }
    }

    // 제네릭 타입
    parser_consume(parser, TOKEN_LESS, "Expected '<' after slot type");
    with_stmt->data.with_stmt.slot_type = parse_type(parser);
    parser_consume(parser, TOKEN_GREATER, "Expected '>' after slot type");

    // 보안 레벨 (선택적)
    if (parser_match(parser, TOKEN_LPAREN)) {
        Token level = parser_consume(parser, TOKEN_IDENTIFIER, "Expected security level");
        with_stmt->data.with_stmt.security_level = pergyra_strdup(level.text);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after security level");
    }

    // as 변수명
    parser_consume(parser, TOKEN_AS, "Expected 'as' in with statement");
    Token alias = consume_name_token(parser, "Expected variable name after 'as'");
    with_stmt->data.with_stmt.alias = pergyra_strdup(alias.text);

    // 블록
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after with statement");
    parser->in_with_statement = true;
    with_stmt->data.with_stmt.body = parser_parse_block(parser);
    parser->in_with_statement = false;

    return with_stmt;
}

// parallel 블록 파싱
ASTNode* parser_parse_parallel_block(Parser* parser) {
    ASTNode* parallel = ast_create_parallel_block();

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'parallel'");

    parser->in_parallel_block = true;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_parallel_task(parallel, stmt);
        }
        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }
    parser->in_parallel_block = false;

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after parallel block");

    return parallel;
}

// 블록 파싱
ASTNode* parser_parse_block(Parser* parser) {
    ASTNode* block = ast_create_block();

    parser->scope_depth++;

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_statement(block, stmt);
        }
        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }

    parser->scope_depth--;

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after block");

    return block;
}

// 표현식 문장
ASTNode* parser_parse_expression_statement(Parser* parser) {
    ASTNode* expr = parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression");
    return expr;
}
