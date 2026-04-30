#include "transpiler_helpers_core_a.h"
#include "transpiler_helpers_core_b.h"

/* -----------------------------------------------------------------
 * Expression emitters — return heap-allocated C expression string
 * ----------------------------------------------------------------- */

static char *
escape_c_string_literal(const char *src)
{
    size_t len = 0;
    size_t i;
    size_t j = 0;
    char *out;

    if (src == NULL)
        return pergyra_strdup("");

    for (i = 0; src[i] != '\0'; i++) {
        switch (src[i]) {
        case '\n':
        case '\r':
        case '\t':
        case '\\':
        case '"':
            len += 2;
            break;
        default:
            len += 1;
            break;
        }
    }

    out = (char *)malloc(len + 1);
    if (out == NULL)
        return pergyra_strdup("");

    for (i = 0; src[i] != '\0'; i++) {
        switch (src[i]) {
        case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
        case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
        case '\t': out[j++] = '\\'; out[j++] = 't'; break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '"': out[j++] = '\\'; out[j++] = '"'; break;
        default: out[j++] = src[i]; break;
        }
    }
    out[j] = '\0';
    return out;
}

static char *
strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    char *s = malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

/* =================================================================
 * ABI-Based MIR Resource Op Emitter
 *
 * This is the new "dumb emission" path for slot/resource operations.
 * Instead of the transpiler inventing type names and function calls,
 * it reads the MIRTypeLayout from the instruction and mechanically
 * emits the corresponding C code.
 *
 * Rules: see docs/40_lowering_rules.md (Rules 1-5)
 * ================================================================= */

/* Extract suffix from runtime function name:
 *   "pgy_claim_Int" → "Int"
 *   "pgy_write_Long" → "Long"
 *   "pgy_option_some_String" → "String"
 */
static const char *
transpiler_extract_type_suffix_from_fn(const char *fn_name)
{
    if (fn_name == NULL)
        return NULL;

    /* Find last underscore */
    const char *last_underscore = NULL;
    for (const char *p = fn_name; *p; p++) {
        if (*p == '_')
            last_underscore = p;
    }
    if (last_underscore == NULL)
        return NULL;

    return last_underscore + 1;
}

/* Emit a single MIR RESOURCE_OP instruction as C code.
 *
 * This is the core "dumb emitter": it reads type_layout and runtime_fn
 * from the MIR instruction and emits C code mechanically.
 *
 * Rule 1: CLAIM
 *   MIR:  RESOURCE_OP { runtime_fn: "pgy_claim_Int", slot_anchor: "s", arg0: "Slot<Int>" }
 *   C:    PgySlot_Int s = pgy_claim_Int();
 *
 * Rule 4: READ
 *   MIR:  DEF { arg0: "pgy_read_Int", arg1: "&s" }
 *   C:    int32_t val = pgy_read_Int(&s);
 *
 * Rule 5a: WRITE
 *   MIR:  RESOURCE_OP { runtime_fn: "pgy_write_Int", arg0: "42", arg1: "&s" }
 *   C:    pgy_write_Int(&s, 42);
 *
 * Rule 5b: RELEASE
 *   MIR:  RESOURCE_OP { runtime_fn: "pgy_release_Int", slot_anchor: "s" }
 *   C:    pgy_release_Int(&s);
 */
