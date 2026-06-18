#ifndef PERGYRA_ABI_SPEC_ASSERTS_H
#define PERGYRA_ABI_SPEC_ASSERTS_H

/* =================================================================
 * STATIC ASSERTIONS — Slot<T> Debug
 * ================================================================= */

/* Slot<Int> Debug: value@0, occupied after value, size >= 8 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_int_dbg, value) == 0,
                  slot_int_dbg_value_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_int_dbg, occupied) >= 4,
                  slot_int_dbg_occupied_after_value);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_int_dbg) >= 8,
                  slot_int_dbg_min_size_8);

/* Slot<Long> Debug: value@0, size >= 16 (8 + 1 + 7 padding) */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_long_dbg, value) == 0,
                  slot_long_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_long_dbg) >= 16,
                  slot_long_dbg_min_size_16);

/* Slot<Float> Debug: value@0, size >= 8 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_float_dbg, value) == 0,
                  slot_float_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_float_dbg) >= 8,
                  slot_float_dbg_min_size_8);

/* Slot<Double> Debug: value@0, size >= 16 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_double_dbg, value) == 0,
                  slot_double_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_double_dbg) >= 16,
                  slot_double_dbg_min_size_16);

/* Slot<Bool> Debug: size >= 2 */
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_bool_dbg) >= 2,
                  slot_bool_dbg_min_size_2);

/* Slot<String> Debug: value@0, size >= 16 (8 + 1 + 7 padding on LP64) */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_string_dbg, value) == 0,
                  slot_string_dbg_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_string_dbg) >= 16,
                  slot_string_dbg_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — Slot<T> Release
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_int_rel) == 4,
                  slot_int_rel_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_long_rel) == 8,
                  slot_long_rel_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_float_rel) == 4,
                  slot_float_rel_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_double_rel) == 8,
                  slot_double_rel_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_bool_rel) == 1,
                  slot_bool_rel_size_1);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_string_rel) == sizeof(char*),
                  slot_string_rel_size_ptr);

