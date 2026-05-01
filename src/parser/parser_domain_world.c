#include "parser_domain_internal.h"

static bool
parser_world_append_input_name(Parser *parser, const char ***items,
                               size_t *count, size_t *capacity,
                               const char *name)
{
    const char **grown;
    size_t next_capacity;

    if (parser == NULL || items == NULL || count == NULL || capacity == NULL)
        return false;

    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity <= *count
            || next_capacity > (size_t)-1 / sizeof(char *)) {
            parser_error(parser, "Out of memory while parsing composed world state");
            return false;
        }
        grown = realloc((void *)*items, next_capacity * sizeof(char *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while parsing composed world state");
            return false;
        }
        *items = grown;
        *capacity = next_capacity;
    }

    (*items)[*count] = name;
    *count += 1;
    return true;
}

/*
 * world GameWorld {
 *     roster combat: CombatSystem
 *     roster economy: EconomySystem
 *     shared tick: Int = 0
 *     func Update() -> Void { ... }
 * }
 */
ASTNode* parse_world_declaration(Parser* parser) {
    Token name = consume_name_token(parser, "Expected world name");
    ASTNode* world = ast_create_world_declaration(name.text);
    world->data.world_decl.doc_comment = parser_take_pending_doc_comment(parser);
    world->line = name.line;
    world->column = name.column;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after world name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        if (parser_match(parser, TOKEN_ROSTER)) {
            Token slot_name = consume_name_token(parser,
                "Expected roster name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after roster name");
            Token sys_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected roster type");

            ASTNode* ws = ast_create_world_roster(
                slot_name.text, sys_type.text);
            ws->line = slot_name.line;
            ws->column = slot_name.column;

            append_child_node(&world->data.world_decl.rosters,
                &world->data.world_decl.roster_count,
                &world->data.world_decl.roster_capacity, ws);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_ZONE)) {
            Token slot_name = consume_name_token(parser,
                "Expected zone name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after zone name");
            Token zone_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone type");

            ASTNode* wz = ast_create_world_zone(slot_name.text, zone_type.text);
            wz->line = slot_name.line;
            wz->column = slot_name.column;

            append_child_node(&world->data.world_decl.zones,
                &world->data.world_decl.zone_count,
                &world->data.world_decl.zone_capacity, wz);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "activate")) {
            Token zone_or_state = consume_name_token(parser,
                "Expected zone slot name or world state name after 'activate'");
            ASTNode *activate = ast_create_world_activate(zone_or_state.text);
            activate->data.world_activate.state_name = pergyra_strdup(zone_or_state.text);
            free(activate->data.world_activate.zone_slot_name);
            activate->data.world_activate.zone_slot_name = NULL;
            activate->line = zone_or_state.line;
            activate->column = zone_or_state.column;

            append_child_node(&world->data.world_decl.activations,
                &world->data.world_decl.activate_count,
                &world->data.world_decl.activate_capacity, activate);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "deactivate")) {
            Token zone_or_state = consume_name_token(parser,
                "Expected zone slot name or world state name after 'deactivate'");
            ASTNode *deactivate = ast_create_world_deactivate(zone_or_state.text);
            deactivate->data.world_deactivate.state_name = pergyra_strdup(zone_or_state.text);
            free(deactivate->data.world_deactivate.zone_slot_name);
            deactivate->data.world_deactivate.zone_slot_name = NULL;
            deactivate->line = zone_or_state.line;
            deactivate->column = zone_or_state.column;

            append_child_node(&world->data.world_decl.deactivations,
                &world->data.world_decl.deactivate_count,
                &world->data.world_decl.deactivate_capacity, deactivate);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "maintain")) {
            Token zone_or_state = consume_name_token(parser,
                "Expected zone slot name or world state name after 'maintain'");
            ASTNode *maintain = ast_create_world_maintain(zone_or_state.text);
            maintain->data.world_maintain.state_name = pergyra_strdup(zone_or_state.text);
            free(maintain->data.world_maintain.zone_slot_name);
            maintain->data.world_maintain.zone_slot_name = NULL;
            maintain->line = zone_or_state.line;
            maintain->column = zone_or_state.column;

            append_child_node(&world->data.world_decl.maintained_zones,
                &world->data.world_decl.maintained_zone_count,
                &world->data.world_decl.maintained_zone_capacity, maintain);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "state")) {
            Token state_name = consume_name_token(parser,
                "Expected world state name after 'state'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after world state name");
            if (parser_match_identifier_keyword(parser, "all")
                || parser_match_identifier_keyword(parser, "any")) {
                WorldStateSourceKind source_kind = WORLD_STATE_SOURCE_ANY;
                const char **input_names = NULL;
                size_t input_count = 0;
                size_t input_capacity = 0;
                if (parser->previous_token.text != NULL
                    && strcmp(parser->previous_token.text, "all") == 0) {
                    source_kind = WORLD_STATE_SOURCE_ALL;
                }

                do {
                    Token input = consume_name_token(parser,
                        "Expected world zone/state name in composed world state");
                    if (!parser_world_append_input_name(parser, &input_names,
                            &input_count, &input_capacity, input.text)) {
                        break;
                    }
                } while (parser_match(parser, TOKEN_COMMA));

                ASTNode *state = ast_create_world_state_compose(
                    state_name.text, source_kind, input_names, input_count);
                free((void*)input_names);
                state->line = state_name.line;
                state->column = state_name.column;

                append_child_node(&world->data.world_decl.states,
                    &world->data.world_decl.state_count,
                    &world->data.world_decl.state_capacity, state);

                parser_match(parser, TOKEN_SEMICOLON);
                parser_discard_pending_doc_comment(parser);
                continue;
            }
            if (!parser_match(parser, TOKEN_ZONE)) {
                parser_error(parser, "Expected 'zone' after ':' in world state");
                parser_advance(parser);
                continue;
            }
            Token zone_slot = consume_name_token(parser,
                "Expected zone slot name after 'zone'");
            WorldStateSourceKind source_kind = WORLD_STATE_SOURCE_ZONE;
            const char *detail_name = NULL;
            Token detail;

            if (parser_match_identifier_keyword_on_line(parser, "projection",
                    zone_slot.line)) {
                detail = consume_name_token(parser,
                    "Expected projection slot name after 'projection'");
                source_kind = WORLD_STATE_SOURCE_PROJECTION;
                detail_name = detail.text;
            } else if (parser_match_identifier_keyword_on_line(parser, "layer",
                           zone_slot.line)) {
                detail = consume_name_token(parser,
                    "Expected layer slot name after 'layer'");
                source_kind = WORLD_STATE_SOURCE_LAYER;
                detail_name = detail.text;
            } else if (parser_match_identifier_keyword_on_line(parser, "state",
                           zone_slot.line)) {
                detail = consume_name_token(parser,
                    "Expected zone state name after 'state'");
                source_kind = WORLD_STATE_SOURCE_STATE;
                detail_name = detail.text;
            }

            ASTNode *state = ast_create_world_state(state_name.text, zone_slot.text,
                source_kind, detail_name);
            state->line = state_name.line;
            state->column = state_name.column;

            append_child_node(&world->data.world_decl.states,
                &world->data.world_decl.state_count,
                &world->data.world_decl.state_capacity, state);

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

            append_child_node(&world->data.world_decl.shared_fields,
                &world->data.world_decl.shared_count,
                &world->data.world_decl.shared_capacity, shared);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            append_child_node(&world->data.world_decl.methods,
                &world->data.world_decl.method_count,
                &world->data.world_decl.method_capacity, method);

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'roster', 'zone', 'activate', 'deactivate', 'maintain', 'state', 'shared', or 'func' in world body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after world body");
    return world;
}
