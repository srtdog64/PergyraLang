#ifndef PGY_RUNTIME_MAP_INT_KEY_INLINE_H
#define PGY_RUNTIME_MAP_INT_KEY_INLINE_H

#include "pgy_runtime_linkage.h"

PGY_RT_DECL int32_t pgy_map_size_int(PgyHashMap_Int *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_map_int_is_initialized(m) ? (int32_t)m->count : 0;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i32_int(PgyHashMap_Int *m, int32_t key, int32_t val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i32_int", "key formatting failed");
        return;
    }
    pgy_map_set_int(m, key_str, val);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_get_i32_int(PgyHashMap_Int *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    int32_t value = 0;
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i32_int", "key formatting failed");
        return value;
    }
    value = pgy_map_get_int(m, key_str);
    free(key_str);
    return value;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i32_int(PgyHashMap_Int *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    bool result = false;
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i32_int", "key formatting failed");
        return false;
    }
    result = pgy_map_has_int(m, key_str);
    free(key_str);
    return result;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i32_int(PgyHashMap_Int *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i32_int", "key formatting failed");
        return;
    }
    pgy_map_remove_int(m, key_str);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i64_int(PgyHashMap_Int *m, int64_t key, int32_t val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i64_int", "key formatting failed");
        return;
    }
    pgy_map_set_int(m, key_str, val);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_get_i64_int(PgyHashMap_Int *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    int32_t value = 0;
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i64_int", "key formatting failed");
        return value;
    }
    value = pgy_map_get_int(m, key_str);
    free(key_str);
    return value;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i64_int(PgyHashMap_Int *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    bool result = false;
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i64_int", "key formatting failed");
        return false;
    }
    result = pgy_map_has_int(m, key_str);
    free(key_str);
    return result;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i64_int(PgyHashMap_Int *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i64_int", "key formatting failed");
        return;
    }
    pgy_map_remove_int(m, key_str);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_bool_int(PgyHashMap_Int *m, bool key, int32_t val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_bool_int", "key formatting failed");
        return;
    }
    pgy_map_set_int(m, key_str, val);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_get_bool_int(PgyHashMap_Int *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    int32_t value = 0;
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_bool_int", "key formatting failed");
        return value;
    }
    value = pgy_map_get_int(m, key_str);
    free(key_str);
    return value;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_bool_int(PgyHashMap_Int *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    bool result = false;
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_bool_int", "key formatting failed");
        return false;
    }
    result = pgy_map_has_int(m, key_str);
    free(key_str);
    return result;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_bool_int(PgyHashMap_Int *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_bool_int", "key formatting failed");
        return;
    }
    pgy_map_remove_int(m, key_str);
    free(key_str);
}
#else
;
#endif


#endif /* PGY_RUNTIME_MAP_INT_KEY_INLINE_H */
