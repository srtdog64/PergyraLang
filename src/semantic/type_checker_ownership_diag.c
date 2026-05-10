/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker ownership escape diagnostics.
 */

#include <string.h>
#include <stdio.h>

#include "diag_codes.h"
#include "diag_payload.h"
#include "type_checker_ownership_diag_internal.h"
#include "type_checker_internal.h"

bool
semantic_format_secure_token_name(char *out,
                                  size_t out_size,
                                  const char *slot_name,
                                  ASTNode *site,
                                  SemanticContext *ctx)
{
    int written;

    if (out == NULL || out_size == 0 || slot_name == NULL)
        return false;

    written = snprintf(out, out_size, "%s_token", slot_name);
    if (written >= 0 && (size_t)written < out_size)
        return true;

    semantic_error_with_hints(
        ctx,
        PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
        PGY_FIX_USE_SLOT_BOUND_IDENTIFIER,
        site,
        "SecureSlot token name for '%s' is too long to materialize safely.\n"
        "Reason:\n"
        "- SecureSlot lowering derives a paired token binding from the slot name\n"
        "- truncating that token name would break the slot/token capability invariant\n"
        "Fix:\n"
        "- shorten the SecureSlot binding name\n"
        "- or bind the token explicitly with a shorter stable name",
        slot_name);
    return false;
}

void
semantic_report_borrowed_new_binding_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *binding_name,
                                            const char *value_label,
                                            const char *provenance_label)
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
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);
    payload.secondary_name = binding_name;

    semantic_emit_payload(ctx, &payload,
        "Borrowed ref %s '%s' cannot escape into %s '%s' from '%s'.\n"
        "Reason:\n"
        "- consumer path is function '%s'\n"
        "- '%s' entered this function as a borrowed 'ref' %s\n"
        "- '%s' is derived from that borrowed %s\n"
        "- binding it as '%s' would extend that borrow beyond its original boundary provenance\n"
        "Fix:\n"
        "- keep using '%s' directly within this function\n"
        "- or change the current parameter to 'own' if transfer is intended",
        value_label,
        borrowed_name,
        site != NULL && site->type == AST_LET_DESTRUCTURE
            ? "destructure target binding"
            : "new binding",
        binding_name != NULL ? binding_name : "<binding>",
        source_path != NULL ? source_path : borrowed_name,
        semantic_current_consumer_name(ctx),
        borrowed_name,
        value_label,
        source_path != NULL ? source_path : borrowed_name,
        provenance_label,
        binding_name != NULL ? binding_name : "<binding>",
        source_path != NULL ? source_path : borrowed_name);
}

void
semantic_report_borrowed_return_escape(ASTNode *site,
                                       ASTNode *source_expr,
                                       SemanticContext *ctx,
                                       const char *borrowed_name,
                                       const char *value_label,
                                       const char *provenance_label,
                                       const char *replacement_label,
                                       bool summary_only)
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
    payload.replacement_label = replacement_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);

    if (summary_only) {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through return from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- summary analysis proves a return path can let this borrow outlive the current call boundary\n"
            "Fix:\n"
            "- return %s instead\n"
            "- or change the current parameter to 'own' if transfer is intended",
            value_label,
            borrowed_name,
            semantic_current_consumer_name(ctx),
            source_path != NULL ? source_path : borrowed_name,
            semantic_current_consumer_name(ctx),
            borrowed_name,
            value_label,
            source_path != NULL ? source_path : borrowed_name,
            provenance_label,
            replacement_label != NULL ? replacement_label : "a value result");
    } else {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through return from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- returning it would let the borrow outlive the current call boundary\n"
            "Fix:\n"
            "- return %s instead\n"
            "- or change the current parameter to 'own' if transfer is intended",
            value_label,
            borrowed_name,
            source_path != NULL ? source_path : borrowed_name,
            semantic_current_consumer_name(ctx),
            borrowed_name,
            value_label,
            source_path != NULL ? source_path : borrowed_name,
            provenance_label,
            replacement_label != NULL ? replacement_label : "a value result");
    }
}

