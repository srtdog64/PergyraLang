#include "parser_domain_internal.h"


/* =================================================================
 * Roster/World system parsing functions
 * ================================================================= */

/*
 * roster CombatSystem {
 *     party slot team1: DungeonTeam
 *     party slot team2: DungeonTeam
 *     shared rules: CombatRules
 *     func ScheduleMatches() -> Void { ... }
 * }
 */
/*
 * world GameWorld {
 *     roster combat: CombatSystem
 *     roster economy: EconomySystem
 *     shared tick: Int = 0
 *     func Update() -> Void { ... }
 * }
 */
bool
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

bool
parser_match_identifier_keyword_on_line(Parser *parser, const char *keyword,
                                        unsigned line)
{
    if (parser == NULL || parser->current_token.line != line)
        return false;

    return parser_match_identifier_keyword(parser, keyword);
}

static bool
parser_match_domain_slot_kind(Parser *parser, bool *is_subject, bool *is_vessel,
                              bool *is_tobject)
{
    if (parser_match(parser, TOKEN_SUBJECT)) {
        if (is_subject != NULL)
            *is_subject = true;
        if (is_vessel != NULL)
            *is_vessel = false;
        if (is_tobject != NULL)
            *is_tobject = false;
        return true;
    }

    if (parser_match(parser, TOKEN_VESSEL)) {
        if (is_subject != NULL)
            *is_subject = false;
        if (is_vessel != NULL)
            *is_vessel = true;
        if (is_tobject != NULL)
            *is_tobject = false;
        return true;
    }

    if (parser_match(parser, TOKEN_TOBJECT)) {
        if (is_subject != NULL)
            *is_subject = false;
        if (is_vessel != NULL)
            *is_vessel = false;
        if (is_tobject != NULL)
            *is_tobject = true;
        return true;
    }

    if (parser_match(parser, TOKEN_OBJECT)) {
        if (is_subject != NULL)
            *is_subject = false;
        if (is_vessel != NULL)
            *is_vessel = false;
        if (is_tobject != NULL)
            *is_tobject = false;
        return true;
    }

    return false;
}

ASTNode *
parse_domain_slot_entry(Parser *parser, const char *owner_name)
{
    bool is_subject = false;
    bool is_vessel = false;
    bool is_tobject = false;
    if (!parser_match_domain_slot_kind(parser, &is_subject, &is_vessel, &is_tobject))
        return NULL;

    parser_consume(parser, TOKEN_SLOT,
        is_subject
            ? "Expected 'slot' after 'subject' in domain body"
            : (is_vessel
                ? "Expected 'slot' after 'vessel' in domain body"
            : (is_tobject
                ? "Expected 'slot' after 'tobject' in domain body"
                : "Expected 'slot' after 'object' in domain body")));
    Token slot_name = parser_consume(parser, TOKEN_IDENTIFIER,
        "Expected slot name");
    parser_consume(parser, TOKEN_COLON,
        "Expected ':' after domain slot name");
    ASTNode *slot_type = parse_type(parser);

    ASTNode *slot = ast_create_domain_slot(slot_name.text, is_subject);
    slot->data.domain_slot.is_vessel = is_vessel;
    slot->data.domain_slot.is_tobject = is_tobject;
    slot->data.domain_slot.type = slot_type;
    if (parser_match(parser, TOKEN_ASSIGN))
        slot->data.domain_slot.initializer = parser_parse_expression(parser);
    slot->line = slot_name.line;
    slot->column = slot_name.column;

    (void)owner_name;
    return slot;
}

void
append_domain_slot(ASTNode ***slots, size_t *slot_count, ASTNode *slot)
{
    if (slots == NULL || slot_count == NULL || slot == NULL)
        return;

    *slot_count += 1;
    *slots = realloc(*slots, *slot_count * sizeof(ASTNode *));
    (*slots)[*slot_count - 1] = slot;
}

void
append_child_node(ASTNode ***nodes, size_t *count, ASTNode *node)
{
    if (nodes == NULL || count == NULL || node == NULL)
        return;

    *count += 1;
    *nodes = realloc(*nodes, *count * sizeof(ASTNode *));
    (*nodes)[*count - 1] = node;
}

char *
parse_optional_zone_participant_name(Parser *parser)
{
    Token participant_slot;

    if (!parser_match_identifier_keyword(parser, "by"))
        return NULL;

    participant_slot = consume_name_token(parser,
        "Expected subject slot name after 'by'");
    return pergyra_strdup(participant_slot.text);
}

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

