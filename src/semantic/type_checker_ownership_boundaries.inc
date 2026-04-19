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

#include "type_checker_ownership_let.inc"

#include "type_checker_ownership_return.inc"
