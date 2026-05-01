#include "parser_domain_internal.h"

/*
 * roster CombatSystem {
 *     party slot team1: DungeonTeam
 *     party slot team2: DungeonTeam
 *     shared rules: CombatRules
 *     func ScheduleMatches() -> Void { ... }
 * }
 */
ASTNode* parse_roster_declaration(Parser* parser) {
    Token name = consume_name_token(parser, "Expected roster name");
    ASTNode* sys = ast_create_roster_declaration(name.text);
    sys->data.roster_decl.doc_comment = parser_take_pending_doc_comment(parser);
    sys->line = name.line;
    sys->column = name.column;

    sys->data.roster_decl.generic_params = parse_generic_params(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after roster name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        if (parser_match(parser, TOKEN_PARTY)) {
            parser_consume(parser, TOKEN_SLOT,
                "Expected 'slot' after 'party' in roster");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after party slot name");
            Token party_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected party type");

            ASTNode* ps = ast_create_roster_slot(slot_name.text, party_type.text);
            ps->line = slot_name.line;
            ps->column = slot_name.column;

            append_child_node(&sys->data.roster_decl.party_slots,
                &sys->data.roster_decl.party_count,
                &sys->data.roster_decl.party_capacity, ps);

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

            append_child_node(&sys->data.roster_decl.shared_fields,
                &sys->data.roster_decl.shared_count,
                &sys->data.roster_decl.shared_capacity, shared);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            append_child_node(&sys->data.roster_decl.methods,
                &sys->data.roster_decl.method_count,
                &sys->data.roster_decl.method_capacity, method);

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'party slot', 'shared', or 'func' in roster body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after roster body");
    return sys;
}
