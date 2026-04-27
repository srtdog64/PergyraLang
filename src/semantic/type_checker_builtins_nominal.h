static bool
type_is_rc_weak_beta_payload(Type *type)
{
    return type_equals(type, TYPE_INT)
        || type_equals(type, TYPE_LONG)
        || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE)
        || type_equals(type, TYPE_BOOL)
        || type_equals(type, TYPE_STRING);
}

static bool
require_rc_weak_beta_payload(ASTNode *node, SemanticContext *ctx,
                             const char *builtin_name, Type *payload)
{
    if (payload == NULL || payload == TYPE_UNKNOWN)
        return false;
    if (type_is_rc_weak_beta_payload(payload))
        return true;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
        PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        node,
        "%s beta-stable shared ownership supports only Int, Long, Float, Double, Bool, or String payloads; got '%s'",
        builtin_name,
        type_name_or_unknown(payload));
    return false;
}

static Type *
type_check_rc_new(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcNew requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *payload = type_check_expression(call->data.call.arguments[0], ctx);
    if (!require_rc_weak_beta_payload(call, ctx, "RcNew", payload))
        return TYPE_UNKNOWN;
    return wrap_constructed(TYPE_RC, payload);
}

static Type *
type_check_rc_clone(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcClone requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcClone requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    if (!require_rc_weak_beta_payload(call, ctx, "RcClone",
            type_get_constructed_arg(rc_type, 0)))
        return TYPE_UNKNOWN;
    return rc_type;
}

static Type *
type_check_rc_get(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcGet requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    Type *payload = type_get_constructed_arg(rc_type, 0);
    if (!require_rc_weak_beta_payload(call, ctx, "RcGet", payload))
        return TYPE_UNKNOWN;
    return payload;
}

static Type *
type_check_rc_downgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcDowngrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "RcDowngrade requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    Type *payload = type_get_constructed_arg(rc_type, 0);
    if (!require_rc_weak_beta_payload(call, ctx, "RcDowngrade", payload))
        return TYPE_UNKNOWN;
    return wrap_constructed(TYPE_WEAK, payload);
}

static Type *
type_check_weak_upgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakUpgrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakUpgrade requires Weak<T>, got '%s'",
            weak_type->name);
        return TYPE_UNKNOWN;
    }
    Type *payload = type_get_constructed_arg(weak_type, 0);
    if (!require_rc_weak_beta_payload(call, ctx, "WeakUpgrade", payload))
        return TYPE_UNKNOWN;
    return wrap_constructed(TYPE_RC, payload);
}

static Type *
type_check_weak_drop(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "WeakDrop requires Weak<T>, got '%s'", weak_type->name);
        return TYPE_UNKNOWN;
    }
    if (!require_rc_weak_beta_payload(call, ctx, "WeakDrop",
            type_get_constructed_arg(weak_type, 0)))
        return TYPE_UNKNOWN;
    return TYPE_VOID;
}

static Type *
type_check_allocator_builtin(ASTNode *call, SemanticContext *ctx,
                             bool requires_capacity)
{
    if ((!requires_capacity && call->data.call.arg_count != 0)
        || (requires_capacity && call->data.call.arg_count != 1)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            requires_capacity
                ? "AllocatorPool requires exactly 1 capacity argument"
                : "Allocator constructor takes no arguments");
        return TYPE_UNKNOWN;
    }

    if (requires_capacity) {
        Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
        if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_CAPACITY_NON_INTEGER, PGY_FIX_USE_INT_OR_LONG_CAPACITY,
                call->data.call.arguments[0],
                "AllocatorPool capacity must be Int or Long, got '%s'",
                cap_type->name);
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_ALLOCATOR;
}

