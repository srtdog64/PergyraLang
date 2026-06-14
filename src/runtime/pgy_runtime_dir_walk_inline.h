#ifndef PGY_RUNTIME_DIR_WALK_INLINE_H
#define PGY_RUNTIME_DIR_WALK_INLINE_H

#define PGY_RUNTIME_DIR_WALK_PUBLIC static inline
#define PGY_RUNTIME_DIR_WALK_STRDUP pgy_runtime_strdup
#include "pgy_runtime_dir_walk_core.h"
#undef PGY_RUNTIME_DIR_WALK_STRDUP
#undef PGY_RUNTIME_DIR_WALK_PUBLIC

#endif /* PGY_RUNTIME_DIR_WALK_INLINE_H */
