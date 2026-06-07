#ifndef PERGYRA_TRANSPILER_TYPE_MAPPING_H
#define PERGYRA_TRANSPILER_TYPE_MAPPING_H

#include <stdbool.h>

#include "transpiler.h"

const char *pergyra_primitive_to_c(const char *name);
bool pergyra_type_to_c_copy(const char *name, char *out, size_t out_size);
bool slot_inner_type_name_copy(const char *slot_type_name,
                               char *out,
                               size_t out_size);
void sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size);
void copy_capped_string(char *dst, size_t dst_size, const char *src);
void copy_constructed_arg_name_at(const char *type_name, int arg_index,
                                  char *buf, size_t buf_size);
bool generic_args_to_c_suffix_copy(const char *inner_body,
                                   char *out,
                                   size_t out_size);
bool transpiler_type_name_contains_unknown_sentinel(const char *type_name);
bool transpiler_type_name_is_concrete_fact(const char *type_name);
bool transpiler_type_name_is_channel(const char *type_name);
bool transpiler_type_name_is_future(const char *type_name);
bool transpiler_type_name_is_remote_future(const char *type_name);
bool transpiler_type_name_is_any_future(const char *type_name);
bool transpiler_type_name_is_result(const char *type_name);
bool transpiler_type_name_is_option(const char *type_name);
bool transpiler_type_name_is_array(const char *type_name);
bool transpiler_type_name_is_slice(const char *type_name);
bool transpiler_type_name_is_array_or_slice(const char *type_name);
bool transpiler_type_name_is_list(const char *type_name);
bool transpiler_type_name_is_queue(const char *type_name);
bool transpiler_type_name_is_set(const char *type_name);
bool transpiler_type_name_is_hashmap(const char *type_name);
bool transpiler_type_name_is_box(const char *type_name);
bool transpiler_type_name_is_box_array(const char *type_name);
bool transpiler_type_name_is_rc(const char *type_name);
bool transpiler_type_name_is_weak(const char *type_name);
const char *codegen_worker_boundary_storage_kind_from_constructor_name(
    const char *constructor_name,
    bool include_channel,
    bool include_array_slice_alias);
const char *codegen_worker_boundary_storage_kind_from_type_name(
    const char *type_name, bool include_channel);

#endif /* PERGYRA_TRANSPILER_TYPE_MAPPING_H */
