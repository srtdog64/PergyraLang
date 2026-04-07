#ifndef PERGYRA_MIR_H
#define PERGYRA_MIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "hir.h"
#include "rir.h"

typedef struct MIRProgram MIRProgram;

typedef enum
{
    MIR_SCOPE_FUNCTION,
    MIR_SCOPE_METHOD,
    MIR_SCOPE_INTENT
} MIRScopeKind;

typedef enum
{
    MIR_INST_DEF,
    MIR_INST_RESOURCE_OP,
    MIR_INST_PHI,
    MIR_INST_BRANCH,
    MIR_INST_RETURN,
    MIR_INST_CLEANUP_EDGE,
    MIR_INST_STMT
} MIRInstKind;

typedef struct
{
    size_t      predecessor_block;
    const char *value_name;
} MIRPhiIncoming;

typedef struct
{
    size_t           id;
    MIRInstKind      kind;
    const char      *name;
    const char      *slot_anchor;
    const char      *arg0;
    const char      *arg1;
    const char      *result_name;
    const char     **uses;
    size_t           use_count;
    MIRPhiIncoming  *phi_incomings;
    size_t           phi_incoming_count;
    const RIROp     *rir_op;
    ASTNode         *ast;
} MIRInstruction;

typedef struct
{
    size_t           id;
    bool             is_entry;
    bool             is_reachable;
    bool             is_cleanup;
    size_t           source_hir_block_id;
    size_t          *predecessors;
    size_t           predecessor_count;
    size_t           succ_true;
    size_t           succ_false;
    bool             has_succ_true;
    bool             has_succ_false;
    size_t           cleanup_succ;
    bool             has_cleanup_succ;
    size_t           rollback_succ;
    bool             has_rollback_succ;
    size_t           invalidation_succ;
    bool             has_invalidation_succ;
    const char     **renamed_locals;
    size_t           renamed_local_count;
    const char     **ssa_entry_values;
    size_t           ssa_entry_value_count;
    const char     **ssa_exit_values;
    size_t           ssa_exit_value_count;
    const char     **use_names;
    size_t           use_name_count;
    const char     **def_names;
    size_t           def_name_count;
    const char     **live_in_names;
    size_t           live_in_name_count;
    const char     **live_out_names;
    size_t           live_out_name_count;
    size_t          *ssa_entry_versions;
    size_t          *ssa_exit_versions;
    size_t           ssa_version_count;
    MIRInstruction  *instructions;
    size_t           instruction_count;
} MIRBasicBlock;

typedef struct
{
    const char *name;
    const char *slot_anchor;
    size_t      def_block;
    size_t      def_inst;
    size_t      use_count;
    size_t      first_use_block;
    size_t      last_use_block;
    size_t      live_in_block_count;
    size_t      live_out_block_count;
    bool        reaches_cleanup;
} MIRValueSummary;

typedef struct
{
    size_t             id;
    MIRScopeKind       kind;
    const char        *owner_name;
    const char        *name;
    const HIRRoutine  *hir_routine;
    const RIRScope    *rir_scope;
    MIRBasicBlock     *blocks;
    size_t             block_count;
    size_t             entry_block;
    size_t             cleanup_block;
    bool               has_cleanup_block;
    size_t             rollback_block;
    bool               has_rollback_block;
    size_t             invalidation_block;
    bool               has_invalidation_block;
    size_t             instruction_count;
    size_t             cleanup_instruction_count;
    size_t             phi_inserted_count;
    size_t             renamed_value_count;
    size_t             cleanup_edge_count;
    size_t             use_edge_count;
    size_t             live_value_count;
    size_t             dce_removed_count;
    bool               has_liveness;
    bool               has_dce;
    MIRValueSummary   *value_summaries;
    size_t             value_summary_count;
    bool               has_use_def_summary;
} MIRRoutine;

struct MIRProgram
{
    MIRRoutine *routines;
    size_t      routine_count;
};

MIRProgram *mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message);
bool        mir_run_liveness_pass(MIRProgram *mir, char **error_message);
bool        mir_run_dce_pass(MIRProgram *mir, char **error_message);
bool        mir_validate(const MIRProgram *mir, char **error_message);
bool        mir_validate_emission_topology(const MIRRoutine *routine,
                                          bool require_cleanup,
                                          bool require_cleanup_source_mapping,
                                          char **error_message);
void        mir_destroy(MIRProgram *mir);
void        mir_dump(const MIRProgram *mir, FILE *out);

const char *mir_scope_kind_name(MIRScopeKind kind);
const char *mir_inst_kind_name(MIRInstKind kind);

#endif
