#include "parser_domain_internal.h"

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
        parser_consume(parser, TOKEN_RBRACKET,
            "Expected ']' after projection group slot names");
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
