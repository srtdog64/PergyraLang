/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend block emission and source-level pin cleanup.
 */

#include "transpiler_block_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../compiler/mir_abi_layout.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_defer_emit.h"
#define PGY_TRANSPILER_MIR_EMIT_STATE_OWNER
#include "transpiler_mir_emit_state.h"

static SlotVarEntry *
transpiler_find_slot_var_local(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].name, name) == 0)
            return &ctx->slot_vars[i];
    }

    return NULL;
}

static const char *
transpiler_block_slot_address_local(const SlotVarEntry *slot,
                                    char *buf,
                                    size_t buf_size)
{
    if (slot == NULL)
        return "NULL";
    if (slot->is_indirect)
        return slot->name;
    if (buf != NULL && buf_size > 0) {
        int written = snprintf(buf, buf_size, "&%s", slot->name);
        if (written >= 0 && (size_t)written < buf_size)
            return buf;
    }
    return NULL;
}

static const char *
transpiler_block_slot_token_address_local(const SlotVarEntry *slot,
                                          char *buf,
                                          size_t buf_size)
{
    const char *token_name;
    if (slot == NULL)
        return "NULL";
    token_name = slot->token_name[0] != '\0' ? slot->token_name : slot->name;
    if (buf != NULL && buf_size > 0) {
        int written = snprintf(buf, buf_size, "&%s", token_name);
        if (written >= 0 && (size_t)written < buf_size)
            return buf;
    }
    return NULL;
}

static void
transpiler_block_pin_address_too_long(TranspilerCtx *ctx)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "pin block slot address is too long for C backend emission");
}

static bool
emit_pin_block_enter_local(ASTNode *node, TranspilerCtx *ctx)
{
    SlotVarEntry *slot;
    int pin_id;
    const char *pin_op;
    const char *pin_fn;
    const char *cleanup_fn;
    char slot_addr[96];
    char token_addr[96];
    const char *slot_addr_expr;
    const char *token_addr_expr;
    MIRResourceAbiKind abi_kind;

    if (node == NULL || ctx == NULL || node->type != AST_BLOCK
        || !ast_block_is_pin_block(node)
        || ast_block_pin_source_name(node) == NULL)
        return false;

    slot = transpiler_find_slot_var_local(ctx, ast_block_pin_source_name(node));
    if (slot == NULL || slot->inner_type[0] == '\0')
        return false;

    pin_id = ctx->tmp_counter++;
    pin_op = ast_block_pin_view_is_write(node) ? "PinWrite" : "PinRead";
    abi_kind = slot->is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                               : MIR_RESOURCE_ABI_SLOT;
    pin_fn = mir_abi_resource_runtime_fn_by_kind(
        abi_kind, slot->inner_type, pin_op);
    cleanup_fn = mir_abi_resource_runtime_fn_by_kind(
        abi_kind, slot->inner_type, "UnpinCleanup");
    if (pin_fn == NULL || cleanup_fn == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "pin block requires MIR ABI runtime function rows");
        return false;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    write_indent(ctx);
    if (slot->is_secure) {
        slot_addr_expr = transpiler_block_slot_address_local(
            slot, slot_addr, sizeof(slot_addr));
        token_addr_expr = transpiler_block_slot_token_address_local(
            slot, token_addr, sizeof(token_addr));
        if (slot_addr_expr == NULL || token_addr_expr == NULL) {
            transpiler_block_pin_address_too_long(ctx);
            return false;
        }
        codebuf_write(ctx->out,
            "PgyPinnedSecureSlotView_%s __pgy_pin_%d "
            "__attribute__((cleanup(%s))) = "
            "%s(%s, %s);\n",
            slot->inner_type, pin_id,
            cleanup_fn,
            pin_fn,
            slot_addr_expr,
            token_addr_expr);
    } else {
        slot_addr_expr = transpiler_block_slot_address_local(
            slot, slot_addr, sizeof(slot_addr));
        if (slot_addr_expr == NULL) {
            transpiler_block_pin_address_too_long(ctx);
            return false;
        }
        codebuf_write(ctx->out,
            "PgyPinnedSlotView_%s __pgy_pin_%d "
            "__attribute__((cleanup(%s))) = "
            "%s(%s);\n",
            slot->inner_type, pin_id,
            cleanup_fn,
            pin_fn,
            slot_addr_expr);
    }

    return true;
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        bool pin_scope_open = emit_pin_block_enter_local(node, ctx);
        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        int saved_alias_count = ctx->alias_var_count;
        transpiler_defer_scope_push(ctx);
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            emit_statement(ast_block_statement(node, i), ctx);

        transpiler_emit_defers_from(ctx, ctx->defer_scope_depth - 1);

        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            SlotVarEntry *e = &ctx->slot_vars[i];
            if (e->released)
                continue;
            write_indent(ctx);
            if (e->is_secure) {
                codebuf_write(ctx->out,
                    "pgy_secure_release_%s(&%s, &%s_token);\n",
                    e->inner_type, e->name, e->name);
            } else {
                codebuf_write(ctx->out,
                    "pgy_release_%s(&%s);\n",
                    e->inner_type, e->name);
            }
        }

        transpiler_restore_local_binding_counts_local(ctx, saved_slot_count,
                                                      saved_typed_count,
                                                      saved_alias_count);
        transpiler_defer_scope_pop(ctx);
        if (pin_scope_open) {
            ctx->indent--;
            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
        }
    } else {
        emit_statement(node, ctx);
    }
}
