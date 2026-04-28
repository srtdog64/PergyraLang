#include "parser_domain_internal.h"

static void
parse_header_subject_targets(Parser *parser, ASTNode ***slots, size_t *slot_count,
                             const char *owner_kind)
{
    if (!parser_match(parser, TOKEN_FOR))
        return;

    do {
        bool is_subject = true;
        if (parser_match(parser, TOKEN_OBJECT)) {
            is_subject = false;
        } else if (parser_match(parser, TOKEN_SUBJECT)) {
            is_subject = true;
        }
        Token slot_name = consume_name_token(parser,
            "Expected relation/effect target name after 'for'");
        parser_consume(parser, TOKEN_COLON,
            "Expected ':' after relation/effect target name");
        ASTNode *slot_type = parse_type(parser);

        ASTNode *slot = ast_create_domain_slot(slot_name.text, is_subject);
        slot->data.domain_slot.type = slot_type;
        slot->data.domain_slot.is_binding = true;
        slot->line = slot_name.line;
        slot->column = slot_name.column;
        append_domain_slot(slots, slot_count, slot);
    } while (parser_match(parser, TOKEN_COMMA));

    (void)owner_kind;
}

static RelationEndpointKind
relation_endpoint_kind_from_token(Token token)
{
    switch (token.type) {
    case TOKEN_SUBJECT:
        return RELATION_ENDPOINT_SUBJECT;
    case TOKEN_OBJECT:
        return RELATION_ENDPOINT_OBJECT;
    case TOKEN_CLASS:
        return RELATION_ENDPOINT_CLASS;
    case TOKEN_TOBJECT:
        return RELATION_ENDPOINT_TOBJECT;
    default:
        return RELATION_ENDPOINT_NAMED;
    }
}

static ASTNode *
parse_relation_endpoint_type(Parser *parser,
                             RelationEndpointKind *kind_out,
                             bool *many_out,
                             const char *message)
{
    ASTNode *type_ref = NULL;
    RelationEndpointKind kind;

    if (kind_out != NULL)
        *kind_out = RELATION_ENDPOINT_NAMED;
    if (many_out != NULL)
        *many_out = false;

    if (!parser_check(parser, TOKEN_IDENTIFIER)
        && !parser_check(parser, TOKEN_SUBJECT)
        && !parser_check(parser, TOKEN_OBJECT)
        && !parser_check(parser, TOKEN_CLASS)
        && !parser_check(parser, TOKEN_TOBJECT)) {
        parser_error(parser, message);
        return NULL;
    }

    kind = relation_endpoint_kind_from_token(parser->current_token);
    if (kind != RELATION_ENDPOINT_NAMED) {
        parser_advance(parser);
    } else {
        type_ref = parse_type(parser);
    }

    if (parser_match(parser, TOKEN_LBRACKET)) {
        parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after '['");
        if (many_out != NULL)
            *many_out = true;
    }

    if (kind_out != NULL)
        *kind_out = kind;
    return type_ref;
}

