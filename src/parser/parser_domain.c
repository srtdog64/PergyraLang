#include "parser_internal.h"

static bool parser_match_identifier_keyword(Parser *parser, const char *keyword);
static void append_child_node(ASTNode ***nodes, size_t *count, ASTNode *node);

/* =================================================================
 * Systemic/World system parsing functions
 * ================================================================= */

/*
 * systemic CombatSystem {
 *     party slot team1: DungeonTeam
 *     party slot team2: DungeonTeam
 *     shared rules: CombatRules
 *     func ScheduleMatches() -> Void { ... }
 * }
 */
ASTNode* parse_systemic_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected systemic name");
    ASTNode* sys = ast_create_systemic_declaration(name.text);
    sys->data.systemic_decl.doc_comment = parser_take_pending_doc_comment(parser);
    sys->line = name.line;
    sys->column = name.column;

    sys->data.systemic_decl.generic_params = parse_generic_params(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after systemic name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        if (parser_match(parser, TOKEN_PARTY)) {
            /* party slot name: PartyType */
            parser_consume(parser, TOKEN_SLOT,
                "Expected 'slot' after 'party' in systemic");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after party slot name");
            Token party_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected party type");

            ASTNode* ps = ast_create_systemic_slot(slot_name.text, party_type.text);
            ps->line = slot_name.line;
            ps->column = slot_name.column;

            sys->data.systemic_decl.party_count++;
            sys->data.systemic_decl.party_slots = realloc(
                sys->data.systemic_decl.party_slots,
                sys->data.systemic_decl.party_count * sizeof(ASTNode*));
            sys->data.systemic_decl.party_slots[
                sys->data.systemic_decl.party_count - 1] = ps;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_SHARED)) {
            /* shared field_name: Type = init */
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

            sys->data.systemic_decl.shared_count++;
            sys->data.systemic_decl.shared_fields = realloc(
                sys->data.systemic_decl.shared_fields,
                sys->data.systemic_decl.shared_count * sizeof(ASTNode*));
            sys->data.systemic_decl.shared_fields[
                sys->data.systemic_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            sys->data.systemic_decl.method_count++;
            sys->data.systemic_decl.methods = realloc(
                sys->data.systemic_decl.methods,
                sys->data.systemic_decl.method_count * sizeof(ASTNode*));
            sys->data.systemic_decl.methods[
                sys->data.systemic_decl.method_count - 1] = method;

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'party slot', 'shared', or 'func' in systemic body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after systemic body");
    return sys;
}

/*
 * world GameWorld {
 *     systemic combat: CombatSystem
 *     systemic economy: EconomySystem
 *     shared tick: Int = 0
 *     func Update() -> Void { ... }
 * }
 */
ASTNode* parse_world_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected world name");
    ASTNode* world = ast_create_world_declaration(name.text);
    world->data.world_decl.doc_comment = parser_take_pending_doc_comment(parser);
    world->line = name.line;
    world->column = name.column;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after world name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        if (parser_match(parser, TOKEN_SYSTEMIC)) {
            /* systemic name: SystemicType */
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected systemic name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after systemic name");
            Token sys_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected systemic type");

            ASTNode* ws = ast_create_world_systemic(
                slot_name.text, sys_type.text);
            ws->line = slot_name.line;
            ws->column = slot_name.column;

            world->data.world_decl.systemic_count++;
            world->data.world_decl.systemics = realloc(
                world->data.world_decl.systemics,
                world->data.world_decl.systemic_count * sizeof(ASTNode*));
            world->data.world_decl.systemics[
                world->data.world_decl.systemic_count - 1] = ws;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_ZONE)) {
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after zone name");
            Token zone_type = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone type");

            ASTNode* wz = ast_create_world_zone(slot_name.text, zone_type.text);
            wz->line = slot_name.line;
            wz->column = slot_name.column;

            world->data.world_decl.zone_count++;
            world->data.world_decl.zones = realloc(
                world->data.world_decl.zones,
                world->data.world_decl.zone_count * sizeof(ASTNode*));
            world->data.world_decl.zones[
                world->data.world_decl.zone_count - 1] = wz;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "activate")) {
            Token zone_or_state = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone slot name or world state name after 'activate'");
            ASTNode *activate = ast_create_world_activate(zone_or_state.text);
            activate->data.world_activate.state_name = pergyra_strdup(zone_or_state.text);
            free(activate->data.world_activate.zone_slot_name);
            activate->data.world_activate.zone_slot_name = NULL;
            activate->line = zone_or_state.line;
            activate->column = zone_or_state.column;

            append_child_node(&world->data.world_decl.activations,
                &world->data.world_decl.activate_count, activate);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "deactivate")) {
            Token zone_or_state = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone slot name or world state name after 'deactivate'");
            ASTNode *deactivate = ast_create_world_deactivate(zone_or_state.text);
            deactivate->data.world_deactivate.state_name = pergyra_strdup(zone_or_state.text);
            free(deactivate->data.world_deactivate.zone_slot_name);
            deactivate->data.world_deactivate.zone_slot_name = NULL;
            deactivate->line = zone_or_state.line;
            deactivate->column = zone_or_state.column;

            append_child_node(&world->data.world_decl.deactivations,
                &world->data.world_decl.deactivate_count, deactivate);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "maintain")) {
            Token zone_or_state = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone slot name or world state name after 'maintain'");
            ASTNode *maintain = ast_create_world_maintain(zone_or_state.text);
            maintain->data.world_maintain.state_name = pergyra_strdup(zone_or_state.text);
            free(maintain->data.world_maintain.zone_slot_name);
            maintain->data.world_maintain.zone_slot_name = NULL;
            maintain->line = zone_or_state.line;
            maintain->column = zone_or_state.column;

            append_child_node(&world->data.world_decl.maintained_zones,
                &world->data.world_decl.maintained_zone_count, maintain);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match_identifier_keyword(parser, "state")) {
            Token state_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected world state name after 'state'");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after world state name");
            parser_consume(parser, TOKEN_ZONE,
                "Expected 'zone' after ':' in world state");
            Token zone_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected zone slot name after 'zone'");
            ASTNode *state = ast_create_world_state(state_name.text, zone_slot.text);
            state->line = state_name.line;
            state->column = state_name.column;

            append_child_node(&world->data.world_decl.states,
                &world->data.world_decl.state_count, state);

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

            world->data.world_decl.shared_count++;
            world->data.world_decl.shared_fields = realloc(
                world->data.world_decl.shared_fields,
                world->data.world_decl.shared_count * sizeof(ASTNode*));
            world->data.world_decl.shared_fields[
                world->data.world_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            world->data.world_decl.method_count++;
            world->data.world_decl.methods = realloc(
                world->data.world_decl.methods,
                world->data.world_decl.method_count * sizeof(ASTNode*));
            world->data.world_decl.methods[
                world->data.world_decl.method_count - 1] = method;

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'systemic', 'zone', 'activate', 'deactivate', 'maintain', 'state', 'shared', or 'func' in world body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after world body");
    return world;
}

