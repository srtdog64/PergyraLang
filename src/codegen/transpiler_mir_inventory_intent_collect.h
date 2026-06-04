/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR intent metadata collectors.
 */

#ifndef PERGYRA_TRANSPILER_MIR_INVENTORY_INTENT_COLLECT_H
#define PERGYRA_TRANSPILER_MIR_INVENTORY_INTENT_COLLECT_H

#include "transpiler.h"

const MIRRoutine *transpiler_find_mir_function(const TranspilerCtx *ctx,
                                               const ASTNode *func_decl);
const MIRRoutine *transpiler_find_mir_intent(const TranspilerCtx *ctx,
                                             const ASTNode *intent_decl);
const char *transpiler_find_mir_intent_meta_arg(
    const MIRRoutine *routine,
    const char *step_name,
    const char *inst_name);
size_t transpiler_collect_mir_intent_step_names(
    const MIRRoutine *routine,
    const char ***names_out);
ASTNode *transpiler_find_mir_intent_check_expr(
    const MIRRoutine *routine,
    const char *step_name,
    const char *phase_name);
size_t transpiler_collect_mir_intent_eval_exprs(
    const MIRRoutine *routine,
    const char *step_name,
    const char *phase_name,
    ASTNode ***exprs_out);
ASTNode *transpiler_find_mir_intent_eval_expr(
    const MIRRoutine *routine,
    const char *step_name,
    const char *phase_name);
size_t transpiler_collect_mir_intent_who_aliases(
    const MIRRoutine *routine,
    const char *step_name,
    const char ***aliases_out);
size_t transpiler_collect_mir_intent_authorized_aliases(
    const MIRRoutine *routine,
    const char *step_name,
    const char ***aliases_out);
size_t transpiler_collect_mir_intent_bindings(
    const MIRRoutine *routine,
    const char ***kinds_out,
    const char ***aliases_out,
    const char ***types_out);
size_t transpiler_collect_mir_intent_dispatch_aliases(
    const MIRRoutine *routine,
    const char *step_name,
    const char ***aliases_out);

#endif /* PERGYRA_TRANSPILER_MIR_INVENTORY_INTENT_COLLECT_H */
