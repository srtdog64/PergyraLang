/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend enum lowering helpers.
 */

#ifndef PERGYRA_TRANSPILER_ENUM_H
#define PERGYRA_TRANSPILER_ENUM_H

#include "transpiler.h"

const char *lookup_enum_variant_qualified_name(TranspilerCtx *ctx,
                                               const char *variant_name);

#endif /* PERGYRA_TRANSPILER_ENUM_H */