static bool
parser_match_identifier_keyword(Parser *parser, const char *keyword)
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
parser_match_domain_slot_kind(Parser *parser, bool *is_subject, bool *is_dto)
{
    if (parser_match(parser, TOKEN_CLASS)) {
        if (is_subject != NULL)
            *is_subject = true;
        if (is_dto != NULL)
            *is_dto = false;
        return true;
    }

    if ((parser_check(parser, TOKEN_IDENTIFIER)
         || parser_check(parser, TOKEN_STRUCT))
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "dto") == 0) {
        parser_advance(parser);
        if (is_subject != NULL)
            *is_subject = false;
        if (is_dto != NULL)
            *is_dto = true;
        return true;
    }

    if ((parser_check(parser, TOKEN_IDENTIFIER)
         || parser_check(parser, TOKEN_STRUCT))
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "object") == 0) {
        parser_advance(parser);
        if (is_subject != NULL)
            *is_subject = false;
        if (is_dto != NULL)
            *is_dto = false;
        return true;
    }

    return false;
}

static ASTNode *
parse_domain_slot_entry(Parser *parser, const char *owner_name)
{
    bool is_subject = false;
    bool is_dto = false;
    if (!parser_match_domain_slot_kind(parser, &is_subject, &is_dto))
        return NULL;

    parser_consume(parser, TOKEN_SLOT,
        is_subject
            ? "Expected 'slot' after 'subject' in domain body"
            : (is_dto
                ? "Expected 'slot' after 'dto' in domain body"
                : "Expected 'slot' after 'object' in domain body"));
    Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected slot name");
    parser_consume(parser, TOKEN_COLON,
        "Expected ':' after domain slot name");
    ASTNode *slot_type = parse_type(parser);

    ASTNode *slot = ast_create_domain_slot(slot_name.text, is_subject);
    slot->data.domain_slot.is_dto = is_dto;
    slot->data.domain_slot.type = slot_type;
    if (parser_match(parser, TOKEN_ASSIGN))
        slot->data.domain_slot.initializer = parser_parse_expression(parser);
    slot->line = slot_name.line;
    slot->column = slot_name.column;

    (void)owner_name;
    return slot;
}

