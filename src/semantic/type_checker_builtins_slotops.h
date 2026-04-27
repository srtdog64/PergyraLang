BuiltinKind
builtin_resolve(const char *name)
{
    if (strcmp(name, "ClaimSlot")       == 0) return BUILTIN_CLAIM_SLOT;
    if (strcmp(name, "ClaimSecureSlot") == 0) return BUILTIN_CLAIM_SECURE_SLOT;
    if (strcmp(name, "ClaimDeviceSlot") == 0) return BUILTIN_CLAIM_DEVICE_SLOT;
    if (strcmp(name, "ViewRead")        == 0) return BUILTIN_VIEW_READ;
    if (strcmp(name, "ViewWrite")       == 0) return BUILTIN_VIEW_WRITE;
    if (strcmp(name, "Move")            == 0) return BUILTIN_MOVE;
    if (strcmp(name, "Write")           == 0) return BUILTIN_WRITE;
    if (strcmp(name, "Read")            == 0) return BUILTIN_READ;
    if (strcmp(name, "Release")         == 0) return BUILTIN_RELEASE;
    if (strcmp(name, "DeviceWrite")     == 0) return BUILTIN_DEVICE_WRITE;
    if (strcmp(name, "DeviceRead")      == 0) return BUILTIN_DEVICE_READ;
    if (strcmp(name, "ReleaseDeviceSlot") == 0) return BUILTIN_RELEASE_DEVICE_SLOT;
    if (strcmp(name, "SubmitDeviceRead") == 0) return BUILTIN_SUBMIT_DEVICE_READ;
    if (strcmp(name, "SlotRawPointer") == 0) return BUILTIN_SLOT_RAW_POINTER;
    if (strcmp(name, "Log")             == 0) return BUILTIN_LOG;
    if (strcmp(name, "LogBanner")       == 0) return BUILTIN_LOG_BANNER;
    if (strcmp(name, "LogBlock")        == 0) return BUILTIN_LOG_BLOCK;
    if (strcmp(name, "LogRaw")          == 0) return BUILTIN_LOG_RAW;
    if (strcmp(name, "Clone")           == 0) return BUILTIN_CLONE;
    if (strcmp(name, "RcNew")           == 0) return BUILTIN_RC_NEW;
    if (strcmp(name, "RcClone")         == 0) return BUILTIN_RC_CLONE;
    if (strcmp(name, "RcDrop")          == 0) return BUILTIN_RC_DROP;
    if (strcmp(name, "RcDowngrade")     == 0) return BUILTIN_RC_DOWNGRADE;
    if (strcmp(name, "RcGet")           == 0) return BUILTIN_RC_GET;
    if (strcmp(name, "WeakUpgrade")     == 0) return BUILTIN_WEAK_UPGRADE;
    if (strcmp(name, "WeakDrop")        == 0) return BUILTIN_WEAK_DROP;
    if (strcmp(name, "AllocatorSystem") == 0) return BUILTIN_ALLOCATOR_SYSTEM;
    if (strcmp(name, "AllocatorTracing")== 0) return BUILTIN_ALLOCATOR_TRACING;
    if (strcmp(name, "AllocatorDebug")  == 0) return BUILTIN_ALLOCATOR_DEBUG;
    if (strcmp(name, "AllocatorPool")   == 0) return BUILTIN_ALLOCATOR_POOL;
    if (strcmp(name, "Box")             == 0) return BUILTIN_BOX;
    if (strcmp(name, "BoxGet")          == 0) return BUILTIN_BOX_GET;
    if (strcmp(name, "BoxSet")          == 0) return BUILTIN_BOX_SET;
    if (strcmp(name, "BoxDrop")         == 0) return BUILTIN_BOX_DROP;
    if (strcmp(name, "BoxIsValid")      == 0) return BUILTIN_BOX_IS_VALID;
    if (strcmp(name, "BoxArray")        == 0) return BUILTIN_BOX_ARRAY;
    if (strcmp(name, "ToObject")        == 0) return BUILTIN_TO_OBJECT;
    if (strcmp(name, "ToTObject")       == 0) return BUILTIN_TO_TOBJECT;
    if (strcmp(name, "HasProjection")   == 0) return BUILTIN_HAS_PROJECTION;
    if (strcmp(name, "HasLayer")        == 0) return BUILTIN_HAS_LAYER;
    if (strcmp(name, "HasState")        == 0) return BUILTIN_HAS_STATE;
    if (strcmp(name, "HasZone")         == 0) return BUILTIN_HAS_ZONE;
    if (strcmp(name, "HasZoneProjection") == 0) return BUILTIN_HAS_ZONE_PROJECTION;
    if (strcmp(name, "HasZoneLayer")    == 0) return BUILTIN_HAS_ZONE_LAYER;
    if (strcmp(name, "HasZoneState")    == 0) return BUILTIN_HAS_ZONE_STATE;
    if (strcmp(name, "StringSplit")     == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringJoin")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringContains")  == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringReplace")   == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Substring")       == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringTrim")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ToUpper")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ToLower")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringConcat")    == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ClaimQubit")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Measure")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Entangle")        == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "QubitState")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "IsCollapsed")     == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ReleaseQubit")    == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "FileOpen")        == 0) return BUILTIN_FILE_OPEN;
    if (strcmp(name, "FileRead")        == 0) return BUILTIN_FILE_READ;
    if (strcmp(name, "FileWrite")       == 0) return BUILTIN_FILE_WRITE;
    if (strcmp(name, "FileClose")       == 0) return BUILTIN_FILE_CLOSE;
    if (strcmp(name, "ReadFile")        == 0) return BUILTIN_READ_FILE;
    if (strcmp(name, "WriteFile")       == 0) return BUILTIN_WRITE_FILE;
    if (strcmp(name, "Input")           == 0) return BUILTIN_INPUT;
    if (strcmp(name, "Print")           == 0) return BUILTIN_PRINT;
    if (strcmp(name, "ReadLine")        == 0) return BUILTIN_READ_LINE;
    if (strcmp(name, "Now")             == 0) return BUILTIN_NOW;
    if (strcmp(name, "Sleep")           == 0) return BUILTIN_SLEEP;
    if (strcmp(name, "IntentLastTrace") == 0) return BUILTIN_INTENT_LAST_TRACE;
    if (strcmp(name, "IntentLastFailure") == 0) return BUILTIN_INTENT_LAST_FAILURE;
    if (strcmp(name, "IntentLastName")  == 0) return BUILTIN_INTENT_LAST_NAME;
    if (strcmp(name, "IntentLastHandle") == 0) return BUILTIN_INTENT_LAST_HANDLE;
    if (strcmp(name, "IntentLastTraceId") == 0) return BUILTIN_INTENT_LAST_TRACE_ID;
    if (strcmp(name, "IntentLastStepCount") == 0) return BUILTIN_INTENT_LAST_STEP_COUNT;
    if (strcmp(name, "IntentLastFailed") == 0) return BUILTIN_INTENT_LAST_FAILED;
    if (strcmp(name, "IntentHistoryCount") == 0) return BUILTIN_INTENT_HISTORY_COUNT;
    if (strcmp(name, "IntentHistoryStepName") == 0) return BUILTIN_INTENT_HISTORY_STEP_NAME;
    if (strcmp(name, "IntentHistoryStepZone") == 0) return BUILTIN_INTENT_HISTORY_STEP_ZONE;
    if (strcmp(name, "IntentHistoryStepPhase") == 0) return BUILTIN_INTENT_HISTORY_STEP_PHASE;
    if (strcmp(name, "IntentHistoryStepParticipant") == 0) return BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT;
    if (strcmp(name, "IntentHistoryStepSlot") == 0) return BUILTIN_INTENT_HISTORY_STEP_SLOT;
    if (strcmp(name, "IntentHistoryStepFromZone") == 0) return BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE;
    if (strcmp(name, "IntentHistoryStepFromSlot") == 0) return BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT;
    if (strcmp(name, "IntentHistoryStepToZone") == 0) return BUILTIN_INTENT_HISTORY_STEP_TO_ZONE;
    if (strcmp(name, "IntentHistoryStepToSlot") == 0) return BUILTIN_INTENT_HISTORY_STEP_TO_SLOT;
    if (strcmp(name, "IntentHistoryStepOk") == 0) return BUILTIN_INTENT_HISTORY_STEP_OK;
    if (strcmp(name, "IntentHistoryStepFailure") == 0) return BUILTIN_INTENT_HISTORY_STEP_FAILURE;
    if (strcmp(name, "IntentActiveCount") == 0) return BUILTIN_INTENT_ACTIVE_COUNT;
    if (strcmp(name, "IntentActiveName") == 0) return BUILTIN_INTENT_ACTIVE_NAME;
    if (strcmp(name, "IntentActiveHandle") == 0) return BUILTIN_INTENT_ACTIVE_HANDLE;
    if (strcmp(name, "IntentActiveParentHandle") == 0)
        return BUILTIN_INTENT_ACTIVE_PARENT_HANDLE;
    if (strcmp(name, "IntentActiveTraceId") == 0) return BUILTIN_INTENT_ACTIVE_TRACE_ID;
    if (strcmp(name, "IntentActivePriority") == 0) return BUILTIN_INTENT_ACTIVE_PRIORITY;
    if (strcmp(name, "IntentActiveSubjectCount") == 0)
        return BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT;
    if (strcmp(name, "IntentActiveStepCount") == 0)
        return BUILTIN_INTENT_ACTIVE_STEP_COUNT;
    if (strcmp(name, "IntentActiveConcurrent") == 0) return BUILTIN_INTENT_ACTIVE_CONCURRENT;
    if (strcmp(name, "IntentActiveFailed") == 0) return BUILTIN_INTENT_ACTIVE_FAILED;
    if (strcmp(name, "IntentActiveFailure") == 0) return BUILTIN_INTENT_ACTIVE_FAILURE;
    if (strcmp(name, "IntentActiveTrace") == 0) return BUILTIN_INTENT_ACTIVE_TRACE;
    if (strcmp(name, "IntentActiveStepName") == 0) return BUILTIN_INTENT_ACTIVE_STEP_NAME;
    if (strcmp(name, "IntentActiveStepZone") == 0) return BUILTIN_INTENT_ACTIVE_STEP_ZONE;
    if (strcmp(name, "IntentActiveStepPhase") == 0) return BUILTIN_INTENT_ACTIVE_STEP_PHASE;
    if (strcmp(name, "IntentActiveStepParticipant") == 0) return BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT;
    if (strcmp(name, "IntentActiveStepSlot") == 0) return BUILTIN_INTENT_ACTIVE_STEP_SLOT;
    if (strcmp(name, "IntentActiveStepFromZone") == 0) return BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE;
    if (strcmp(name, "IntentActiveStepFromSlot") == 0) return BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT;
    if (strcmp(name, "IntentActiveStepToZone") == 0) return BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE;
    if (strcmp(name, "IntentActiveStepToSlot") == 0) return BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT;
    if (strcmp(name, "IntentActiveStepOk") == 0) return BUILTIN_INTENT_ACTIVE_STEP_OK;
    if (strcmp(name, "IntentActiveStepFailure") == 0) return BUILTIN_INTENT_ACTIVE_STEP_FAILURE;
    if (strcmp(name, "IntentCurrentHandle") == 0) return BUILTIN_INTENT_CURRENT_HANDLE;
    if (strcmp(name, "IntentRecentCount") == 0) return BUILTIN_INTENT_RECENT_COUNT;
    if (strcmp(name, "IntentRecentHandle") == 0) return BUILTIN_INTENT_RECENT_HANDLE;
    if (strcmp(name, "IntentRecentTraceId") == 0) return BUILTIN_INTENT_RECENT_TRACE_ID;
    if (strcmp(name, "IntentRecentName") == 0) return BUILTIN_INTENT_RECENT_NAME;
    if (strcmp(name, "IntentRecentTrace") == 0) return BUILTIN_INTENT_RECENT_TRACE;
    if (strcmp(name, "IntentRecentFailure") == 0) return BUILTIN_INTENT_RECENT_FAILURE;
    if (strcmp(name, "IntentRecentStepCount") == 0) return BUILTIN_INTENT_RECENT_STEP_COUNT;
    if (strcmp(name, "IntentRecentFailed") == 0) return BUILTIN_INTENT_RECENT_FAILED;
    return BUILTIN_NOT_BUILTIN;
}

