#ifndef PERGYRA_AIR_INTERNAL_H
#define PERGYRA_AIR_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#include "air.h"

typedef enum
{
    AIR_EVIDENCE_SCOPE_UNKNOWN,
    AIR_EVIDENCE_SCOPE_BOUNDARY,
    AIR_EVIDENCE_SCOPE_GLOBAL
} AIREvidenceKindScope;

char       *air_vformat_owned(const char *fmt, va_list args);
char       *air_format_owned(const char *fmt, ...);
void        air_set_error(char **error_message, const char *fmt, ...);
void        air_set_invariant_error(char **error_message, const char *fmt, ...);
bool        air_next_capacity(size_t *capacity, size_t initial, size_t elem_size);
bool        air_name_is_empty(const char *name);
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
bool        air_append_drift(AIRProgram *air,
                             AIRDriftKind kind,
                             size_t intent_index,
                             size_t boundary_index,
                             const char *message,
                             char **error_message);
bool        air_append_driftf(AIRProgram *air,
                              AIRDriftKind kind,
                              size_t intent_index,
                              size_t boundary_index,
                              char **error_message,
                              const char *fmt,
                              ...);
bool        air_name_matches(const char *a, const char *b);
bool        air_ast_contains_node(const ASTNode *container,
                                  const ASTNode *needle);
bool        air_step_has_zone_boundary(const DIRIntentStep *step);
bool        air_step_has_world_boundary(const DIRIntentStep *step);
AIRBoundaryKind air_boundary_kind_from_ast(const ASTNode *node);
AIRSyncClass    air_boundary_sync_from_kind(AIRBoundaryKind kind);
const char     *air_boundary_source_from_ast(const ASTNode *node);
bool        air_intent_storage_valid(const AIRProgram *air);
bool        air_boundary_storage_valid(const AIRProgram *air);
bool        air_drift_storage_valid(const AIRProgram *air);
bool        air_has_hir_input(const AIRProgram *air);
bool        air_has_rir_input(const AIRProgram *air);
bool        air_has_mir_input(const AIRProgram *air);
bool        air_requires_strict_evidence(const AIRProgram *air);
void        air_mark_hir_input(AIRProgram *air);
void        air_mark_rir_input(AIRProgram *air);
void        air_mark_mir_input(AIRProgram *air);
AIRBoundaryNode *air_boundary_node_mut_at(AIRProgram *air, size_t index);
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
bool        air_collect_observability_schema_evidence(AIRProgram *air,
                                                      char **error_message);
bool        air_collect_runtime_frontier_policy_evidence(AIRProgram *air,
                                                         char **error_message);
bool        air_mir_cleanup_root_is_valid(const MIRRoutine *routine);
size_t      air_mir_routine_cleanup_fact_count(const MIRRoutine *routine);
size_t      air_mir_routine_terminator_fact_count(const MIRRoutine *routine);
size_t      air_mir_routine_select_receive_fact_count(const MIRRoutine *routine);
size_t      air_mir_routine_unproven_retain_fact_count(const MIRRoutine *routine);
size_t      air_mir_routine_inherent_concurrency_fact_count(
                const MIRRoutine *routine);
size_t      air_mir_routine_slot_capability_retain_fact_count(
                const MIRRoutine *routine);
AIREvidenceKind air_mir_cleanup_evidence_kind(void);
AIREvidenceKind air_mir_terminator_evidence_kind(void);
AIREvidenceKind air_mir_select_receive_evidence_kind(void);
const char *air_mir_routine_provider_name(const MIRRoutine *routine);
bool        air_has_global_evidence_provider_subject(
                const AIRProgram *air,
                AIREvidenceKind kind,
                const char *provider_name,
                const char *subject_name);
const AIREvidenceNode *air_global_evidence_node_provider_subject(
                const AIRProgram *air,
                AIREvidenceKind kind,
                const char *provider_name,
                const char *subject_name);
bool        air_has_global_evidence_provider(
                const AIRProgram *air,
                AIREvidenceKind kind,
                const char *provider_name);
bool        air_collect_mir_pin_block_evidence(AIRProgram *air,
                                               const MIRRoutine *routine,
                                               const MIRBasicBlock *block,
                                               const char *routine_name,
                                               char **error_message);
const char *air_rir_scope_provider_name(const RIRScope *scope);
bool        air_require_rir_scope_provider(const RIRScope *scope,
                                           char **error_message);
bool        air_rir_parallel_op_matches_boundary(
                const RIROp *op,
                const AIRBoundaryNode *boundary);
bool        air_rir_scope_provides_boundary_evidence(
                const RIRScope *scope,
                const AIRBoundaryNode *boundary);
bool        air_collect_rir_scope_propagation_evidence(AIRProgram *air,
                                                       const RIRScope *scope,
                                                       char **error_message);
bool        air_collect_rir_scope_boundary_evidence(AIRProgram *air,
                                                    const RIRScope *scope,
                                                    char **error_message);
bool        air_boundary_has_evidence_kind(const AIRProgram *air,
                                           size_t boundary_index,
                                           AIREvidenceKind kind);
bool        air_boundary_has_evidence_kind_subject(const AIRProgram *air,
                                                   size_t boundary_index,
                                                   AIREvidenceKind kind,
                                                   const char *subject_name);
bool        air_boundary_has_evidence_kind_provider(const AIRProgram *air,
                                                    size_t boundary_index,
                                                    AIREvidenceKind kind,
                                                    const char *provider_name);
const char *air_boundary_missing_authority_evidence(const AIRProgram *air,
                                                    const AIRBoundaryNode *boundary,
                                                    size_t boundary_index);