static void
append_domain_slot(ASTNode ***slots, size_t *slot_count, ASTNode *slot)
{
    if (slots == NULL || slot_count == NULL || slot == NULL)
        return;

    *slot_count += 1;
    *slots = realloc(*slots, *slot_count * sizeof(ASTNode *));
    (*slots)[*slot_count - 1] = slot;
}

static void
append_child_node(ASTNode ***nodes, size_t *count, ASTNode *node)
{
    if (nodes == NULL || count == NULL || node == NULL)
        return;

    *count += 1;
    *nodes = realloc(*nodes, *count * sizeof(ASTNode *));
    (*nodes)[*count - 1] = node;
}

static char *
parse_optional_zone_actor_name(Parser *parser)
{
    Token actor_slot;

    if (!parser_match_identifier_keyword(parser, "by"))
        return NULL;

    actor_slot = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected subject slot name after 'by'");
    return pergyra_strdup(actor_slot.text);
}

static void
parse_header_subject_targets(Parser *parser, ASTNode ***slots, size_t *slot_count,
                             const char *owner_kind)
{
    if (!parser_match(parser, TOKEN_FOR))
        return;

    do {
        Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
            "Expected subject target name after 'for'");
        parser_consume(parser, TOKEN_COLON,
            "Expected ':' after subject target name");
        ASTNode *slot_type = parse_type(parser);

        ASTNode *slot = ast_create_domain_slot(slot_name.text, true);
        slot->data.domain_slot.type = slot_type;
        slot->line = slot_name.line;
        slot->column = slot_name.column;
        append_domain_slot(slots, slot_count, slot);
    } while (parser_match(parser, TOKEN_COMMA));

    (void)owner_kind;
}

ASTNode* parse_relation_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected relation name");
    ASTNode* relation = ast_create_relation_declaration(name.text);
    relation->data.relation_decl.doc_comment = parser_take_pending_doc_comment(parser);
    relation->line = name.line;
    relation->column = name.column;
    parse_header_subject_targets(parser,
        &relation->data.relation_decl.slots,
        &relation->data.relation_decl.slot_count,
        "relation");
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after relation name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        ASTNode *slot = parse_domain_slot_entry(parser, "relation");
        if (slot != NULL) {
            append_domain_slot(&relation->data.relation_decl.slots,
                &relation->data.relation_decl.slot_count, slot);

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

            relation->data.relation_decl.shared_count++;
            relation->data.relation_decl.shared_fields = realloc(
                relation->data.relation_decl.shared_fields,
                relation->data.relation_decl.shared_count * sizeof(ASTNode*));
            relation->data.relation_decl.shared_fields[
                relation->data.relation_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            relation->data.relation_decl.method_count++;
            relation->data.relation_decl.methods = realloc(
                relation->data.relation_decl.methods,
                relation->data.relation_decl.method_count * sizeof(ASTNode*));
            relation->data.relation_decl.methods[
                relation->data.relation_decl.method_count - 1] = method;
        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'subject slot', 'object slot', 'dto slot', 'shared', or 'func' in relation body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after relation body");
    return relation;
}

ASTNode* parse_effect_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected effect name");
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
        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            effect->data.effect_decl.method_count++;
            effect->data.effect_decl.methods = realloc(
                effect->data.effect_decl.methods,
                effect->data.effect_decl.method_count * sizeof(ASTNode*));
            effect->data.effect_decl.methods[
                effect->data.effect_decl.method_count - 1] = method;
        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'subject slot', 'object slot', 'dto slot', 'shared', or 'func' in effect body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after effect body");
    return effect;
}

