#ifndef PGY_RUNTIME_LIB_SET_INTENT_TRACE_EXPORTS_H
#define PGY_RUNTIME_LIB_SET_INTENT_TRACE_EXPORTS_H

#include <stdbool.h>
#include <stdint.h>

int32_t pgy_intent_current_handle_export(void);
int32_t pgy_intent_enter_export(char *name,
                                void **subjects,
                                int32_t subject_count,
                                bool is_concurrent,
                                int32_t priority);
void pgy_intent_trace_step_export(int32_t handle,
                                  char *step_name,
                                  char *zone_name);
void pgy_intent_trace_bind_export(int32_t handle,
                                  char *participant_name,
                                  char *slot_name);
void pgy_intent_trace_materialize_export(int32_t handle,
                                         char *participant_name,
                                         char *slot_name,
                                         char *zone_name);
void pgy_intent_trace_transfer_export(int32_t handle,
                                      char *participant_name,
                                      char *from_zone_name,
                                      char *from_slot_name,
                                      char *to_zone_name,
                                      char *to_slot_name);
void pgy_intent_trace_step_ok_export(int32_t handle, char *step_name);
void pgy_intent_trace_fail_export(int32_t handle, char *reason);
void pgy_mir_resource_op_export(int32_t handle,
                                const char *op_name,
                                const char *slot_anchor,
                                const char *arg_name);
void pgy_mir_cleanup_op_export(int32_t handle,
                               const char *op_name,
                               const char *slot_anchor,
                               const char *arg_name);

#endif /* PGY_RUNTIME_LIB_SET_INTENT_TRACE_EXPORTS_H */
