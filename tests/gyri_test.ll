; ModuleID = 'pergyra_module'
source_filename = "pergyra_module"

%PgySlot_Int = type { i32, i1 }
%PgyTaskHandle = type { ptr }
%PgySlot_Long = type { i64, i1 }
%PgySlot_Float = type { float, i1 }
%PgySlot_Double = type { double, i1 }
%PgySlot_Bool = type { i1, i1 }
%PgySlot_String = type { ptr, i1 }
%PgyArray_Int = type { ptr, i64, i64, ptr }
%PgyArray_Long = type { ptr, i64, i64, ptr }
%PgyArray_Float = type { ptr, i64, i64, ptr }
%PgyArray_Double = type { ptr, i64, i64, ptr }
%PgyArray_Bool = type { ptr, i64, i64, ptr }
%PgyArray_String = type { ptr, i64, i64, ptr }
%PgySecureSlot_Int = type { i32, i1, i64 }
%PgySecureSlot_Long = type { i64, i1, i64 }
%PgySecureSlot_Float = type { float, i1, i64 }
%PgySecureSlot_Double = type { double, i1, i64 }
%PgySecureSlot_Bool = type { i1, i1, i64 }
%PgySecureSlot_String = type { ptr, i1, i64 }

@llvm.used = appending constant [1 x ptr] [ptr @main], section "llvm.metadata"
@llvm.compiler.used = appending constant [1 x ptr] [ptr @main], section "llvm.metadata"

declare void @pgy_log_int(i32)

declare void @pgy_log_long(i64)

declare void @pgy_log_float(float)

declare void @pgy_log_double(double)

declare void @pgy_log_bool(i1)

declare void @pgy_log_string(ptr)

declare void @pgy_log_banner(ptr)

declare i32 @printf(ptr, ...)

declare i1 @StringContains(ptr, ptr)

declare ptr @StringReplace(ptr, ptr, ptr)

declare ptr @Substring(ptr, i32, i32)

declare ptr @StringTrim(ptr)

declare ptr @ToUpper(ptr)

declare ptr @ToLower(ptr)

declare ptr @StringConcat(ptr, ptr)

declare i1 @pgy_string_equals(ptr, ptr)

declare i32 @ToInt(ptr)

declare float @ToFloat(ptr)

declare i32 @Random(i32)

declare ptr @pgy_int_to_string(i32)

declare i32 @pgy_intent_enter_export(ptr, ptr, i32, i1, i32)

declare void @pgy_intent_exit_export(i32)

declare void @pgy_intent_trace_step_export(i32, ptr, ptr)

declare void @pgy_intent_trace_bind_export(i32, ptr, ptr)

declare void @pgy_intent_trace_step_ok_export(i32, ptr)

declare void @pgy_intent_trace_fail_export(i32, ptr)

declare ptr @pgy_intent_last_trace_export()

declare ptr @pgy_intent_last_failure_export()

declare ptr @pgy_intent_last_name_export()

declare i32 @pgy_intent_last_handle_export()

declare i32 @pgy_intent_last_trace_id_export()

declare i32 @pgy_intent_last_step_count_export()

declare i1 @pgy_intent_last_failed_export()

declare i32 @pgy_intent_history_count_export()

declare ptr @pgy_intent_history_step_name_export(i32)

declare ptr @pgy_intent_history_step_zone_export(i32)

declare ptr @pgy_intent_history_step_phase_export(i32)

declare ptr @pgy_intent_history_step_actor_export(i32)

declare ptr @pgy_intent_history_step_slot_export(i32)

declare ptr @pgy_intent_history_step_from_zone_export(i32)

declare ptr @pgy_intent_history_step_from_slot_export(i32)

declare ptr @pgy_intent_history_step_to_zone_export(i32)

declare ptr @pgy_intent_history_step_to_slot_export(i32)

declare i1 @pgy_intent_history_step_ok_export(i32)

declare ptr @pgy_intent_history_step_failure_export(i32)

declare i32 @pgy_intent_active_count_export()

declare ptr @pgy_intent_active_name_export(i32)

declare i32 @pgy_intent_active_handle_export(i32)

declare i32 @pgy_intent_active_trace_id_export(i32)

declare i32 @pgy_intent_active_priority_export(i32)

declare i1 @pgy_intent_active_concurrent_export(i32)

declare ptr @pgy_intent_active_trace_export(i32)

declare void @pgy_intent_trace_materialize_export(i32, ptr, ptr, ptr)

declare void @pgy_intent_trace_transfer_export(i32, ptr, ptr, ptr, ptr, ptr)

declare ptr @pgy_read_file(ptr)

declare void @pgy_write_file(ptr, ptr)

declare ptr @pgy_input(ptr)

declare void @SeedRandom(i32)

declare i32 @pgy_file_open(ptr, ptr)

declare ptr @pgy_file_read(i32)

declare void @pgy_file_write(i32, ptr)

declare void @pgy_file_close(i32)

