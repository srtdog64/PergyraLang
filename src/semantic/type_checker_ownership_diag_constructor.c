/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Ownership diagnostics for constructor field escape paths.
 */

#include "diag_codes.h"
#include "diag_payload.h"
#include "type_checker_ownership_diag_internal.h"

void
semantic_report_borrowed_constructor_field_escape(ASTNode *site,
                                                  ASTNode *source_expr,
                                                  SemanticContext *ctx,
                                                  const char *borrowed_name,
                                                  const char *value_label,
                                                  const char *provenance_label,
                                                  const char *constructor_name,
                                                  const char *constructor_field,
                                                  const char *snapshot_label,
                                                  bool identity_binding)
{
    const char *source_path = semantic_assignment_target_path_scratch(source_expr, ctx);
    DiagPayload payload;
    diag_payload_init(&payload);
    payload.code = PGY_CODE_SEM_BORROW_ESCAPE;
    payload.cause_ir = PGY_CAUSE_BORROW_ESCAPE;
    payload.fix_source = PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE;
    payload.site = site;
    payload.value_label = value_label;
    payload.provenance_label = provenance_label;
    payload.replacement_label = snapshot_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);
    payload.secondary_name = constructor_name;
    payload.kind_label = constructor_field;

    semantic_emit_payload(ctx, &payload,
        "Borrowed ref %s '%s' cannot escape through constructor field store '%s.%s' from '%s'.\n"
        "Reason:\n"
        "- consumer path is function '%s'\n"
        "- '%s' entered this function as a borrowed 'ref' %s\n"
        "- '%s' is derived from that borrowed %s\n"
        "- constructor '%s' would store it into field '%s'\n"
        "%s"
        "Fix:\n"
        "- %s\n"
        "- or construct the field from %s instead\n"
        "- or change the current parameter to 'own' if transfer is intended",
        value_label != NULL ? value_label : "boundary value",
        borrowed_name,
        constructor_name != NULL ? constructor_name : "<constructor>",
        constructor_field != NULL ? constructor_field : "<field>",
        source_path != NULL ? source_path : borrowed_name,
        semantic_current_consumer_name(ctx),
        borrowed_name,
        value_label != NULL ? value_label : "boundary value",
        source_path != NULL ? source_path : borrowed_name,
        provenance_label != NULL ? provenance_label : "boundary provenance",
        constructor_name != NULL ? constructor_name : "<constructor>",
        constructor_field != NULL ? constructor_field : "<field>",
        identity_binding
            ? "- constructor field storage would create a second boundary-visible binding for the borrowed identity\n"
            : "- storing the borrow would create a longer-lived ownership alias the compiler cannot keep boundary-safe\n",
        identity_binding
            ? "keep mutating the original value through its existing binding"
            : "keep using the borrowed value directly within this function",
        snapshot_label != NULL ? snapshot_label : "a copied/value/projection result");
}
