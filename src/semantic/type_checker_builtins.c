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
#include "type_checker_builtins_internal.h"

bool
type_is_future_like(const Type *type)
{
    return type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

#include "type_checker_builtins_query_domain.h"

#include "type_checker_builtins_query.h"

#include "type_checker_builtins_slotops.h"

#include "type_checker_builtins_nominal.h"
