#ifndef PGY_THREAD_POOL_USAGE_H
#define PGY_THREAD_POOL_USAGE_H

#include <stdbool.h>

#include "../compiler/mir.h"

bool pgy_mir_program_uses_thread_pool(const MIRProgram *mir);

#endif
