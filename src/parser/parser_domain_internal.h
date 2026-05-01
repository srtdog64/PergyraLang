#ifndef PERGYRA_PARSER_DOMAIN_INTERNAL_H
#define PERGYRA_PARSER_DOMAIN_INTERNAL_H

#include "parser_internal.h"

typedef enum {
    DOMAIN_GROUP_NONE,
    DOMAIN_GROUP_SUBJECTS,
    DOMAIN_GROUP_OBJECTS,
    DOMAIN_GROUP_TOBJECTS
} DomainSlotGroupKind;

typedef enum {
    DOMAIN_LAYER_GROUP_NONE,
    DOMAIN_LAYER_GROUP_EFFECTS,
    DOMAIN_LAYER_GROUP_RELATIONS
} DomainLayerGroupKind;

bool parser_match_identifier_keyword(Parser *parser, const char *keyword);
bool parser_match_identifier_keyword_on_line(Parser *parser, const char *keyword,
                                             unsigned line);
void append_child_node(ASTNode ***nodes, size_t *count, size_t *capacity,
                       ASTNode *node);
void append_domain_slot(ASTNode ***slots, size_t *slot_count,
                        size_t *slot_capacity, ASTNode *slot);
ASTNode *parse_domain_slot_entry(Parser *parser, const char *owner_name);
char *parse_optional_zone_participant_name(Parser *parser);
DomainSlotGroupKind parser_match_domain_slot_group_kind(Parser *parser);
DomainLayerGroupKind parser_match_domain_layer_group_kind(Parser *parser);
void append_domain_projection_sync_entries(Parser *parser,
                                           ASTNode ***refreshes,
                                           size_t *refresh_count,
                                           size_t *refresh_capacity,
                                           bool allow_participant);

#endif
