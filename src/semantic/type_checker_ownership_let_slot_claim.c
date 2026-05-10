/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * ClaimSlot / ClaimSecureSlot let-binding ownership handling.
 */

#include <stdio.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_let_internal.h"

bool
ownership_let_try_claim_slot_decl(ASTNode *node,
                                  SemanticContext *ctx,
                                  const char *name,
                                  ASTNode *init,
                                  ASTNode *ann,
                                  bool *handled)
{
    const char *callee_name;
    BuiltinKind bk;
    bool is_secure;
    Type *slot_type = NULL;
    char token_name_buf[256];
    const char *paired_token = NULL;
    Symbol *sym;

    if (handled != NULL)
        *handled = false;
    if (init == NULL || init->type != AST_CALL
        || init->data.call.callee == NULL
        || init->data.call.callee->type != AST_IDENTIFIER) {
        return true;
    }

    callee_name = init->data.call.callee->data.identifier.name;
    bk = builtin_resolve(callee_name);
    if (bk != BUILTIN_CLAIM_SLOT && bk != BUILTIN_CLAIM_SECURE_SLOT)
        return true;

    if (handled != NULL)
        *handled = true;
    is_secure = (bk == BUILTIN_CLAIM_SECURE_SLOT);
    if (is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (ann != NULL) {
        Type *ann_type = ownership_let_resolve_type_ref(ann, ctx);
        if (ann_type == NULL)
            ann_type = TYPE_UNKNOWN;
        if (ann_type->kind == TYPE_KIND_SLOT) {
            slot_type = ann_type;
            is_secure = ann_type->data.slot.is_secure;
        } else {
            slot_type = type_create_slot(ann_type, is_secure);
        }
    } else {
        Type *inner_type =
            ownership_let_resolve_first_call_type_arg(init, ctx);
        if (inner_type == NULL || inner_type == TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_INFER_REQUIRED,
                PGY_CAUSE_INFER_NO_SOURCE,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                init,
                "Cannot infer %s<T> from %s without a type argument or annotation.\n"
                "Reason:\n"
                "- %s allocates a resource but carries no payload value\n"
                "- defaulting the slot payload to Int would discard generic evidence\n"
                "Fix:\n"
                "- write 'let %s: %s<T> = %s()' with a concrete T\n"
                "- or call '%s<T>()' with a concrete T",
                is_secure ? "SecureSlot" : "Slot",
                callee_name,
                callee_name,
                name != NULL ? name : "slot",
                is_secure ? "SecureSlot" : "Slot",
                callee_name,
                callee_name);
            inner_type = TYPE_UNKNOWN;
        }
        slot_type = type_create_slot(inner_type, is_secure);
    }

    if (is_secure) {
        if (!semantic_format_secure_token_name(
                token_name_buf, sizeof(token_name_buf), name, node, ctx))
            return true;
        paired_token = token_name_buf;
    }
    sym = symbol_create_slot(name, slot_type, is_secure, paired_token,
                             node->line, node->column);
    scope_declare(ctx->scope, sym);
    if (is_secure) {
        Symbol *tok = symbol_create_token(paired_token, name,
                                          node->line, node->column);
        if (tok != NULL && slot_type != NULL
            && slot_type->kind == TYPE_KIND_SLOT) {
            Type *token_args[1] = { slot_type->data.slot.inner_type };
            tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
        }
        if (!scope_declare(ctx->scope, tok))
            symbol_destroy(tok);
    }
    scope_register_slot(ctx->scope, sym);
    return true;
}
