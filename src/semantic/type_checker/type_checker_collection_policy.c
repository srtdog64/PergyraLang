#include "type_checker_collection_policy.h"

static bool
type_checker_stable_scalar_key_supported(const Type *key_type)
{
    return type_equals(key_type, TYPE_STRING)
        || type_equals(key_type, TYPE_INT)
        || type_equals(key_type, TYPE_LONG)
        || type_equals(key_type, TYPE_BOOL);
}

bool
type_checker_hashmap_key_supported(const Type *key_type)
{
    return type_checker_stable_scalar_key_supported(key_type);
}

const char *
type_checker_hashmap_key_policy_text(void)
{
    return "String, Int, Long, or Bool";
}

const char *
type_checker_hashmap_type_policy_text(void)
{
    return "HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, "
           "and HashMap<Bool, T>";
}

bool
type_checker_ordered_collection_key_supported(const Type *key_type)
{
    return type_checker_stable_scalar_key_supported(key_type);
}

const char *
type_checker_ordered_collection_key_policy_text(void)
{
    return "String, Int, Long, or Bool";
}