ASTNode* parse_relation_declaration(Parser* parser) {
    Token name = consume_name_token(parser, "Expected relation name");
    ASTNode* relation = ast_create_relation_declaration(name.text);
    relation->data.relation_decl.doc_comment = parser_take_pending_doc_comment(parser);
    relation->line = name.line;
    relation->column = name.column;

    relation->data.relation_decl.between_left_type = NULL;
    relation->data.relation_decl.between_right_type = NULL;
    relation->data.relation_decl.between_left_many = false;
    relation->data.relation_decl.between_right_many = false;

    if (parser_match_identifier_keyword(parser, "between")) {
        relation->data.relation_decl.between_left_type =
            parse_relation_endpoint_type(parser,
                &relation->data.relation_decl.between_left_kind,
                &relation->data.relation_decl.between_left_many,
                "Expected type keyword after 'between' (subject, object, class, tobject, or type name)");

        parser_consume(parser, TOKEN_COMMA,
            "Expected ',' between left and right types in 'between' clause");

        relation->data.relation_decl.between_right_type =
            parse_relation_endpoint_type(parser,
                &relation->data.relation_decl.between_right_kind,
                &relation->data.relation_decl.between_right_many,
                "Expected type keyword after ',' (subject, object, class, tobject, or type name)");
    }

    parse_header_subject_targets(parser,
        &relation->data.relation_decl.slots,
        &relation->data.relation_decl.slot_count,
        "relation");
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after relation name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        if (parser_check(parser, TOKEN_FOR)) {
            parse_header_subject_targets(parser,
                &relation->data.relation_decl.slots,
                &relation->data.relation_decl.slot_count,
                "relation");
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else {
            ASTNode *slot = parse_domain_slot_entry(parser, "relation");
            if (slot != NULL) {
                append_domain_slot(&relation->data.relation_decl.slots,
                    &relation->data.relation_decl.slot_count, slot);
                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
            } else if (parser_match(parser, TOKEN_SHARED)) {
                Token field_name = consume_name_token(parser,
                    "Expected field name after 'shared'");
                parser_consume(parser, TOKEN_COLON,
                    "Expected ':' after shared field name");
                ASTNode* field_type = parse_type(parser);

                ASTNode* shared = ast_create_party_shared(field_name.text);
                shared->data.party_shared.type = field_type;
                shared->line = field_name.line;
                shared->column = field_name.column;

                if (parser_match(parser, TOKEN_ASSIGN)) {
                    shared->data.party_shared.initializer =
                        parser_parse_expression(parser);
                }

                relation->data.relation_decl.shared_count++;
                relation->data.relation_decl.shared_fields = realloc(
                    relation->data.relation_decl.shared_fields,
                    relation->data.relation_decl.shared_count * sizeof(ASTNode*));
                relation->data.relation_decl.shared_fields[
                    relation->data.relation_decl.shared_count - 1] = shared;

                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
            } else if (parser_match_identifier_keyword(parser, "refresh")
                       || parser_match_identifier_keyword(parser, "publish")
                       || parser_match(parser, TOKEN_BIND)) {
                append_domain_projection_sync_entries(parser,
                    &relation->data.relation_decl.refreshes,
                    &relation->data.relation_decl.refresh_count, false);
                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
            } else if (parser_match(parser, TOKEN_FUNC)) {
                ASTNode* method = parser_finalize_statement(parser,
                    parse_function_declaration(parser));
                relation->data.relation_decl.method_count++;
                relation->data.relation_decl.methods = realloc(
                    relation->data.relation_decl.methods,
                    relation->data.relation_decl.method_count * sizeof(ASTNode*));
                relation->data.relation_decl.methods[
                    relation->data.relation_decl.method_count - 1] = method;
            } else {
                parser_discard_pending_doc_comment(parser);
                parser_error(parser,
                    "Expected 'subject slot', 'object slot', 'tobject slot', 'refresh', 'publish', 'bind', 'shared', or 'func' in relation body");
                parser_advance(parser);
            }
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after relation body");
    return relation;
}

ASTNode* parse_effect_declaration(Parser* parser) {
    Token name = consume_name_token(parser, "Expected effect name");
    ASTNode* effect = ast_create_effect_declaration(name.text);
    effect->data.effect_decl.doc_comment = parser_take_pending_doc_comment(parser);
    effect->line = name.line;
    effect->column = name.column;
    parse_header_subject_targets(parser,
        &effect->data.effect_decl.slots,
        &effect->data.effect_decl.slot_count,
        "effect");
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after effect name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        if (parser_check(parser, TOKEN_FOR)) {
            parse_header_subject_targets(parser,
                &effect->data.effect_decl.slots,
                &effect->data.effect_decl.slot_count,
                "effect");
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else {
            ASTNode *slot = parse_domain_slot_entry(parser, "effect");
            if (slot != NULL) {
                append_domain_slot(&effect->data.effect_decl.slots,
                    &effect->data.effect_decl.slot_count, slot);
                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
            } else if (parser_match(parser, TOKEN_SHARED)) {
                Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected field name after 'shared'");
                parser_consume(parser, TOKEN_COLON,
                    "Expected ':' after shared field name");
                ASTNode* field_type = parse_type(parser);

                ASTNode* shared = ast_create_party_shared(field_name.text);
                shared->data.party_shared.type = field_type;
                shared->line = field_name.line;
                shared->column = field_name.column;

                if (parser_match(parser, TOKEN_ASSIGN)) {
                    shared->data.party_shared.initializer =
                        parser_parse_expression(parser);
                }

                effect->data.effect_decl.shared_count++;
                effect->data.effect_decl.shared_fields = realloc(
                    effect->data.effect_decl.shared_fields,
                    effect->data.effect_decl.shared_count * sizeof(ASTNode*));
                effect->data.effect_decl.shared_fields[
                    effect->data.effect_decl.shared_count - 1] = shared;

                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
            } else if (parser_match_identifier_keyword(parser, "refresh")
                       || parser_match_identifier_keyword(parser, "publish")
                       || parser_match(parser, TOKEN_BIND)) {
                append_domain_projection_sync_entries(parser,
                    &effect->data.effect_decl.refreshes,
                    &effect->data.effect_decl.refresh_count, false);
                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
            } else if (parser_match(parser, TOKEN_FUNC)) {
                ASTNode* method = parser_finalize_statement(parser,
                    parse_function_declaration(parser));
                effect->data.effect_decl.method_count++;
                effect->data.effect_decl.methods = realloc(
                    effect->data.effect_decl.methods,
                    effect->data.effect_decl.method_count * sizeof(ASTNode*));
                effect->data.effect_decl.methods[
                    effect->data.effect_decl.method_count - 1] = method;
            } else {
                parser_discard_pending_doc_comment(parser);
                parser_error(parser,
                    "Expected 'subject slot', 'object slot', 'tobject slot', 'refresh', 'publish', 'bind', 'shared', or 'func' in effect body");
                parser_advance(parser);
            }
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after effect body");
    return effect;
}