Type *
type_check_claim_slot(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call, "ClaimSlot takes no arguments");
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_claim_device_slot(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 0, "ClaimDeviceSlot", ctx))
        return TYPE_UNKNOWN;
    semantic_record_effect(ctx, EFFECT_REMOTE);
    return wrap_constructed(TYPE_DEVICE_SLOT, TYPE_INT);
}

static Type *
type_check_view_source_type(ASTNode *arg, SemanticContext *ctx)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER
        && arg->data.identifier.name != NULL) {
        Symbol *sym = scope_lookup(ctx->scope, arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT && sym->type != NULL) {
            sym->is_used = true;
            return sym->type;
        }
    }

    return type_check_expression(arg, ctx);
}

static Type *
type_check_view_read(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "ViewRead", ctx))
        return TYPE_UNKNOWN;

    Type *slot_type = type_check_view_source_type(
        call->data.call.arguments[0], ctx);
    if (type_is_qubit(slot_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_QUBIT_REJECT,
            PGY_CAUSE_PIN_QUBIT_REJECT,
            PGY_FIX_DO_NOT_PIN_QUBIT,
            call->data.call.arguments[0],
            "ViewRead cannot pin QubitSlot resources.\n"
            "Reason:\n"
            "- QubitSlot has a movable quantum state machine, not a stable resource-boundary lease\n"
            "- pinning it would bypass measurement/entanglement lifecycle checks\n"
            "Fix:\n"
            "- use the quantum operations directly\n"
            "- or convert to a classical value before creating a view");
        return TYPE_UNKNOWN;
    }
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED, PGY_FIX_PASS_OWNING_SLOT,
            call->data.call.arguments[0],
            "ViewRead requires owning Slot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_create_read_view(slot_type->data.slot.inner_type);
}