ASTNode* parse_zone_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected zone name");
    ASTNode* zone = ast_create_zone_declaration(name.text);
    zone->data.zone_decl.doc_comment = parser_take_pending_doc_comment(parser);
    zone->line = name.line;
    zone->column = name.column;
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after zone name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);

        ASTNode *slot = parse_domain_slot_entry(parser, "zone");
        if (slot != NULL) {
            append_domain_slot(&zone->data.zone_decl.slots,
                &zone->data.zone_decl.slot_count, slot);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_RELATION) || parser_match(parser, TOKEN_EFFECT)) {
            bool is_relation = parser->previous_token.type == TOKEN_RELATION;
            parser_consume(parser, TOKEN_SLOT,
                is_relation
                    ? "Expected 'slot' after 'relation' in zone"
                    : "Expected 'slot' after 'effect' in zone");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after zone layer slot name");
            Token layer_type = parser_consume(parser, TOKEN_IDENTIFIER,
                is_relation ? "Expected relation type" : "Expected effect type");

            ASTNode *layer_slot = ast_create_zone_layer_slot(
                slot_name.text, layer_type.text, is_relation);
            layer_slot->line = slot_name.line;
            layer_slot->column = slot_name.column;

            append_child_node(&zone->data.zone_decl.layer_slots,
                &zone->data.zone_decl.layer_slot_count, layer_slot);

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

            apply->data.zone_apply.actor_slot_name =
                parse_optional_zone_actor_name(parser);
            apply->line = effect_slot.line;
            apply->column = effect_slot.column;

            append_child_node(&zone->data.zone_decl.applies,
                &zone->data.zone_decl.apply_count, apply);

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

            link->data.zone_link.actor_slot_name =
                parse_optional_zone_actor_name(parser);
            link->line = relation_slot.line;
            link->column = relation_slot.column;

            append_child_node(&zone->data.zone_decl.links,
                &zone->data.zone_decl.link_count, link);

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

            detach->data.zone_detach.actor_slot_name =
                parse_optional_zone_actor_name(parser);
            detach->line = effect_slot.line;
            detach->column = effect_slot.column;

            append_child_node(&zone->data.zone_decl.detaches,
                &zone->data.zone_decl.detach_count, detach);

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

            unlink->data.zone_unlink.actor_slot_name =
                parse_optional_zone_actor_name(parser);
            unlink->line = relation_slot.line;
            unlink->column = relation_slot.column;

            append_child_node(&zone->data.zone_decl.unlinks,
                &zone->data.zone_decl.unlink_count, unlink);

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "refresh")
                   || parser_match_identifier_keyword(parser, "publish")) {
            bool requires_dto =
                parser->previous_token.text != NULL
                && strcmp(parser->previous_token.text, "publish") == 0;
            Token object_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                requires_dto
                    ? "Expected dto slot name after 'publish'"
                    : "Expected object slot name after 'refresh'");
            if (!parser_match_identifier_keyword(parser, "from")) {
                parser_error(parser,
                    requires_dto
                        ? "Expected 'from' after dto slot name in publish"
                        : "Expected 'from' after object slot name in refresh");
                parser_advance(parser);
                continue;
            }
            Token source_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected source slot name after 'from'");

            ASTNode *refresh = ast_create_zone_refresh(
                object_slot.text, source_slot.text);
            refresh->data.zone_refresh.requires_dto = requires_dto;
            refresh->data.zone_refresh.actor_slot_name =
                parse_optional_zone_actor_name(parser);
            refresh->line = object_slot.line;
            refresh->column = object_slot.column;

            append_child_node(&zone->data.zone_decl.refreshes,
                &zone->data.zone_decl.refresh_count, refresh);

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
                maintain->data.zone_maintain_effect.actor_slot_name =
                    parse_optional_zone_actor_name(parser);
                maintain->line = layer_slot.line;
                maintain->column = layer_slot.column;

                append_child_node(&zone->data.zone_decl.maintained_effects,
                    &zone->data.zone_decl.maintained_effect_count, maintain);
            } else if (parser_match_identifier_keyword(parser, "between")) {
                Token left_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected left slot name after 'between'");
                parser_consume(parser, TOKEN_COMMA,
                    "Expected ',' between maintained slot names");
                Token right_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                    "Expected right slot name after ','");
                ASTNode *maintain = ast_create_zone_maintain_relation(
                    layer_slot.text, left_slot.text, right_slot.text);
                maintain->data.zone_maintain_relation.actor_slot_name =
                    parse_optional_zone_actor_name(parser);
                maintain->line = layer_slot.line;
                maintain->column = layer_slot.column;

                append_child_node(&zone->data.zone_decl.maintained_relations,
                    &zone->data.zone_decl.maintained_relation_count, maintain);
            } else {
                ASTNode *maintain = ast_create_zone_maintain_state(layer_slot.text);
                maintain->data.zone_maintain_state.actor_slot_name =
                    parse_optional_zone_actor_name(parser);
                maintain->line = layer_slot.line;
                maintain->column = layer_slot.column;

                append_child_node(&zone->data.zone_decl.maintained_states,
                    &zone->data.zone_decl.maintained_state_count, maintain);
            }

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match_identifier_keyword(parser, "authority")) {
            Token subject_slot = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected subject slot name after 'authority'");
            ASTNode *authority = ast_create_zone_authority(subject_slot.text);
            if (parser_match_identifier_keyword(parser, "requires")) {
                do {
                    Token ability_name = parser_consume(parser, TOKEN_IDENTIFIER,
                        "Expected ability name after 'requires'");
                    authority->data.zone_authority.ability_count++;
                    authority->data.zone_authority.required_abilities = realloc(
                        authority->data.zone_authority.required_abilities,
                        authority->data.zone_authority.ability_count * sizeof(char *));
                    authority->data.zone_authority.required_abilities[
                        authority->data.zone_authority.ability_count - 1] =
                        pergyra_strdup(ability_name.text);
                } while (parser_match(parser, TOKEN_COMMA));
            }
            authority->line = subject_slot.line;
            authority->column = subject_slot.column;

            append_child_node(&zone->data.zone_decl.authorities,
                &zone->data.zone_decl.authority_count, authority);

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
                    &zone->data.zone_decl.state_count, state);
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
                    &zone->data.zone_decl.state_count, state);
            } else {
                parser_error(parser,
                    "Expected 'effect' or 'relation' after ':' in zone state");
                parser_advance(parser);
                continue;
            }

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

            zone->data.zone_decl.shared_count++;
            zone->data.zone_decl.shared_fields = realloc(
                zone->data.zone_decl.shared_fields,
                zone->data.zone_decl.shared_count * sizeof(ASTNode*));
            zone->data.zone_decl.shared_fields[
                zone->data.zone_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);
        } else if (parser_match(parser, TOKEN_FUNC)) {
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
            zone->data.zone_decl.method_count++;
            zone->data.zone_decl.methods = realloc(
                zone->data.zone_decl.methods,
                zone->data.zone_decl.method_count * sizeof(ASTNode*));
            zone->data.zone_decl.methods[
                zone->data.zone_decl.method_count - 1] = method;
        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'subject slot', 'object slot', 'dto slot', 'relation slot', 'effect slot', 'apply', 'link', 'detach', 'unlink', 'refresh', 'publish', 'maintain', 'authority', 'state', 'shared', or 'func' in zone body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after zone body");
    return zone;
}

