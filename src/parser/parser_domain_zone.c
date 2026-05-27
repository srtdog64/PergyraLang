#include "parser_domain_internal.h"
#include "../common/numeric_parse.h"

static bool
parser_parse_positive_int_token(Parser *parser, Token token, int *out)
{
    if (parser == NULL || token.text == NULL || out == NULL)
        return false;
    if (!pgy_parse_positive_int_strict(token.text, out)) {
        parser_error(parser, "Expected positive pool capacity within Int range");
        return false;
    }
    return true;
}

static bool
parser_zone_append_owned_name(Parser *parser, char ***items, size_t *count,
                              size_t *capacity, const char *name)
{
    char **grown;
    char *owned_name;
    size_t next_capacity;

    if (parser == NULL || items == NULL || count == NULL || capacity == NULL)
        return false;

    if (*count >= *capacity) {
        next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        if (next_capacity <= *count
            || next_capacity > (size_t)-1 / sizeof(char *)) {
            parser_error(parser, "Out of memory while growing zone group");
            return false;
        }
        grown = realloc(*items, next_capacity * sizeof(char *));
        if (grown == NULL) {
            parser_error(parser, "Out of memory while growing zone group");
            return false;
        }
        *items = grown;
        *capacity = next_capacity;
    }

    owned_name = pergyra_strdup(name);
    if (owned_name == NULL) {
        parser_error(parser, "Out of memory while parsing zone group name");
        return false;
    }
    (*items)[*count] = owned_name;
    *count += 1;
    return true;
}

