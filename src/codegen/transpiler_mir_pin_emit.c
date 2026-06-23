/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR pin-region emission helpers.
 */

#include "transpiler_mir_pin_emit.h"

#include <stdio.h>
#include <string.h>

#include "../compiler/mir_cfg_contract_pin.h"
#include "transpiler_context.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_ssa_map.h"

static SlotVarEntry *
transpiler_mir_find_pin_slot_local(TranspilerCtx *ctx,
                                   const MIRBasicBlock *block)
{
    if (ctx == NULL || block == NULL || !block->is_pin_region
        || block->pin_source_name == NULL)
        return NULL;

    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].name, block->pin_source_name) == 0)
            return &ctx->slot_vars[i];
    }

    return NULL;
}

static bool
transpiler_mir_pin_local_name(const MIRBasicBlock *block,
                              char *buf,
                              size_t buf_size)
{
    int written;

    if (buf == NULL || buf_size == 0)
        return false;
    written = snprintf(buf, buf_size, "__pgy_mir_pin_%zu",
                       block != NULL ? block->id : 0);
    return written >= 0 && (size_t)written < buf_size;
}

static bool
transpiler_mir_pin_slot_local_name(const MIRBasicBlock *block,
                                   char *buf,
                                   size_t buf_size)
{
    int written;

    if (buf == NULL || buf_size == 0)
        return false;
    written = snprintf(buf, buf_size, "__pgy_mir_pin_slot_%zu",
                       block != NULL ? block->id : 0);
    return written >= 0 && (size_t)written < buf_size;
}

static bool
transpiler_mir_slot_address_local(const SlotVarEntry *slot,
                                  char *buf,
                                  size_t buf_size,
                                  const char **out)
{
    int written;

    if (out == NULL)
        return false;
    *out = "NULL";
    if (slot == NULL)
        return true;
    if (slot->is_indirect) {
        *out = slot->name;
        return true;
    }
    if (buf != NULL && buf_size > 0) {
        written = snprintf(buf, buf_size, "&%s", slot->name);
        if (written < 0 || (size_t)written >= buf_size)
            return false;
        *out = buf;
        return true;
    }
    *out = slot->name;
    return true;
}

static bool
transpiler_mir_slot_token_address_local(const SlotVarEntry *slot,
                                        char *buf,
                                        size_t buf_size,
                                        const char **out)
{
    const char *token_name;
    int written;

    if (out == NULL)
        return false;
    *out = "NULL";
    if (slot == NULL)
        return true;
    token_name = slot->token_name[0] != '\0' ? slot->token_name : slot->name;
    if (buf != NULL && buf_size > 0) {
        written = snprintf(buf, buf_size, "&%s", token_name);
        if (written < 0 || (size_t)written >= buf_size)
            return false;
        *out = buf;
        return true;
    }
    *out = token_name;
    return true;
}

static void
transpiler_mir_pin_format_reason(char *reason, size_t reason_cap,
                                 const char *message)
{
    transpiler_mir_reasonf(reason, reason_cap, "%s",
        message != NULL ? message : "MIR pin formatting failed");
}

static bool
transpiler_emit_mir_plain_pin_preflight_local(CodeBuf *buf,
                                              TranspilerCtx *ctx,
                                              const MIRBasicBlock *block,
                                              const SlotVarEntry *slot,
                                              const char *pin_name,
                                              const char *slot_expr,
                                              char *reason,
                                              size_t reason_cap)
{
    char slot_local[80];
    const char *read_or_write;

    if (buf == NULL || ctx == NULL || block == NULL || slot == NULL
        || pin_name == NULL || slot_expr == NULL)
        return false;
    if (!mir_block_has_pin_guard_amortization_region(block)) {
        transpiler_mir_reasonf(reason, reason_cap,
            "MIR pin block %zu cannot use guard-amortized preflight: %s",
            block->id,
            mir_block_pin_guard_amortization_missing_reason(block));
        return false;
    }
    if (!transpiler_mir_pin_slot_local_name(block, slot_local,
            sizeof(slot_local))) {
        transpiler_mir_pin_format_reason(reason, reason_cap,
            "MIR pin slot preflight local name is too long");
        return false;
    }

    read_or_write = block->pin_view_is_write ? "write" : "read";
    transpiler_write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "PgySlot_%s *%s = %s;\n",
                  slot->inner_type, slot_local, slot_expr);
    transpiler_write_indent_to(buf, ctx->indent);
    codebuf_write(buf,
        "if (%s == NULL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \"null slot pin %s\");\n",
        slot_local, read_or_write);
    transpiler_write_indent_to(buf, ctx->indent);
    codebuf_write(buf,
        "if (!%s->occupied) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, %s);\n",
        slot_local,
        block->pin_view_is_write
            ? "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE"
            : "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ");
    transpiler_write_indent_to(buf, ctx->indent);
    codebuf_write(buf,
        "PgyPinnedSlotView_%s %s = { .slot = %s, .active = true, .can_write = %s };\n",
        slot->inner_type, pin_name, slot_local,
        block->pin_view_is_write ? "true" : "false");
    return true;
}