/* =================================================================
 * Party system parsing functions
 * ================================================================= */

/*
 * party HolyPaladin extends BaseParty {
 *     role slot tank: Damageable & Taunting
 *     role slot healer: Healing
 *     shared formation: String = "standard"
 *     func Execute() -> Void { ... }
 * }
 */
ASTNode* parse_party_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected party name");
    ASTNode* party = ast_create_party_declaration(name.text);
    party->data.party_decl.doc_comment = parser_take_pending_doc_comment(parser);
    party->line = name.line;
    party->column = name.column;

    /* Optional generic params */
    party->data.party_decl.generic_params = parse_generic_params(parser);

    /* Optional extends */
    if (parser_match(parser, TOKEN_EXTENDS)) {
        party->data.party_decl.extends = parse_type(parser);
    }

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after party header");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        bool is_dyn = parser_match(parser, TOKEN_DYN);

        if (is_dyn || parser_match(parser, TOKEN_ROLE)) {
            if (is_dyn) {
                parser_consume(parser, TOKEN_ROLE,
                    "Expected 'role' after 'dyn'");
            }
            /* role slot name: AbilityType & AbilityType */
            parser_consume(parser, TOKEN_SLOT,
                "Expected 'slot' after 'role' in party");
            Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected slot name");
            parser_consume(parser, TOKEN_COLON,
                "Expected ':' after role slot name");

            ASTNode* rs = ast_create_role_slot(slot_name.text);
            rs->line = slot_name.line;
            rs->column = slot_name.column;
            rs->data.role_slot.is_dynamic = is_dyn;

            /* Parse ability types separated by & */
            do {
                ASTNode* ability_type = parse_type(parser);
                rs->data.role_slot.ability_count++;
                rs->data.role_slot.required_abilities = realloc(
                    rs->data.role_slot.required_abilities,
                    rs->data.role_slot.ability_count * sizeof(ASTNode*));
                rs->data.role_slot.required_abilities[
                    rs->data.role_slot.ability_count - 1] = ability_type;
            } while (parser_match(parser, TOKEN_AND));

            party->data.party_decl.role_count++;
            party->data.party_decl.role_slots = realloc(
                party->data.party_decl.role_slots,
                party->data.party_decl.role_count * sizeof(ASTNode*));
            party->data.party_decl.role_slots[
                party->data.party_decl.role_count - 1] = rs;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_SHARED)) {
            /* shared field_name: Type = initializer */
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

            party->data.party_decl.shared_count++;
            party->data.party_decl.shared_fields = realloc(
                party->data.party_decl.shared_fields,
                party->data.party_decl.shared_count * sizeof(ASTNode*));
            party->data.party_decl.shared_fields[
                party->data.party_decl.shared_count - 1] = shared;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Party method */
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            party->data.party_decl.method_count++;
            party->data.party_decl.methods = realloc(
                party->data.party_decl.methods,
                party->data.party_decl.method_count * sizeof(ASTNode*));
            party->data.party_decl.methods[
                party->data.party_decl.method_count - 1] = method;

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'role slot', 'shared', or 'func' in party body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after party body");
    return party;
}

