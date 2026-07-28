#ifndef PERGYRA_DOMAIN_RUNTIME_FACT_H
#define PERGYRA_DOMAIN_RUNTIME_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Semantic-owned facts for the repeated object/tobject -> effect/relation ->
 * zone runtime boundary.  Syntax supplies names; semantic analysis resolves
 * them once into stable declaration/field identities and canonical type names.
 * HIR, DIR, MIR and target-neutral runtime admission are lossless carriers.
 */

typedef enum
{
    PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER,
    PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE,
    PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
} PgyDomainParticipantRole;

typedef struct
{
    uint32_t                 program_syntax_id;
    uint32_t                 owner_syntax_id;
    PgyDomainParticipantRole role;
    uint32_t                 field_syntax_id;
    char                    *owner_name;
    char                    *field_name;
    char                    *field_type_name;
} PgyDomainParticipantRoleFact;

typedef enum
{
    PGY_DOMAIN_PROJECTION_REFRESH,
    PGY_DOMAIN_PROJECTION_PUBLISH,
    PGY_DOMAIN_PROJECTION_BIND
} PgyDomainProjectionOperation;

typedef struct
{
    uint32_t field_syntax_id;
    char    *field_name;
    char    *field_type_name;
} PgyDomainProjectionPathSegmentFact;

typedef struct
{
    uint32_t                     program_syntax_id;
    uint32_t                     owner_syntax_id;
    uint32_t                     directive_syntax_id;
    PgyDomainProjectionOperation operation;
    uint32_t                     projection_slot_syntax_id;
    uint32_t                     source_slot_syntax_id;
    uint32_t                     target_decl_syntax_id;
    uint32_t                     target_field_syntax_id;
    uint32_t                     source_decl_syntax_id;
    bool                         explicit_map;
    char                        *owner_name;
    char                        *projection_slot_name;
    char                        *source_slot_name;
    char                        *target_field_name;
    char                        *target_field_type_name;
    char                        *source_path;
    char                        *source_leaf_type_name;
    PgyDomainProjectionPathSegmentFact *source_path_segments;
    size_t                       source_path_segment_count;
} PgyDomainProjectionMemberAssignmentFact;

void pgy_domain_participant_role_facts_destroy(
    PgyDomainParticipantRoleFact *facts,
    size_t fact_count);

void pgy_domain_projection_member_assignment_facts_destroy(
    PgyDomainProjectionMemberAssignmentFact *facts,
    size_t fact_count);

void pgy_domain_projection_path_segments_destroy(
    PgyDomainProjectionPathSegmentFact *segments,
    size_t segment_count);

#endif /* PERGYRA_DOMAIN_RUNTIME_FACT_H */
