#ifndef PERGYRA_MIR_PROGRAM_H
#define PERGYRA_MIR_PROGRAM_H

/*
 * Program-level MIR aggregate.
 *
 * Routine/CFG representation facts live in mir_types.h and declaration facts
 * live in mir_decl.h. This owner only binds those facts into one lowered
 * program inventory; public operations remain declared by mir.h.
 */

#include <stdbool.h>
#include <stddef.h>

#include "mir_decl.h"
#include "mir_domain_topology.h"
#include "mir_generic_method_specialization.h"
#include "mir_types.h"

typedef struct MIRProgram MIRProgram;

struct MIRProgram
{
    MIRRoutine *routines;
    size_t      routine_count;
    size_t      routine_capacity;
    MIRDeclHeader *decl_headers;
    size_t      decl_header_count;
    size_t      decl_header_capacity;
    bool        has_domain_topology;
    uint64_t    domain_graph_id;
    MIRDomainTopologyRow *domain_topology_rows;
    size_t      domain_topology_row_count;
    MIRParallelCaptureBoundaryFact *parallel_capture_boundaries;
    size_t      parallel_capture_boundary_count;
    /* HIR-projected semantic region rows retained as MIR-owned facts. */
    bool        has_region_escape_facts;
    PgyRegionEscapeFact *region_escape_facts;
    size_t      region_escape_fact_count;
    MIRGenericMethodSpecializationFact *generic_method_specializations;
    size_t      generic_method_specialization_count;
    size_t      generic_method_specialization_capacity;
    ASTNode   **externs;
    size_t      extern_count;
    ASTNode   **types;
    size_t      type_count;
    ASTNode   **abilities;
    size_t      ability_count;
    ASTNode   **roles;
    size_t      role_count;
    ASTNode   **parties;
    size_t      party_count;
    ASTNode   **rosters;
    size_t      roster_count;
    ASTNode   **worlds;
    size_t      world_count;
    ASTNode   **relations;
    size_t      relation_count;
    ASTNode   **effects;
    size_t      effect_count;
    ASTNode   **zones;
    size_t      zone_count;
    ASTNode   **events;
    size_t      event_count;
    ASTNode   **intents;
    size_t      intent_count;
    ASTNode   **functions;
    size_t      function_count;
    bool        has_inventory_surface_usage_facts;
    bool        inventory_uses_thread_pool_surface;
    bool        inventory_uses_intent_observability_surface;
    bool        has_resource_flow_facts;
    bool        has_function_param_flow_facts;
    bool        has_loop_flow_facts;
    bool        has_non_cfg_body_fallback_inventory;
    size_t      non_cfg_body_fallback_total;
    size_t      non_cfg_body_fallback_routine_count;
    bool        has_top_level_exec;
    bool        has_main_function;
    const char *main_function_name;
    const char *source_path;  /* non-owning; NULL disables debug-line output */
};

#endif
