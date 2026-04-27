#ifndef PERGYRA_HIR_CFG_H
#define PERGYRA_HIR_CFG_H

#include <stdbool.h>

#include "hir.h"

bool hir_finalize_cfg(HIRRoutine *routine);
bool hir_compute_cfg_dominance(HIRRoutine *routine);
bool hir_compute_cfg_dominance_frontier(HIRRoutine *routine);
bool hir_compute_cfg_dom_tree(HIRRoutine *routine);
bool hir_compute_cfg_loops(HIRRoutine *routine);
bool hir_collect_cfg_local_defs(HIRRoutine *routine);
bool hir_compute_cfg_phi_candidates(HIRRoutine *routine);
bool hir_materialize_phi_nodes(HIRRoutine *routine);
void hir_finalize_cfg_summary(HIRRoutine *routine);

#endif
