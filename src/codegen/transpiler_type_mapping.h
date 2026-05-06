#ifndef PERGYRA_TRANSPILER_TYPE_MAPPING_H
#define PERGYRA_TRANSPILER_TYPE_MAPPING_H

#include "transpiler.h"

const char *pergyra_primitive_to_c(const char *name);
const char *slot_inner_type_name(const char *slot_type_name);
void sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size);
void copy_capped_string(char *dst, size_t dst_size, const char *src);
const char *constructed_arg_name_at(const char *type_name, int arg_index);
void copy_constructed_arg_name_at(const char *type_name, int arg_index,
                                  char *buf, size_t buf_size);
const char *generic_args_to_c_suffix(const char *inner_body);

#endif /* PERGYRA_TRANSPILER_TYPE_MAPPING_H */
