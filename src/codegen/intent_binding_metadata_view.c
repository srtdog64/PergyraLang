/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared ordered intent binding metadata view queries.
 */

#include "intent_binding_metadata_view.h"

#include <stdlib.h>
#include <string.h>

bool
intent_binding_metadata_view_is_active(
    const IntentBindingMetadataView *bindings)
{
    return bindings != NULL
        && (bindings->kinds != NULL || bindings->aliases != NULL
            || bindings->types != NULL || bindings->count > 0);
}

bool
intent_binding_metadata_kind_is_supported(const char *kind)
{
    return kind != NULL
        && (strcmp(kind, "participant") == 0
            || strcmp(kind, "value") == 0);
}

bool
intent_binding_metadata_view_has_complete_row(
    const IntentBindingMetadataView *bindings,
    size_t index)
{
    return bindings != NULL
        && index < bindings->count
        && bindings->kinds != NULL
        && bindings->aliases != NULL
        && bindings->types != NULL
        && bindings->kinds[index] != NULL
        && bindings->aliases[index] != NULL
        && bindings->types[index] != NULL;
}

bool
intent_binding_metadata_view_has_supported_row(
    const IntentBindingMetadataView *bindings,
    size_t index)
{
    return intent_binding_metadata_view_has_complete_row(bindings, index)
        && intent_binding_metadata_kind_is_supported(bindings->kinds[index]);
}

const char *
intent_binding_metadata_view_kind_at(const IntentBindingMetadataView *bindings,
                                     size_t index)
{
    if (bindings == NULL || bindings->kinds == NULL || index >= bindings->count)
        return NULL;
    return bindings->kinds[index];
}

const char *
intent_binding_metadata_view_alias_at(const IntentBindingMetadataView *bindings,
                                      size_t index)
{
    if (bindings == NULL || bindings->aliases == NULL || index >= bindings->count)
        return NULL;
    return bindings->aliases[index];
}

const char *
intent_binding_metadata_view_type_at(const IntentBindingMetadataView *bindings,
                                     size_t index)
{
    if (bindings == NULL || bindings->types == NULL || index >= bindings->count)
        return NULL;
    return bindings->types[index];
}

bool
intent_binding_metadata_view_row_is_kind(
    const IntentBindingMetadataView *bindings,
    size_t index,
    const char *kind)
{
    const char *row_kind = intent_binding_metadata_view_kind_at(bindings, index);
    return row_kind != NULL && kind != NULL && strcmp(row_kind, kind) == 0;
}

void
intent_binding_metadata_view_dispose(IntentBindingMetadataView *bindings)
{
    if (bindings == NULL)
        return;
    if (bindings->owns_storage) {
        free((void *)bindings->kinds);
        free((void *)bindings->aliases);
        free((void *)bindings->types);
    }
    bindings->kinds = NULL;
    bindings->aliases = NULL;
    bindings->types = NULL;
    bindings->count = 0;
    bindings->owns_storage = false;
}

const char *
intent_binding_type_name_from_metadata(
    const IntentBindingMetadataView *bindings,
    const char *alias,
    const char *required_kind)
{
    if (!intent_binding_metadata_view_is_active(bindings))
        return NULL;
    if (alias == NULL || !intent_binding_metadata_kind_is_supported(required_kind))
        return NULL;
    for (size_t i = 0; i < bindings->count; i++) {
        if (intent_binding_metadata_view_has_supported_row(bindings, i)
            && intent_binding_metadata_view_row_is_kind(bindings, i,
                required_kind)
            && strcmp(intent_binding_metadata_view_alias_at(bindings, i),
                alias) == 0) {
            return intent_binding_metadata_view_type_at(bindings, i);
        }
    }
    return NULL;
}
