#ifndef PERGYRA_TYPE_CHECKER_COLLECTION_POLICY_H
#define PERGYRA_TYPE_CHECKER_COLLECTION_POLICY_H

#include <stdbool.h>

#include "type_system.h"

bool type_checker_hashmap_key_supported(const Type *key_type);
const char *type_checker_hashmap_key_policy_text(void);
const char *type_checker_hashmap_type_policy_text(void);

#endif /* PERGYRA_TYPE_CHECKER_COLLECTION_POLICY_H */