/* =================================================================
 * Role/Ability system parsing functions
 * ================================================================= */

/*
 * ability Damageable {
 *     require health: Int
 *     func TakeDamage(amount: Int) -> Void
 *     func GetHealth() -> Int { return self.health; }
 * }
 */
ASTNode* parse_ability_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected ability name");
    ASTNode* ability = ast_create_ability_declaration(name.text);
    ability->data.ability_decl.doc_comment = parser_take_pending_doc_comment(parser);
    ability->line = name.line;
    ability->column = name.column;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after ability name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        if (parser_match(parser, TOKEN_REQUIRE)) {
            /* require field_name: Type */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'require'");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after require field name");
            ASTNode* field_type = parse_type(parser);

            ASTNode* req = ast_create_require_field(field_name.text);
            req->data.require_field.type = field_type;
            req->line = field_name.line;
            req->column = field_name.column;

            ability->data.ability_decl.require_count++;
            ability->data.ability_decl.require_fields = realloc(
                ability->data.ability_decl.require_fields,
                ability->data.ability_decl.require_count * sizeof(ASTNode*));
            ability->data.ability_decl.require_fields[
                ability->data.ability_decl.require_count - 1] = req;

            /* Optional semicolon */
            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Method declaration (may have body or be abstract) */
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            ability->data.ability_decl.method_count++;
            ability->data.ability_decl.methods = realloc(
                ability->data.ability_decl.methods,
                ability->data.ability_decl.method_count * sizeof(ASTNode*));
            ability->data.ability_decl.methods[
                ability->data.ability_decl.method_count - 1] = method;

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser, "Expected 'require' or 'func' in ability body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after ability body");
    return ability;
}