void
semantic_report_borrowed_assignment_rebind_escape(ASTNode *site,
                                                  ASTNode *target_expr,
                                                  ASTNode *source_expr,
                                                  SemanticContext *ctx,
                                                  const char *borrowed_name,
                                                  const char *value_label,
                                                  const char *provenance_label,
                                                  const char *rebind_label)
{
    const char *target_name = semantic_assignment_target_path_scratch(target_expr, ctx);
    const char *source_path = semantic_assignment_target_path_scratch(source_expr, ctx);
    const char *consumer_label = "assignment rebind";
    DiagPayload payload;

    if (target_expr != NULL) {
        if (target_expr->type == AST_MEMBER_ACCESS)
            consumer_label = "member assignment rebind";
        else if (target_expr->type == AST_ARRAY_ACCESS)
            consumer_label = "array element rebind";
    }

    diag_payload_init(&payload);
    payload.code = PGY_CODE_SEM_BORROW_ESCAPE;
    payload.cause_ir = PGY_CAUSE_BORROW_ESCAPE;
    payload.fix_source = PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE;
    payload.site = site;
    payload.value_label = value_label;
    payload.provenance_label = provenance_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);
    payload.secondary_name = target_name;
    payload.extra = rebind_label;

    semantic_emit_payload(ctx, &payload,
        "Borrowed ref %s '%s' cannot escape through %s into '%s' from '%s'.\n"
        "Reason:\n"
        "- consumer path is function '%s'\n"
        "- '%s' entered this function as a borrowed 'ref' %s\n"
        "- '%s' is derived from that borrowed %s\n"
        "- assigning it into '%s' would create %s\n"
        "Fix:\n"
        "- keep using '%s' directly without rebinding it\n"
        "- or change the current parameter to 'own' if transfer is intended",
        value_label,
        borrowed_name,
        consumer_label,
        target_name != NULL ? target_name : "<target>",
        source_path != NULL ? source_path : borrowed_name,
        semantic_current_consumer_name(ctx),
        borrowed_name,
        value_label,
        source_path != NULL ? source_path : borrowed_name,
        provenance_label,
        target_name != NULL ? target_name : "<target>",
        rebind_label != NULL ? rebind_label
                             : "a second boundary-visible binding for the borrowed provenance",
        borrowed_name);
}

void
semantic_report_borrowed_container_store_escape(ASTNode *site,
                                                ASTNode *source_expr,
                                                SemanticContext *ctx,
                                                const char *borrowed_name,
                                                const char *value_label,
                                                const char *provenance_label,
                                                const char *container_kind,
                                                const char *container_name,
                                                const char *replacement_label,
                                                const char *transfer_label)
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
    payload.replacement_label = replacement_label;
    payload.transfer_label = transfer_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);
    payload.secondary_name = container_name;
    payload.kind_label = container_kind;

    semantic_emit_payload(ctx, &payload,
        "Borrowed ref %s '%s' cannot escape through %s store%s%s%s.\n"
        "Reason:\n"
        "- consumer path is function '%s'\n"
        "- '%s' entered this function as a borrowed 'ref' %s\n"
        "- '%s' is derived from that borrowed %s\n"
        "- %s inserts values into %s state that may outlive the current call boundary\n"
        "- storing the borrow would create a longer-lived ownership alias the compiler cannot keep boundary-safe\n"
        "Fix:\n"
        "- store a %s instead\n"
        "- or change the current parameter to 'own' if %s into %s is intended",
        value_label != NULL ? value_label : "boundary value",
        borrowed_name != NULL ? borrowed_name : "<value>",
        container_kind != NULL ? container_kind : "container",
        source_path != NULL ? " from '" : "",
        source_path != NULL ? source_path : "",
        source_path != NULL ? "'" : "",
        semantic_current_consumer_name(ctx),
        borrowed_name != NULL ? borrowed_name : "<value>",
        value_label != NULL ? value_label : "boundary value",
        source_path != NULL ? source_path
                            : (borrowed_name != NULL ? borrowed_name : "<value>"),
        provenance_label != NULL ? provenance_label : "boundary provenance",
        container_kind != NULL ? container_kind : "container",
        container_name != NULL ? container_name : "<container store>",
        replacement_label != NULL ? replacement_label : "a value result",
        transfer_label != NULL ? transfer_label : "transfer",
        container_kind != NULL ? container_kind : "container");
}