static Type *
type_check_view_write(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "ViewWrite", ctx))
        return TYPE_UNKNOWN;

    Type *slot_type = type_check_view_source_type(
        call->data.call.arguments[0], ctx);
    if (type_is_qubit(slot_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_QUBIT_REJECT,
            PGY_CAUSE_PIN_QUBIT_REJECT,
            PGY_FIX_DO_NOT_PIN_QUBIT,
            call->data.call.arguments[0],
            "ViewWrite cannot pin QubitSlot resources.\n"
            "Reason:\n"
            "- QubitSlot has a movable quantum state machine, not a stable resource-boundary lease\n"
            "- pinning it would bypass measurement/entanglement lifecycle checks\n"
            "Fix:\n"
            "- use the quantum operations directly\n"
            "- or convert to a classical value before creating a view");
        return TYPE_UNKNOWN;
    }
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED, PGY_FIX_PASS_OWNING_SLOT,
            call->data.call.arguments[0],
            "ViewWrite requires owning Slot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_create_write_view(slot_type->data.slot.inner_type);
}

static Type *
type_check_move_token(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "Move", ctx))
        return TYPE_UNKNOWN;

    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg == NULL || slot_arg->type != AST_IDENTIFIER) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "Move requires a named owning Slot<T>/SecureSlot<T> binding");
        return TYPE_UNKNOWN;
    }

    Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
    Type *slot_type = sym != NULL ? sym->type : TYPE_UNKNOWN;
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "Move requires owning Slot<T>/SecureSlot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "Move requires an owning slot binding");
        return TYPE_UNKNOWN;
    }
    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, slot_arg,
            "Cannot move released slot '%s'",
            sym->name);
        return TYPE_UNKNOWN;
    }
    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;
    if (semantic_find_active_slot_view_for_source(ctx->scope, sym->name,
            &active_view_name, &active_view_kind, NULL)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
            PGY_CAUSE_PIN_PARALLEL_CONFLICT,
            PGY_FIX_SERIALIZE_PIN_ACCESS,
            slot_arg,
            "Cannot move slot '%s' while %s '%s' is live.\n"
            "Reason:\n"
            "- pinned views are scoped capability leases over the source slot\n"
            "- moving the owner while a view is live would invalidate cleanup and aliasing order\n"
            "Fix:\n"
            "- end the pin/view scope before Move(%s)\n"
            "- or move ownership before acquiring '%s'",
            sym->name,
            active_view_kind != NULL ? active_view_kind : "view",
            active_view_name != NULL ? active_view_name : "<view>",
            sym->name,
            active_view_name != NULL ? active_view_name : "<view>");
        return TYPE_UNKNOWN;
    }

    scope_release_slot(ctx->scope, sym->name);
    return type_create_slot_access(slot_type->data.slot.inner_type,
        slot_type->data.slot.is_secure, SLOT_ACCESS_MOVE_TOKEN);
}