ASTNode* parse_zone_declaration(Parser* parser) {
    Token name = consume_name_token(parser, "Expected zone name");
    ASTNode* zone = ast_create_zone_declaration(name.text);
    zone->data.zone_decl.doc_comment = parser_take_pending_doc_comment(parser);
    zone->line = name.line;
    zone->column = name.column;
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after zone name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        DomainSlotGroupKind slot_group = parser_match_domain_slot_group_kind(parser);
        if (slot_group != DOMAIN_GROUP_NONE) {
            bool is_subject = (slot_group == DOMAIN_GROUP_SUBJECTS);
            bool is_tobject = (slot_group == DOMAIN_GROUP_TOBJECTS);
            parser_consume(parser, TOKEN_LBRACKET, "Expected '[' after group slot keyword");
            char **names = NULL;
            size_t name_count = 0;
            size_t name_capacity = 0;
            do {
                Token n = consume_name_token(parser, "Expected slot name in group");
                if (!parser_zone_append_owned_name(parser, &names, &name_count,
                        &name_capacity, n.text)) {
                    break;
                }
            } while (parser_match(parser, TOKEN_COMMA));
            parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after group slot names");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after group slot list");
            ASTNode *slot_type = parse_type(parser);
            for (size_t gi = 0; gi < name_count; gi++) {
                ASTNode *slot = ast_create_domain_slot(names[gi], is_subject);
                slot->data.domain_slot.is_tobject = is_tobject;
                slot->data.domain_slot.type = (gi == 0) ? slot_type : ast_clone(slot_type);
                slot->line = zone->line;
                slot->column = zone->column;
                append_domain_slot(&zone->data.zone_decl.slots,
                    &zone->data.zone_decl.slot_count,
                    &zone->data.zone_decl.slot_capacity, slot);
                free(names[gi]);
            }
            free(names);
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else {
            DomainLayerGroupKind layer_group = parser_match_domain_layer_group_kind(parser);
            if (layer_group != DOMAIN_LAYER_GROUP_NONE) {
            bool is_relation = (layer_group == DOMAIN_LAYER_GROUP_RELATIONS);
            parser_consume(parser, TOKEN_LBRACKET, "Expected '[' after group layer keyword");
            char **names = NULL;
            size_t name_count = 0;
            size_t name_capacity = 0;
            do {
                Token n = consume_name_token(parser, "Expected slot name in group");
                if (!parser_zone_append_owned_name(parser, &names, &name_count,
                        &name_capacity, n.text)) {
                    break;
                }
            } while (parser_match(parser, TOKEN_COMMA));
            parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after group layer names");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after group layer list");
            Token layer_type = parser_consume(parser, TOKEN_IDENTIFIER,
                is_relation ? "Expected relation type" : "Expected effect type");
            for (size_t gi = 0; gi < name_count; gi++) {
                ASTNode *layer_slot = ast_create_zone_layer_slot(
                    names[gi], layer_type.text, is_relation);
                layer_slot->line = zone->line;
                layer_slot->column = zone->column;
                append_child_node(&zone->data.zone_decl.layer_slots,
                    &zone->data.zone_decl.layer_slot_count,
                    &zone->data.zone_decl.layer_slot_capacity, layer_slot);
                free(names[gi]);
            }
            free(names);
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else {
        ASTNode *slot = parse_domain_slot_entry(parser, "zone");
        if (slot != NULL) {
            append_domain_slot(&zone->data.zone_decl.slots,
                &zone->data.zone_decl.slot_count,
                &zone->data.zone_decl.slot_capacity, slot);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_RELATION)
                   || parser_match(parser, TOKEN_EFFECT)) {
            bool is_relation = parser->previous_token.type == TOKEN_RELATION;
            bool is_pool = false;
            if (is_relation) {
                parser_consume(parser, TOKEN_SLOT,
                    "Expected 'slot' after 'relation' in zone");
            } else if (parser_match(parser, TOKEN_SLOT)) {
                is_pool = false;
            } else if (parser_match_identifier_keyword(parser, "pool")) {
                is_pool = true;
            } else {
                parser_error(parser,
                    "Expected 'slot' or 'pool' after 'effect' in zone");
            }
            Token slot_name = consume_name_token(parser,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after zone layer slot name");
            Token layer_type = parser_consume(parser, TOKEN_IDENTIFIER,
                is_relation ? "Expected relation type" : "Expected effect type");

            ASTNode *layer_slot = ast_create_zone_layer_slot(
                slot_name.text, layer_type.text, is_relation);
            if (is_pool) {
                if (!parser_match_identifier_keyword(parser, "capacity")) {
                    parser_error(parser,
                        "Expected 'capacity' after effect pool type");
                }
                Token capacity = parser_consume(parser, TOKEN_NUMBER,
                    "Expected numeric capacity after 'capacity'");
                int pool_capacity = 0;
                layer_slot->data.zone_layer_slot.is_pool = true;
                if (parser_parse_positive_int_token(parser, capacity,
                        &pool_capacity)) {
                    layer_slot->data.zone_layer_slot.pool_capacity =
                        pool_capacity;
                }
            }
            layer_slot->line = slot_name.line;
            layer_slot->column = slot_name.column;

            append_child_node(&zone->data.zone_decl.layer_slots,
                &zone->data.zone_decl.layer_slot_count,
                &zone->data.zone_decl.layer_slot_capacity, layer_slot);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "apply")) {
            Token effect_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected effect slot name or state name after 'apply'");
            ASTNode *apply = NULL;

            if (parser_match_identifier_keyword(parser, "to")) {
                Token target_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected target slot name after 'to'");
                apply = ast_create_zone_apply(
                    effect_slot.text, target_slot.text);
            } else {
                apply = ast_create_zone_apply(NULL, NULL);
                apply->data.zone_apply.state_name = pergyra_strdup(effect_slot.text);
            }

            apply->data.zone_apply.participant_slot_name =
                parse_optional_zone_participant_name(parser);
            apply->line = effect_slot.line;
            apply->column = effect_slot.column;

            append_child_node(&zone->data.zone_decl.applies,
                &zone->data.zone_decl.apply_count,
                &zone->data.zone_decl.apply_capacity, apply);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "link")) {
            Token relation_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected relation slot name or state name after 'link'");
            ASTNode *link = NULL;

            if (parser_match_identifier_keyword(parser, "between")) {
                Token left_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected left slot name after 'between'");
                parser_consume(parser, TOKEN_COMMA,
                    "Expected ',' between linked slot names");
                Token right_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected right slot name after ','");

                link = ast_create_zone_link(
                    relation_slot.text, left_slot.text, right_slot.text);
            } else {
                link = ast_create_zone_link(NULL, NULL, NULL);
                link->data.zone_link.state_name = pergyra_strdup(relation_slot.text);
            }

            link->data.zone_link.participant_slot_name =
                parse_optional_zone_participant_name(parser);
            link->line = relation_slot.line;
            link->column = relation_slot.column;

            append_child_node(&zone->data.zone_decl.links,
                &zone->data.zone_decl.link_count,
                &zone->data.zone_decl.link_capacity, link);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "detach")) {
            Token effect_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected effect slot name or state name after 'detach'");
            ASTNode *detach = NULL;

            if (parser_match_identifier_keyword(parser, "from")) {
                Token target_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected target slot name after 'from'");
                detach = ast_create_zone_detach(
                    effect_slot.text, target_slot.text);
            } else {
                detach = ast_create_zone_detach(NULL, NULL);
                detach->data.zone_detach.state_name = pergyra_strdup(effect_slot.text);
            }

            detach->data.zone_detach.participant_slot_name =
                parse_optional_zone_participant_name(parser);
            detach->line = effect_slot.line;
            detach->column = effect_slot.column;

            append_child_node(&zone->data.zone_decl.detaches,
                &zone->data.zone_decl.detach_count,
                &zone->data.zone_decl.detach_capacity, detach);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "unlink")) {
            Token relation_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected relation slot name or state name after 'unlink'");
            ASTNode *unlink = NULL;

            if (parser_match_identifier_keyword(parser, "between")) {
                Token left_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected left slot name after 'between'");
                parser_consume(parser, TOKEN_COMMA,
                    "Expected ',' between unlinked slot names");
                Token right_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected right slot name after ','");

                unlink = ast_create_zone_unlink(
                    relation_slot.text, left_slot.text, right_slot.text);
            } else {
                unlink = ast_create_zone_unlink(NULL, NULL, NULL);
                unlink->data.zone_unlink.state_name = pergyra_strdup(relation_slot.text);
            }

            unlink->data.zone_unlink.participant_slot_name =
                parse_optional_zone_participant_name(parser);
            unlink->line = relation_slot.line;
            unlink->column = relation_slot.column;

            append_child_node(&zone->data.zone_decl.unlinks,
                &zone->data.zone_decl.unlink_count,
                &zone->data.zone_decl.unlink_capacity, unlink);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "refresh")
                   || parser_match_identifier_keyword(parser, "publish")
                   || parser_match(parser, TOKEN_BIND)) {
            append_domain_projection_sync_entries(parser,
                &zone->data.zone_decl.refreshes,
                &zone->data.zone_decl.refresh_count,
                &zone->data.zone_decl.refresh_capacity, true);
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "maintain")) {
            Token layer_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected effect slot, relation slot, or state name after 'maintain'");

            if (parser_match_identifier_keyword(parser, "on")) {
                Token target_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected target slot name after 'on'");
                ASTNode *maintain = ast_create_zone_maintain_effect(
                    layer_slot.text, target_slot.text);
                maintain->data.zone_maintain_effect.participant_slot_name =
                    parse_optional_zone_participant_name(parser);
                maintain->line = layer_slot.line;
                maintain->column = layer_slot.column;

                append_child_node(&zone->data.zone_decl.maintained_effects,
                    &zone->data.zone_decl.maintained_effect_count,
                    &zone->data.zone_decl.maintained_effect_capacity, maintain);
            } else if (parser_match_identifier_keyword(parser, "between")) {
                Token left_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected left slot name after 'between'");
                parser_consume(parser, TOKEN_COMMA,
                    "Expected ',' between maintained slot names");
                Token right_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected right slot name after ','");
                ASTNode *maintain = ast_create_zone_maintain_relation(
                    layer_slot.text, left_slot.text, right_slot.text);
                maintain->data.zone_maintain_relation.participant_slot_name =
                    parse_optional_zone_participant_name(parser);
                maintain->line = layer_slot.line;
                maintain->column = layer_slot.column;

                append_child_node(&zone->data.zone_decl.maintained_relations,
                    &zone->data.zone_decl.maintained_relation_count,
                    &zone->data.zone_decl.maintained_relation_capacity, maintain);
            } else {
                ASTNode *maintain = ast_create_zone_maintain_state(layer_slot.text);
                maintain->data.zone_maintain_state.participant_slot_name =
                    parse_optional_zone_participant_name(parser);
                maintain->line = layer_slot.line;
                maintain->column = layer_slot.column;

                append_child_node(&zone->data.zone_decl.maintained_states,
                    &zone->data.zone_decl.maintained_state_count,
                    &zone->data.zone_decl.maintained_state_capacity, maintain);
            }

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "authority")) {
            Token subject_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected subject slot name after 'authority'");
            ASTNode *authority = ast_create_zone_authority(subject_slot.text);
            if (parser_match_identifier_keyword(parser, "requires")) {
                do {
                    ASTNode *ability_name = parse_type(parser);
                    append_child_node(&authority->data.zone_authority.required_abilities,
                        &authority->data.zone_authority.ability_count,
                        &authority->data.zone_authority.ability_capacity,
                        ability_name);
                } while (parser_match(parser, TOKEN_COMMA));
            }
            authority->line = subject_slot.line;
            authority->column = subject_slot.column;

            append_child_node(&zone->data.zone_decl.authorities,
                &zone->data.zone_decl.authority_count,
                &zone->data.zone_decl.authority_capacity, authority);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "state")) {
            Token state_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected state name after 'state'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after zone state name");

            if (parser_match(parser, TOKEN_EFFECT)) {
                Token effect_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected effect slot name after 'effect'");
                if (!parser_match_identifier_keyword(parser, "on")) {
                    parser_error(parser, "Expected 'on' after effect slot name in state");
                    parser_advance(parser);
                    continue;
                }
                Token target_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected target slot name after 'on'");
                ASTNode *state = ast_create_zone_state(
                    state_name.text, false, effect_slot.text, target_slot.text, NULL);
                state->line = state_name.line;
                state->column = state_name.column;

                append_child_node(&zone->data.zone_decl.states,
                    &zone->data.zone_decl.state_count,
                    &zone->data.zone_decl.state_capacity, state);
            } else if (parser_match(parser, TOKEN_RELATION)) {
                Token relation_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected relation slot name after 'relation'");
                if (!parser_match_identifier_keyword(parser, "between")) {
                    parser_error(parser, "Expected 'between' after relation slot name in state");
                    parser_advance(parser);
                    continue;
                }
                Token left_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected left slot name after 'between'");
                parser_consume(parser, TOKEN_COMMA,
                    "Expected ',' between state relation slots");
                Token right_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected right slot name after ','");
                ASTNode *state = ast_create_zone_state(
                    state_name.text, true, relation_slot.text, left_slot.text, right_slot.text);
                state->line = state_name.line;
                state->column = state_name.column;

                append_child_node(&zone->data.zone_decl.states,
                    &zone->data.zone_decl.state_count,
                    &zone->data.zone_decl.state_capacity, state);
            } else {
                parser_error(parser,
                    "Expected 'effect' or 'relation' after ':' in zone state");
                parser_advance(parser);
                continue;
            }

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

            append_child_node(&zone->data.zone_decl.shared_fields,
                &zone->data.zone_decl.shared_count,
                &zone->data.zone_decl.shared_capacity, shared);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            append_child_node(&zone->data.zone_decl.methods,
                &zone->data.zone_decl.method_count,
                &zone->data.zone_decl.method_capacity, method);
        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'subject slot', 'object slot', 'tobject slot', 'relation slot', 'effect slot', 'subjects', 'effects', 'relations', 'apply', 'link', 'detach', 'unlink', 'refresh', 'publish', 'bind', 'maintain', 'authority', 'state', 'shared', or 'func' in zone body");
            parser_advance(parser);
        }
        }
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after zone body");
    return zone;
}
