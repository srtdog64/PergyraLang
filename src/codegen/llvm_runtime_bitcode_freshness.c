#ifdef PGY_LLVM_ENABLED

#include "llvm_runtime_bitcode_freshness.h"

#include <stddef.h>
#include <time.h>
#ifdef _WIN32
#include <sys/stat.h>
#define PGY_LLVM_STAT _stat
#define PGY_LLVM_STAT_STRUCT struct _stat
#else
#include <sys/stat.h>
#define PGY_LLVM_STAT stat
#define PGY_LLVM_STAT_STRUCT struct stat
#endif

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
#endif

static bool
llvm_runtime_file_mtime(const char *path, time_t *mtime_out)
{
    PGY_LLVM_STAT_STRUCT st;

    if (path == NULL || mtime_out == NULL)
        return false;
    if (PGY_LLVM_STAT(path, &st) != 0)
        return false;
    *mtime_out = st.st_mtime;
    return true;
}

bool
llvm_runtime_bitcode_is_fresh(const char *bc_path)
{
    time_t bc_mtime;
    const char *deps[] = {
        PGY_RUNTIME_LIB_C,
        PGY_RUNTIME_DIR "/pgy_runtime.h",
        PGY_RUNTIME_DIR "/pgy_runtime_inline_core.h",
        PGY_RUNTIME_DIR "/pgy_runtime_panic_contract.h",
        PGY_RUNTIME_DIR "/pgy_runtime_panic_checked_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_zone_result_option_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_authority_file_core.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_array_exports.h",
        NULL
    };

    if (!llvm_runtime_file_mtime(bc_path, &bc_mtime))
        return false;
    for (size_t i = 0; deps[i] != NULL; i++) {
        time_t dep_mtime;
        if (!llvm_runtime_file_mtime(deps[i], &dep_mtime))
            return false;
        if (bc_mtime < dep_mtime)
            return false;
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
