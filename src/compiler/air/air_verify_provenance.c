/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR verification provenance formatting owner.
 */

#include "air_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char *
air_format_authority_names_owned(const AIRBoundaryNode *boundary)
{
    size_t total = 1;
    bool emitted = false;
    char *out;
    size_t used = 0;

    if (boundary == NULL || !air_boundary_authority_storage_valid(boundary))
        return NULL;
    for (size_t i = 0; i < air_boundary_authority_name_count(boundary); i++) {
        const char *name = air_boundary_authority_name_at(boundary, i);
        if (name == NULL || name[0] == '\0')
            continue;
        if (emitted && total > SIZE_MAX - 2)
            return NULL;
        if (emitted)
            total += 2;
        if (strlen(name) > SIZE_MAX - total)
            return NULL;
        total += strlen(name);
        emitted = true;
    }
    if (!emitted)
        return NULL;
    out = (char *)malloc(total);
    if (out == NULL)
        return NULL;
    out[0] = '\0';
    emitted = false;
    for (size_t i = 0; i < air_boundary_authority_name_count(boundary); i++) {
        const char *name = air_boundary_authority_name_at(boundary, i);
        size_t len;
        if (name == NULL || name[0] == '\0')
            continue;
        if (emitted) {
            memcpy(out + used, ", ", 2);
            used += 2;
        }
        len = strlen(name);
        memcpy(out + used, name, len);
        used += len;
        out[used] = '\0';
        emitted = true;
    }
    return out;
}

char *
air_format_boundary_provenance_owned(const AIRIntentNode *intent,
                                     const AIRBoundaryNode *boundary)
{
    const char *source_provenance;
    const char *who_provenance;
    const char *authority_provenance;

    if (intent == NULL || boundary == NULL)
        return air_strdup_owned("");
    if (boundary->source_from_intent_default && boundary->source_from_transfer)
        source_provenance = "intent-default+transfer";
    else if (boundary->source_from_intent_default)
        source_provenance = "intent-default";
    else if (boundary->source_from_action)
        source_provenance = "action-inherited";
    else if (boundary->source_from_transfer)
        source_provenance = "transfer";
    else
        source_provenance = "explicit";
    if (intent->who_from_intent_default)
        who_provenance = "intent-default";
    else if (intent->who_from_on_receiver)
        who_provenance = "on-receiver";
    else if (intent->who_from_single_participant)
        who_provenance = "single-participant";
    else
        who_provenance = "explicit";
    if (boundary->authority_from_zone)
        authority_provenance = "legacy-zone-field";
    else if (boundary->authority_from_action)
        authority_provenance = "action-inherited";
    else
        authority_provenance = boundary->authority_required ? "explicit" : "none";
    return air_format_owned(
        "; owner=%s step=%s boundary_source=%s source_provenance=%s who_provenance=%s authority_provenance=%s",
        intent->intent_owner != NULL ? intent->intent_owner : "<intent>",
        intent->step_name != NULL ? intent->step_name : "<step>",
        boundary->source_name != NULL ? boundary->source_name : "<boundary>",
        source_provenance,
        who_provenance,
        authority_provenance);
}
