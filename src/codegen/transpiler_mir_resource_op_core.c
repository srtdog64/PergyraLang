/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend concrete MIR resource operation emission.
 */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_mir_resource_op_core.h"
#include "codegen_mir_resource_name_helpers.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_require.h"

#include "../compiler/mir_abi_layout.h"
#include "../compiler/mir_machine_layer.h"
#include "../semantic/diag_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
transpiler_mir_resource_format(char *out,
                               size_t out_size,
                               const char *fmt,
                               const char *value)
{
    int written;

    if (out == NULL || out_size == 0 || fmt == NULL || value == NULL)
        return false;
    written = snprintf(out, out_size, fmt, value);
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_mir_resource_format_addr(char *out,
                                    size_t out_size,
                                    bool indirect,
                                    const char *name)
{
    return transpiler_mir_resource_format(out, out_size,
        indirect ? "%s" : "&%s", name);
}

static const char *
transpiler_mir_resource_expected_call_shape(bool secure,
                                           const char *operation)
{
    if (operation == NULL)
        return NULL;
    if (strcmp(operation, "Claim") == 0)
        return secure ? "token_ptr_to_container" : "returns_container";
    if (strcmp(operation, "Read") == 0)
        return secure ? "container_ptr_token_ptr_to_value"
                      : "container_ptr_to_value";
    if (strcmp(operation, "Write") == 0)
        return secure ? "container_ptr_value_token_ptr_to_void"
                      : "container_ptr_value_to_void";
    if (strcmp(operation, "Release") == 0)
        return secure ? "container_ptr_token_ptr_to_void"
                      : "container_ptr_to_void";
    return NULL;
}

static bool
transpiler_mir_resource_abi_type_is_slot_family(const char *abi_type_name)
{
    return abi_type_name != NULL
        && (strncmp(abi_type_name, "Slot<", 5) == 0
            || strncmp(abi_type_name, "SecureSlot<", 11) == 0
            || strncmp(abi_type_name, "DeviceSlot<", 11) == 0);
}

static bool
transpiler_mir_resource_operation_requires_runtime_row(const char *operation)
{
    return operation != NULL
        && (strcmp(operation, "Claim") == 0
            || strcmp(operation, "Read") == 0
            || strcmp(operation, "Write") == 0
            || strcmp(operation, "Release") == 0);
}

bool
transpiler_emit_mir_resource_op(TranspilerCtx *ctx,
                                CodeBuf *out,
                                int indent,
                                const MIRInstruction *inst,
                                const MIRTypeLayout *layout,
                                const char *ssa_result_name)
{
    const char *fn = NULL;
    const char *suffix = NULL;
    const MIRTypeLayout *effective_layout = layout;
    const MIRResourceRuntimeRow *runtime_row = NULL;
    char suffix_buf[96];
    char inner_name_buf[128];
    const char *slot_anchor;
    const char *effective_abi_type_name = NULL;
    const char *typed_name = NULL;
    const char *inner_name = NULL;
    const char *inner_c = NULL;
    bool anchor_is_indirect = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;
    const char *op_name;
    TranspilerMIRResourceOp op;
    char anchor_expr_buf[128];
    const char *anchor_expr = NULL;
    bool constructed_nominal = false;
    bool mir_active = transpiler_active_has_mir(ctx);

    if (out == NULL || inst == NULL)
        return false;

    if (rir_machine_contact_kind_is_present(inst->machine_contact_kind)
        && !transpiler_machine_layer_projection_is_bound(ctx)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C backend rejected machine resource op '%s': C machine-layer projection is not admitted",
            inst->name != NULL ? inst->name : "<op>");
        return false;
    }

    if (rir_machine_contact_kind_is_present(inst->machine_contact_kind)
        && !mir_machine_layer_fact_is_valid(inst)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C backend rejected machine resource op '%s': MIR machine-layer fact is missing or invalid",
            inst->name != NULL ? inst->name : "<op>");
        return false;
    }

    slot_anchor = inst->slot_anchor;
    effective_abi_type_name = inst->abi_type_name;
    op_name = inst->name;
    op = transpiler_mir_resource_op_lookup(op_name);
    constructed_nominal = inst->resource_runtime_fact_present
        && mir_abi_resource_runtime_row_is_constructed_nominal(
            &inst->resource_runtime_fact);

    if (effective_layout == NULL) {
        if (mir_active && !constructed_nominal) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource op '%s' is missing its lowered ABI layout fact",
                op_name != NULL ? op_name : "<op>");
            return false;
        }
        const char *abi_key = effective_abi_type_name != NULL
            ? effective_abi_type_name
            : (inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor);
        if (abi_key != NULL)
            effective_layout = mir_abi_lookup(abi_key);
    }

    if (mir_active && !constructed_nominal
        && (inst->type_layout == NULL
            || inst->abi_layout_id == 0
            || inst->abi_layout_id != mir_abi_layout_id(inst->type_layout))) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR resource op '%s' carries a missing or mismatched ABI layout identity",
            op_name != NULL ? op_name : "<op>");
        return false;
    }

    if (mir_active && inst->resource_runtime_fact_present) {
        runtime_row = &inst->resource_runtime_fact;
        if (runtime_row->runtime_fn == NULL
            || runtime_row->call_shape == NULL
            || runtime_row->resource_op_name == NULL
            || (op_name != NULL
                && strcmp(runtime_row->resource_op_name, op_name) != 0)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource op '%s' carries an invalid runtime-call ABI row",
                op_name != NULL ? op_name : "<op>");
            return false;
        }
        if (runtime_row->runtime_call_abi_id == 0
            || runtime_row->runtime_call_abi_id
                != mir_abi_resource_runtime_row_id(runtime_row)
            || !mir_abi_resource_runtime_row_matches_owner(runtime_row)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource op '%s' carries an invalid runtime-call ABI identity",
                op_name != NULL ? op_name : "<op>");
            return false;
        }
        fn = runtime_row->runtime_fn;
        suffix = transpiler_extract_type_suffix_from_fn(runtime_row->runtime_fn);
    }
    if (mir_active
        && transpiler_mir_resource_abi_type_is_slot_family(
            effective_abi_type_name)
        && transpiler_mir_resource_operation_requires_runtime_row(op_name)
        && !inst->resource_runtime_fact_present) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C MIR resource op '%s' is missing its lowered runtime-call ABI row",
            op_name != NULL ? op_name : "<op>");
        return false;
    }

    if (runtime_row == NULL
        && effective_layout != NULL && effective_layout->runtime_fn != NULL) {
        if (effective_layout->abi_type_name != NULL)
            effective_abi_type_name = effective_layout->abi_type_name;
        if (effective_layout->abi_type_name != NULL) {
            runtime_row = mir_abi_resource_runtime_row_by_type_name(
                effective_layout->abi_type_name, op_name);
        }
        if (runtime_row != NULL && runtime_row->runtime_fn != NULL) {
            fn = runtime_row->runtime_fn;
            suffix = transpiler_extract_type_suffix_from_fn(
                runtime_row->runtime_fn);
        }
        if (inner_name == NULL
            && effective_layout->abi_type_name != NULL
            && (strncmp(effective_layout->abi_type_name, "Slot<", 5) == 0
                || strncmp(effective_layout->abi_type_name, "SecureSlot<", 11) == 0
                || strncmp(effective_layout->abi_type_name, "DeviceSlot<", 11) == 0)) {
            if (slot_inner_type_name_copy(effective_layout->abi_type_name,
                    inner_name_buf, sizeof(inner_name_buf)))
                inner_name = inner_name_buf;
        }
    }
    if (effective_abi_type_name != NULL) {
        if (strncmp(effective_abi_type_name, "SecureSlot<", 11) == 0) {
            is_secure_slot = true;
        } else if (strncmp(effective_abi_type_name, "DeviceSlot<", 11) == 0) {
            is_device_slot = true;
        }
        if (inner_name == NULL
            && (strncmp(effective_abi_type_name, "Slot<", 5) == 0
                || strncmp(effective_abi_type_name, "SecureSlot<", 11) == 0
                || strncmp(effective_abi_type_name, "DeviceSlot<", 11) == 0)) {
            if (slot_inner_type_name_copy(effective_abi_type_name,
                    inner_name_buf, sizeof(inner_name_buf)))
                inner_name = inner_name_buf;
        }
    }
    if (!mir_active && fn == NULL && inner_name != NULL
        && effective_abi_type_name != NULL) {
        MIRResourceAbiKind kind = MIR_RESOURCE_ABI_SLOT;
        bool has_resource_kind = false;

        if (is_device_slot) {
            kind = MIR_RESOURCE_ABI_DEVICE_SLOT;
            has_resource_kind = true;
        } else if (is_secure_slot) {
            kind = MIR_RESOURCE_ABI_SECURE_SLOT;
            has_resource_kind = true;
        } else if (strncmp(effective_abi_type_name, "Slot<", 5) == 0) {
            kind = MIR_RESOURCE_ABI_SLOT;
            has_resource_kind = true;
        }

        if (has_resource_kind) {
            runtime_row = mir_abi_resource_runtime_row_by_kind(
                kind, inner_name, op_name);
            if (runtime_row != NULL && runtime_row->runtime_fn != NULL) {
                fn = runtime_row->runtime_fn;
                if (!sanitize_c_suffix(inner_name, suffix_buf,
                        sizeof(suffix_buf)))
                    return false;
                suffix = suffix_buf;
            }
        }
    }

    if (ctx != NULL && slot_anchor != NULL && mir_active) {
        TypedVarEntry *typed_entry;
        anchor_is_indirect = lookup_slot_is_indirect(ctx, slot_anchor);
        typed_entry = lookup_typed_entry(ctx, slot_anchor);
        if (typed_entry != NULL && typed_entry->is_indirect_ref)
            anchor_is_indirect = true;
    }

    if (fn == NULL && ctx != NULL && slot_anchor != NULL && !mir_active) {
        typed_name = lookup_typed_var(ctx, slot_anchor);
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].name, slot_anchor) == 0) {
                anchor_is_indirect = ctx->slot_vars[i].is_indirect;
                break;
            }
        }
        if (typed_name != NULL) {
            if (fn == NULL && effective_layout == NULL)
                effective_layout = mir_abi_lookup(typed_name);
            if (fn == NULL
                && effective_layout != NULL
                && effective_layout->runtime_fn != NULL) {
                if (effective_layout->abi_type_name != NULL) {
                    runtime_row = mir_abi_resource_runtime_row_by_type_name(
                        effective_layout->abi_type_name, op_name);
                }
                if (runtime_row != NULL && runtime_row->runtime_fn != NULL) {
                    fn = runtime_row->runtime_fn;
                    suffix = transpiler_extract_type_suffix_from_fn(fn);
                }
            }
            if (strncmp(typed_name, "SecureSlot<", 11) == 0) {
                is_secure_slot = true;
            } else if (strncmp(typed_name, "DeviceSlot<", 11) == 0) {
                is_device_slot = true;
            }
            if (strncmp(typed_name, "Slot<", 5) == 0
                || is_secure_slot
                || is_device_slot) {
                if (slot_inner_type_name_copy(typed_name, inner_name_buf,
                        sizeof(inner_name_buf)))
                    inner_name = inner_name_buf;
            } else if (strncmp(typed_name, "ReadView<", 9) == 0
                       || strncmp(typed_name, "WriteView<", 10) == 0
                       || strncmp(typed_name, "MoveView<", 9) == 0) {
                /* Closure #84: pinned views carry the slot inner type as
                 * their generic parameter. Without this branch a `view`
                 * anchor on a Write/Read op was rejected with "missing
                 * typed runtime layout" (pin_secure_param_read_view_block)
                 * because the lookup only knew how to extract from raw
                 * Slot/SecureSlot/DeviceSlot type names. */
                if (slot_inner_type_name_copy(typed_name, inner_name_buf,
                        sizeof(inner_name_buf)))
                    inner_name = inner_name_buf;
                if (strncmp(typed_name, "Read", 4) == 0
                    || strncmp(typed_name, "Write", 5) == 0) {
                    /* view source slot might still be SecureSlot — detect
                     * via lookup if the underlying slot binding is secure. */
                    for (int i = 0; i < ctx->slot_var_count; i++) {
                        if (strcmp(ctx->slot_vars[i].name, slot_anchor) == 0) {
                            if (ctx->slot_vars[i].is_secure)
                                is_secure_slot = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (inner_name != NULL) {
        char inner_c_buf[128];
        if (transpiler_require_type_name_c_type_copy(ctx, inner_name,
                "MIR resource operation payload", inner_c_buf,
                sizeof(inner_c_buf))) {
            inner_c = inner_c_buf;
        }
    }

    if (runtime_row != NULL) {
        if (rir_machine_contact_kind_is_present(inst->machine_contact_kind)
            && !mir_machine_layer_fact_matches_runtime_operation(
                inst, runtime_row->resource_op_name)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource op '%s' disagrees with machine-layer runtime operation",
                op_name != NULL ? op_name : "<op>");
            return false;
        }
        const char *expected_shape =
            transpiler_mir_resource_expected_call_shape(is_secure_slot,
                                                        op_name);
        if (runtime_row->call_shape == NULL ||
            (expected_shape != NULL &&
             strcmp(runtime_row->call_shape, expected_shape) != 0)) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C MIR resource op '%s' requires MIR ABI call shape %s",
                op_name != NULL ? op_name : "<op>",
                expected_shape != NULL ? expected_shape : "<known>");
            return false;
        }
    }

    if (fn == NULL) {
        if (mir_active) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "MIR resource op '%s' is missing runtime ABI layout metadata",
                op_name != NULL ? op_name : "<op>");
        }
        return false;
    }

    if (slot_anchor == NULL)
        slot_anchor = inst->slot_anchor;
    if (slot_anchor == NULL)
        slot_anchor = "slot";
    {
        /* An object-field slot is addressed through `self->name`; the registry
         * is still keyed by the bare field name, so render the address form
         * separately here. */
        const char *addr_anchor = slot_anchor;
        char self_field_buf[160];
        if (ctx != NULL && lookup_slot_is_self_field(ctx, slot_anchor)) {
            if (!transpiler_mir_resource_format(self_field_buf,
                    sizeof(self_field_buf), "self->%s", slot_anchor))
                return false;
            addr_anchor = self_field_buf;
        }
        if (!transpiler_mir_resource_format_addr(anchor_expr_buf,
                sizeof(anchor_expr_buf), anchor_is_indirect, addr_anchor))
            return false;
    }
    anchor_expr = anchor_expr_buf;

    if (op == TRANS_MIR_RESOURCE_OP_CLAIM) {
        char c_type_buf[64];
        const char *anchor = inst->slot_anchor != NULL ? inst->slot_anchor : "slot";
        if (suffix == NULL)
            return false;
        if (is_device_slot) {
            if (!transpiler_mir_resource_format(c_type_buf,
                    sizeof(c_type_buf), "PgyDeviceSlot_%s", suffix))
                return false;
        } else if (is_secure_slot) {
            if (!transpiler_mir_resource_format(c_type_buf,
                    sizeof(c_type_buf), "PgySecureSlot_%s", suffix))
                return false;
        } else if (!transpiler_mir_resource_format(c_type_buf,
                sizeof(c_type_buf), "PgySlot_%s", suffix)) {
            return false;
        }

        if (is_secure_slot) {
            write_indent_to(out, indent);
            codebuf_write(out, "PgyToken_%s %s_token;\n", suffix, anchor);
            write_indent_to(out, indent);
            codebuf_write(out, "%s %s = %s(&%s_token);\n",
                c_type_buf, anchor, fn, anchor);
        } else {
            write_indent_to(out, indent);
            codebuf_write(out, "%s %s = %s();\n", c_type_buf, anchor, fn);
        }
        write_indent_to(out, indent);
        codebuf_write(out, "(void)%s;\n", anchor);
    } else if (op == TRANS_MIR_RESOURCE_OP_READ) {
        const char *read_inner_c = inner_c;
        const char *anchor = inst->slot_anchor != NULL ? inst->slot_anchor : "slot";
        const char *result = ssa_result_name != NULL ? ssa_result_name : "tmp";
        char local_anchor_expr_buf[128];
        const char *local_anchor_expr = anchor_expr;
        const char *token_name = NULL;

        if (effective_layout != NULL && effective_layout->inner_c_type != NULL)
            read_inner_c = effective_layout->inner_c_type;
        if (read_inner_c == NULL)
            return false;
        if (anchor != NULL && strcmp(anchor, slot_anchor) != 0) {
            if (!transpiler_mir_resource_format(local_anchor_expr_buf,
                    sizeof(local_anchor_expr_buf), "&%s", anchor))
                return false;
            local_anchor_expr = local_anchor_expr_buf;
        }
        if (is_secure_slot) {
            token_name = lookup_slot_token_name(ctx, anchor);
            if (token_name == NULL || token_name[0] == '\0')
                return false;
        }
        write_indent_to(out, indent);
        if (is_secure_slot) {
            codebuf_write(out, "%s %s = %s(%s, &%s);\n", read_inner_c, result, fn,
                          local_anchor_expr, token_name);
        } else {
            codebuf_write(out, "%s %s = %s(%s);\n", read_inner_c, result, fn,
                          local_anchor_expr);
        }
    } else if (op == TRANS_MIR_RESOURCE_OP_WRITE) {
        const char *value = inst->arg1 != NULL ? inst->arg1
            : (inst->arg0 != NULL ? inst->arg0 : "0");
        char value_expr_buf[160];
        char *ast_value_expr = NULL;
        const char *token_name = NULL;
        if (ctx != NULL && ctx->active_ssa_map != NULL && value != NULL) {
            const char *mapped = transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ctx->active_ssa_map, value);
            if (mapped != NULL)
                value = mapped;
        }
        if ((value == NULL
             || (slot_anchor != NULL && strcmp(value, slot_anchor) == 0))
            && inst->expr0 != NULL) {
            ASTNode *value_node = inst->expr0;
            if (value_node != NULL) {
                ast_value_expr = emit_expression(value_node, ctx);
                if (ast_value_expr != NULL && ast_value_expr[0] != '\0')
                    value = ast_value_expr;
            }
        }
        if (value != NULL) {
            char version_base[128];
            size_t version = 0;
            bool looks_like_plain_ssa = true;
            for (const char *p = value; *p != '\0'; p++) {
                if (!( (*p >= 'a' && *p <= 'z')
                    || (*p >= 'A' && *p <= 'Z')
                    || (*p >= '0' && *p <= '9')
                    || *p == '_'
                    || *p == '.')) {
                    looks_like_plain_ssa = false;
                    break;
                }
            }
            if (looks_like_plain_ssa
                && transpiler_parse_versioned_name(value,
                                                version_base,
                                                sizeof(version_base),
                                                &version)) {
                int written = snprintf(value_expr_buf, sizeof(value_expr_buf),
                    "_pgy_ssa_%s_%zu", version_base, version);
                if (written < 0 || (size_t)written >= sizeof(value_expr_buf)) {
                    free(ast_value_expr);
                    return false;
                }
                value = value_expr_buf;
            }
        }
        if (is_secure_slot) {
            token_name = lookup_slot_token_name(ctx, slot_anchor);
            if (token_name == NULL || token_name[0] == '\0') {
                free(ast_value_expr);
                return false;
            }
        }
        write_indent_to(out, indent);
        if (is_secure_slot) {
            codebuf_write(out, "%s(%s, %s, &%s);\n", fn, anchor_expr, value, token_name);
        } else {
            codebuf_write(out, "%s(%s, %s);\n", fn, anchor_expr, value);
        }
        free(ast_value_expr);
    } else if (op == TRANS_MIR_RESOURCE_OP_RELEASE) {
        const char *token_name = NULL;
        if (is_secure_slot) {
            token_name = lookup_slot_token_name(ctx, slot_anchor);
            if (token_name == NULL || token_name[0] == '\0')
                return false;
        }
        write_indent_to(out, indent);
        if (is_secure_slot) {
            codebuf_write(out, "%s(%s, &%s);\n", fn, anchor_expr, token_name);
        } else {
            codebuf_write(out, "%s(%s);\n", fn, anchor_expr);
        }
    } else if (op == TRANS_MIR_RESOURCE_OP_MOVE) {
        const char *src = inst->arg0 != NULL ? inst->arg0 : "src";
        const char *dst = inst->arg1 != NULL ? inst->arg1 : "dst";
        if (ctx != NULL && ctx->active_ssa_map != NULL) {
            const char *mapped_src = transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ctx->active_ssa_map, src);
            const char *mapped_dst = transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ctx->active_ssa_map, dst);
            if (mapped_src != NULL)
                src = mapped_src;
            if (mapped_dst != NULL)
                dst = mapped_dst;
        }
        write_indent_to(out, indent);
        codebuf_write(out, "%s = %s;\n", dst, src);
    } else {
        return false;
    }

    return true;
}
