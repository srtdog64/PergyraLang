#ifndef PERGYRA_MIR_DOMAIN_TOPOLOGY_H
#define PERGYRA_MIR_DOMAIN_TOPOLOGY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct DIRProgram DIRProgram;
typedef struct MIRProgram MIRProgram;

typedef enum
{
    MIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH,
    MIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH,
    MIR_DOMAIN_TOPOLOGY_PROJECTION_BIND,
    MIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT,
    MIR_DOMAIN_TOPOLOGY_LINK_RELATION
} MIRDomainTopologyKind;

typedef struct
{
    uint32_t              owner_source_syntax_id;
    uint32_t              source_syntax_id;
    MIRDomainTopologyKind kind;
    char                 *owner_name;
    char                 *projection_slot_name;
    uint32_t              projection_slot_source_syntax_id;
    char                 *source_slot_name;
    uint32_t              source_slot_source_syntax_id;
    char                 *layer_slot_name;
    uint32_t              layer_slot_source_syntax_id;
    char                 *target_slot_name;
    uint32_t              target_slot_source_syntax_id;
    char                 *left_slot_name;
    uint32_t              left_slot_source_syntax_id;
    char                 *right_slot_name;
    uint32_t              right_slot_source_syntax_id;
    char                 *participant_slot_name;
    uint32_t              participant_slot_source_syntax_id;
} MIRDomainTopologyRow;

bool mir_domain_topology_project_from_dir(MIRProgram *mir,
                                           const DIRProgram *dir,
                                           char **error_message);
bool mir_domain_topology_validate(const MIRProgram *mir,
                                  char **error_message);
void mir_domain_topology_clear(MIRProgram *mir);
const char *mir_domain_topology_kind_name(MIRDomainTopologyKind kind);

#endif /* PERGYRA_MIR_DOMAIN_TOPOLOGY_H */
