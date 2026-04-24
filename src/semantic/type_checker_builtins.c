/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker built-in dispatch and stdlib helpers
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_channel_transport_internal.h"

static bool
type_is_future_like(const Type *type)
{
    return type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

#include "type_checker_builtins_query_domain.inc"

#include "type_checker_builtins_query.inc"

#include "type_checker_builtins_stdlib.inc"

#include "type_checker_builtins_nominal.inc"
