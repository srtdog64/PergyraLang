/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Ownership diagnostics for named boundaries and helper-call escapes.
 */

#include <string.h>

#include "diag_codes.h"
#include "diag_payload.h"
#include "type_checker_ownership_diag_internal.h"

void
semantic_report_named_boundary_argument_required(ASTNode *site,
                                                 ASTNode *source_expr,
                                                 SemanticContext *ctx,
                                                 const char *value_label_cap,
                                                 const char *value_label_lower,
                                                 const char *bind_fix)
{
    const char *source_path = semantic_assignment_target_path_scratch(source_expr, ctx);
    DiagPayload payload;
    diag_payload_init(&payload);
    payload.code = PGY_CODE_SEM_TYPE_MISMATCH;
    payload.cause_ir = PGY_CAUSE_MOVE_SOURCE_NOT_NAMED;
    payload.fix_source = PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE;
    payload.site = site;
    payload.value_label = value_label_lower;
    payload.consumer_name = semantic_current_consumer_name(ctx);
    payload.extra = bind_fix;
    (void)value_label_cap;

    semantic_emit_payload(ctx, &payload,
        "%s boundary arguments must use a named variable%s%s%s.\n"
        "Reason:\n"
        "- own/ref %s boundaries need one stable source binding for moved-here / borrowed-here provenance\n"
        "- unnamed %s expressions would make later ownership diagnostics ambiguous\n"
        "Fix:\n"
        "- %s\n"
        "- then pass that named variable through the boundary",
        value_label_cap != NULL ? value_label_cap : "Value",
        source_path != NULL ? " instead of '" : "",
        source_path != NULL ? source_path : "",
        source_path != NULL ? "'" : "",
        value_label_lower != NULL ? value_label_lower : "value",
        value_label_lower != NULL ? value_label_lower : "value",
        bind_fix != NULL ? bind_fix : "bind the value to a local variable first");
}

void
semantic_report_borrowed_helper_call_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *value_label,
                                            const char *provenance_label,
                                            const char *callee_name,
                                            bool transitive_ref_escape,
                                            const char *mode_label,
                                            const char *local_fix_label)
{
    const char *source_path = semantic_assignment_target_path_scratch(source_expr, ctx);
    const char *consumer_name = semantic_current_consumer_name(ctx);
    bool summary_only_escape = transitive_ref_escape
        && callee_name != NULL
        && strcmp(callee_name, consumer_name) == 0;
    DiagPayload payload;
    diag_payload_init(&payload);
    payload.code = PGY_CODE_SEM_BORROW_ESCAPE;
    payload.cause_ir = PGY_CAUSE_BORROW_ESCAPE;
    payload.fix_source = PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE;
    payload.site = site;
    payload.value_label = value_label;
    payload.provenance_label = provenance_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = consumer_name;
    payload.secondary_name = callee_name;
    payload.kind_label = mode_label;
    payload.extra = local_fix_label;
    (void)mode_label;

    if (summary_only_escape) {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- summary analysis proves a transitive helper path can let this borrow escape beyond the current call boundary\n"
            "Fix:\n"
            "- perform the %s operation locally in this function\n"
            "- or change the current parameter to 'own' if transfer/forwarding is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name,
            consumer_name,
            source_path != NULL ? source_path : borrowed_name,
            consumer_name,
            borrowed_name,
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path : borrowed_name,
            provenance_label != NULL ? provenance_label : "boundary provenance",
            local_fix_label != NULL ? local_fix_label : "value");
    } else if (transitive_ref_escape) {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- callee '%s' derives a transitive helper-escape path for its borrowed parameter\n"
            "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
            "Fix:\n"
            "- perform the %s operation locally in this function\n"
            "- or change the current parameter to 'own' if transfer/forwarding is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name,
            callee_name != NULL ? callee_name : "<callee>",
            source_path != NULL ? source_path : borrowed_name,
            consumer_name,
            borrowed_name,
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path : borrowed_name,
            provenance_label != NULL ? provenance_label : "boundary provenance",
            callee_name != NULL ? callee_name : "<callee>",
            local_fix_label != NULL ? local_fix_label : "value");
    } else {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- forwarding it to '%s' as '%s' would create a transitive helper transfer beyond the current call boundary\n"
            "Fix:\n"
            "- call a 'ref' helper instead\n"
            "- or change the current parameter to 'own' if forwarding/transfer is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name,
            callee_name != NULL ? callee_name : "<callee>",
            source_path != NULL ? source_path : borrowed_name,
            consumer_name,
            borrowed_name,
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path : borrowed_name,
            provenance_label != NULL ? provenance_label : "boundary provenance",
            callee_name != NULL ? callee_name : "<callee>",
            mode_label != NULL ? mode_label : "default");
    }
}