__attribute__((unused))
static bool
transpiler_emit_mir_resource_op(TranspilerCtx *ctx,
                                CodeBuf *out,
                                int indent,
                                const MIRInstruction *inst,
                                const MIRTypeLayout *layout,
                                const char *ssa_result_name)
{
    if (out == NULL || inst == NULL)
        return false;

    const char *fn = NULL;
    const char *suffix = NULL;
    const MIRTypeLayout *effective_layout = layout;
    char runtime_fn_buf[160];
    const char *slot_anchor = inst->slot_anchor;
    const char *typed_name = NULL;
    const char *inner_name = NULL;
    const char *inner_c = NULL;
    bool anchor_is_indirect = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;

    if (effective_layout == NULL) {
        const char *abi_key = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
        if (abi_key != NULL)
            effective_layout = mir_abi_lookup(abi_key);
    }

    /* Priority 1: use runtime_fn from type_layout */
    if (effective_layout != NULL && effective_layout->runtime_fn != NULL) {
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

    if (fn == NULL && ctx != NULL && slot_anchor != NULL) {
        typed_name = lookup_typed_var(ctx, slot_anchor);
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].name, slot_anchor) == 0) {
                anchor_is_indirect = ctx->slot_vars[i].is_indirect;
                break;
            }
        }
        if (typed_name != NULL) {
            if (strncmp(typed_name, "SecureSlot<", 11) == 0) {
                is_secure_slot = true;
            } else if (strncmp(typed_name, "DeviceSlot<", 11) == 0) {
                is_device_slot = true;
            }
            if (strncmp(typed_name, "Slot<", 5) == 0
                || is_secure_slot
                || is_device_slot) {
                inner_name = slot_inner_type_name(typed_name);
            }
        }
        if (inner_name != NULL) {
            inner_c = pergyra_type_to_c(inner_name);
            if (inner_c != NULL && inner_c[0] != '\0') {
                const char *op = inst->name;
                if (op != NULL) {
                    if (strcmp(op, "Claim") == 0) {
                        if (is_secure_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_claim_secure_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else if (is_device_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_claim_device_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_claim_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        }
                    } else if (strcmp(op, "Read") == 0) {
                        if (is_secure_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_secure_read_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else if (is_device_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_device_read_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_read_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        }
                    } else if (strcmp(op, "Write") == 0) {
                        if (is_secure_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_secure_write_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else if (is_device_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_device_write_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_write_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        }
                    } else if (strcmp(op, "Release") == 0) {
                        if (is_secure_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_secure_release_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else if (is_device_slot) {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_release_device_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        } else {
                            snprintf(runtime_fn_buf, sizeof(runtime_fn_buf),
                                "pgy_release_%s", inner_name);
                            fn = runtime_fn_buf;
                            suffix = inner_name;
                        }
                    }
                }
            }
        }
    }

    if (fn == NULL)
        return false;

    const char *op_name = inst->name;  /* "Claim", "Read", "Write", "Release", etc. */
    char anchor_expr_buf[128];
    const char *anchor_expr = NULL;

    if (slot_anchor == NULL)
        slot_anchor = inst->slot_anchor;
    if (slot_anchor == NULL)
        slot_anchor = "slot";
    snprintf(anchor_expr_buf, sizeof(anchor_expr_buf), "%s%s",
             anchor_is_indirect ? "" : "&",
             slot_anchor);
    anchor_expr = anchor_expr_buf;

    if (op_name != NULL && strcmp(op_name, "Claim") == 0) {
        /* Rule 1: Slot Claim
         * Plain:  PgySlot_<T> <anchor> = pgy_claim_<T>();
         * Secure: PgyToken_<T> <anchor>_token;
         *         PgySecureSlot_<T> <anchor> = pgy_claim_secure_<T>(&<anchor>_token);
         */
        char c_type_buf[64];
        if (suffix == NULL)
            return false;
        if (is_device_slot)
            snprintf(c_type_buf, sizeof(c_type_buf), "PgyDeviceSlot_%s", suffix);
        else if (is_secure_slot)
            snprintf(c_type_buf, sizeof(c_type_buf), "PgySecureSlot_%s", suffix);
        else
            snprintf(c_type_buf, sizeof(c_type_buf), "PgySlot_%s", suffix);

        const char *anchor = inst->slot_anchor != NULL ? inst->slot_anchor : "slot";
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
    }
    else if (op_name != NULL && strcmp(op_name, "Read") == 0) {
        /* Rule 4: Slot Read
         * C: <inner_type> <name> = pgy_read_<T>(&<anchor>);
         */
        const char *read_inner_c = inner_c;
        if (effective_layout != NULL && effective_layout->inner_c_type != NULL)
            read_inner_c = effective_layout->inner_c_type;
        if (read_inner_c == NULL)
            return false;
        const char *anchor = inst->slot_anchor != NULL ? inst->slot_anchor : "slot";
        const char *result = ssa_result_name != NULL ? ssa_result_name : "tmp";
        char local_anchor_expr_buf[128];
        const char *local_anchor_expr = anchor_expr;
        const char *token_name = NULL;

        if (anchor != NULL && strcmp(anchor, slot_anchor) != 0) {
            snprintf(local_anchor_expr_buf, sizeof(local_anchor_expr_buf), "&%s", anchor);
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
    }
    else if (op_name != NULL && strcmp(op_name, "Write") == 0) {
        /* Rule 5a: Slot Write
         * C: pgy_write_<T>(&<anchor>, <value>);
         */
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
            && inst->ast != NULL
            && inst->ast->type == AST_CALL) {
            ASTNode *callee = inst->ast->data.call.callee;
            ASTNode *value_node = NULL;
            if (callee != NULL && callee->type == AST_IDENTIFIER
                && callee->data.identifier.name != NULL
                && strcmp(callee->data.identifier.name, "Write") == 0
                && inst->ast->data.call.arg_count >= 2) {
                value_node = inst->ast->data.call.arguments[1];
            } else if (callee != NULL && callee->type == AST_MEMBER_ACCESS
                       && callee->data.member.name != NULL
                       && strcmp(callee->data.member.name, "Write") == 0
                       && inst->ast->data.call.arg_count >= 1) {
                value_node = inst->ast->data.call.arguments[0];
            }
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
                snprintf(value_expr_buf, sizeof(value_expr_buf), "_pgy_ssa_%s_%zu",
                         version_base, version);
                value = value_expr_buf;
            }
        }
        if (is_secure_slot) {
            token_name = lookup_slot_token_name(ctx, slot_anchor);
            if (token_name == NULL || token_name[0] == '\0')
                return false;
        }

        write_indent_to(out, indent);
        if (is_secure_slot) {
            codebuf_write(out, "%s(%s, %s, &%s);\n", fn, anchor_expr, value, token_name);
        } else {
            codebuf_write(out, "%s(%s, %s);\n", fn, anchor_expr, value);
        }
        free(ast_value_expr);
    }
    else if (op_name != NULL && strcmp(op_name, "Release") == 0) {
        /* Rule 5b: Slot Release
         * C: pgy_release_<T>(&<anchor>);
         */
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
    }
    else if (op_name != NULL && strcmp(op_name, "Move") == 0) {
        /* Rule 6: Ownership Move
         * C: <dst> = <src>; <src>.occupied = false;
         */
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
    }
    else {
        return false;
    }

    return true;
}

/* Emit a MIR DEF instruction (variable definition with runtime call)
 *
 * Rule 4 (alternate): DEF with runtime_fn
 *   MIR:  DEF { arg0: "pgy_read_Int", arg1: "&s", result_name: "%val_s_1" }
 *   C:    int32_t val_s_1 = pgy_read_Int(&s);
 */
__attribute__((unused))
static bool
transpiler_emit_mir_def(CodeBuf *out,
                        int indent,
                        const MIRInstruction *inst,
                        const MIRTypeLayout *layout)
{
    if (out == NULL || inst == NULL)
        return false;

    const MIRTypeLayout *effective_layout = layout;
    if (effective_layout == NULL && inst->slot_anchor != NULL)
        effective_layout = mir_abi_lookup(inst->slot_anchor);

    /* If this DEF has a type_layout with runtime_fn, emit as function call */
    if (effective_layout != NULL && effective_layout->runtime_fn != NULL) {
        const char *inner_c = effective_layout->inner_c_type;
        const char *result = inst->result_name != NULL ? inst->result_name : "tmp";
        const char *arg = inst->arg1 != NULL ? inst->arg1 : "";
        if (inner_c == NULL)
            return false;

        write_indent_to(out, indent);
        codebuf_write(out, "%s %s = %s(%s);\n", inner_c, result,
                      effective_layout->runtime_fn, arg);
        return true;
    }

    /* Otherwise, it's a plain SSA def — handled by existing logic */
    return false;  /* signal caller to use default handling */
}

static bool
transpiler_mir_intent_has_stmt(const MIRRoutine *routine,
                               const char *step_name,
                               const char *inst_name,
                               const char *arg0)
{
    if (routine == NULL || inst_name == NULL)
        return false;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, inst_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            if (arg0 != NULL) {
                if (inst->arg0 == NULL || strcmp(inst->arg0, arg0) != 0)
                    continue;
            }
            return true;
        }
    }

    return false;
}

static char *
transpiler_emit_none_with_context(TranspilerCtx *ctx, ASTNode *site)
{
    const char *inner = transpiler_contextual_option_inner_type_name(ctx);
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "None requires contextual Option<T> during C emission");
        (void)site;
        return pergyra_strdup("0");
    }
    return strdup_fmt("None_%s()", inner);
}

#include "transpiler_expr_emitters.h"
