#ifndef PGY_TRANSPILER_MIR_SSA_LOCAL_FACTS_H
#define PGY_TRANSPILER_MIR_SSA_LOCAL_FACTS_H

#include <stdbool.h>

#include "transpiler.h"

void transpiler_mir_ssa_local_trim_type_annotation_suffix(char *type_name);

const char *transpiler_mir_ssa_local_find_receive_payload_type_name(
    TranspilerCtx *ctx,
    ASTNode *func_decl,
    const MIRRoutine *routine,
    const char *base_name);

char *transpiler_mir_ssa_local_find_versioned_type_name(
    TranspilerCtx *ctx,
    const ASTNode *func_decl,
    const MIRRoutine *routine,
    const char *versioned_name);

bool transpiler_mir_ssa_local_entry_has_source_def(
    const MIRRoutine *routine,
    const char *base_name);

bool transpiler_mir_ssa_local_routine_has_source_def(
    const MIRRoutine *routine,
    const char *base_name);

bool transpiler_mir_ssa_local_routine_has_param_name(
    const MIRRoutine *routine,
    const char *base_name);

bool transpiler_mir_ssa_local_routine_has_destructure_binding(
    const MIRRoutine *routine,
    const char *base_name);

void transpiler_mir_ssa_local_register_base_type_fact(
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const char *versioned_name,
    const char *base_name,
    const char *type_name);

#endif /* PGY_TRANSPILER_MIR_SSA_LOCAL_FACTS_H */
