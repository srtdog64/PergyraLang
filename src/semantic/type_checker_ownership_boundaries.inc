/* Own/ref boundary diagnostics and local binding rules */

static const char *
semantic_current_consumer_name(SemanticContext *ctx)
{
    if (ctx != NULL
        && ctx->current_function_decl != NULL
        && ctx->current_function_decl->data.func_decl.name != NULL) {
        return ctx->current_function_decl->data.func_decl.name;
    }
    return "<anonymous>";
}

static void
semantic_report_borrowed_new_binding_escape(ASTNode *site,
                                            ASTNode *source_expr,
                                            SemanticContext *ctx,
                                            const char *borrowed_name,
                                            const char *binding_name,
                                            const char *value_label,
                                            const char *provenance_label)
{
    char *source_path = semantic_assignment_target_path(source_expr);
    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", site,
        "Borrowed ref %s '%s' cannot escape into a new binding '%s' from '%s'.\n"
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
        binding_name != NULL ? binding_name : "<binding>",
        source_path != NULL ? source_path : borrowed_name,
        semantic_current_consumer_name(ctx),
        borrowed_name,
        value_label,
        source_path != NULL ? source_path : borrowed_name,
        provenance_label,
        binding_name != NULL ? binding_name : "<binding>",
        source_path != NULL ? source_path : borrowed_name);
    free(source_path);
}

static void
semantic_report_borrowed_return_escape(ASTNode *site,
                                       ASTNode *source_expr,
                                       SemanticContext *ctx,
                                       const char *borrowed_name,
                                       const char *value_label,
                                       const char *provenance_label,
                                       const char *replacement_label)
{
    char *source_path = semantic_assignment_target_path(source_expr);
    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", site,
        "Borrowed ref %s '%s' cannot escape via return from '%s'.\n"
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
    free(source_path);
}

static void
semantic_report_borrowed_assignment_rebind_escape(ASTNode *site,
                                                  ASTNode *target_expr,
                                                  ASTNode *source_expr,
                                                  SemanticContext *ctx,
                                                  const char *borrowed_name,
                                                  const char *value_label,
                                                  const char *provenance_label,
                                                  const char *rebind_label)
{
    char *target_name = semantic_assignment_target_path(target_expr);
    char *source_path = semantic_assignment_target_path(source_expr);
    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", site,
        "Borrowed ref %s '%s' cannot escape through assignment rebind into '%s' from '%s'.\n"
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
    free(target_name);
    free(source_path);
}

static void
semantic_report_named_boundary_argument_required(ASTNode *site,
                                                 ASTNode *source_expr,
                                                 SemanticContext *ctx,
                                                 const char *value_label_cap,
                                                 const char *value_label_lower,
                                                 const char *bind_fix)
{
    char *source_path = semantic_assignment_target_path(source_expr);
    semantic_error(ctx, site,
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
    free(source_path);
}

static void
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
    char *source_path = semantic_assignment_target_path(source_expr);

    if (transitive_ref_escape) {
        semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE",
            "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", site,
            "Borrowed ref %s '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
            "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
            "Fix:\n"
            "- perform the %s operation locally in this function\n"
            "- or change the current parameter to 'own' if transfer/forwarding is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name,
            callee_name != NULL ? callee_name : "<callee>",
            source_path != NULL ? source_path : borrowed_name,
            semantic_current_consumer_name(ctx),
            borrowed_name,
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path : borrowed_name,
            provenance_label != NULL ? provenance_label : "boundary provenance",
            callee_name != NULL ? callee_name : "<callee>",
            local_fix_label != NULL ? local_fix_label : "value");
    } else {
        semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE",
            "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", site,
            "Borrowed ref %s '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
            "Fix:\n"
            "- call a 'ref' helper instead\n"
            "- or change the current parameter to 'own' if forwarding/transfer is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_name,
            callee_name != NULL ? callee_name : "<callee>",
            source_path != NULL ? source_path : borrowed_name,
            semantic_current_consumer_name(ctx),
            borrowed_name,
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path : borrowed_name,
            provenance_label != NULL ? provenance_label : "boundary provenance",
            callee_name != NULL ? callee_name : "<callee>",
            mode_label != NULL ? mode_label : "default");
    }

    free(source_path);
}

static void
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
    char *source_path = semantic_assignment_target_path(source_expr);

    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE",
        "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", site,
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

    free(source_path);
}

#include "type_checker_ownership_let.inc"

#include "type_checker_ownership_return.inc"
