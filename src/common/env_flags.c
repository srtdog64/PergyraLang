/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared environment flag parsing.
 */

#include "env_flags.h"

#include <string.h>

bool
pgy_env_value_is_false(const char *value)
{
    if (value == NULL || value[0] == '\0')
        return false;
    return strcmp(value, "0") == 0
        || strcmp(value, "false") == 0
        || strcmp(value, "FALSE") == 0
        || strcmp(value, "off") == 0
        || strcmp(value, "OFF") == 0
        || strcmp(value, "no") == 0
        || strcmp(value, "NO") == 0;
}

bool
pgy_env_value_is_truthy(const char *value)
{
    return value != NULL && value[0] != '\0'
        && !pgy_env_value_is_false(value);
}