DomainSlotGroupKind
parser_match_domain_slot_group_kind(Parser *parser)
{
    if (parser_match_identifier_keyword(parser, "subjects"))
        return DOMAIN_GROUP_SUBJECTS;
    if (parser_match_identifier_keyword(parser, "objects"))
        return DOMAIN_GROUP_OBJECTS;
    if (parser_match_identifier_keyword(parser, "tobjects"))
        return DOMAIN_GROUP_TOBJECTS;
    return DOMAIN_GROUP_NONE;
}

DomainLayerGroupKind
parser_match_domain_layer_group_kind(Parser *parser)
{
    if (parser_match_identifier_keyword(parser, "effects"))
        return DOMAIN_LAYER_GROUP_EFFECTS;
    if (parser_match_identifier_keyword(parser, "relations"))
        return DOMAIN_LAYER_GROUP_RELATIONS;
    return DOMAIN_LAYER_GROUP_NONE;
}

static void
parser_parse_projection_field_map(Parser *parser,
                                  char ***mapped_target_fields,
                                  char ***mapped_source_fields,
                                  size_t *field_map_count)
{
    if (mapped_target_fields != NULL)
        *mapped_target_fields = NULL;
    if (mapped_source_fields != NULL)
        *mapped_source_fields = NULL;
    if (field_map_count != NULL)
        *field_map_count = 0;
    if (parser == NULL || !parser_match_identifier_keyword(parser, "map"))
        return;

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'map'");
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        Token target_field = consume_name_token(parser,
            "Expected target field name in projection map");
        Token source_field;

        parser_consume(parser, TOKEN_CHANNEL_OP,
            "Expected '<-' in projection map entry");
        source_field = consume_name_token(parser,
            "Expected source field name after '<-' in projection map");

        if (mapped_target_fields != NULL && mapped_source_fields != NULL
            && field_map_count != NULL) {
            *mapped_target_fields = realloc(*mapped_target_fields,
                sizeof(char *) * (*field_map_count + 1));
            *mapped_source_fields = realloc(*mapped_source_fields,
                sizeof(char *) * (*field_map_count + 1));
            (*mapped_target_fields)[*field_map_count] =
                pergyra_strdup(target_field.text);
            (*mapped_source_fields)[*field_map_count] =
                pergyra_strdup(source_field.text);
            (*field_map_count)++;
        }

        parser_match(parser, TOKEN_SEMICOLON);
    }
    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after projection map");

    if (field_map_count != NULL && *field_map_count == 0) {
        parser_error(parser,
            "Expected at least one 'target <- source' entry inside projection map");
    }
}

