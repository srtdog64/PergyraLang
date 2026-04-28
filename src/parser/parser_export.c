#include "parser_internal.h"

bool
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

ASTNode *
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
        char *raw;

        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after import");
        raw = pergyra_strndup(path.text + 1, path.length - 2);
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
    else if (parser_match(parser, TOKEN_SUBJECT))
        node = parse_subject_declaration(parser);
    else if (parser_match(parser, TOKEN_CLASS))
        node = parse_class_declaration(parser);
    else if (parser_match(parser, TOKEN_OBJECT))
        node = parse_object_declaration(parser);
    else if (parser_match(parser, TOKEN_VESSEL))
        node = parse_vessel_declaration(parser);
    else if (parser_match(parser, TOKEN_INTENT))
        node = parse_intent_declaration(parser);
    else if (parser_match(parser, TOKEN_TOBJECT))
        node = parse_tobject_declaration(parser);
    else if (parser_match(parser, TOKEN_STRUCT))
        node = parse_struct_declaration(parser);
    else if (parser_match(parser, TOKEN_LET))
        node = parser_parse_let_declaration(parser);
    else if (parser_match(parser, TOKEN_ENUM))
        node = parser_parse_enum_declaration_after_keyword(parser);
    else if (parser_match(parser, TOKEN_ROSTER))
        node = parse_roster_declaration(parser);
    else if (parser_match(parser, TOKEN_WORLD))
        node = parse_world_declaration(parser);
    else if (parser_match(parser, TOKEN_RELATION))
        node = parse_relation_declaration(parser);
    else if (parser_match(parser, TOKEN_EFFECT))
        node = parse_effect_declaration(parser);
    else if (parser_match(parser, TOKEN_ZONE))
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
    else if (parser_match(parser, TOKEN_EVENT))
        node = parse_event_declaration(parser);

    if (node == NULL) {
        parser_error(parser, "'export' can only apply to declarations");
        return NULL;
    }

    node->is_exported = true;
    node->has_explicit_export = true;
    return parser_finalize_statement(parser, node);
}