bool
type_check_write_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "Write requires at least 2 arguments: Write(slot, value)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
        Symbol *target_sym = scope_lookup(ctx->scope,
            slot_arg->data.identifier.name);
        if (target_sym != NULL && target_sym->kind == SYMBOL_SLOT) {
            const char *active_view_name = NULL;
            const char *active_view_kind = NULL;
            if (semantic_find_active_slot_view_for_source(ctx->scope,
                    target_sym->name, &active_view_name, &active_view_kind,
                    NULL)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    slot_arg,
                    "Cannot write slot '%s' while %s '%s' is live.\n"
                    "Reason:\n"
                    "- pinned views are scoped capability leases over the source slot\n"
                    "- owner writes during a live view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- write through the active view when it is a WriteView<T>\n"
                    "- or end the pin/view scope before Write(%s, ...)",
                    target_sym->name,
                    active_view_kind != NULL ? active_view_kind : "view",
                    active_view_name != NULL ? active_view_name : "<view>",
                    target_sym->name);
                return false;
            }
        }
    }
    Type *slot_type = type_check_expression(slot_arg, ctx);

    if (type_is_constructed_named(slot_type, "RemoteFuture")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_REMOTE_FUTURE_MISUSE, PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS, PGY_FIX_AWAIT_FUTURE, slot_arg,
            "RemoteFuture does not support Write(); remote resources are "
            "read-only via 'await'. Use Channel to send data to remote World");
        return false;
    }
    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "First argument to Write must be a Slot, got '%s'",
            slot_type->name);
        return false;
    }
    if (type_is_read_view(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, slot_arg,
            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
        return false;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, slot_arg,
            "Cannot write through MoveToken<T>");
        return false;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ctx->in_parallel && slot_type->data.slot.is_secure) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize authority-bearing slot reads/writes/releases outside the parallel block");
        return false;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_SLOT_RELEASED,
                    PGY_CAUSE_SLOT_LIFECYCLE_WRITE_AFTER_RELEASE,
                    PGY_FIX_RECLAIM_BEFORE_USE,
                    slot_arg,
                    "Cannot write to released slot '%s'",
                    sym->name);
                return false;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 3) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                        PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                        call,
                        "Write to SecureSlot '%s' requires a token argument",
                        sym->name);
                    return false;
                }

                ASTNode *token_arg = call->data.call.arguments[2];
                if (!validate_secure_token_arg(token_arg, sym, slot_type, ctx))
                    return false;
            } else if (arg_count > 2) {
                semantic_warning(ctx, call,
                    "Write to plain Slot '%s' ignores extra token argument",
                    sym->name);
            }
        } else if (sym != NULL && type_is_write_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED, PGY_CAUSE_SLOT_VIEW_WRITE_THROUGH_RELEASED_OWNER, PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW, slot_arg,
                    "Cannot write through WriteView '%s' because source slot '%s' was released",
                    sym->name, owner->name);
                return false;
            }
            if (owner != NULL && owner->slot_info.is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);
        }
    }

    ASTNode *value_arg = call->data.call.arguments[1];
    Type *value_type = type_check_expression(value_arg, ctx);
    Type *inner_type = slot_type->data.slot.inner_type;

    if (!type_is_assignable(value_type, inner_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_SLOT_WRITE_VALUE_TYPE_MISMATCH,
            PGY_FIX_ALIGN_VALUE_TO_SLOT_INNER,
            value_arg,
            "Cannot write '%s' to %s (expected '%s')",
            value_type->name, slot_type->name, inner_type->name);
        return false;
    }

    return true;
}