/*
 * role PlayerDamageable for Player {
 *     include role BuffableRole<Int>
 *     impl ability Damageable {
 *         func TakeDamage(amount: Int) -> Void { ... }
 *     }
 *     override func GetHealth() -> Int { super.GetHealth() + bonus; }
 * }
 */
ASTNode* parse_role_declaration(Parser* parser) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected role name");
    ASTNode* role = ast_create_role_declaration(name.text);
    role->data.role_decl.doc_comment = parser_take_pending_doc_comment(parser);
    role->line = name.line;
    role->column = name.column;

    /* Optional generic params */
    role->data.role_decl.generic_params = parse_generic_params(parser);

    /* 'for' TargetType (reuse TOKEN_FOR) */
    if (parser_match(parser, TOKEN_FOR)) {
        role->data.role_decl.for_type = parse_type(parser);
    }

    /* Optional where clause */
    role->data.role_decl.where_clause = parse_where_clause(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after role header");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        if (parser_match(parser, TOKEN_INCLUDE)) {
            /* include role RoleName<T> */
            parser_match(parser, TOKEN_ROLE); /* optional 'role' keyword */
            Token role_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected role name after 'include'");
            ASTNode* inc = ast_create_include_statement(role_name.text);
            inc->line = role_name.line;
            inc->column = role_name.column;
            /* Optional generic args */
            inc->data.include_stmt.type_args = parse_generic_params(parser);

            role->data.role_decl.include_count++;
            role->data.role_decl.includes = realloc(
                role->data.role_decl.includes,
                role->data.role_decl.include_count * sizeof(ASTNode*));
            role->data.role_decl.includes[
                role->data.role_decl.include_count - 1] = inc;

            parser_match(parser, TOKEN_SEMICOLON);
            parser_discard_pending_doc_comment(parser);

        } else if (parser_match(parser, TOKEN_IMPL)) {
            /* impl ability AbilityName { ... } */
            parser_match(parser, TOKEN_ABILITY); /* optional 'ability' keyword */
            Token ability_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected ability name after 'impl'");
            ASTNode* impl = ast_create_impl_ability(ability_name.text);
            impl->line = ability_name.line;
            impl->column = ability_name.column;

            parser_consume(parser, TOKEN_LBRACE,
                "Expected '{' after impl ability name");

            while (!parser_check(parser, TOKEN_RBRACE)
                   && !parser_is_at_end(parser)) {
                parser_collect_doc_comments(parser);
                if (parser_match(parser, TOKEN_FUNC)) {
                    ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));
                    impl->data.impl_ability.method_count++;
                    impl->data.impl_ability.methods = realloc(
                        impl->data.impl_ability.methods,
                        impl->data.impl_ability.method_count * sizeof(ASTNode*));
                    impl->data.impl_ability.methods[
                        impl->data.impl_ability.method_count - 1] = method;
                } else {
                    parser_discard_pending_doc_comment(parser);
                    parser_error(parser,
                        "Expected 'func' in impl ability body");
                    parser_advance(parser);
                }
            }
            parser_consume(parser, TOKEN_RBRACE,
                "Expected '}' after impl ability body");

            role->data.role_decl.impl_count++;
            role->data.role_decl.impl_abilities = realloc(
                role->data.role_decl.impl_abilities,
                role->data.role_decl.impl_count * sizeof(ASTNode*));
            role->data.role_decl.impl_abilities[
                role->data.role_decl.impl_count - 1] = impl;

        } else if (parser_match(parser, TOKEN_OVERRIDE)) {
            /* override func FuncName(...) { ... } */
            parser_consume(parser, TOKEN_FUNC,
                "Expected 'func' after 'override'");
            ASTNode* func = parser_finalize_statement(parser, parse_function_declaration(parser));
            ASTNode* ovr = ast_create_override_func(func);
            ovr->line = func->line;
            ovr->column = func->column;

            /* Check if body contains 'super' calls — simple heuristic */
            ovr->data.override_func.calls_super = false;

            /* Add as an impl with special name "__override__" */
            role->data.role_decl.impl_count++;
            role->data.role_decl.impl_abilities = realloc(
                role->data.role_decl.impl_abilities,
                role->data.role_decl.impl_count * sizeof(ASTNode*));
            role->data.role_decl.impl_abilities[
                role->data.role_decl.impl_count - 1] = ovr;

        } else if (parser_match(parser, TOKEN_FUNC)) {
            /* Direct method in role (not in impl block) */
            ASTNode* method = parser_finalize_statement(parser, parse_function_declaration(parser));

            /* Wrap as impl with no ability name (role's own method) */
            ASTNode* impl = ast_create_impl_ability(NULL);
            impl->data.impl_ability.method_count = 1;
            impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode*));
            impl->data.impl_ability.methods[0] = method;

            role->data.role_decl.impl_count++;
            role->data.role_decl.impl_abilities = realloc(
                role->data.role_decl.impl_abilities,
                role->data.role_decl.impl_count * sizeof(ASTNode*));
            role->data.role_decl.impl_abilities[
                role->data.role_decl.impl_count - 1] = impl;

        } else {
            parser_discard_pending_doc_comment(parser);
            parser_error(parser,
                "Expected 'include', 'impl', 'override', or 'func' in role body");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after role body");
    return role;
}