void
semantic_report_borrowed_channel_send_escape(ASTNode *site,
                                             ASTNode *source_expr,
                                             SemanticContext *ctx,
                                             const char *borrowed_name,
                                             const char *value_label,
                                             const char *provenance_label,
                                             const char *replacement_label,
                                             bool summary_only)
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
    payload.replacement_label = replacement_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);

    if (summary_only) {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through channel send from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- summary analysis proves a channel-send path can transfer that borrow beyond the current call boundary\n"
            "Fix:\n"
            "- send a %s instead\n"
            "- or change the current parameter to 'own' if transfer is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name != NULL ? borrowed_name : "<value>",
            semantic_current_consumer_name(ctx),
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<value>"),
            semantic_current_consumer_name(ctx),
            borrowed_name != NULL ? borrowed_name : "<value>",
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<value>"),
            provenance_label != NULL ? provenance_label : "boundary provenance",
            replacement_label != NULL ? replacement_label
                                      : "copied/value/projection result");
    } else {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref %s '%s' cannot escape through channel send from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- channel send would transfer that borrow beyond the current call boundary\n"
            "Fix:\n"
            "- send a %s instead\n"
            "- or change the current parameter to 'own' if transfer is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name != NULL ? borrowed_name : "<value>",
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<value>"),
            semantic_current_consumer_name(ctx),
            borrowed_name != NULL ? borrowed_name : "<value>",
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<value>"),
            provenance_label != NULL ? provenance_label : "boundary provenance",
            replacement_label != NULL ? replacement_label
                                      : "copied/value/projection result");
    }
}

void
semantic_report_borrowed_slot_handle_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *escape_kind,
                                            const char *detail_line,
                                            const char *replacement_label,
                                            const char *secondary_fix,
                                            bool summary_only)
{
    const char *source_path = semantic_assignment_target_path_scratch(source_expr, ctx);
    DiagPayload payload;
    diag_payload_init(&payload);
    payload.code = PGY_CODE_SEM_BORROW_ESCAPE;
    payload.cause_ir = PGY_CAUSE_BORROW_ESCAPE;
    payload.fix_source = PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE;
    payload.site = site;
    payload.replacement_label = replacement_label;
    payload.borrowed_name = borrowed_name;
    payload.consumer_name = semantic_current_consumer_name(ctx);
    payload.kind_label = escape_kind;
    payload.extra = secondary_fix;
    (void)detail_line;

    if (summary_only) {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref slot handle (anchored) '%s' cannot escape %s summary in '%s' from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' slot handle (anchored)\n"
            "- '%s' is derived from slot-handle (anchored) provenance rooted at '%s'\n"
            "- 'ref' is a non-owning borrow tied to the current call boundary\n"
            "- summary analysis proves a %s path can let this slot handle (anchored) outlive that boundary\n"
            "Fix:\n"
            "- %s\n"
            "- %s",
            borrowed_name != NULL ? borrowed_name : "<slot>",
            escape_kind != NULL ? escape_kind : "ownership-boundary",
            semantic_current_consumer_name(ctx),
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<slot>"),
            semantic_current_consumer_name(ctx),
            borrowed_name != NULL ? borrowed_name : "<slot>",
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<slot>"),
            borrowed_name != NULL ? borrowed_name : "<slot>",
            escape_kind != NULL ? escape_kind : "ownership-boundary",
            replacement_label != NULL ? replacement_label
                                      : "keep the slot operation local to this function",
            secondary_fix != NULL ? secondary_fix
                                  : "or change the parameter to 'own' if transfer is intended");
    } else {
        semantic_emit_payload(ctx, &payload,
            "Borrowed ref slot handle (anchored) '%s' cannot escape %s from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' slot handle (anchored)\n"
            "- '%s' is derived from slot-handle (anchored) provenance rooted at '%s'\n"
            "- 'ref' is a non-owning borrow tied to the current call boundary\n"
            "%s"
            "Fix:\n"
            "- %s\n"
            "- %s",
            borrowed_name != NULL ? borrowed_name : "<slot>",
            escape_kind != NULL ? escape_kind : "through an ownership boundary",
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<slot>"),
            semantic_current_consumer_name(ctx),
            borrowed_name != NULL ? borrowed_name : "<slot>",
            source_path != NULL ? source_path
                                : (borrowed_name != NULL ? borrowed_name : "<slot>"),
            borrowed_name != NULL ? borrowed_name : "<slot>",
            detail_line != NULL ? detail_line
                                : "- this borrow cannot outlive the current call boundary\n",
            replacement_label != NULL ? replacement_label
                                      : "keep the slot operation local to this function",
            secondary_fix != NULL ? secondary_fix
                                  : "or change the parameter to 'own' if transfer is intended");
    }
}