static Type *
type_check_box_builtin(ASTNode *call, SemanticContext *ctx)
{
    Type *payload;

    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "Box requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    payload = type_check_expression(call->data.call.arguments[0], ctx);
    if (semantic_reject_active_slot_owner_escape(
            call->data.call.arguments[0], ctx, "box", "Box")) {
        return TYPE_UNKNOWN;
    }
    if (type_is_resource_handle(payload)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call->data.call.arguments[0],
            "Box<T> beta-stable payloads cannot be resource handles; got '%s'.\n"
            "Reason:\n"
            "- resource handles already carry ownership, lifecycle, and runtime anchor contracts\n"
            "- boxing them would create a second storage owner the current CFG/ABI layer cannot prove\n"
            "Fix:\n"
            "- box a copied value or passive class/object payload instead\n"
            "- or keep the resource handle in its original owning binding",
            type_name_or_unknown(payload));
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_BOX, payload);
}

static Type *
type_check_box_get(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxGet requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(box_type, 0);
}

static Type *
type_check_box_set(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxSet requires exactly 2 arguments");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxSet requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }

    Type *inner = type_get_constructed_arg(box_type, 0);
    Type *value_type = type_check_expression(call->data.call.arguments[1], ctx);
    if (semantic_reject_active_slot_owner_escape(
            call->data.call.arguments[1], ctx, "box", "BoxSet")) {
        return TYPE_UNKNOWN;
    }
    if (type_is_resource_handle(value_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call->data.call.arguments[1],
            "BoxSet beta-stable payloads cannot be resource handles; got '%s'.\n"
            "Reason:\n"
            "- resource handles already carry ownership, lifecycle, and runtime anchor contracts\n"
            "- storing them in Box<T> would create a second storage owner the current CFG/ABI layer cannot prove\n"
            "Fix:\n"
            "- store a copied value or passive class/object payload instead\n"
            "- or keep the resource handle in its original owning binding",
            type_name_or_unknown(value_type));
        return TYPE_UNKNOWN;
    }
    require_assignable(value_type, inner, call->data.call.arguments[1], ctx);
    return TYPE_VOID;
}

static Type *
type_check_box_drop(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxDrop requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return TYPE_VOID;
}

static Type *
type_check_box_is_valid(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxIsValid requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "BoxIsValid requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return TYPE_BOOL;
}

