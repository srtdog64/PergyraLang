/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Active slot view boundary diagnostics.
 */

#include "type_checker_internal.h"
#include "diag_codes.h"

static const char *
semantic_boundary_article(const char *boundary_name)
{
    char c;

    if (boundary_name == NULL || boundary_name[0] == '\0')
        return "a";

    c = boundary_name[0];
    if (c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
        ? "an"
        : "a";
}

bool
semantic_reject_active_slot_view_boundary(ASTNode *site,
                                          SemanticContext *ctx,
                                          const char *boundary_name,
                                          const char *resume_detail,
                                          const char *fix_action)
{
    const char *view_name = NULL;
    const char *view_kind = NULL;
    const char *source_slot = NULL;

    if (site == NULL || ctx == NULL)
        return false;
    if (!semantic_find_active_slot_view(ctx->scope, &view_name, &view_kind,
                                        &source_slot))
        return false;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_PIN_AWAIT_BOUNDARY,
        PGY_CAUSE_PIN_AWAIT_BOUNDARY,
        PGY_FIX_END_PIN_BEFORE_AWAIT,
        site,
        "Pinned view '%s' cannot cross %s %s.\n"
        "Reason:\n"
        "- %s for slot '%s' is a scoped capability lease\n"
        "- %s\n"
        "- the stable beta subset requires view cleanup before this boundary\n"
        "Fix:\n"
        "- end the view scope before the boundary\n"
        "- or %s before acquiring the view",
        view_name != NULL ? view_name : "<view>",
        semantic_boundary_article(boundary_name),
        boundary_name != NULL ? boundary_name : "suspension boundary",
        view_kind != NULL ? view_kind : "View",
        source_slot != NULL ? source_slot : "<slot>",
        resume_detail != NULL ? resume_detail
                              : "the boundary may resume after unrelated runtime work",
        fix_action != NULL ? fix_action : "move the boundary");
    return true;
}
