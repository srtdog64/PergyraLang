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

bool
semantic_validate_channel_transport_ownership(ASTNode *value_expr,
                                              Type *value_type,
                                              SemanticContext *ctx,
                                              const char *transport_name,
                                              OwnershipTypeClass expected_class,
                                              OwnershipTypeClass element_ownership,
                                              OwnershipTypeClass value_ownership,
                                              const char *contract_label,
                                              const char *expected_name,
                                              const char *actual_name,
                                              const char *value_label,
                                              const char *named_binding_fix);

void
semantic_report_channel_transport_policy(ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *transport_name,
                                         const char *why_text,
                                         const char *fix_text);

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