declare i32 @ClaimQubit()

declare i32 @Measure(i32)

declare void @Entangle(i32, i32)

declare i32 @QubitState(i32)

declare i1 @IsCollapsed(i32)

declare void @ReleaseQubit(i32)

declare void @H(i32)

declare i1 @IntoClassical(i32)

declare %PgySlot_Int @pgy_claim_Int()

declare void @pgy_write_Int(ptr, i32)

declare i32 @pgy_read_Int(ptr)

declare void @pgy_release_Int(ptr)

declare %PgySlot_Int @pgy_claim_device_Int()

declare void @pgy_device_write_Int(ptr, i32)

declare i32 @pgy_device_read_Int(ptr)

declare void @pgy_release_device_Int(ptr)

declare %PgyTaskHandle @pgy_submit_device_read_Int(ptr)

declare %PgySlot_Long @pgy_claim_Long()

declare void @pgy_write_Long(ptr, i64)

declare i64 @pgy_read_Long(ptr)

declare void @pgy_release_Long(ptr)

declare %PgySlot_Long @pgy_claim_device_Long()

declare void @pgy_device_write_Long(ptr, i64)

declare i64 @pgy_device_read_Long(ptr)

declare void @pgy_release_device_Long(ptr)

declare %PgyTaskHandle @pgy_submit_device_read_Long(ptr)

declare %PgySlot_Float @pgy_claim_Float()

declare void @pgy_write_Float(ptr, float)

declare float @pgy_read_Float(ptr)

declare void @pgy_release_Float(ptr)

declare %PgySlot_Float @pgy_claim_device_Float()

declare void @pgy_device_write_Float(ptr, float)

declare float @pgy_device_read_Float(ptr)

declare void @pgy_release_device_Float(ptr)

declare %PgyTaskHandle @pgy_submit_device_read_Float(ptr)

declare %PgySlot_Double @pgy_claim_Double()

declare void @pgy_write_Double(ptr, double)

declare double @pgy_read_Double(ptr)

declare void @pgy_release_Double(ptr)

declare %PgySlot_Double @pgy_claim_device_Double()

declare void @pgy_device_write_Double(ptr, double)

declare double @pgy_device_read_Double(ptr)

declare void @pgy_release_device_Double(ptr)

declare %PgyTaskHandle @pgy_submit_device_read_Double(ptr)

declare %PgySlot_Bool @pgy_claim_Bool()

declare void @pgy_write_Bool(ptr, i1)

declare i1 @pgy_read_Bool(ptr)

declare void @pgy_release_Bool(ptr)

declare %PgySlot_Bool @pgy_claim_device_Bool()

declare void @pgy_device_write_Bool(ptr, i1)

declare i1 @pgy_device_read_Bool(ptr)

declare void @pgy_release_device_Bool(ptr)

declare %PgyTaskHandle @pgy_submit_device_read_Bool(ptr)

declare %PgySlot_String @pgy_claim_String()

declare void @pgy_write_String(ptr, ptr)

declare ptr @pgy_read_String(ptr)

declare void @pgy_release_String(ptr)

declare %PgySlot_String @pgy_claim_device_String()

declare void @pgy_device_write_String(ptr, ptr)

declare ptr @pgy_device_read_String(ptr)

declare void @pgy_release_device_String(ptr)

declare %PgyTaskHandle @pgy_submit_device_read_String(ptr)

declare %PgyArray_Int @pgy_array_new_Int(i64)

declare void @pgy_array_push_Int(ptr, i32)

declare %PgyArray_Long @pgy_array_new_Long(i64)

declare void @pgy_array_push_Long(ptr, i64)

declare %PgyArray_Float @pgy_array_new_Float(i64)

declare void @pgy_array_push_Float(ptr, float)

declare %PgyArray_Double @pgy_array_new_Double(i64)

declare void @pgy_array_push_Double(ptr, double)

declare %PgyArray_Bool @pgy_array_new_Bool(i64)

declare void @pgy_array_push_Bool(ptr, i1)

declare %PgyArray_String @pgy_array_new_String(i64)

declare void @pgy_array_push_String(ptr, ptr)

declare void @pgy_pool_init_export(i64)

declare void @pgy_pool_shutdown_export()

declare %PgyTaskHandle @pgy_spawn_export(ptr, ptr)

declare %PgyTaskHandle @pgy_async_spawn_export(ptr, ptr)

declare %PgyTaskHandle @pgy_spawn_blocking_export(ptr, ptr)

declare void @pgy_async_detach_export(%PgyTaskHandle)

declare ptr @pgy_await_export(%PgyTaskHandle)

declare i1 @pgy_task_cancel_export(%PgyTaskHandle)

declare i1 @pgy_task_is_cancelled_export()

declare ptr @malloc(i64)

declare void @free(ptr)

declare void @pgy_list_new_raw_export(ptr, i64)

declare void @pgy_queue_new_raw_export(ptr, i64)

