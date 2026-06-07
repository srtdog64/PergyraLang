/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared ordered intent binding metadata view for C and LLVM backends.
 */

#ifndef PGY_INTENT_BINDING_METADATA_VIEW_H
#define PGY_INTENT_BINDING_METADATA_VIEW_H

#include <stddef.h>

typedef struct IntentBindingMetadataView {
    const char **kinds;
    const char **aliases;
    const char **types;
    size_t count;
} IntentBindingMetadataView;

#endif /* PGY_INTENT_BINDING_METADATA_VIEW_H */