void
append_domain_projection_sync_entries(Parser *parser,
                                      ASTNode ***refreshes,
                                      size_t *refresh_count,
                                      bool allow_participant)
{
    bool derive_target_kind =
        parser->previous_token.text != NULL
        && strcmp(parser->previous_token.text, "bind") == 0;
    bool requires_dto =
        !derive_target_kind
        && parser->previous_token.text != NULL
        && strcmp(parser->previous_token.text, "publish") == 0;
    char **target_names = NULL;
    size_t target_count = 0;
    unsigned first_line = parser->current_token.line;
    unsigned first_column = parser->current_token.column;
    char **mapped_target_fields = NULL;
    char **mapped_source_fields = NULL;
    size_t field_map_count = 0;

    if (parser_match(parser, TOKEN_LBRACKET)) {
        do {
            Token target_name = consume_name_token(parser,
                derive_target_kind
                    ? "Expected object/tobject slot name in bind group"
                    : (requires_dto
                        ? "Expected tobject slot name in publish group"
                        : "Expected object slot name in refresh group"));
            target_names = realloc(target_names, sizeof(char *) * (target_count + 1));
            target_names[target_count++] = pergyra_strdup(target_name.text);
            if (target_count == 1) {
                first_line = target_name.line;
                first_column = target_name.column;
            }
        } while (parser_match(parser, TOKEN_COMMA));
        parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after projection group slot names");
    } else {
        Token target_name = consume_name_token(parser,
            derive_target_kind
                ? "Expected object/tobject slot name after 'bind'"
                : (requires_dto
                    ? "Expected tobject slot name after 'publish'"
                    : "Expected object slot name after 'refresh'"));
        target_names = realloc(target_names, sizeof(char *));
        target_names[target_count++] = pergyra_strdup(target_name.text);
        first_line = target_name.line;
        first_column = target_name.column;
    }

    if (!parser_match_identifier_keyword(parser, "from")) {
        parser_error(parser,
            derive_target_kind
                ? "Expected 'from' after bind slot name or group"
                : (requires_dto
                    ? "Expected 'from' after publish slot name or group"
                    : "Expected 'from' after refresh slot name or group"));
        for (size_t i = 0; i < target_count; i++)
            free(target_names[i]);
        free(target_names);
        parser_advance(parser);
        return;
    }

    Token source_slot = consume_name_token(parser,
        "Expected source slot name after 'from'");
    char *participant_slot_name = allow_participant
        ? parse_optional_zone_participant_name(parser)
        : NULL;
    parser_parse_projection_field_map(parser, &mapped_target_fields,
        &mapped_source_fields, &field_map_count);
    if (field_map_count > 0 && target_count != 1) {
        parser_error(parser,
            "Projection map is currently supported only for a single refresh/publish/bind target");
    }

    for (size_t i = 0; i < target_count; i++) {
        ASTNode *refresh = ast_create_zone_refresh(target_names[i], source_slot.text);
        refresh->data.zone_refresh.requires_dto = requires_dto;
        refresh->data.zone_refresh.derive_target_kind = derive_target_kind;
        refresh->data.zone_refresh.participant_slot_name =
            participant_slot_name != NULL ? pergyra_strdup(participant_slot_name) : NULL;
        if (field_map_count > 0 && target_count == 1) {
            refresh->data.zone_refresh.field_map_count = field_map_count;
            refresh->data.zone_refresh.mapped_target_fields = calloc(
                field_map_count, sizeof(char *));
            refresh->data.zone_refresh.mapped_source_fields = calloc(
                field_map_count, sizeof(char *));
            for (size_t j = 0; j < field_map_count; j++) {
                refresh->data.zone_refresh.mapped_target_fields[j] =
                    pergyra_strdup(mapped_target_fields[j]);
                refresh->data.zone_refresh.mapped_source_fields[j] =
                    pergyra_strdup(mapped_source_fields[j]);
            }
        }
        refresh->line = first_line;
        refresh->column = first_column;
        append_child_node(refreshes, refresh_count, refresh);
        free(target_names[i]);
    }

    free(participant_slot_name);
    free(target_names);
    for (size_t i = 0; i < field_map_count; i++) {
        free(mapped_target_fields[i]);
        free(mapped_source_fields[i]);
    }
    free(mapped_target_fields);
    free(mapped_source_fields);
}

ASTNode* parse_relation_declaration(Parser* parser) {
    Token name = consume_name_token(parser, "Expected relation name");
    ASTNode* relation = ast_create_relation_declaration(name.text);
    relation->data.relation_decl.doc_comment = parser_take_pending_doc_comment(parser);
    relation->line = name.line;
    relation->column = name.column;

    /* Parse 'between Left, Right' clause (optional for backward compat) */
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
                "Expected 'subject slot', 'object slot', 'tobject slot', 'refresh', 'publish', 'bind', 'shared', or 'func' in effect body");
            parser_advance(parser);
        }
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after effect body");
    return effect;
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
 *     fields health: Int
 *     func TakeDamage(amount: Int) -> Void
 *     func GetHealth() -> Int { return self.health; }
 * }
 */
ASTNode* parse_ability_declaration(Parser* parser, bool is_innate) {
    Token name = parser_consume(parser, TOKEN_IDENTIFIER, "Expected ability name");
    ASTNode* ability = ast_create_ability_declaration(name.text);
    ability->data.ability_decl.is_innate = is_innate;
    ability->data.ability_decl.doc_comment = parser_take_pending_doc_comment(parser);
    ability->line = name.line;
    ability->column = name.column;
    ability->data.ability_decl.generic_params = parse_generic_params(parser);
    ability->data.ability_decl.where_clause = parse_where_clause(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after ability name");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        parser_collect_doc_comments(parser);
        if (parser_match_identifier_keyword(parser, "fields")) {
            /* fields field_name: Type */
            Token field_name = parser_consume(parser, TOKEN_IDENTIFIER,
                "Expected field name after 'fields'");
            parser_consume(parser, TOKEN_COLON, "Expected ':' after fields field name");
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
            parser_error(parser, "Expected 'fields' or 'func' in ability body");
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
            /* Optional type args */
            inc->data.include_stmt.type_args = parse_type_arguments(parser);

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
            ASTNode *ability_ref = parse_type(parser);
            ASTNode* impl = ast_create_impl_ability(ability_ref);
            impl->line = ability_ref != NULL ? ability_ref->line : name.line;
            impl->column = ability_ref != NULL ? ability_ref->column : name.column;

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

            /* Check if body contains 'super' calls ??simple heuristic */
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

// ?´ë²¤??? ì–¸ ?Œì‹±: event OnClick(sender: Object, args: EventArgs);
