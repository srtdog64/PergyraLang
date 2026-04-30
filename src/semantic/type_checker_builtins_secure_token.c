#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

static const char *
secure_token_slot_name_or_unknown(const Symbol *slot_sym)
{
    return (slot_sym != NULL && slot_sym->name != NULL)
        ? slot_sym->name
        : "<slot>";
}

bool
builtin_validate_secure_token_arg(ASTNode *token_arg,
                                  Symbol *slot_sym,
                                  Type *slot_type,
                                  SemanticContext *ctx)
{
    Symbol *token_sym;
    Type *expected_token_type;
    Type *token_args[1];
    const char *token_name;

    if (token_arg == NULL || slot_sym == NULL || slot_type == NULL
        || slot_type->kind != TYPE_KIND_SLOT || !slot_type->data.slot.is_secure) {
        return false;
    }

    if (token_arg->type != AST_IDENTIFIER
        || token_arg->data.identifier.name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
            "SecureSlot '%s' requires a named paired token identifier",
            secure_token_slot_name_or_unknown(slot_sym));
        return false;
    }

    token_name = token_arg->data.identifier.name;
    token_sym = scope_lookup(ctx->scope, token_name);
    if (token_sym == NULL || token_sym->kind != SYMBOL_TOKEN) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
            "'%s' is not a capability token for slot '%s'",
            token_name, secure_token_slot_name_or_unknown(slot_sym));
        return false;
    }

    if (slot_sym->slot_info.paired_token_name == NULL
        || strcmp(slot_sym->slot_info.paired_token_name, token_name) != 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
            "Token '%s' is not paired with slot '%s'",
            token_name, secure_token_slot_name_or_unknown(slot_sym));
        return false;
    }

    token_args[0] = slot_type->data.slot.inner_type != NULL
        ? slot_type->data.slot.inner_type
        : TYPE_UNKNOWN;
    expected_token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
    if (token_sym->type != NULL && expected_token_type != NULL
        && !type_equals(token_sym->type, expected_token_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
            "Token '%s' does not match SecureSlot '%s' capability type",
            token_name, secure_token_slot_name_or_unknown(slot_sym));
        return false;
    }

    return true;
}