bool        air_boundary_declares_authority_name(
                const AIRBoundaryNode *boundary,
                const char *authority_name);
bool        air_boundary_authority_storage_valid(
                const AIRBoundaryNode *boundary);
size_t      air_boundary_authority_name_count(
                const AIRBoundaryNode *boundary);
const char *air_boundary_authority_name_at(
                const AIRBoundaryNode *boundary,
                size_t index);
const char *air_boundary_first_authority_name_or(
                const AIRBoundaryNode *boundary,
                const char *fallback);
bool        air_boundary_requires_hir_routine_evidence(
                const AIRBoundaryNode *boundary);
bool        air_boundary_requires_hir_cfg_for_program(
                const AIRProgram *air,
                const AIRBoundaryNode *boundary);
bool        air_boundary_requires_mir_pin_cleanup_evidence(
                const AIRBoundaryNode *boundary);
bool        air_evidence_inventory_is_authoritative(const AIRProgram *air);
bool        air_evidence_kind_is_known(AIREvidenceKind kind);
bool        air_evidence_kind_is_boundary_scoped(AIREvidenceKind kind);
AIREvidenceKindScope air_evidence_kind_scope(AIREvidenceKind kind);
bool        air_evidence_kind_is_global(AIREvidenceKind kind);
bool        air_evidence_kind_has_global_validator(AIREvidenceKind kind);
AIREvidenceProviderKind air_evidence_kind_provider_kind(
                AIREvidenceKind kind);
AIREvidenceSubjectKind air_evidence_kind_subject_kind(
                AIREvidenceKind kind);
bool        air_evidence_inventory_storage_valid(const AIRProgram *air);
size_t      air_evidence_node_count(const AIRProgram *air);
const AIREvidenceNode *air_evidence_node_at(const AIRProgram *air,
                                            size_t index);
AIREvidenceKind air_evidence_node_kind(const AIREvidenceNode *evidence);
AIREvidenceProviderKind air_evidence_node_provider_kind(
                const AIREvidenceNode *evidence);
AIREvidenceSubjectKind air_evidence_node_subject_kind(
                const AIREvidenceNode *evidence);
bool        air_evidence_node_has_declared_kind_facts(
                const AIREvidenceNode *evidence);
size_t      air_evidence_node_boundary_index_or(
                const AIREvidenceNode *evidence,
                size_t fallback);
const char *air_evidence_node_provider_name_or(
                const AIREvidenceNode *evidence,
                const char *fallback);
const char *air_evidence_node_subject_name_or(
                const AIREvidenceNode *evidence,
                const char *fallback);
bool        air_evidence_node_has_boundary_shape(
                const AIREvidenceNode *evidence);
AIRBoundaryKind air_evidence_node_boundary_kind_or(
                const AIREvidenceNode *evidence,
                AIRBoundaryKind fallback);
const char *air_evidence_node_boundary_owner_name_or(
                const AIREvidenceNode *evidence,
                const char *fallback);
const char *air_evidence_node_boundary_source_name_or(
                const AIREvidenceNode *evidence,
                const char *fallback);
size_t      air_evidence_node_fact_count(const AIREvidenceNode *evidence);
size_t      air_evidence_node_fallback_count(const AIREvidenceNode *evidence);
size_t      air_global_evidence_node_count(const AIRProgram *air,
                                           AIREvidenceKind kind);
size_t      air_global_evidence_fact_count(const AIRProgram *air,
                                           AIREvidenceKind kind);
size_t      air_global_evidence_fallback_count(const AIRProgram *air,
                                               AIREvidenceKind kind);
bool        air_global_has_evidence_kind(const AIRProgram *air,
                                         AIREvidenceKind kind);
size_t      air_boundary_evidence_node_count(const AIRProgram *air,
                                             AIREvidenceKind kind);
size_t      air_evidence_summary_count(const AIRProgram *air,
                                        AIREvidenceKind kind);
bool        air_increment_evidence_summary_count(AIRProgram *air,
                                                 AIREvidenceKind kind);
size_t      air_evidence_required_count(const AIRProgram *air,
                                         AIREvidenceKind kind);
bool        air_increment_evidence_required_count(AIRProgram *air,
                                                  AIREvidenceKind kind);
bool        air_boundary_has_summary_flag(const AIRBoundaryNode *boundary,
                                          AIREvidenceKind kind);
bool        air_boundary_mark_summary_flag(AIRBoundaryNode *boundary,
                                           AIREvidenceKind kind);
bool        air_validate_global_evidence_node(const AIREvidenceNode *evidence,
                                              size_t evidence_index,
                                              char **error_message);
bool        air_evidence_node_matches_boundary_shape(const AIRProgram *air,
                                                     size_t evidence_index,
                                                     char **error_message);
bool        air_validate_boundary_summary_shape(const AIRProgram *air,
                                                size_t boundary_index,
                                                char **error_message);
bool        air_validate_boundary_summary_inventory(const AIRProgram *air,
                                                    size_t boundary_index,
                                                    char **error_message);
bool        air_validate_summary_counters(const AIRProgram *air,
                                          char **error_message);
char       *air_format_authority_names_owned(
                const AIRBoundaryNode *boundary);
char       *air_format_boundary_provenance_owned(
                const AIRIntentNode *intent,
                const AIRBoundaryNode *boundary);
bool        air_validate_evidence_inventory(const AIRProgram *air,
                                            char **error_message);
bool        air_verify_global_evidence_requirements(AIRProgram *air,
                                                    char **error_message);

#endif
