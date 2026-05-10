/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * AIR owned-name and diagnostic formatting helpers.
 */

#include "air_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *
air_vformat_owned(const char *fmt, va_list args)
{
    va_list copy;
    char *buffer;
    int needed;
    int written;

    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return NULL;

    buffer = (char *)malloc((size_t)needed + 1);
    if (buffer == NULL)
        return NULL;
    written = vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    if (written < 0 || written != needed) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

char *
air_format_owned(const char *fmt, ...)
{
    va_list args;
    char *result;

    va_start(args, fmt);
    result = air_vformat_owned(fmt, args);
    va_end(args);
    return result;
}

void
air_set_error(char **error_message, const char *fmt, ...)
{
    if (error_message == NULL)
        return;
    va_list args;
    va_start(args, fmt);
    *error_message = air_vformat_owned(fmt, args);
    va_end(args);
}

bool
air_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    if (capacity == NULL || initial == 0 || elem_size == 0)
        return false;

    size_t current = *capacity;
    size_t next_capacity = initial;
    if (current != 0) {
        if (current > SIZE_MAX / 2)
            return false;
        next_capacity = current * 2;
    }
    if (next_capacity > SIZE_MAX / elem_size)
        return false;

    *capacity = next_capacity;
    return true;
}

char *
air_strdup_owned(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        text = "";
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static bool
air_ensure_owned_name_capacity(AIRProgram *air)
{
    char **grown;
    size_t new_capacity;

    if (air == NULL)
        return false;
    if (air->owned_name_count < air->owned_name_capacity)
        return true;

    new_capacity = air->owned_name_capacity;
    if (!air_next_capacity(&new_capacity, 16, sizeof(char *))) {
        return false;
    }

    grown = (char **)realloc(air->owned_names,
                             new_capacity * sizeof(char *));
    if (grown == NULL)
        return false;
    air->owned_names = grown;
    air->owned_name_capacity = new_capacity;
    return true;
}

const char *
air_program_owned_name(AIRProgram *air, const char *text)
{
    char *copy;

    if (air == NULL || text == NULL)
        return NULL;

    copy = air_strdup_owned(text);
    if (copy == NULL)
        return NULL;

    if (!air_ensure_owned_name_capacity(air)) {
        free(copy);
        return NULL;
    }

    air->owned_names[air->owned_name_count++] = copy;
    return copy;
}

bool
air_assign_owned_name(AIRProgram *air, const char **slot, const char *text)
{
    if (slot == NULL)
        return false;
    *slot = NULL;
    if (text == NULL)
        return true;
    *slot = air_program_owned_name(air, text);
    return *slot != NULL;
}

bool
air_assign_first_owned_name(AIRProgram *air,
                            const char **slot,
                            const char *text,
                            char **error_message,
                            const char *what)
{
    if (slot == NULL || *slot != NULL || text == NULL)
        return true;
    if (!air_assign_owned_name(air, slot, text)) {
        air_set_error(error_message, "AIR %s evidence name allocation failed", what);
        return false;
    }
    return true;
}

bool
air_name_matches(const char *a, const char *b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}
