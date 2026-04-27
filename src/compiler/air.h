#ifndef PERGYRA_AIR_H
#define PERGYRA_AIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast.h"
#include "dir.h"
#include "hir.h"
#include "rir.h"

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
    AIR_BOUNDARY_CHANNEL
} AIRBoundaryKind;

typedef enum
{
    AIR_DRIFT_NONE,
    AIR_DRIFT_SYNC_ASYNC_CONFLICT,
    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
} AIRDriftKind;

typedef struct
{
    const char      *intent_owner;
    const char      *step_name;
    size_t           step_index;
    ASTNode         *ast;
    AIRSyncClass     sync_class;
    AIRFailureClass  failure_class;
    const char      *compensation_hook;
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
    const char    **authority_names;
    size_t          authority_name_count;
    bool            has_hir_routine_evidence;
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

typedef struct AIRProgram
{
    AIRIntentNode   *intents;
    size_t           intent_count;
    AIRBoundaryNode *boundaries;
    size_t           boundary_count;
    AIRDrift        *drifts;
    size_t           drift_count;
    bool             strict_evidence;
    size_t           hir_routine_evidence_count;
    size_t           rir_boundary_evidence_count;
    size_t           rir_authority_evidence_count;
    char           **owned_names;
    size_t           owned_name_count;
} AIRProgram;

AIRProgram *air_synthesize(const HIRProgram *hir,
                           const DIRProgram *dir,
                           const RIRProgram *rir,
                           char **error_message);
bool        air_validate(const AIRProgram *air, char **error_message);
bool        air_verify(AIRProgram *air, char **error_message);
bool        air_check_drift(AIRProgram *air, char **error_message);
void        air_destroy(AIRProgram *air);
void        air_dump(const AIRProgram *air, FILE *out);

const char *air_sync_class_name(AIRSyncClass sync_class);
const char *air_failure_class_name(AIRFailureClass failure_class);
const char *air_boundary_kind_name(AIRBoundaryKind kind);
const char *air_drift_kind_name(AIRDriftKind kind);

#endif