bool
transpiler_emit_mir_pin_enter_local(CodeBuf *buf,
                                    TranspilerCtx *ctx,
                                    const MIRBasicBlock *block,
                                    char *reason,
                                    size_t reason_cap)
{
    SlotVarEntry *slot;
    char pin_name[64];
    char slot_addr[96];
    char token_addr[96];
    const char *slot_expr;
    const char *token_expr;
    const char *mode;

    if (block == NULL || !block->is_pin_region)
        return true;

    slot = transpiler_mir_find_pin_slot_local(ctx, block);
    if (slot == NULL || slot->inner_type[0] == '\0') {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR pin block %zu cannot resolve source slot '%s'",
                     block->id,
                     block->pin_source_name != NULL
                         ? block->pin_source_name : "<slot>");
        }
        return false;
    }

    if (!transpiler_mir_pin_local_name(block, pin_name, sizeof(pin_name))) {
        transpiler_mir_pin_format_reason(reason, reason_cap,
            "MIR pin block local name is too long");
        return false;
    }
    if (!transpiler_mir_slot_address_local(slot, slot_addr, sizeof(slot_addr),
            &slot_expr)) {
        transpiler_mir_pin_format_reason(reason, reason_cap,
            "MIR pin block slot address expression is too long");
        return false;
    }
    if (!transpiler_mir_slot_token_address_local(slot, token_addr,
            sizeof(token_addr), &token_expr)) {
        transpiler_mir_pin_format_reason(reason, reason_cap,
            "MIR pin block token address expression is too long");
        return false;
    }
    mode = block->pin_view_is_write ? "write" : "read";
    if (slot->is_secure) {
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf,
            "PgyPinnedSecureSlotView_%s %s = "
            "pgy_secure_pin_%s_%s(%s, %s);\n",
            slot->inner_type, pin_name,
            mode, slot->inner_type,
            slot_expr,
            token_expr);
    } else if (mir_block_has_pin_guard_amortization_region(block)) {
        return transpiler_emit_mir_plain_pin_preflight_local(
            buf, ctx, block, slot, pin_name, slot_expr, reason, reason_cap);
    } else {
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf,
            "PgyPinnedSlotView_%s %s = pgy_pin_%s_%s(%s);\n",
            slot->inner_type, pin_name,
            mode, slot->inner_type,
            slot_expr);
    }

    return true;
}

bool
transpiler_emit_mir_pin_exit_local(CodeBuf *buf,
                                   TranspilerCtx *ctx,
                                   const MIRBasicBlock *block,
                                   char *reason,
                                   size_t reason_cap)
{
    SlotVarEntry *slot;
    char pin_name[64];

    if (block == NULL || !block->is_pin_region)
        return true;

    slot = transpiler_mir_find_pin_slot_local(ctx, block);
    if (slot == NULL || slot->inner_type[0] == '\0') {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR pin block %zu cannot resolve source slot '%s' for exit",
                     block->id,
                     block->pin_source_name != NULL
                         ? block->pin_source_name : "<slot>");
        }
        return false;
    }

    if (!transpiler_mir_pin_local_name(block, pin_name, sizeof(pin_name))) {
        transpiler_mir_pin_format_reason(reason, reason_cap,
            "MIR pin block local name is too long for exit");
        return false;
    }
    if (slot->is_secure) {
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "pgy_secure_unpin_%s(&%s);\n",
                      slot->inner_type, pin_name);
    } else if (mir_block_has_pin_guard_amortization_region(block)) {
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf,
            "if (!%s.active || %s.slot == NULL) "
            "PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "
            "\"inactive slot unpin\");\n",
            pin_name, pin_name);
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s.active = false;\n", pin_name);
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s.slot = NULL;\n", pin_name);
    } else {
        transpiler_write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "pgy_unpin_%s(&%s);\n",
                      slot->inner_type, pin_name);
    }

    return true;
}

bool
transpiler_mir_block_has_local_def_for_anchor(const MIRBasicBlock *block,
                                              const char *anchor)
{
    if (block == NULL || anchor == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_DEF
            && inst->arg0 != NULL
            && strcmp(inst->arg0, anchor) == 0) {
            return true;
        }
    }
    return false;
}

bool
transpiler_mir_seed_resource_alias_local(TranspilerSSANameMap *ssa_map,
                                         const MIRInstruction *inst)
{
    if (ssa_map == NULL || inst == NULL || inst->kind != MIR_INST_RESOURCE_OP
        || inst->name == NULL) {
        return true;
    }

    TranspilerMIRResourceOp op = transpiler_mir_resource_op_lookup(inst->name);

    if (op == TRANS_MIR_RESOURCE_OP_CLAIM && inst->arg0 != NULL) {
        return transpiler_ssa_name_map_set(ssa_map, inst->arg0, inst->arg0);
    }
    if ((op == TRANS_MIR_RESOURCE_OP_BORROW_READ
         || op == TRANS_MIR_RESOURCE_OP_BORROW_WRITE)
        && inst->arg0 != NULL
        && inst->arg1 != NULL) {
        return transpiler_ssa_name_map_set(ssa_map, inst->arg1, inst->arg0);
    }

    return true;
}