Type *
type_check_read_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "Read requires at least 1 argument: Read(slot)");
        return TYPE_UNKNOWN;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    Type *slot_type = type_check_expression(slot_arg, ctx);

    if (type_is_constructed_named(slot_type, "RemoteFuture")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_REMOTE_FUTURE_MISUSE, PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS, PGY_FIX_AWAIT_FUTURE, slot_arg,
            "RemoteFuture does not support Read(); use 'await' to obtain "
            "Result<T>, then Unwrap() or '?' to extract the value");
        return TYPE_UNKNOWN;
    }
    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "First argument to Read must be a Slot, got '%s'",
            slot_type->name);
        return TYPE_UNKNOWN;
    }
    if (type_is_write_view(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, slot_arg,
            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
        return TYPE_UNKNOWN;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, slot_arg,
            "Cannot read through MoveToken<T>");
        return TYPE_UNKNOWN;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ctx->in_parallel && slot_type->data.slot.is_secure) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize authority-bearing slot reads/writes/releases outside the parallel block");
        return TYPE_UNKNOWN;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_SLOT_RELEASED,
                    PGY_CAUSE_SLOT_LIFECYCLE_READ_AFTER_RELEASE,
                    PGY_FIX_RECLAIM_BEFORE_USE,
                    slot_arg,
                    "Cannot read from released slot '%s'",
                    sym->name);
                return TYPE_UNKNOWN;
            }

            const char *active_view_name = NULL;
            bool active_is_write = false;
            if (semantic_find_active_slot_view_for_source(ctx->scope, sym->name,
                    &active_view_name, NULL, &active_is_write)
                && active_is_write) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    slot_arg,
                    "Cannot read slot '%s' while WriteView '%s' is live.\n"
                    "Reason:\n"
                    "- WriteView<T> is the exclusive mutable view over the source slot\n"
                    "- owner reads during a live write view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- end the write view scope before Read(%s)\n"
                    "- or split the operation into a read-only view followed by a write view",
                    sym->name,
                    active_view_name != NULL ? active_view_name : "<view>",
                    sym->name);
                return TYPE_UNKNOWN;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 2) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                        "Read from SecureSlot '%s' requires a token argument",
                        sym->name);
                    return TYPE_UNKNOWN;
                }
                ASTNode *token_arg = call->data.call.arguments[1];
                if (!validate_secure_token_arg(token_arg, sym, slot_type, ctx))
                    return TYPE_UNKNOWN;
            }
        } else if (sym != NULL && type_is_read_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED, PGY_CAUSE_SLOT_VIEW_READ_THROUGH_RELEASED_OWNER, PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW, slot_arg,
                    "Cannot read through ReadView '%s' because source slot '%s' was released",
                    sym->name, owner->name);
                return TYPE_UNKNOWN;
            }
            if (owner != NULL && owner->slot_info.is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);
        }
    }

    return slot_type->data.slot.inner_type;
}