static Type *
type_check_box_array_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count < 1 || call->data.call.arg_count > 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "BoxArray requires capacity and optional allocator");
        return TYPE_UNKNOWN;
    }

    Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call->data.call.arguments[0],
            "BoxArray capacity must be Int or Long, got '%s'", cap_type->name);
        return TYPE_UNKNOWN;
    }

    if (call->data.call.arg_count == 2) {
        Type *alloc_type = type_check_expression(call->data.call.arguments[1], ctx);
        if (!type_equals(alloc_type, TYPE_ALLOCATOR)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call->data.call.arguments[1],
                "BoxArray allocator must be Allocator, got '%s'", alloc_type->name);
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_builtin_call(ASTNode *call, BuiltinKind kind, SemanticContext *ctx)
{
    switch (kind) {
    case BUILTIN_CLAIM_SLOT:
        return type_check_claim_slot(call, ctx);
    case BUILTIN_CLAIM_SECURE_SLOT:
        semantic_record_effect(ctx, EFFECT_SECURE);
        return type_check_claim_slot(call, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return type_check_claim_device_slot(call, ctx);
    case BUILTIN_VIEW_READ:
        return type_check_view_read(call, ctx);
    case BUILTIN_VIEW_WRITE:
        return type_check_view_write(call, ctx);
    case BUILTIN_MOVE:
        return type_check_move_token(call, ctx);
    case BUILTIN_WRITE:
        type_check_write_slot(call, ctx);
        return TYPE_VOID;
    case BUILTIN_READ:
        return type_check_read_slot(call, ctx);
    case BUILTIN_RELEASE:
        type_check_release_slot(call, ctx);
        return TYPE_VOID;
    case BUILTIN_DEVICE_WRITE:
    case BUILTIN_DEVICE_READ:
    case BUILTIN_RELEASE_DEVICE_SLOT:
    case BUILTIN_SUBMIT_DEVICE_READ:
        return type_check_stdlib_call(call, call->data.call.callee->data.identifier.name, ctx);
    case BUILTIN_LOG:
        for (size_t i = 0; i < call->data.call.arg_count; i++)
            type_check_expression(call->data.call.arguments[i], ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_RAW:
        if (!check_call_arity(call, 1, "LogRaw", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                          TYPE_STRING, call->data.call.arguments[0], ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_BANNER:
        if (!check_call_arity(call, 1, "LogBanner", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                          TYPE_STRING, call->data.call.arguments[0], ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_BLOCK:
        if (!check_call_arity(call, 1, "LogBlock", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                          TYPE_STRING, call->data.call.arguments[0], ctx);
        return TYPE_VOID;
    case BUILTIN_CLONE:
        return type_check_stdlib_call(call, "Clone", ctx);
    case BUILTIN_RC_NEW:
        return type_check_rc_new(call, ctx);
    case BUILTIN_RC_CLONE:
        return type_check_rc_clone(call, ctx);
    case BUILTIN_RC_DROP:
        (void)type_check_rc_clone(call, ctx);
        return TYPE_VOID;
    case BUILTIN_RC_DOWNGRADE:
        return type_check_rc_downgrade(call, ctx);
    case BUILTIN_RC_GET:
        return type_check_rc_get(call, ctx);
    case BUILTIN_WEAK_UPGRADE:
        return type_check_weak_upgrade(call, ctx);
    case BUILTIN_WEAK_DROP:
        return type_check_weak_drop(call, ctx);
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
        return type_check_allocator_builtin(call, ctx, false);
    case BUILTIN_ALLOCATOR_POOL:
        return type_check_allocator_builtin(call, ctx, true);
    case BUILTIN_BOX:
        return type_check_box_builtin(call, ctx);
    case BUILTIN_BOX_GET:
        return type_check_box_get(call, ctx);
    case BUILTIN_BOX_SET:
        return type_check_box_set(call, ctx);
    case BUILTIN_BOX_DROP:
        return type_check_box_drop(call, ctx);
    case BUILTIN_BOX_IS_VALID:
        return type_check_box_is_valid(call, ctx);
    case BUILTIN_BOX_ARRAY:
        return type_check_box_array_builtin(call, ctx);
    case BUILTIN_TO_OBJECT:
        return type_check_to_object(call, ctx);
        case BUILTIN_TO_TOBJECT:
            return type_check_to_tobject(call, ctx);
    case BUILTIN_HAS_PROJECTION:
        return type_check_has_projection(call, ctx);
    case BUILTIN_HAS_LAYER:
        return type_check_has_layer(call, ctx);
    case BUILTIN_HAS_STATE:
        return type_check_has_state(call, ctx);
    case BUILTIN_HAS_ZONE:
        return type_check_has_zone(call, ctx);
    case BUILTIN_HAS_ZONE_PROJECTION:
        return type_check_has_world_zone_detail(call, ctx, "HasZoneProjection",
            find_zone_projection_slot_local, "projection slot");
    case BUILTIN_HAS_ZONE_LAYER:
        return type_check_has_world_zone_detail(call, ctx, "HasZoneLayer",
            find_zone_layer_slot_local, "layer slot");
    case BUILTIN_HAS_ZONE_STATE:
        return type_check_has_world_zone_detail(call, ctx, "HasZoneState",
            find_zone_state_decl_local_builtin, "state");
    case BUILTIN_PARALLEL:
        return TYPE_VOID;
    case BUILTIN_FILE_OPEN:
        if (check_call_arity(call, 2, "FileOpen", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_INT;
    case BUILTIN_FILE_READ:
        if (check_call_arity(call, 1, "FileRead", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_FILE_WRITE:
        if (check_call_arity(call, 2, "FileWrite", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_FILE_CLOSE:
        if (check_call_arity(call, 1, "FileClose", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_READ_FILE:
        if (check_call_arity(call, 1, "ReadFile", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_WRITE_FILE:
        if (check_call_arity(call, 2, "WriteFile", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_INPUT:
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        if (call->data.call.arg_count > 1) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                "'Input' expects at most 1 argument, got %llu",
                (unsigned long long) call->data.call.arg_count);
            return TYPE_STRING;
        }
        if (call->data.call.arg_count == 1) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_PRINT:
        return type_check_stdlib_call(call, "Print", ctx);
    case BUILTIN_READ_LINE:
        return type_check_stdlib_call(call, "ReadLine", ctx);
    case BUILTIN_NOW:
        return type_check_stdlib_call(call, "Now", ctx);
    case BUILTIN_SLEEP:
        return type_check_stdlib_call(call, "Sleep", ctx);
    case BUILTIN_INTENT_LAST_TRACE:
        check_call_arity(call, 0, "IntentLastTrace", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_FAILURE:
        check_call_arity(call, 0, "IntentLastFailure", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_NAME:
        check_call_arity(call, 0, "IntentLastName", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_HANDLE:
        check_call_arity(call, 0, "IntentLastHandle", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_TRACE_ID:
        check_call_arity(call, 0, "IntentLastTraceId", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_STEP_COUNT:
        check_call_arity(call, 0, "IntentLastStepCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_FAILED:
        check_call_arity(call, 0, "IntentLastFailed", ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_HISTORY_COUNT:
        check_call_arity(call, 0, "IntentHistoryCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_HISTORY_STEP_NAME:
        check_call_arity(call, 1, "IntentHistoryStepName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_PHASE:
        check_call_arity(call, 1, "IntentHistoryStepPhase", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT:
        check_call_arity(call, 1, "IntentHistoryStepParticipant", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepFromZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepFromSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepToZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepToSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_OK:
        check_call_arity(call, 1, "IntentHistoryStepOk", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE:
        check_call_arity(call, 1, "IntentHistoryStepFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_COUNT:
        check_call_arity(call, 0, "IntentActiveCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_NAME:
        check_call_arity(call, 1, "IntentActiveName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_HANDLE:
        check_call_arity(call, 1, "IntentActiveHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE:
        check_call_arity(call, 1, "IntentActiveParentHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_TRACE_ID:
        check_call_arity(call, 1, "IntentActiveTraceId", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_PRIORITY:
        check_call_arity(call, 1, "IntentActivePriority", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT:
        check_call_arity(call, 1, "IntentActiveSubjectCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT:
        check_call_arity(call, 1, "IntentActiveStepCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
        check_call_arity(call, 1, "IntentActiveConcurrent", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_FAILED:
        check_call_arity(call, 1, "IntentActiveFailed", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_FAILURE:
        check_call_arity(call, 1, "IntentActiveFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_TRACE:
        check_call_arity(call, 1, "IntentActiveTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_NAME:
        check_call_arity(call, 2, "IntentActiveStepName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE:
        check_call_arity(call, 2, "IntentActiveStepZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE:
        check_call_arity(call, 2, "IntentActiveStepPhase", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT:
        check_call_arity(call, 2, "IntentActiveStepParticipant", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT:
        check_call_arity(call, 2, "IntentActiveStepSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE:
        check_call_arity(call, 2, "IntentActiveStepFromZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT:
        check_call_arity(call, 2, "IntentActiveStepFromSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE:
        check_call_arity(call, 2, "IntentActiveStepToZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT:
        check_call_arity(call, 2, "IntentActiveStepToSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_OK:
        check_call_arity(call, 2, "IntentActiveStepOk", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE:
        check_call_arity(call, 2, "IntentActiveStepFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_CURRENT_HANDLE:
        check_call_arity(call, 0, "IntentCurrentHandle", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_COUNT:
        check_call_arity(call, 0, "IntentRecentCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_HANDLE:
        check_call_arity(call, 1, "IntentRecentHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_TRACE_ID:
        check_call_arity(call, 1, "IntentRecentTraceId", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_NAME:
        check_call_arity(call, 1, "IntentRecentName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_TRACE:
        check_call_arity(call, 1, "IntentRecentTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_FAILURE:
        check_call_arity(call, 1, "IntentRecentFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_STEP_COUNT:
        check_call_arity(call, 1, "IntentRecentStepCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_FAILED:
        check_call_arity(call, 1, "IntentRecentFailed", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    default:
        return TYPE_UNKNOWN;
    }
}
