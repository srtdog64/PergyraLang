#ifndef PERGYRA_AIR_INTERNAL_H
#define PERGYRA_AIR_INTERNAL_H

#include <stdbool.h>

#include "air.h"

void        air_set_error(char **error_message, const char *fmt, ...);
char       *air_strdup_owned(const char *text);
const char *air_program_owned_name(AIRProgram *air, const char *text);
bool        air_assign_owned_name(AIRProgram *air, const char **slot, const char *text);
bool        air_assign_first_owned_name(AIRProgram *air,
                                        const char **slot,
                                        const char *text,
                                        char **error_message,
                                        const char *what);
bool        air_append_evidence_node(AIRProgram *air,
                                     AIREvidenceKind kind,
                                     size_t boundary_index,
                                     const char *provider_name,
                                     const char *subject_name,
                                     char **error_message);
bool        air_append_evidence_node_ex(AIRProgram *air,
                                        AIREvidenceKind kind,
                                        size_t boundary_index,
                                        const char *provider_name,
                                        const char *subject_name,
                                        size_t fact_count,
                                        size_t fallback_count,
                                        char **error_message);
void        air_clear_drifts(AIRProgram *air);
bool        air_name_matches(const char *a, const char *b);
bool        air_step_has_zone_boundary(const DIRIntentStep *step);
bool        air_step_has_world_boundary(const DIRIntentStep *step);
AIRBoundaryKind air_boundary_kind_from_ast(const ASTNode *node);
AIRSyncClass    air_boundary_sync_from_kind(AIRBoundaryKind kind);
const char     *air_boundary_source_from_ast(const ASTNode *node);
size_t      air_count_step_expr_boundaries(const DIRIntentStep *step);
bool        air_append_step_expr_boundaries(AIRProgram *air,
                                            AIRBoundaryNode *boundaries,
                                            size_t *boundary_index,
                                            size_t intent_index,
                                            const char *owner,
                                            const DIRIntentStep *step);
bool        air_collect_hir_evidence(AIRProgram *air,
                                     const HIRProgram *hir,
                                     char **error_message);
bool        air_collect_rir_evidence(AIRProgram *air,
                                     const RIRProgram *rir,
                                     char **error_message);

#endif
