#ifndef PGY_RUNTIME_LIB_RAW_MAP_KEY_EXPORTS_H
#define PGY_RUNTIME_LIB_RAW_MAP_KEY_EXPORTS_H

static char *
pgy_map_i32_key_string_export(int32_t key)
{
    return pergyra_strdup_printf("%d", key);
}

static char *
pgy_map_i64_key_string_export(int64_t key)
{
    return pergyra_strdup_printf("%lld", (long long)key);
}

static char *
pgy_map_bool_key_string_export(bool key)
{
    const char *src = key ? "true" : "false";
    size_t len = strlen(src);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return NULL;
    memcpy(buf, src, len + 1);
    return buf;
}

void
pgy_map_set_raw_i32_export(void *map_ptr, int32_t key, void *value_ptr,
                           int64_t value_size)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i32", "key formatting failed");
        return;
    }
    pgy_map_set_raw_export(map_ptr, key_str, value_ptr, value_size);
    free(key_str);
}

void
pgy_map_get_raw_i32_export(void *map_ptr, int32_t key, void *out_ptr,
                           int64_t value_size)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i32", "key formatting failed");
        if (out_ptr != NULL && value_size > 0)
            memset(out_ptr, 0, (size_t)value_size);
        return;
    }
    pgy_map_get_raw_export(map_ptr, key_str, out_ptr, value_size);
    free(key_str);
}

void
pgy_map_set_raw_i64_export(void *map_ptr, int64_t key, void *value_ptr,
                           int64_t value_size)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i64", "key formatting failed");
        return;
    }
    pgy_map_set_raw_export(map_ptr, key_str, value_ptr, value_size);
    free(key_str);
}

void
pgy_map_get_raw_i64_export(void *map_ptr, int64_t key, void *out_ptr,
                           int64_t value_size)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i64", "key formatting failed");
        if (out_ptr != NULL && value_size > 0)
            memset(out_ptr, 0, (size_t)value_size);
        return;
    }
    pgy_map_get_raw_export(map_ptr, key_str, out_ptr, value_size);
    free(key_str);
}

bool
pgy_map_has_raw_i64_export(void *map_ptr, int64_t key)
{
    bool result;
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i64", "key formatting failed");
        return false;
    }
    result = pgy_map_has_raw_export(map_ptr, key_str);
    free(key_str);
    return result;
}

void
pgy_map_remove_raw_i64_export(void *map_ptr, int64_t key, int64_t value_size)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i64", "key formatting failed");
        return;
    }
    pgy_map_remove_raw_export(map_ptr, key_str, value_size);
    free(key_str);
}

void
pgy_map_set_raw_bool_export(void *map_ptr, bool key, void *value_ptr,
                            int64_t value_size)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_bool", "key formatting failed");
        return;
    }
    pgy_map_set_raw_export(map_ptr, key_str, value_ptr, value_size);
    free(key_str);
}

void
pgy_map_get_raw_bool_export(void *map_ptr, bool key, void *out_ptr,
                            int64_t value_size)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_bool", "key formatting failed");
        if (out_ptr != NULL && value_size > 0)
            memset(out_ptr, 0, (size_t)value_size);
        return;
    }
    pgy_map_get_raw_export(map_ptr, key_str, out_ptr, value_size);
    free(key_str);
}

bool
pgy_map_has_raw_bool_export(void *map_ptr, bool key)
{
    bool result;
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_bool", "key formatting failed");
        return false;
    }
    result = pgy_map_has_raw_export(map_ptr, key_str);
    free(key_str);
    return result;
}

void
pgy_map_remove_raw_bool_export(void *map_ptr, bool key, int64_t value_size)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_bool", "key formatting failed");
        return;
    }
    pgy_map_remove_raw_export(map_ptr, key_str, value_size);
    free(key_str);
}

bool
pgy_map_has_raw_i32_export(void *map_ptr, int32_t key)
{
    bool result;
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i32", "key formatting failed");
        return false;
    }
    result = pgy_map_has_raw_export(map_ptr, key_str);

    free(key_str);
    return result;
}

void
pgy_map_remove_raw_i32_export(void *map_ptr, int32_t key, int64_t value_size)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i32", "key formatting failed");
        return;
    }
    pgy_map_remove_raw_export(map_ptr, key_str, value_size);
    free(key_str);
}

void
pgy_map_set_string_value_raw_i32_export(void *map_ptr, int32_t key,
                                        const char *value)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i32", "key formatting failed");
        return;
    }
    pgy_map_set_string_value_raw_export(map_ptr, key_str, value);
    free(key_str);
}

void
pgy_map_get_string_value_raw_i32_export(void *map_ptr, int32_t key,
                                        char **out_ptr)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_string_value_i32", "key formatting failed");
        if (out_ptr != NULL)
            *out_ptr = NULL;
        return;
    }
    pgy_map_get_string_value_raw_export(map_ptr, key_str, out_ptr);
    free(key_str);
}

void
pgy_map_remove_string_value_raw_i32_export(void *map_ptr, int32_t key)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_string_value_i32", "key formatting failed");
        return;
    }
    pgy_map_remove_string_value_raw_export(map_ptr, key_str);
    free(key_str);
}

void
pgy_map_set_string_value_raw_i64_export(void *map_ptr, int64_t key,
                                        const char *value)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i64", "key formatting failed");
        return;
    }
    pgy_map_set_string_value_raw_export(map_ptr, key_str, value);
    free(key_str);
}

void
pgy_map_get_string_value_raw_i64_export(void *map_ptr, int64_t key,
                                        char **out_ptr)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_string_value_i64", "key formatting failed");
        if (out_ptr != NULL)
            *out_ptr = NULL;
        return;
    }
    pgy_map_get_string_value_raw_export(map_ptr, key_str, out_ptr);
    free(key_str);
}

void
pgy_map_remove_string_value_raw_i64_export(void *map_ptr, int64_t key)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_string_value_i64", "key formatting failed");
        return;
    }
    pgy_map_remove_string_value_raw_export(map_ptr, key_str);
    free(key_str);
}

void
pgy_map_set_string_value_raw_bool_export(void *map_ptr, bool key,
                                         const char *value)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_bool", "key formatting failed");
        return;
    }
    pgy_map_set_string_value_raw_export(map_ptr, key_str, value);
    free(key_str);
}

void
pgy_map_get_string_value_raw_bool_export(void *map_ptr, bool key,
                                         char **out_ptr)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_string_value_bool", "key formatting failed");
        if (out_ptr != NULL)
            *out_ptr = NULL;
        return;
    }
    pgy_map_get_string_value_raw_export(map_ptr, key_str, out_ptr);
    free(key_str);
}

void
pgy_map_remove_string_value_raw_bool_export(void *map_ptr, bool key)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_string_value_bool", "key formatting failed");
        return;
    }
    pgy_map_remove_string_value_raw_export(map_ptr, key_str);
    free(key_str);
}

#endif /* PGY_RUNTIME_LIB_RAW_MAP_KEY_EXPORTS_H */
