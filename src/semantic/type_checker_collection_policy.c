#include "type_checker_collection_policy.h"

bool
type_checker_hashmap_key_supported(const Type *key_type)
{
    return type_equals(key_type, TYPE_STRING)
        || type_equals(key_type, TYPE_INT)
        || type_equals(key_type, TYPE_LONG)
        || type_equals(key_type, TYPE_BOOL);
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
