#ifndef PERGYRA_CODEGEN_HASHMAP_KEY_POLICY_H
#define PERGYRA_CODEGEN_HASHMAP_KEY_POLICY_H

#include <stdbool.h>
#include <stddef.h>

typedef enum PgyHashMapKeyKind {
    PGY_HASHMAP_KEY_STRING,
    PGY_HASHMAP_KEY_INT,
    PGY_HASHMAP_KEY_LONG,
    PGY_HASHMAP_KEY_BOOL,
    PGY_HASHMAP_KEY_UNKNOWN
} PgyHashMapKeyKind;

PgyHashMapKeyKind pgy_hashmap_key_kind_from_name(const char *name);
const char *pgy_hashmap_key_c_infix(const char *key_name);
bool pgy_hashmap_key_raw_export_name(const char *operation,
                                     const char *key_name,
                                     char *out,
                                     size_t out_size);
bool pgy_hashmap_key_raw_string_value_export_name(const char *operation,
                                                  const char *key_name,
                                                  char *out,
                                                  size_t out_size);

#endif /* PERGYRA_CODEGEN_HASHMAP_KEY_POLICY_H */
