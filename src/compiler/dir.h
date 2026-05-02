#ifndef PERGYRA_DIR_H
#define PERGYRA_DIR_H

#include <stddef.h>
#include <stdio.h>

#include "../parser/ast.h"

typedef struct DIRProgram DIRProgram;

typedef enum
{
    DIR_NODE_TYPE,
    DIR_NODE_ABILITY,
    DIR_NODE_ROLE,
    DIR_NODE_PARTY,
    DIR_NODE_PARTY_SLOT,
    DIR_NODE_SYSTEMIC,
    DIR_NODE_WORLD,
    DIR_NODE_RELATION,
    DIR_NODE_EFFECT,
    DIR_NODE_ZONE,
    DIR_NODE_ZONE_SLOT,
    DIR_NODE_PROJECTION_SLOT,
    DIR_NODE_AUTHORITY_SLOT,
    DIR_NODE_INTENT
} DIRNodeKind;

typedef enum
{
    DIR_EDGE_ROLE_FOR_TYPE,
    DIR_EDGE_ROLE_INCLUDE,
    DIR_EDGE_ROLE_IMPL_ABILITY,
    DIR_EDGE_ROLE_COMPLETES_ABILITY,
    DIR_EDGE_ROLE_MISSING_ABILITY_METHOD,
    DIR_EDGE_PARTY_HAS_SLOT,
    DIR_EDGE_PARTY_SLOT_ABILITY,
    DIR_EDGE_SYSTEMIC_PARTY,
    DIR_EDGE_WORLD_SYSTEMIC,
    DIR_EDGE_WORLD_ZONE,
    DIR_EDGE_ZONE_HAS_SLOT,
    DIR_EDGE_ZONE_SLOT_TYPE,
    DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
    DIR_EDGE_PROJECTION_SLOT_TYPE,
    DIR_EDGE_PROJECTION_SLOT_SOURCE,
    DIR_EDGE_ZONE_HAS_AUTHORITY_SLOT,
    DIR_EDGE_AUTHORITY_SLOT_SUBJECT,
    DIR_EDGE_ZONE_LAYER_TYPE,
    DIR_EDGE_ZONE_AUTHORITY_ABILITY,
    DIR_EDGE_ZONE_STATE_LAYER,
    DIR_EDGE_INTENT_PARTICIPANT_TYPE,
    DIR_EDGE_INTENT_STEP_ZONE,
    DIR_EDGE_INTENT_STEP_WHO,
    DIR_EDGE_INTENT_STEP_REQUIRES,
    DIR_EDGE_INTENT_STEP_AUTHORIZED_BY,
    DIR_EDGE_INTENT_STEP_CAUSES,
    DIR_EDGE_INTENT_STEP_DEPENDS_ON
} DIREdgeKind;

typedef struct
{
    size_t      id;
    DIRNodeKind kind;
    const char *name;
    ASTNode    *ast;
} DIRNode;

typedef struct
{
    DIREdgeKind kind;
    size_t      from_node_id;
    size_t      to_node_id;
    const char *label;
    const char *target_name;
} DIREdge;

typedef struct
{
    const char *alias;
    const char *subject_type_name;
    size_t      subject_type_node_id;
    bool        is_value_binding;
} DIRIntentParticipant;

typedef struct
{
    size_t      index;
    const char *name;
    const char *where_type_name;
    size_t      where_type_node_id;
    const char *using_alias;
    const char *predecessor_step_name;
    size_t      predecessor_step_index;
    const char *transfer_from_alias;
    const char *transfer_to_alias;
    bool        who_inherited_from_intent;
    bool        who_derived_from_on_receiver;
    bool        who_derived_from_single_participant;
    bool        where_inherited_from_intent;
    bool        where_inherited_from_action;
    bool        requires_inherited_from_action;
    bool        causes_inherited_from_action;
    bool        authorized_by_derived_from_zone;
    bool        authorized_by_inherited_from_action;
    const char **who_names;
    size_t      who_count;
    size_t      who_capacity;
    const char **required_abilities;
    size_t      required_ability_count;
    size_t      required_ability_capacity;
    const char **authorized_by;
    size_t      authorized_by_count;
    size_t      authorized_by_capacity;
    const char *causes_effect_name;
    size_t      causes_effect_node_id;
    ASTNode    *ast;
} DIRIntentStep;

typedef struct
{
    size_t                 node_id;
    DIRIntentParticipant  *participants;
    size_t                 participant_count;
    size_t                 participant_capacity;
    DIRIntentStep         *steps;
    size_t                 step_count;
    size_t                 step_capacity;
} DIRIntentInfo;

struct DIRProgram
{
    DIRNode       *nodes;
    size_t         node_count;
    size_t         node_capacity;
    DIREdge       *edges;
    size_t         edge_count;
    size_t         edge_capacity;
    DIRIntentInfo *intents;
    size_t         intent_count;
    size_t         intent_capacity;
    char         **owned_names;
    size_t         owned_name_count;
    size_t         owned_name_capacity;
};

DIRProgram *dir_lower(ASTNode *annotated_ast, char **error_message);
bool        dir_validate(const DIRProgram *dir, char **error_message);
void        dir_destroy(DIRProgram *dir);
void        dir_dump(const DIRProgram *dir, FILE *out);
const char *dir_node_kind_name(DIRNodeKind kind);
const char *dir_edge_kind_name(DIREdgeKind kind);

#endif
