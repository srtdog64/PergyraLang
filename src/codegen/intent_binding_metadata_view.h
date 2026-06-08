/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared ordered intent binding metadata view for C and LLVM backends.
 */

#ifndef PGY_INTENT_BINDING_METADATA_VIEW_H
#define PGY_INTENT_BINDING_METADATA_VIEW_H

#include <stddef.h>
#include <stdbool.h>

typedef struct IntentBindingMetadataView {
    const char **kinds;
    const char **aliases;
    const char **types;
    size_t count;
    bool owns_storage;
} IntentBindingMetadataView;

bool intent_binding_metadata_view_is_active(
    const IntentBindingMetadataView *bindings);
bool intent_binding_metadata_kind_is_supported(const char *kind);
bool intent_binding_metadata_view_has_complete_row(
    const IntentBindingMetadataView *bindings,
    size_t index);
bool intent_binding_metadata_view_has_supported_row(
    const IntentBindingMetadataView *bindings,
    size_t index);
const char *intent_binding_metadata_view_kind_at(
    const IntentBindingMetadataView *bindings,
    size_t index);
const char *intent_binding_metadata_view_alias_at(
    const IntentBindingMetadataView *bindings,
    size_t index);
const char *intent_binding_metadata_view_type_at(
    const IntentBindingMetadataView *bindings,
    size_t index);
bool intent_binding_metadata_view_row_is_kind(
    const IntentBindingMetadataView *bindings,
    size_t index,
    const char *kind);
void intent_binding_metadata_view_dispose(
    IntentBindingMetadataView *bindings);
const char *intent_binding_type_name_from_metadata(
    const IntentBindingMetadataView *bindings,
    const char *alias,
    const char *required_kind);

#endif /* PGY_INTENT_BINDING_METADATA_VIEW_H */
