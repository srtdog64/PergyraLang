/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker built-in dispatch and stdlib helpers
 */

#include <string.h>
#include "type_checker_internal.h"

static bool
check_call_arity(ASTNode *expr, size_t expected, const char *name,
                 SemanticContext *ctx)
{
    if (expr->data.call.arg_count != expected) {
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu",
            name, expected, expr->data.call.arg_count);
        return false;
    }
    return true;
}

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
    if (strcmp(name, "Log")             == 0) return BUILTIN_LOG;
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
    if (strcmp(name, "BoxArray")        == 0) return BUILTIN_BOX_ARRAY;
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
    return BUILTIN_NOT_BUILTIN;
}

Type *
type_check_claim_slot(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 0) {
        semantic_error(ctx, call, "ClaimSlot takes no arguments");
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

static Type *
type_check_claim_device_slot(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 0, "ClaimDeviceSlot", ctx))
        return TYPE_UNKNOWN;
    semantic_record_effect(ctx, EFFECT_REMOTE);
    return wrap_constructed(TYPE_DEVICE_SLOT, TYPE_INT);
}

static Type *
type_check_view_read(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "ViewRead", ctx))
        return TYPE_UNKNOWN;

    Type *slot_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error(ctx, call->data.call.arguments[0],
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

    Type *slot_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error(ctx, call->data.call.arguments[0],
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
        semantic_error(ctx, call,
            "Move requires a named owning Slot<T>/SecureSlot<T> binding");
        return TYPE_UNKNOWN;
    }

    Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
    Type *slot_type = type_check_expression(slot_arg, ctx);
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Move requires owning Slot<T>/SecureSlot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        semantic_error(ctx, slot_arg,
            "Move requires an owning slot binding");
        return TYPE_UNKNOWN;
    }
    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error(ctx, slot_arg,
            "Cannot move released slot '%s'",
            sym->name);
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
        semantic_error(ctx, call,
            "Write requires at least 2 arguments: Write(slot, value)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    Type *slot_type = type_check_expression(slot_arg, ctx);

    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error(ctx, slot_arg,
            "First argument to Write must be a Slot, got '%s'",
            slot_type->name);
        return false;
    }
    if (type_is_read_view(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
        return false;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot write through MoveToken<T>");
        return false;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot write to released slot '%s'",
                    sym->name);
                return false;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 3) {
                    semantic_error(ctx, call,
                        "Write to SecureSlot '%s' requires a token argument",
                        sym->name);
                    return false;
                }

                ASTNode *token_arg = call->data.call.arguments[2];
                if (token_arg->type == AST_IDENTIFIER) {
                    const char *token_name = token_arg->data.identifier.name;
                    if (sym->slot_info.paired_token_name == NULL
                        || strcmp(sym->slot_info.paired_token_name,
                                  token_name) != 0) {
                        semantic_error(ctx, token_arg,
                            "Token '%s' is not paired with slot '%s'",
                            token_name, sym->name);
                        return false;
                    }
                }
            } else if (arg_count > 2) {
                semantic_warning(ctx, call,
                    "Write to plain Slot '%s' ignores extra token argument",
                    sym->name);
            }
        } else if (sym != NULL && type_is_write_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
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
        semantic_error(ctx, value_arg,
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
        semantic_error(ctx, call,
            "Read requires at least 1 argument: Read(slot)");
        return TYPE_UNKNOWN;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    Type *slot_type = type_check_expression(slot_arg, ctx);

    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error(ctx, slot_arg,
            "First argument to Read must be a Slot, got '%s'",
            slot_type->name);
        return TYPE_UNKNOWN;
    }
    if (type_is_write_view(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
        return TYPE_UNKNOWN;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot read through MoveToken<T>");
        return TYPE_UNKNOWN;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot read from released slot '%s'",
                    sym->name);
                return TYPE_UNKNOWN;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 2) {
                    semantic_error(ctx, call,
                        "Read from SecureSlot '%s' requires a token argument",
                        sym->name);
                    return TYPE_UNKNOWN;
                }
                ASTNode *token_arg = call->data.call.arguments[1];
                if (token_arg->type == AST_IDENTIFIER) {
                    const char *token_name = token_arg->data.identifier.name;
                    if (sym->slot_info.paired_token_name == NULL
                        || strcmp(sym->slot_info.paired_token_name,
                                  token_name) != 0) {
                        semantic_error(ctx, token_arg,
                            "Token '%s' is not paired with slot '%s'",
                            token_name, sym->name);
                        return TYPE_UNKNOWN;
                    }
                }
            }
        } else if (sym != NULL && type_is_read_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
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
    if (call->data.call.arg_count < 1) {
        semantic_error(ctx, call,
            "Release requires at least 1 argument: Release(slot)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];

    if (slot_arg->type != AST_IDENTIFIER) {
        semantic_error(ctx, slot_arg,
            "Argument to Release must be a slot identifier");
        return false;
    }

    const char *slot_name = slot_arg->data.identifier.name;
    Symbol *sym = scope_lookup(ctx->scope, slot_name);

    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        if (sym != NULL && sym->type != NULL && sym->type->kind == TYPE_KIND_SLOT) {
            semantic_error(ctx, slot_arg,
                "Only owning Slot<T>/SecureSlot<T> values can be released; views are non-owning and move tokens are transfer-only");
        } else {
            semantic_error(ctx, slot_arg,
                "'%s' is not a slot", slot_name);
        }
        return false;
    }

    if (sym->slot_info.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error(ctx, slot_arg,
            "Slot '%s' has already been released", slot_name);
        return false;
    }

    if (sym->slot_info.is_secure && call->data.call.arg_count < 2) {
        semantic_error(ctx, call,
            "Release of SecureSlot '%s' requires a token argument",
            slot_name);
        return false;
    }

    scope_release_slot(ctx->scope, slot_name);
    return true;
}

static Type *
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
        semantic_error(ctx, expr,
            "%s requires DeviceSlot<T>, got '%s'",
            builtin_name, slot_type->name);
        return TYPE_UNKNOWN;
    }

    if (expr->type == AST_IDENTIFIER) {
        sym = scope_lookup(ctx->scope, expr->data.identifier.name);
        if (!allow_released
            && sym != NULL
            && sym->slot_info.state == SLOT_STATE_RELEASED) {
            semantic_error(ctx, expr,
                "Cannot use released DeviceSlot '%s' in %s",
                expr->data.identifier.name, builtin_name);
            return TYPE_UNKNOWN;
        }
    }

    semantic_record_effect(ctx, EFFECT_REMOTE);
    return slot_type;
}

