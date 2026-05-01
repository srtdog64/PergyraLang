#ifndef PERGYRA_HIR_CFG_INTERNAL_H
#define PERGYRA_HIR_CFG_INTERNAL_H

#include "hir_cfg.h"

bool hir_cfg_append_name_unique(const char ***names,
                                size_t *count,
                                size_t *capacity,
                                const char *name);

#endif
