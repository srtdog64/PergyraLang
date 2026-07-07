#ifndef PERGYRA_ABI_SPEC_ASSERTS_H
#define PERGYRA_ABI_SPEC_ASSERTS_H

/* =================================================================
 * STATIC ASSERTIONS - Slot<T> Canonical Checked ABI
 * ================================================================= */

/* Slot<Int> canonical checked: value@0, occupied after value, size >= 8 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_int, value) == 0,
                  slot_int_value_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_int, occupied) >= 4,
                  slot_int_occupied_after_value);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_int) >= 8,
                  slot_int_min_size_8);

/* Slot<Long> canonical checked: value@0, size >= 16 (8 + 1 + 7 padding) */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_long, value) == 0,
                  slot_long_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_long) >= 16,
                  slot_long_min_size_16);

/* Slot<Float> canonical checked: value@0, size >= 8 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_float, value) == 0,
                  slot_float_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_float) >= 8,
                  slot_float_min_size_8);

/* Slot<Double> canonical checked: value@0, size >= 16 */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_double, value) == 0,
                  slot_double_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_double) >= 16,
                  slot_double_min_size_16);

/* Slot<Bool> canonical checked: size >= 2 */
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_bool) >= 2,
                  slot_bool_min_size_2);

/* Slot<String> canonical checked: value@0, size >= 16 (8 + 1 + 7 padding on LP64) */
ABI_STATIC_ASSERT(offsetof(pgy_abi_slot_string, value) == 0,
                  slot_string_value_at_0);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slot_string) >= 16,
                  slot_string_min_size_16);

/* =================================================================
 * STATIC ASSERTIONS — SecureSlot<T>
 * ================================================================= */

ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int, value) == 0,
                  secure_slot_int_value_at_0);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_int, token) > 4,
                  secure_slot_int_token_after_value);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int) > sizeof(pgy_abi_slot_int),
                  secure_slot_int_larger_than_slot);
ABI_STATIC_ASSERT(sizeof(pgy_abi_secure_slot_int) >= 16,
                  secure_slot_int_min_size_16);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_long, token) > 8,
                  secure_slot_long_token_after_value);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_float, token) > 4,
                  secure_slot_float_token_after_value);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_double, token) > 8,
                  secure_slot_double_token_after_value);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_bool, token) > 1,
                  secure_slot_bool_token_after_value);
ABI_STATIC_ASSERT(offsetof(pgy_abi_secure_slot_string, token) > sizeof(void *),
                  secure_slot_string_token_after_value);

ABI_STATIC_ASSERT(sizeof(pgy_abi_token_int) >= 16,
                  token_int_min_size_16);
ABI_STATIC_ASSERT(offsetof(pgy_abi_token_int, can_write) > 0,
                  token_int_can_write_after_id);
ABI_STATIC_ASSERT(offsetof(pgy_abi_token_int, can_read)
                      > offsetof(pgy_abi_token_int, can_write),
                  token_int_can_read_after_can_write);

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
ABI_STATIC_ASSERT(offsetof(pgy_abi_array_int, length) == sizeof(void *),
                  array_int_length_after_data);
ABI_STATIC_ASSERT(offsetof(pgy_abi_array_int, allocator)
                      > offsetof(pgy_abi_array_int, capacity),
                  array_int_allocator_after_capacity);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_int) >= sizeof(void *) * 4,
                  array_int_min_four_words);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_long) == sizeof(pgy_abi_array_int),
                  array_long_shape_matches_int);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_float) == sizeof(pgy_abi_array_int),
                  array_float_shape_matches_int);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_double) == sizeof(pgy_abi_array_int),
                  array_double_shape_matches_int);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_bool) == sizeof(pgy_abi_array_int),
                  array_bool_shape_matches_int);
ABI_STATIC_ASSERT(sizeof(pgy_abi_array_string) == sizeof(pgy_abi_array_int),
                  array_string_shape_matches_int);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slice_int) == sizeof(void *) + sizeof(size_t),
                  slice_int_two_word_shape);
ABI_STATIC_ASSERT(sizeof(pgy_abi_slice_string) == sizeof(pgy_abi_slice_int),
                  slice_string_shape_matches_int);

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