Type *
type_check_stdlib_call(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (strcmp(name, "Abs") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_check_expression(expr->data.call.arguments[0], ctx);
    }
    if (strcmp(name, "Min") == 0 || strcmp(name, "Max") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *a = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *b = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(b, a, expr->data.call.arguments[1], ctx);
        return a;
    }
    if (strcmp(name, "StringLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "Contains") == 0 || strcmp(name, "StringContains") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Replace") == 0 || strcmp(name, "StringReplace") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < 3; i++) {
            require_assignable(type_check_expression(expr->data.call.arguments[i], ctx),
                TYPE_STRING, expr->data.call.arguments[i], ctx);
        }
        return TYPE_STRING;
    }
    if (strcmp(name, "Substring") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Trim") == 0 || strcmp(name, "StringTrim") == 0
        || strcmp(name, "Upper") == 0 || strcmp(name, "ToUpper") == 0
        || strcmp(name, "Lower") == 0 || strcmp(name, "ToLower") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Concat") == 0 || strcmp(name, "StringConcat") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "ArrayLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arg, "Array")
            && !type_is_constructed_named(arg, "Slice")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayLength requires Array<T> or Slice<T>, got '%s'",
                arg->name);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ArrayPush") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *val = type_check_expression(expr->data.call.arguments[1], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayPush requires Array<T>, got '%s'", arr->name);
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySet") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        require_assignable(
            type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        Type *val = type_check_expression(expr->data.call.arguments[2], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArraySet requires Array<T>, got '%s'", arr->name);
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[2], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArrayPop") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayPop requires Array<T>, got '%s'", arr->name);
        return TYPE_VOID;
    }
    if (strcmp(name, "ToString") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Print") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }

    if (strcmp(name, "ClaimQubit") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_QUBIT;
    }
    if (strcmp(name, "ClaimDeviceSlot") == 0) {
        return type_check_claim_device_slot(expr, ctx);
    }
    if (strcmp(name, "DeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_get_constructed_arg(
            type_check_device_handle_arg(expr->data.call.arguments[0], ctx, name, false), 0);
    }
    if (strcmp(name, "DeviceWrite") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        Type *inner = type_get_constructed_arg(slot_type, 0);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            inner, expr->data.call.arguments[1], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReleaseDeviceSlot") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ASTNode *slot_arg = expr->data.call.arguments[0];
        Type *slot_type = type_check_device_handle_arg(slot_arg, ctx, name, true);
        if (slot_type == TYPE_UNKNOWN)
            return TYPE_UNKNOWN;
        if (slot_arg->type != AST_IDENTIFIER) {
            semantic_error(ctx, slot_arg,
                "ReleaseDeviceSlot requires a DeviceSlot identifier");
            return TYPE_UNKNOWN;
        }
        {
            Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
            if (sym != NULL && sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "DeviceSlot '%s' has already been released",
                    slot_arg->data.identifier.name);
                return TYPE_UNKNOWN;
            }
        }
        scope_release_slot(ctx->scope, slot_arg->data.identifier.name);
        return TYPE_VOID;
    }
    if (strcmp(name, "SubmitDeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        return wrap_constructed(TYPE_REMOTE_FUTURE,
            type_get_constructed_arg(slot_type, 0));
    }
    if (strcmp(name, "Measure") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC | EFFECT_COLLAPSE);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot be measured */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot Measure() a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_COLLAPSED);
        /* Propagate collapse to all qubits in the same entanglement pool */
        {
            int32_t pool = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            if (pool >= 0)
                propagate_collapse_to_pool(ctx, pool);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "Entangle") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[1], ctx),
            TYPE_QUBIT, expr->data.call.arguments[1], ctx);
        /* State validation: only SUPERPOSITION/NONE qubits can be entangled */
        {
            QubitSemanticState sa = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            QubitSemanticState sb = get_qubit_semantic_state(
                expr->data.call.arguments[1], ctx);
            if (sa == QUBIT_STATE_COLLAPSED || sa == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sa));
            if (sb == QUBIT_STATE_COLLAPSED || sb == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sb));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_ENTANGLED);
        set_qubit_semantic_state(expr->data.call.arguments[1], ctx,
                                 QUBIT_STATE_ENTANGLED);
        /* Compile-time entanglement pool: allocate / merge */
        {
            int32_t pa = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            int32_t pb = get_qubit_entangle_pool(
                expr->data.call.arguments[1], ctx);
            if (pa >= 0 && pb >= 0) {
                if (pa != pb)
                    merge_entangle_pools(ctx, pa, pb);
            } else if (pa >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx, pa);
            } else if (pb >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx, pb);
            } else {
                int32_t new_pool = alloc_entangle_pool(ctx);
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx,
                                        new_pool);
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx,
                                        new_pool);
            }
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "QubitState") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "IsCollapsed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "ReleaseQubit") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        consume_qubit_value(expr->data.call.arguments[0], ctx, "released");
        return TYPE_VOID;
    }
    if (strcmp(name, "H") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot receive gate operations */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot apply H() to a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_SUPERPOSITION);
        return TYPE_VOID;
    }
    if (strcmp(name, "IntoClassical") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: only COLLAPSED qubits can be converted.
         * NONE is allowed for backward compat (unknown boundary state). */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs != QUBIT_STATE_COLLAPSED && qs != QUBIT_STATE_NONE)
                semantic_error(ctx, expr,
                    "IntoClassical() requires a COLLAPSED qubit (after Measure) "
                    "or unknown boundary state, got %s", qubit_state_name(qs));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_CLASSICAL);
        consume_qubit_value(expr->data.call.arguments[0], ctx,
                            "converted to classical");
        return TYPE_BOOL;
    }

    return NULL;
}

