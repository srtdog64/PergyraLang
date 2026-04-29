#include "type_checker_internal.h"

/*
 * Retired compatibility resolver audit counters.
 *
 * The recursive type-node evaluator has been removed from the beta path. These
 * counters intentionally remain exported so `PGY_TYPE_RES_STATS=1` can keep
 * reporting the retired compatibility surface as 0 calls / 0 body fallbacks.
 */
size_t g_type_resolution_compat_calls = 0;
size_t g_type_resolution_compat_unique_nodes = 0;
size_t g_type_resolution_compat_ast_type_calls = 0;
size_t g_type_resolution_compat_channel_type_calls = 0;
size_t g_type_resolution_compat_future_type_calls = 0;
size_t g_type_resolution_compat_event_handler_type_calls = 0;
size_t g_type_resolution_compat_other_ast_calls = 0;
size_t g_type_resolution_compat_cache_hits = 0;
size_t g_type_resolution_compat_cache_misses = 0;
