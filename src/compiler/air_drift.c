/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR drift storage owner. Verification passes append drift diagnostics through
 * this TU instead of owning AIRProgram drift allocation directly.
 */

#include "air_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

void
air_clear_drifts(AIRProgram *air)
{
    if (air == NULL)
        return;
    for (size_t i = 0; i < air->drift_count; i++)
        free((char *)air->drifts[i].message);
    free(air->drifts);
    air->drifts = NULL;
    air->drift_count = 0;
    air->drift_capacity = 0;
}

bool
air_append_drift(AIRProgram *air,
                 AIRDriftKind kind,
                 size_t intent_index,
                 size_t boundary_index,
                 const char *message,
                 char **error_message)
{
    char *message_copy = air_strdup_owned(message);

    if (message_copy == NULL) {
        air_set_error(error_message, "AIR drift message allocation failed");
        return false;
    }
    if (air->drift_count >= air->drift_capacity) {
        AIRDrift *next;
        size_t new_capacity = air->drift_capacity;
        if (!air_next_capacity(&new_capacity, 8, sizeof(AIRDrift))) {
            free(message_copy);
            air_set_error(error_message, "AIR drift allocation failed");
            return false;
        }
        next = (AIRDrift *)realloc(air->drifts,
                                   sizeof(AIRDrift) * new_capacity);
        if (next == NULL) {
            free(message_copy);
            air_set_error(error_message, "AIR drift allocation failed");
            return false;
        }
        air->drifts = next;
        air->drift_capacity = new_capacity;
    }
    air->drifts[air->drift_count].kind = kind;
    air->drifts[air->drift_count].intent_index = intent_index;
    air->drifts[air->drift_count].boundary_index = boundary_index;
    air->drifts[air->drift_count].message = message_copy;
    air->drift_count++;
    return true;
}

bool
air_append_driftf(AIRProgram *air,
                  AIRDriftKind kind,
                  size_t intent_index,
                  size_t boundary_index,
                  char **error_message,
                  const char *fmt,
                  ...)
{
    va_list args;
    char *message;
    bool ok;

    va_start(args, fmt);
    message = air_vformat_owned(fmt, args);
    va_end(args);
    if (message == NULL) {
        air_set_error(error_message, "AIR drift message formatting failed");
        return false;
    }

    ok = air_append_drift(air, kind, intent_index, boundary_index,
                          message, error_message);
    free(message);
    return ok;
}