/* =================================================================
 * Event system parsing functions
 * ================================================================= */

// 이벤트 선언 파싱: event OnClick(sender: Object, args: EventArgs);
ASTNode* parse_event_declaration(Parser* parser) {
    // 이벤트 이름
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected event name");

    ASTNode* event_decl = ast_create_event_declaration(name.text);

    // 접근 제어자 (선택적)
    if (parser_match(parser, TOKEN_PUBLIC)) {
        event_decl->data.event_decl.access = ACCESS_PUBLIC;
    } else if (parser_match(parser, TOKEN_PRIVATE)) {
        event_decl->data.event_decl.access = ACCESS_PRIVATE;
    }

    // 파라미터 파싱
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' after event name");

    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        // 파라미터 이름
        Token param_name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        parser_consume(parser, TOKEN_COLON, "Expected ':' after parameter name");

        // 파라미터 타입
        ASTNode* param_type = parse_type(parser);

        // 파라미터 추가
        event_decl->data.event_decl.param_count++;
        event_decl->data.event_decl.params = realloc(
            event_decl->data.event_decl.params,
            event_decl->data.event_decl.param_count * sizeof(ASTNode*)
        );

        // 파라미터 노드 생성 (let decl 와 유사)
        ASTNode* param = ast_create_let_declaration(param_name.text);
        param->data.let_decl.type = param_type;
        event_decl->data.event_decl.params[event_decl->data.event_decl.param_count - 1] = param;

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after event parameters");

    // 반환 타입 (선택적, 기본은 Void)
    if (parser_match(parser, TOKEN_ARROW)) {
        event_decl->data.event_decl.return_type = parse_type(parser);
    }

    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after event declaration");

    return event_decl;
}
