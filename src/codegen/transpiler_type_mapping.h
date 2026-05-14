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

#endif /* PERGYRA_TRANSPILER_TYPE_MAPPING_H */