/* =================================================================
 * STATIC ASSERTIONS — SecureSlot<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int_dbg, value) == 0,
                  secure_slot_int_dbg_value_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int_dbg, token) > 4,
                  secure_slot_int_dbg_token_after_value);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int_dbg) > sizeof(pgy_abi_slot_int_dbg),
                  secure_slot_int_dbg_larger_than_slot);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int_dbg) >= 16,
                  secure_slot_int_dbg_min_size_16);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int_rel) == sizeof(pgy_abi_secure_slot_int_dbg),
                  secure_slot_int_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int_rel, token) == offsetof(pgy_abi_secure_slot_int_dbg, token),
                  secure_slot_int_rel_same_token_offset_as_dbg);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_long_rel) == sizeof(pgy_abi_secure_slot_long_dbg),
                  secure_slot_long_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_long_rel, token) == offsetof(pgy_abi_secure_slot_long_dbg, token),
                  secure_slot_long_rel_same_token_offset_as_dbg);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_float_rel) == sizeof(pgy_abi_secure_slot_float_dbg),
                  secure_slot_float_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_float_rel, token) == offsetof(pgy_abi_secure_slot_float_dbg, token),
                  secure_slot_float_rel_same_token_offset_as_dbg);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_double_rel) == sizeof(pgy_abi_secure_slot_double_dbg),
                  secure_slot_double_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_double_rel, token) == offsetof(pgy_abi_secure_slot_double_dbg, token),
                  secure_slot_double_rel_same_token_offset_as_dbg);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_bool_rel) == sizeof(pgy_abi_secure_slot_bool_dbg),
                  secure_slot_bool_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_bool_rel, token) == offsetof(pgy_abi_secure_slot_bool_dbg, token),
                  secure_slot_bool_rel_same_token_offset_as_dbg);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_string_rel) == sizeof(pgy_abi_secure_slot_string_dbg),
                  secure_slot_string_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_string_rel, token) == offsetof(pgy_abi_secure_slot_string_dbg, token),
                  secure_slot_string_rel_same_token_offset_as_dbg);

ABI_STATIC_ASSERT(sizeof(pgy_abi_token_int_dbg) >= 16,
                  token_int_dbg_min_size_16);
ABI_STATIC_ASSERT(sizeof(pgy_abi_token_int_rel) == sizeof(pgy_abi_token_int_dbg),
                  token_int_rel_same_size_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_token_int_rel, can_write) == offsetof(pgy_abi_token_int_dbg, can_write),
                  token_int_rel_can_write_same_offset_as_dbg);
ABI_STATIC_ASSERT(offsetof(pgy_abi_token_int_rel, can_read) == offsetof(pgy_abi_token_int_dbg, can_read),
                  token_int_rel_can_read_same_offset_as_dbg);

ABI_STATIC_ASSERT(offsetof(pgy_abi_pinned_slot_view_int, slot) == 0,
                  pinned_slot_view_int_slot_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_pinned_slot_view_int, active) == sizeof(void*),
                  pinned_slot_view_int_active_after_slot);
ABI_STATIC_ASSERT(sizeof(pgy_abi_pinned_slot_view_int) >= sizeof(void*) + 2,
                  pinned_slot_view_int_min_size);
ABI_STATIC_ASSERT(offsetof(pgy_abi_pinned_secure_slot_view_int, slot) == 0,
                  pinned_secure_slot_view_int_slot_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_pinned_secure_slot_view_int, token) == sizeof(void*),
                  pinned_secure_slot_view_int_token_after_slot);
ABI_STATIC_ASSERT(offsetof(pgy_abi_pinned_secure_slot_view_int, active) >= sizeof(void*) * 2,
                  pinned_secure_slot_view_int_active_after_token);
ABI_STATIC_ASSERT(sizeof(pgy_abi_pinned_secure_slot_view_int) >= sizeof(void*) * 2 + 2,
                  pinned_secure_slot_view_int_min_size);

/* =================================================================
 * STATIC ASSERTIONS — DeviceSlot<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_device_slot_int, value) == 0,
                  device_slot_int_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_device_slot_int) >= 8,
                  device_slot_int_min_size_8);

/* =================================================================
 * STATIC ASSERTIONS — Option<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_int, tag) == 0,
                  option_int_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_int, value) == 4,
                  option_int_value_at_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_int) == 8,
                  option_int_size_8);

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_long, tag) == 0,
                  option_long_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_long, value) == 8,
                  option_long_value_at_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_long) == 16,
                  option_long_size_16);

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_float, tag) == 0,
                  option_float_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_float, value) == 4,
                  option_float_value_at_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_float) == 8,
                  option_float_size_8);

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_double, tag) == 0,
                  option_double_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_double, value) == 8,
                  option_double_value_at_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_double) == 16,
                  option_double_size_16);

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_bool, tag) == 0,
                  option_bool_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_bool, value) == 4,
                  option_bool_value_at_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_bool) == 8,
                  option_bool_size_8);

ABI_STATIC_ASSERT(offsetof(pgy_abi_option_string, tag) == 0,
                  option_string_tag_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_option_string, value) == sizeof(void*),
                  option_string_value_after_pointer_sized_padding);
ABI_STATIC_ASSERT(sizeof(pgy_abi_option_string) == sizeof(void*) * 2,
                  option_string_size_two_words);

/* =================================================================
 * STATIC ASSERTIONS — Result<T, E>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_result_int, tag) == 0,
                  result_int_tag_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_result_int) >= 16,
                  result_int_min_size_16);

ABI_STATIC_ASSERT(offsetof(pgy_abi_result_bool, tag) == 0,
                  result_bool_tag_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_result_bool) >= 16,
                  result_bool_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — ZoneChannel<T> / WorldChannel<T> opaque handles
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_zone_channel_handle) == 4,
                  zone_channel_handle_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_world_channel_handle) == 4,
                  world_channel_handle_size_4);
ABI_STATIC_ASSERT(sizeof(pgy_abi_zone_channel_handle) == sizeof(uint32_t),
                  zone_channel_handle_is_u32);
ABI_STATIC_ASSERT(sizeof(pgy_abi_world_channel_handle) == sizeof(uint32_t),
                  world_channel_handle_is_u32);

/* =================================================================
 * STATIC ASSERTIONS — Box<T>
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_box_int) == sizeof(void*),
                  box_int_size_ptr);
ABI_STATIC_ASSERT(sizeof(pgy_abi_box_string) == sizeof(void*),
                  box_string_size_ptr);

/* =================================================================
 * STATIC ASSERTIONS — Rc/Weak
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_rc_ctrl_int) >= 16,
                  rc_ctrl_int_min_size_16);
ABI_STATIC_ASSERT(offsetof(pgy_abi_rc_ctrl_int, strong_count) == 0,
                  rc_ctrl_int_strong_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_rc_ctrl_int, weak_count) == 4,
                  rc_ctrl_int_weak_at_4);
ABI_STATIC_ASSERT(offsetof(pgy_abi_rc_ctrl_int, alive) == 8,
                  rc_ctrl_int_alive_at_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_rc_int) == sizeof(void*),
                  rc_int_size_ptr);
ABI_STATIC_ASSERT(sizeof(pgy_abi_weak_int) == sizeof(void*),
                  weak_int_size_ptr);

/* =================================================================
 * STATIC ASSERTIONS — Array<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_array_int, data) == 0,
                  array_int_data_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_int) >= 24,
                  array_int_min_size_24);

/* =================================================================
 * STATIC ASSERTIONS — Miscellaneous
 * ================================================================= */

ABI_STATIC_ASSERT(sizeof(pgy_abi_qubit) >= 12,
                  qubit_min_size_12);
ABI_STATIC_ASSERT(sizeof(pgy_abi_task_handle) >= 8,
                  task_handle_min_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_timer) >= 12,
                  timer_min_size_12);
ABI_STATIC_ASSERT(sizeof(pgy_abi_arena) >= 24,
                  arena_min_size_24);
ABI_STATIC_ASSERT(sizeof(pgy_abi_allocator) >= 48,
                  allocator_min_size_48);
ABI_STATIC_ASSERT(sizeof(pgy_abi_future) >= 8,
                  future_min_size_8);
ABI_STATIC_ASSERT(sizeof(pgy_abi_remote_future) >= 24,
                  remote_future_min_size_24);

#endif /* PERGYRA_ABI_SPEC_ASSERTS_H */
