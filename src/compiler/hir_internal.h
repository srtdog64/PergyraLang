#ifndef PERGYRA_HIR_INTERNAL_H
#define PERGYRA_HIR_INTERNAL_H

#include "hir.h"

bool hir_append_decl_and_routine(HIRProgram *hir,
                                 HIRTopLevelItem item,
                                 char **error_message);
bool hir_finish_callgraph(HIRProgram *hir, char **error_message);
bool hir_finish_cfg_routine(HIRRoutine *routine);

#endif