static Type *
type_check_rc_new(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcNew requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_RC,
        type_check_expression(call->data.call.arguments[0], ctx));
}

static Type *
type_check_rc_clone(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcClone requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcClone requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return rc_type;
}

static Type *
type_check_rc_get(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcGet requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(rc_type, 0);
}

static Type *
type_check_rc_downgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcDowngrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcDowngrade requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_WEAK, type_get_constructed_arg(rc_type, 0));
}

static Type *
type_check_weak_upgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "WeakUpgrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error(ctx, call, "WeakUpgrade requires Weak<T>, got '%s'",
            weak_type->name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_RC, type_get_constructed_arg(weak_type, 0));
}

static Type *
type_check_weak_drop(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "WeakDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error(ctx, call, "WeakDrop requires Weak<T>, got '%s'", weak_type->name);
        return TYPE_UNKNOWN;
    }
    return TYPE_VOID;
}

static Type *
type_check_allocator_builtin(ASTNode *call, SemanticContext *ctx,
                             bool requires_capacity)
{
    if ((!requires_capacity && call->data.call.arg_count != 0)
        || (requires_capacity && call->data.call.arg_count != 1)) {
        semantic_error(ctx, call,
            requires_capacity
                ? "AllocatorPool requires exactly 1 capacity argument"
                : "Allocator constructor takes no arguments");
        return TYPE_UNKNOWN;
    }

    if (requires_capacity) {
        Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
        if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
            semantic_error(ctx, call->data.call.arguments[0],
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
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "Box requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_BOX,
        type_check_expression(call->data.call.arguments[0], ctx));
}

static Type *
type_check_box_array_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count < 1 || call->data.call.arg_count > 2) {
        semantic_error(ctx, call,
            "BoxArray requires capacity and optional allocator");
        return TYPE_UNKNOWN;
    }

    Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
        semantic_error(ctx, call->data.call.arguments[0],
            "BoxArray capacity must be Int or Long, got '%s'", cap_type->name);
        return TYPE_UNKNOWN;
    }

    if (call->data.call.arg_count == 2) {
        Type *alloc_type = type_check_expression(call->data.call.arguments[1], ctx);
        if (!type_equals(alloc_type, TYPE_ALLOCATOR)) {
            semantic_error(ctx, call->data.call.arguments[1],
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
    case BUILTIN_BOX_ARRAY:
        return type_check_box_array_builtin(call, ctx);
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
            semantic_error(ctx, call,
                "'Input' expects at most 1 argument, got %zu",
                call->data.call.arg_count);
            return TYPE_STRING;
        }
        if (call->data.call.arg_count == 1) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_PRINT:
        return type_check_stdlib_call(call, "Print", ctx);
    default:
        return TYPE_UNKNOWN;
    }
}