bool
type_check_release_slot(ASTNode *call, SemanticContext *ctx)
{
    semantic_record_body_summary(ctx, BODY_SUMMARY_DROPS_RESOURCE);

    if (call->data.call.arg_count < 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "Release requires at least 1 argument: Release(slot)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];

    /* RemoteFuture has no Release — it is consumed by await */
    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *rsym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (rsym != NULL && rsym->type != NULL
            && type_is_constructed_named(rsym->type, "RemoteFuture")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_REMOTE_FUTURE_MISUSE, PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS, PGY_FIX_AWAIT_FUTURE, slot_arg,
                "RemoteFuture does not support Release(); it is consumed by "
                "'await' and returns Result<T>");
            return false;
        }
    }

    if (slot_arg->type != AST_IDENTIFIER) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
            "Argument to Release must be a slot identifier");
        return false;
    }

    const char *slot_name = slot_arg->data.identifier.name;
    Symbol *sym = scope_lookup(ctx->scope, slot_name);

    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        if (sym != NULL && sym->type != NULL && sym->type->kind == TYPE_KIND_SLOT) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
                "Only owning Slot<T>/SecureSlot<T> values can be released; views are non-owning and move tokens are transfer-only");
        } else {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED, PGY_FIX_PASS_OWNING_SLOT,
                slot_arg,
                "'%s' is not a slot", slot_name);
        }
        return false;
    }

    if (sym->slot_info.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ctx->in_parallel && sym->slot_info.is_secure) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize authority-bearing slot reads/writes/releases outside the parallel block");
        return false;
    }

    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_DOUBLE_RELEASE, PGY_CAUSE_RELEASE_DOUBLE, PGY_FIX_REMOVE_REDUNDANT_RELEASE, slot_arg,
            "Slot '%s' has already been released", slot_name);
        return false;
    }

    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;
    if (semantic_find_active_slot_view_for_source(ctx->scope, slot_name,
            &active_view_name, &active_view_kind, NULL)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
            PGY_CAUSE_PIN_PARALLEL_CONFLICT,
            PGY_FIX_SERIALIZE_PIN_ACCESS,
            slot_arg,
            "Cannot release slot '%s' while %s '%s' is live.\n"
            "Reason:\n"
            "- pinned views are scoped capability leases over the source slot\n"
            "- releasing the owner while a view is live would invalidate cleanup and aliasing order\n"
            "Fix:\n"
            "- end the pin/view scope before Release(%s)\n"
            "- or move Release(%s) after the block that owns '%s'",
            slot_name,
            active_view_kind != NULL ? active_view_kind : "view",
            active_view_name != NULL ? active_view_name : "<view>",
            slot_name,
            slot_name,
            active_view_name != NULL ? active_view_name : "<view>");
        return false;
    }

    if (sym->slot_info.is_secure && call->data.call.arg_count < 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "Release of SecureSlot '%s' requires a token argument",
            slot_name);
        return false;
    }
    if (sym->slot_info.is_secure
        && !validate_secure_token_arg(call->data.call.arguments[1], sym, sym->type, ctx)) {
        return false;
    }

    scope_release_slot(ctx->scope, slot_name);
    return true;
}

