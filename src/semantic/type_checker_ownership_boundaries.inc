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

OwnershipTypeClass
semantic_classify_ownership_type(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || ctx == NULL)
        return OWNERSHIP_TYPE_COPY_ONLY;
    if (type_is_anchored_resource_handle(type))
        return OWNERSHIP_TYPE_ANCHORED_HANDLE;
    if (type_is_movable_resource_handle(type))
        return OWNERSHIP_TYPE_MOVE_ONLY;
    if (type_is_subject_type(type, ctx))
        return OWNERSHIP_TYPE_SUBJECT_IDENTITY;
    if (type_requires_boundary_borrow_tracking(type, ctx))
        return OWNERSHIP_TYPE_BORROW_TRACKED;
    return OWNERSHIP_TYPE_COPY_ONLY;
}

const char *
semantic_ownership_value_label(OwnershipTypeClass klass)
{
    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        return "slot handle (movable)";
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        return "subject";
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        return "boundary value";
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        return "slot handle (anchored)";
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        return "value";
    }
}

const char *
semantic_ownership_provenance_label(OwnershipTypeClass klass)
{
    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        return "slot-handle (movable) provenance";
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        return "subject provenance";
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        return "boundary provenance";
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        return "slot-handle (anchored) provenance";
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        return "value provenance";
    }
}

const char *
semantic_ownership_replacement_label(OwnershipTypeClass klass)
{
    switch (klass) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        return "a copied/projection/value result";
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        return "a projection/object/tobject/value result";
    case OWNERSHIP_TYPE_BORROW_TRACKED:
        return "a copied/value/projection result";
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        return "a projection/value result";
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        return "a value result";
    }
}

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
                "via return",
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

/* DiagPayload pattern proof (P5): caller signature is unchanged but the
 * diagnostic is emitted through the payload renderer.  Future renderers
 * (LSP quick-fix, AI-router structured output) can consume the payload
 * fields directly without re-parsing the rendered message. */
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
    free(source_path);
}

static void
semantic_report_borrowed_return_escape(ASTNode *site,
                                       ASTNode *source_expr,
                                       SemanticContext *ctx,
                                       const char *borrowed_name,
                                       const char *value_label,
                                       const char *provenance_label,
                                       const char *replacement_label,
                                       bool summary_only)
{
    char *source_path = semantic_assignment_target_path(source_expr);
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
            "Borrowed ref %s '%s' cannot escape through return summary in '%s' from '%s'.\n"
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
    }
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
    free(target_name);
    free(source_path);
}

/* P5 payload pattern: 10-param helper now carries structured fields via
 * DiagPayload so future renderers can read container_kind/transfer_label
 * etc. without parsing the rendered message. */
static void
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
    char *source_path = semantic_assignment_target_path(source_expr);
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
    free(source_path);
}

static void
semantic_report_borrowed_channel_send_escape(ASTNode *site,
                                             ASTNode *source_expr,
                                             SemanticContext *ctx,
                                             const char *borrowed_name,
                                             const char *value_label,
                                             const char *provenance_label,
                                             const char *replacement_label,
                                             bool summary_only)
{
    char *source_path = semantic_assignment_target_path(source_expr);
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
            "Borrowed ref %s '%s' cannot escape through channel summary in '%s' from '%s'.\n"
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
    free(source_path);
}

static void
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
    char *source_path = semantic_assignment_target_path(source_expr);
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
            "Borrowed ref %s '%s' cannot escape through helper/function summary in '%s' from '%s'.\n"
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

    free(source_path);
}

#include "type_checker_ownership_let.inc"

#include "type_checker_ownership_return.inc"
