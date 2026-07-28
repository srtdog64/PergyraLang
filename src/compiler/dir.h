#ifndef PERGYRA_DIR_H
#define PERGYRA_DIR_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "../parser/ast.h"
#include "../semantic/domain_runtime_fact.h"
#include "../semantic/resource_flow_fact.h"

typedef struct DIRProgram DIRProgram;
typedef struct HIRProgram HIRProgram;

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
    /* Stable identity of the declaration represented by this node. */
    uint32_t    source_syntax_id;
    /* Stable identity of the owning domain declaration for qualified slots. */
    uint32_t    owner_source_syntax_id;
} DIRNode;

typedef struct
{
    DIREdgeKind kind;
    size_t      from_node_id;
    size_t      to_node_id;
    const char *label;
    const char *target_name;
} DIREdge;

/* DIR owns domain runtime topology.  Later IRs may carry these rows, but
 * must not recover them from declaration text or backend AST inventories. */
typedef enum
{
    DIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH,
    DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH,
    DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND,
    DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT,
    DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT,
    DIR_DOMAIN_TOPOLOGY_LINK_RELATION
} DIRDomainTopologyKind;

typedef struct
{
    size_t                owner_node_id;
    uint32_t              owner_source_syntax_id;
    uint32_t              source_syntax_id;
    DIRDomainTopologyKind kind;
    const char           *projection_slot_name;
    uint32_t              projection_slot_source_syntax_id;
    const char           *source_slot_name;
    uint32_t              source_slot_source_syntax_id;
    const char           *layer_slot_name;
    uint32_t              layer_slot_source_syntax_id;
    const char           *target_slot_name;
    uint32_t              target_slot_source_syntax_id;
    const char           *left_slot_name;
    uint32_t              left_slot_source_syntax_id;
    const char           *right_slot_name;
    uint32_t              right_slot_source_syntax_id;
    const char           *participant_slot_name;
    uint32_t              participant_slot_source_syntax_id;
} DIRDomainTopologyRow;

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
    bool        who_inherited_from_action;
    bool        who_derived_from_on_receiver;
    bool        who_derived_from_single_participant;
    bool        where_inherited_from_intent;
    bool        where_inherited_from_action;
    bool        where_derived_from_using;
    bool        where_derived_from_transfer;
    bool        requires_inherited_from_action;
    bool        causes_inherited_from_action;
    /* Compatibility field for older DIR/AIR schema consumers. Active beta
       semantics keep approval explicit or action-inherited. */
    bool        authorized_by_derived_from_zone;
    bool        authorized_by_inherited_from_action;
    bool        using_derived_from_transfer;
    bool        using_derived_from_where;
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
    const char *outcome_binding_name;
    const char *outcome_binding_type_name;
    uint32_t    outcome_action_decl_syntax_id;
    size_t      on_expr_count;
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
    /* Domain graph anchor: all DIR nodes/edges are a projection of this
     * source program identity, not a name-only reconstruction. */
    uint32_t source_program_syntax_id;
    uint64_t domain_graph_id;
    DIRNode       *nodes;
    size_t         node_count;
    size_t         node_capacity;
    DIREdge       *edges;
    size_t         edge_count;
    size_t         edge_capacity;
    DIRDomainTopologyRow *domain_topology_rows;
    size_t                domain_topology_row_count;
    size_t                domain_topology_row_capacity;
    DIRIntentInfo *intents;
    size_t         intent_count;
    size_t         intent_capacity;
    /* Owned semantic ResourceFlowUniverse snapshot. */
    PgyResourceFlowFact *resource_flow_facts;
    size_t               resource_flow_fact_count;
    bool                 has_resource_flow_facts;
    /* Owned copy of the HIR-admitted domain-runtime semantic snapshot. */
    bool                 has_domain_runtime_facts;
    PgyDomainParticipantRoleFact *domain_participant_role_facts;
    size_t            domain_participant_role_fact_count;
    PgyDomainProjectionMemberAssignmentFact
                     *domain_projection_member_assignment_facts;
    size_t            domain_projection_member_assignment_fact_count;
    char         **owned_names;
    size_t         owned_name_count;
    size_t         owned_name_capacity;
    /* Lowering-owned diagnostic storage; transferred out through dir_lower(). */
    char          *error_message;
};

DIRProgram *dir_lower(ASTNode *annotated_ast, char **error_message);
DIRProgram *dir_lower_with_resource_flow_facts(
        ASTNode *annotated_ast,
        const PgyResourceFlowFact *facts,
        size_t fact_count,
        char **error_message);
/* Production lowering consumes the HIR-owned snapshot.  The legacy fact
 * entry point remains available for isolated DIR fixtures, but the compiler
 * driver must not read SemanticResult resource rows a second time. */
DIRProgram *dir_lower_with_hir_resource_flow_facts(
        ASTNode *annotated_ast,
        const HIRProgram *hir,
        char **error_message);
bool        dir_validate(const DIRProgram *dir, char **error_message);
void        dir_destroy(DIRProgram *dir);
void        dir_dump(const DIRProgram *dir, FILE *out);
const char *dir_node_kind_name(DIRNodeKind kind);
const char *dir_edge_kind_name(DIREdgeKind kind);
const char *dir_domain_topology_kind_name(DIRDomainTopologyKind kind);

#endif
