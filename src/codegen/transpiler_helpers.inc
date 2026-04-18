#include "transpiler_helpers_core.inc"

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
                            /* Secure slots require tokens; keep hard error. */
                            return false;
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
                            /* Secure slots require tokens; keep hard error. */
                            return false;
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
                            /* Secure slots require tokens; keep hard error. */
                            return false;
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

        write_indent_to(out, indent);
        codebuf_write(out, "%s %s = %s(&%s);\n", read_inner_c, result, fn, anchor);
    }
    else if (op_name != NULL && strcmp(op_name, "Write") == 0) {
        /* Rule 5a: Slot Write
         * C: pgy_write_<T>(&<anchor>, <value>);
         */
        const char *anchor = inst->slot_anchor != NULL ? inst->slot_anchor : "slot";
        const char *value = inst->arg1 != NULL ? inst->arg1
            : (inst->arg0 != NULL ? inst->arg0 : "0");

        write_indent_to(out, indent);
        codebuf_write(out, "%s(&%s, %s);\n", fn, anchor, value);
    }
    else if (op_name != NULL && strcmp(op_name, "Release") == 0) {
        /* Rule 5b: Slot Release
         * C: pgy_release_<T>(&<anchor>);
         */
        const char *anchor = inst->slot_anchor != NULL ? inst->slot_anchor : "slot";

        write_indent_to(out, indent);
        codebuf_write(out, "%s(&%s);\n", fn, anchor);
    }
    else if (op_name != NULL && strcmp(op_name, "Move") == 0) {
        /* Rule 6: Ownership Move
         * C: <dst> = <src>; <src>.occupied = false;
         */
        const char *src = inst->arg0 != NULL ? inst->arg0 : "src";
        const char *dst = inst->arg1 != NULL ? inst->arg1 : "dst";

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

#include "transpiler_expr_emitters.inc"