Type *
type_check_device_handle_arg(ASTNode *expr, SemanticContext *ctx,
                             const char *builtin_name,
                             bool allow_released)
{
    Type *slot_type;
    Symbol *sym = NULL;

    if (expr == NULL)
        return TYPE_UNKNOWN;

    slot_type = type_check_expression(expr, ctx);
    if (!type_is_constructed_named(slot_type, "DeviceSlot")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SLOT_TYPE_REQUIRED, PGY_FIX_PASS_DEVICE_SLOT,
            expr,
            "%s requires DeviceSlot<T>, got '%s'",
            builtin_name, slot_type->name);
        return TYPE_UNKNOWN;
    }

    if (expr->type == AST_IDENTIFIER) {
        sym = scope_lookup(ctx->scope, expr->data.identifier.name);
        if (!allow_released
            && sym != NULL
            && sym->slot_info.state == SLOT_STATE_RELEASED) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED, PGY_CAUSE_DEVICE_SLOT_USE_AFTER_RELEASE, PGY_FIX_RECLAIM_BEFORE_USE, expr,
                "Cannot use released DeviceSlot '%s' in %s",
                expr->data.identifier.name, builtin_name);
            return TYPE_UNKNOWN;
        }
    }

    if (ctx->in_parallel) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit DeviceSlot operations yet; keep device access serialized outside the parallel block");
        return TYPE_UNKNOWN;
    }

    semantic_record_effect(ctx, EFFECT_REMOTE);
    return slot_type;
}
