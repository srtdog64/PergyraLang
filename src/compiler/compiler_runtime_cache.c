#include "compiler_toolchain.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <sys/stat.h>
#define PGY_STAT _stat
#define PGY_STAT_STRUCT struct _stat
#else
#include <sys/stat.h>
#define PGY_STAT stat
#define PGY_STAT_STRUCT struct stat
#endif

#include "../common/string_compat.h"

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_SRC_DIR
#define PGY_SRC_DIR "src"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
#endif

#ifdef PGY_LLVM_ENABLED

static const char *
compiler_temp_dir(void)
{
    const char *tmpdir = getenv("TMPDIR");

    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TMP");
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = ".";
#else
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = "/tmp";
#endif
    return tmpdir;
}

static bool
compiler_file_mtime(const char *path, time_t *mtime_out)
{
    PGY_STAT_STRUCT st;

    if (path == NULL || mtime_out == NULL)
        return false;
    if (PGY_STAT(path, &st) != 0)
        return false;
    *mtime_out = st.st_mtime;
    return true;
}

bool
compiler_runtime_cache_is_fresh(const char *cache_obj_path)
{
    time_t cache_mtime;
    const char *deps[] = {
        PGY_RUNTIME_LIB_C,
        PGY_SRC_DIR "/common/string_compat.h",
        PGY_RUNTIME_DIR "/pgy_runtime.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_core_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_std_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_authority_file_core.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_file_path_core.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_collection_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_collection_common_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_queue_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_map_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_map_key_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_set_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_set_raw_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_intent_active_index_exports.c",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_intent_trace_events_exports.c",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_mir_trace_exports.c",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_set_intent_trace_exports.c",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_intent_active_index_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_set_intent_trace_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_intent_slot_core_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_slot_array_io_string_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_secure_slot_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_device_slot_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_raw_array_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_array_map_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_array_set_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_allocator_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_box_array_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_io_string_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_process_args_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_channel_quantum_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_channel_int_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_channel_string_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_channel_string_result_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_qubit_state_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_quantum_exports.h",
        PGY_RUNTIME_DIR "/pgy_parallel.h",
        PGY_RUNTIME_DIR "/pgy_parallel_blocking.h",
        PGY_RUNTIME_DIR "/pgy_parallel_coroutine.h",
        PGY_RUNTIME_DIR "/pgy_parallel_run.h",
        PGY_RUNTIME_DIR "/pgy_runtime_platform_io_core.h",
        PGY_RUNTIME_DIR "/pgy_runtime_process_exit.h",
        PGY_RUNTIME_DIR "/pgy_runtime_inline_core.h",
        PGY_RUNTIME_DIR "/pgy_runtime_intent_active_index_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_intent_trace_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_intent_active_exports.h",
        PGY_RUNTIME_DIR "/pgy_runtime_intent_history.h",
        PGY_RUNTIME_DIR "/pgy_runtime_intent_exit.h",
        PGY_RUNTIME_DIR "/pgy_runtime_panic_checked_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_panic_contract.h",
        PGY_RUNTIME_DIR "/pgy_runtime_memory_array_slot_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_plain_slot_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_slot_macros.h",
        PGY_RUNTIME_DIR "/pgy_runtime_array_sort_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_scalar_std_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_builtin_storage_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_process_args_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_map_int_key_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_map_string_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_list_set_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_queue_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_pool_fsm_timer_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_zone_result_option_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_channel_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_channel_string_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_channel_string_result_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_io_qubit_inline.h",
        /* Inline headers pulled in transitively (e.g. via memory_array_slot ->
         * allocator) but not otherwise tracked. Their edits change the runtime
         * object, so they must invalidate the cache -- omitting them silently
         * links a stale runtime on the LLVM path (a C/LLVM divergence). */
        PGY_RUNTIME_DIR "/pgy_runtime_allocator_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_budget.h",
        PGY_RUNTIME_DIR "/pgy_runtime_capability.h",
        PGY_RUNTIME_DIR "/pgy_runtime_media_stub.h",
        PGY_RUNTIME_DIR "/pgy_runtime_list_generic_inline.h",
        PGY_RUNTIME_DIR "/pgy_runtime_lib_list_raw_exports.h",
        NULL
    };

    if (!compiler_file_mtime(cache_obj_path, &cache_mtime))
        return false;
    for (size_t i = 0; deps[i] != NULL; i++) {
        time_t dep_mtime;

        if (!compiler_file_mtime(deps[i], &dep_mtime))
            return false;
        if (cache_mtime < dep_mtime)
            return false;
    }
    return true;
}

char *
compiler_runtime_prebuilt_object_path(PgyOptProfile opt_profile,
                                      bool uses_intent_observability)
{
    char key[64];
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "RELEASE" : "DEV";
    const char *obs_name = uses_intent_observability ? "OBS1" : "OBS0";
    const char *value;

    snprintf(key, sizeof(key), "PGY_PREBUILT_RUNTIME_OBJ_%s_%s", opt_name, obs_name);
    value = getenv(key);
    if (value == NULL || value[0] == '\0')
        value = getenv("PGY_PREBUILT_RUNTIME_OBJ");
    if (value == NULL || value[0] == '\0')
        return NULL;
    return pergyra_strdup(value);
}

char *
compiler_runtime_cache_object_path(PgyOptProfile opt_profile,
                                   bool uses_intent_observability)
{
    const char *tmpdir = compiler_temp_dir();
    const char *opt_name = (opt_profile == PGY_OPT_RELEASE) ? "release" : "dev";
    const char *obs_name = uses_intent_observability ? "obs1" : "obs0";
    char buf[1024];
#ifdef _WIN32
    const char *ext = ".obj";
#else
    const char *ext = ".o";
#endif

    snprintf(buf, sizeof(buf), "%s/pgy_runtime_cache_%s_%s%s",
             tmpdir, opt_name, obs_name, ext);
    return pergyra_strdup(buf);
}

#endif /* PGY_LLVM_ENABLED */