declare void @pgy_map_new_raw_export(ptr, i64)

declare void @pgy_list_push_raw_export(ptr, ptr, i64)

declare void @pgy_queue_push_raw_export(ptr, ptr, i64)

declare void @pgy_list_get_raw_export(ptr, i32, ptr, i64)

declare void @pgy_list_set_raw_export(ptr, i32, ptr, i64)

declare i32 @pgy_list_size_raw_export(ptr)

declare i32 @pgy_queue_size_raw_export(ptr)

declare i32 @pgy_map_size_raw_export(ptr)

declare i1 @pgy_queue_empty_raw_export(ptr)

declare void @pgy_list_remove_raw_export(ptr, i32, i64)

declare void @pgy_queue_pop_raw_export(ptr, ptr, i64)

declare void @pgy_map_set_raw_export(ptr, ptr, ptr, i64)

declare void @pgy_map_get_raw_export(ptr, ptr, ptr, i64)

declare i1 @pgy_map_has_raw_export(ptr, ptr)

declare void @pgy_map_remove_raw_export(ptr, ptr, i64)

declare void @pgy_channel_init_Int(ptr, i64)

declare i1 @pgy_channel_send_Int(ptr, i32)

declare i1 @pgy_channel_try_send_Int(ptr, i32)

declare i1 @pgy_channel_send_timeout_Int(ptr, i32, i64)

declare i32 @pgy_channel_recv_val_Int(ptr)

declare i1 @pgy_channel_try_recv_Int(ptr, ptr)

declare i1 @pgy_channel_recv_timeout_Int(ptr, ptr, i64)

declare i1 @pgy_channel_ready_Int(ptr)

declare i32 @pgy_channel_length_Int(ptr)

declare i32 @pgy_channel_capacity_Int(ptr)

declare i32 @pgy_channel_space_Int(ptr)

declare i1 @pgy_channel_full_Int(ptr)

declare i1 @pgy_channel_closed_Int(ptr)

declare void @pgy_channel_close_Int(ptr)

declare void @pgy_channel_destroy_Int(ptr)

declare void @pgy_channel_init_String(ptr, i64)

declare i1 @pgy_channel_send_String(ptr, ptr)

declare i1 @pgy_channel_try_send_String(ptr, ptr)

declare i1 @pgy_channel_send_timeout_String(ptr, ptr, i64)

declare ptr @pgy_channel_recv_val_String(ptr)

declare i1 @pgy_channel_try_recv_String(ptr, ptr)

declare i1 @pgy_channel_recv_timeout_String(ptr, ptr, i64)

declare i1 @pgy_channel_ready_String(ptr)

declare i32 @pgy_channel_length_String(ptr)

declare i32 @pgy_channel_capacity_String(ptr)

declare i32 @pgy_channel_space_String(ptr)

declare i1 @pgy_channel_full_String(ptr)

declare i1 @pgy_channel_closed_String(ptr)

declare void @pgy_channel_close_String(ptr)

declare void @pgy_channel_destroy_String(ptr)

declare %PgySecureSlot_Int @pgy_claim_secure_Int(ptr)

declare void @pgy_secure_write_Int(ptr, i32, ptr)

declare i32 @pgy_secure_read_Int(ptr, ptr)

declare void @pgy_secure_release_Int(ptr, ptr)

declare %PgySecureSlot_Long @pgy_claim_secure_Long(ptr)

declare void @pgy_secure_write_Long(ptr, i64, ptr)

declare i64 @pgy_secure_read_Long(ptr, ptr)

declare void @pgy_secure_release_Long(ptr, ptr)

declare %PgySecureSlot_Float @pgy_claim_secure_Float(ptr)

declare void @pgy_secure_write_Float(ptr, float, ptr)

declare float @pgy_secure_read_Float(ptr, ptr)

declare void @pgy_secure_release_Float(ptr, ptr)

declare %PgySecureSlot_Double @pgy_claim_secure_Double(ptr)

declare void @pgy_secure_write_Double(ptr, double, ptr)

declare double @pgy_secure_read_Double(ptr, ptr)

declare void @pgy_secure_release_Double(ptr, ptr)

declare %PgySecureSlot_Bool @pgy_claim_secure_Bool(ptr)

declare void @pgy_secure_write_Bool(ptr, i1, ptr)

declare i1 @pgy_secure_read_Bool(ptr, ptr)

declare void @pgy_secure_release_Bool(ptr, ptr)

declare %PgySecureSlot_String @pgy_claim_secure_String(ptr)

declare void @pgy_secure_write_String(ptr, ptr, ptr)

declare ptr @pgy_secure_read_String(ptr, ptr)

declare void @pgy_secure_release_String(ptr, ptr)

define void @Main() {
bb_0:
  ret void
}

define i32 @main() {
entry:
  call void @pgy_pool_init_export(i64 4)
  call void @Main()
  call void @pgy_pool_shutdown_export()
  ret i32 0
}
