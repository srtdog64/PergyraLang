/* Own/ref boundary diagnostics and local binding rules */

bool
semantic_validate_borrowed_escape(ASTNode *site,
                                  ASTNode *source_expr,
                                  SemanticContext *ctx,
                                  const Type *value_type,
                                  const char *borrowed_name_override,
                                  OwnershipConsumerKind consumer_kind,
                                  ASTNode *target_expr,
                                  const char *dest_name,
                                  const char *secondary_name,
                                  bool transitive_call,
                                  const char *mode_label,
                                  const char *local_fix_label)
{
    OwnershipTypeClass klass;
    const char *borrowed_name;
    const char *value_label;
    const char *provenance_label;

    if (ctx == NULL || value_type == NULL)
        return false;

    borrowed_name = borrowed_name_override != NULL
        ? borrowed_name_override
        : semantic_borrowed_boundary_root_name(source_expr, ctx);
    if (borrowed_name == NULL)
        return false;

    klass = semantic_classify_ownership_type(value_type, ctx);
    if (klass == OWNERSHIP_TYPE_COPY_ONLY)
        return false;

    value_label = semantic_ownership_value_label(klass);
    provenance_label = semantic_ownership_provenance_label(klass);

    switch (consumer_kind) {
    case OWNERSHIP_CONSUMER_NEW_BINDING:
        semantic_report_borrowed_new_binding_escape(
            site, source_expr, ctx, borrowed_name, dest_name,
            value_label, provenance_label);
        return true;
    case OWNERSHIP_CONSUMER_DESTRUCTURE_TARGET_BINDING:
        semantic_report_borrowed_new_binding_escape(
            site, source_expr, ctx, borrowed_name, dest_name,
            value_label, provenance_label);
        return true;
    case OWNERSHIP_CONSUMER_ASSIGNMENT_REBIND:
        semantic_report_borrowed_assignment_rebind_escape(
            site, target_expr, source_expr, ctx, borrowed_name,
            value_label, provenance_label,
            klass == OWNERSHIP_TYPE_SUBJECT_IDENTITY
                ? "a second boundary-visible binding for the same borrowed identity"
                : "a second boundary-visible binding for the same borrowed provenance");
        return true;
    case OWNERSHIP_CONSUMER_CONTAINER_STORE:
        if (klass == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            semantic_report_borrowed_slot_handle_escape(
                site, source_expr, ctx, borrowed_name,
                "through container store",
                "- storing it would let the borrowed slot handle outlive the current call boundary\n",
                "keep the slot operation local to this function",
                "or change the parameter to 'own' if transfer is intended",
                false);
            return true;
        }
        semantic_report_borrowed_container_store_escape(
            site, source_expr, ctx, borrowed_name, value_label,
            provenance_label,
            dest_name != NULL ? dest_name : "container",
            secondary_name != NULL ? secondary_name : "<container store>",
            semantic_ownership_replacement_label(klass),
            "transfer");
        return true;
    case OWNERSHIP_CONSUMER_RETURN:
        if (klass == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            semantic_report_borrowed_slot_handle_escape(
                site, source_expr, ctx, borrowed_name,
                "through return",
                "- returning it would let the borrow outlive the call boundary\n",
                "return a projection/object/tobject/value instead",
                "or change the parameter to 'own' if transfer is intended",
                site != NULL && site->type == AST_FUNC_DECL);
            return true;
        }
        semantic_report_borrowed_return_escape(
            site, source_expr, ctx, borrowed_name, value_label,
            provenance_label, semantic_ownership_replacement_label(klass),
            site != NULL && site->type == AST_FUNC_DECL);
        return true;
    case OWNERSHIP_CONSUMER_CHANNEL_SEND:
        if (klass == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            semantic_report_borrowed_slot_handle_escape(
                site, source_expr, ctx, borrowed_name,
                "through channel send",
                "- channel send would transfer the borrow beyond that boundary\n",
                "send a projection/object/tobject/value result instead",
                "or take ownership with 'own' before transfer",
                site != NULL && site->type == AST_FUNC_DECL);
            return true;
        }
        semantic_report_borrowed_channel_send_escape(
            site, source_expr, ctx, borrowed_name, value_label,
            provenance_label, semantic_ownership_replacement_label(klass),
            site != NULL && site->type == AST_FUNC_DECL);
        return true;
    case OWNERSHIP_CONSUMER_HELPER_CALL:
        if (klass == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            semantic_report_borrowed_slot_handle_escape(
                site, source_expr, ctx, borrowed_name,
                "through helper/function call",
                "- forwarding it to another call would create a transitive borrow the compiler does not yet track precisely\n",
                "perform the slot operation locally in this function",
                "or change the parameter to 'own' if transfer/forwarding is intended",
                site != NULL && site->type == AST_FUNC_DECL);
            return true;
        }
        semantic_report_borrowed_helper_call_escape(
            site, source_expr, ctx, borrowed_name, value_label,
            provenance_label, dest_name, transitive_call, mode_label,
            local_fix_label);
        return true;
    case OWNERSHIP_CONSUMER_CONSTRUCTOR_FIELD_STORE:
        semantic_report_borrowed_constructor_field_escape(
            site, source_expr, ctx, borrowed_name, value_label,
            provenance_label,
            dest_name != NULL ? dest_name : "<constructor>",
            secondary_name != NULL ? secondary_name : "<field>",
            semantic_ownership_replacement_label(klass),
            klass == OWNERSHIP_TYPE_SUBJECT_IDENTITY);
        return true;
    default:
        return false;
    }
}

#include "type_checker_ownership_let.inc"

#include "type_checker_ownership_return.inc"
