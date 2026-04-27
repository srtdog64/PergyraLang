#ifndef PERGYRA_HIR_LOWER_CFG_H
#define PERGYRA_HIR_LOWER_CFG_H

#include <stdbool.h>

#include "hir.h"

bool hir_lower_func_body_cfg(ASTNode *body, HIRRoutine *routine);

#endif
