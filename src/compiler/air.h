#ifndef PERGYRA_AIR_H
#define PERGYRA_AIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast.h"
#include "dir.h"
#include "hir.h"
#include "mir.h"
#include "rir.h"

typedef struct SemanticResult SemanticResult;

typedef enum
{
    AIR_SYNC_UNKNOWN,
    AIR_SYNC_SYNC,
    AIR_SYNC_ASYNC,
    AIR_SYNC_EITHER
} AIRSyncClass;

typedef enum
{
    AIR_FAILURE_UNKNOWN,
    AIR_FAILURE_RECOVERABLE,
    AIR_FAILURE_FATAL,
    AIR_FAILURE_COMPENSABLE
} AIRFailureClass;

typedef enum
{
    AIR_BOUNDARY_UNKNOWN,
    AIR_BOUNDARY_ZONE,
    AIR_BOUNDARY_WORLD,
    AIR_BOUNDARY_PARALLEL,
    AIR_BOUNDARY_IO,
    AIR_BOUNDARY_CHANNEL,
    AIR_BOUNDARY_EXECUTION
} AIRBoundaryKind;

typedef enum
{
    AIR_DRIFT_NONE,
    AIR_DRIFT_SYNC_ASYNC_CONFLICT,
    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
    AIR_DRIFT_EFFECT_PROPAGATION_MISSING,
    AIR_DRIFT_RELATION_PROPAGATION_MISSING,
    AIR_DRIFT_DAG_FALLBACK_PRESENT
} AIRDriftKind;

typedef enum
{
    AIR_EVIDENCE_HIR_ROUTINE,
    AIR_EVIDENCE_HIR_CFG,
    AIR_EVIDENCE_RIR_BOUNDARY,
    AIR_EVIDENCE_RIR_AUTHORITY,
    AIR_EVIDENCE_MIR_CLEANUP,
    AIR_EVIDENCE_MIR_PIN_CLEANUP,
    AIR_EVIDENCE_DAG_METADATA,
    AIR_EVIDENCE_DAG_GENERIC,
    AIR_EVIDENCE_DAG_ABILITY,
    AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
    AIR_EVIDENCE_RIR_RELATION_PROPAGATION
} AIREvidenceKind;

typedef struct
{
    const char      *intent_owner;
    const char      *step_name;
    size_t           step_index;
    ASTNode         *ast;
    AIRSyncClass     sync_class;
    AIRFailureClass  failure_class;
    const char      *compensation_hook;
    bool             who_from_intent_default;
} AIRIntentNode;

typedef struct
{
    AIRBoundaryKind kind;
    const char     *owner_name;
    const char     *source_name;
    size_t          intent_index;
    size_t          step_index;
    ASTNode        *ast;
    AIRSyncClass    sync_class;
    bool            authority_required;
    bool            source_from_intent_default;
    bool            source_from_transfer;
    bool            authority_from_zone;
    const char    **authority_names;
    size_t          authority_name_count;
    bool            has_hir_routine_evidence;
    bool            has_hir_cfg_evidence;
    bool            has_rir_boundary_evidence;
    bool            has_rir_authority_evidence;
    const char     *hir_routine_evidence_name;
    const char     *rir_boundary_evidence_scope;
    const char     *rir_authority_evidence_name;
} AIRBoundaryNode;

typedef struct
{
    AIRDriftKind kind;
    size_t       intent_index;
    size_t       boundary_index;
    const char  *message;
} AIRDrift;

typedef struct
{
    AIREvidenceKind kind;
    size_t          boundary_index;
    const char     *provider_name;
    const char     *subject_name;
    size_t          fact_count;
    size_t          fallback_count;
} AIREvidenceNode;

typedef struct AIRProgram
{
    AIRIntentNode   *intents;
    size_t           intent_count;
    AIRBoundaryNode *boundaries;
    size_t           boundary_count;
    AIRDrift        *drifts;
    size_t           drift_count;
    size_t           drift_capacity;
    AIREvidenceNode *evidence_nodes;
    size_t           evidence_count;
    size_t           evidence_capacity;
    bool             strict_evidence;
    bool             has_hir_input;
    bool             has_rir_input;
    bool             has_mir_input;
    size_t           hir_routine_evidence_count;
    size_t           hir_cfg_evidence_count;
    size_t           rir_boundary_evidence_count;
    size_t           rir_authority_evidence_count;
    size_t           mir_cleanup_evidence_count;
    size_t           mir_pin_cleanup_evidence_count;
    size_t           dag_metadata_evidence_count;
    size_t           dag_generic_evidence_count;
    size_t           dag_ability_evidence_count;
    size_t           rir_effect_propagation_required_count;
    size_t           rir_effect_propagation_evidence_count;
    size_t           rir_relation_propagation_required_count;
    size_t           rir_relation_propagation_evidence_count;
    char           **owned_names;
    size_t           owned_name_count;
    size_t           owned_name_capacity;
} AIRProgram;

AIRProgram *air_synthesize(const HIRProgram *hir,
                           const DIRProgram *dir,
                           const RIRProgram *rir,
                           char **error_message);
bool        air_validate(const AIRProgram *air, char **error_message);
bool        air_verify(AIRProgram *air, char **error_message);
bool        air_check_drift(AIRProgram *air, char **error_message);
bool        air_boundary_requires_hir_evidence(const AIRBoundaryNode *boundary);
bool        air_boundary_requires_rir_evidence(const AIRBoundaryNode *boundary);
bool        air_boundary_has_evidence(const AIRProgram *air,
                                      size_t boundary_index,
                                      AIREvidenceKind kind);
bool        air_collect_mir_evidence(AIRProgram *air,
                                     const MIRProgram *mir,
                                     char **error_message);
bool        air_collect_dag_evidence(AIRProgram *air,
                                     const SemanticResult *sem,
                                     char **error_message);
void        air_destroy(AIRProgram *air);
void        air_dump(const AIRProgram *air, FILE *out);
void        air_dump_json(const AIRProgram *air, FILE *out);

const char *air_sync_class_name(AIRSyncClass sync_class);
const char *air_failure_class_name(AIRFailureClass failure_class);
const char *air_boundary_kind_name(AIRBoundaryKind kind);
const char *air_drift_kind_name(AIRDriftKind kind);
const char *air_evidence_kind_name(AIREvidenceKind kind);

#endif
