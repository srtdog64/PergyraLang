#ifndef PERGYRA_HIR_CFG_H
#define PERGYRA_HIR_CFG_H

#include <stdbool.h>

#include "hir.h"

bool hir_finalize_cfg(HIRRoutine *routine);
bool hir_cfg_reachability_current(const HIRRoutine *routine);
bool hir_cfg_execution_current(const HIRRoutine *routine);
bool hir_cfg_execution_owner_current(const HIRRoutine *routine);
void hir_cfg_execution_release(HIRRoutine *routine);
bool hir_cfg_program_execution_current(const HIRProgram *hir, const ASTNode *expected_program);
void hir_cfg_program_execution_release(HIRProgram *hir);
bool hir_compute_cfg_dominance(HIRRoutine *routine);
bool hir_compute_cfg_dominance_frontier(HIRRoutine *routine);
bool hir_compute_cfg_dom_tree(HIRRoutine *routine);
bool hir_compute_cfg_loops(HIRRoutine *routine);
bool hir_collect_cfg_local_defs(HIRRoutine *routine);
bool hir_compute_cfg_phi_candidates(HIRRoutine *routine);
bool hir_materialize_phi_nodes(HIRRoutine *routine);
void hir_finalize_cfg_summary(HIRRoutine *routine);

#endif
