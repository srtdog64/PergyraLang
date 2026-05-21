#ifdef PGY_RUNTIME_LIB_INTERNAL

#include "pgy_runtime_lib_set_intent_trace_exports.h"

#include <stdio.h>

void
pgy_mir_resource_op_export(int32_t handle,
                           const char *op_name,
                           const char *slot_anchor,
                           const char *arg_name)
{
#ifdef PGY_MIR_TRACE
    fprintf(stderr, "[MIR resource-op] handle=%d op=%s slot=%s arg=%s\n",
            handle,
            op_name      != NULL ? op_name      : "-",
            slot_anchor  != NULL ? slot_anchor  : "-",
            arg_name     != NULL ? arg_name     : "-");
#else
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
#endif
}

void
pgy_mir_cleanup_op_export(int32_t handle,
                          const char *op_name,
                          const char *slot_anchor,
                          const char *arg_name)
{
#ifdef PGY_MIR_TRACE
    fprintf(stderr, "[MIR cleanup-op] handle=%d op=%s slot=%s arg=%s\n",
            handle,
            op_name      != NULL ? op_name      : "-",
            slot_anchor  != NULL ? slot_anchor  : "-",
            arg_name     != NULL ? arg_name     : "-");
#else
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
#endif
}

#endif /* PGY_RUNTIME_LIB_INTERNAL */
