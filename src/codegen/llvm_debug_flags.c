#include "llvm_debug_flags.h"

#include <stdlib.h>

bool
llvm_debug_detail_enabled(void)
{
    return getenv("PGY_DEBUG_LLVM_DETAIL") != NULL;
}

bool
llvm_debug_stage_enabled(void)
{
    return getenv("PGY_DEBUG_LLVM_STAGE") != NULL;
}

bool
llvm_debug_verify_enabled(void)
{
    return getenv("PGY_DEBUG_LLVM_VERIFY") != NULL;
}
