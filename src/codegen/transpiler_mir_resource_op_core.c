/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend concrete MIR resource operation emission.
 */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"

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
    char runtime_fn_buf[160];
    char inner_name_buf[128];
    const char *slot_anchor;
    const char *typed_name = NULL;
    const char *inner_name = NULL;
    const char *inner_c = NULL;
    bool anchor_is_indirect = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;
    const char *op_name;
    char anchor_expr_buf[128];
    const char *anchor_expr = NULL;

    if (out == NULL || inst == NULL)
        return false;

    slot_anchor = inst->slot_anchor;
    op_name = inst->name;

    if (effective_layout == NULL) {
        const char *abi_key = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
        if (abi_key != NULL)
            effective_layout = mir_abi_lookup(abi_key);
    }

    if (effective_layout != NULL && effective_layout->runtime_fn != NULL) {
        const char *layout_fn = effective_layout->runtime_fn;
        suffix = transpiler_extract_type_suffix_from_fn(layout_fn);
        if (strncmp(layout_fn, "pgy_claim_secure_", 17) == 0
            || strncmp(layout_fn, "pgy_secure_", 11) == 0) {
            is_secure_slot = true;
        } else if (strncmp(layout_fn, "pgy_claim_device_", 17) == 0
                   || strncmp(layout_fn, "pgy_device_", 11) == 0) {
            is_device_slot = true;
        }
        if (op_name != NULL && strcmp(op_name, "Claim") == 0)
            fn = layout_fn;
        if (inner_name == NULL
            && effective_layout->abi_type_name != NULL
            && (strncmp(effective_layout->abi_type_name, "Slot<", 5) == 0
                || strncmp(effective_layout->abi_type_name, "SecureSlot<", 11) == 0
                || strncmp(effective_layout->abi_type_name, "DeviceSlot<", 11) == 0)) {
            copy_capped_string(inner_name_buf, sizeof(inner_name_buf),
                slot_inner_type_name(effective_layout->abi_type_name));
            inner_name = inner_name_buf;
        }
    }

    if (fn == NULL && ctx != NULL && slot_anchor != NULL) {
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
                fn = effective_layout->runtime_fn;
                suffix = transpiler_extract_type_suffix_from_fn(fn);
                if (strncmp(fn, "pgy_claim_secure_", 17) == 0
                    || strncmp(fn, "pgy_secure_", 11) == 0) {
                    is_secure_slot = true;
                } else if (strncmp(fn, "pgy_claim_device_", 17) == 0
                           || strncmp(fn, "pgy_device_", 11) == 0) {
                    is_device_slot = true;
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
                copy_capped_string(inner_name_buf, sizeof(inner_name_buf),
                    slot_inner_type_name(typed_name));
                inner_name = inner_name_buf;
            }
        }
        if (inner_name != NULL) {
            char inner_c_buf[128];
            if (pergyra_type_to_c_copy(inner_name, inner_c_buf,
                    sizeof(inner_c_buf))) {
                inner_c = inner_c_buf;
            }
            if (inner_c != NULL && inner_c[0] != '\0') {
                if (transpiler_format_slot_runtime_fn(
                        op_name, is_secure_slot, is_device_slot, inner_name,
                        runtime_fn_buf, sizeof(runtime_fn_buf))) {
                    fn = runtime_fn_buf;
                    suffix = inner_name;
                }
            }
        }
    }

    if (fn == NULL)
        return false;

    if (slot_anchor == NULL)
        slot_anchor = inst->slot_anchor;
    if (slot_anchor == NULL)
        slot_anchor = "slot";
    if (!transpiler_mir_resource_format_addr(anchor_expr_buf,
            sizeof(anchor_expr_buf), anchor_is_indirect, slot_anchor))
        return false;
    anchor_expr = anchor_expr_buf;

    if (op_name != NULL && strcmp(op_name, "Claim") == 0) {
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
    } else if (op_name != NULL && strcmp(op_name, "Read") == 0) {
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
    } else if (op_name != NULL && strcmp(op_name, "Write") == 0) {
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
    } else if (op_name != NULL && strcmp(op_name, "Release") == 0) {
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
    } else if (op_name != NULL && strcmp(op_name, "Move") == 0) {
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
