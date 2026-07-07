#include "transpiler_mir_destructure_emit.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_abi_layout.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "codegen_slot_type_policy.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

/* C backend MIR destructuring statement emission owner. */
static const char *
transpiler_mir_destructure_slot_runtime_fn(TranspilerCtx *ctx,
                                           MIRResourceAbiKind kind,
                                           const char *inner_type,
                                           const char *operation)
{
    const char *runtime_fn =
        mir_abi_resource_runtime_fn_by_kind(kind, inner_type, operation);
    if (runtime_fn != NULL)
        return runtime_fn;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "C MIR destructuring %s requires MIR ABI runtime function row",
        operation != NULL ? operation : "<unknown>");
    return NULL;
}

static bool
transpiler_mir_destructure_format_type(char *out, size_t out_size,
                                       const char *prefix, const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || inner == NULL)
        return false;
    written = snprintf(out, out_size, "%s<%s>", prefix, inner);
    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_mir_destructure_ssa_local(char *out, size_t out_size,
                                     const char *binding_name)
{
    int written;

    if (out == NULL || out_size == 0 || binding_name == NULL)
        return false;
    written = snprintf(out, out_size, "%s.1", binding_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
transpiler_emit_mir_let_destructure_stmt(CodeBuf *buf,
                                         const MIRBasicBlock *block,
                                         const MIRInstruction *inst,
                                         TranspilerCtx *ctx,
                                         TranspilerSSANameMap *ssa_map_out,
                                         char *reason,
                                         size_t reason_cap)
{
    ASTNode *init;
    size_t binding_count;
    const char *init_type_name;
    char c_init_type_buf[128];
    const char *c_init_type = NULL;
    const char *elem_inner = NULL;
    const char *elem_c_type = NULL;
    char elem_inner_buf[128];
    char elem_c_type_buf[128];
    const char *claim_abi_type_name = NULL;

    if (buf == NULL || block == NULL || inst == NULL || ctx == NULL
        || ssa_map_out == NULL || inst->kind != MIR_INST_DESTRUCTURE) {
        return false;
    }

    binding_count = mir_instruction_destructure_binding_count(inst);
    if (binding_count == 0)
        return false;

    init = inst->expr0;
    claim_abi_type_name = inst->abi_type_name != NULL
        ? inst->abi_type_name
        : (inst->type_layout != NULL ? inst->type_layout->abi_type_name : NULL);
    if (binding_count == 2
        && pgy_codegen_type_name_is_secure_slot(claim_abi_type_name)) {
        char claim_inner_buf[128];
        const char *inner = claim_inner_buf;
        const char *slot_name;
        const char *token_name;
        const char *claim_fn;
        char typed_tok[64];
        char typed_slot[64];

        if (!slot_inner_type_name_copy(claim_abi_type_name,
                claim_inner_buf, sizeof(claim_inner_buf))
            || claim_inner_buf[0] == '\0'
            || strcmp(claim_inner_buf, "Unknown") == 0) {
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "ClaimSecureSlot destructuring requires concrete generic type");
            }
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: ClaimSecureSlot destructuring requires concrete SecureSlot<T> metadata");
            return false;
        }
        claim_fn = transpiler_mir_destructure_slot_runtime_fn(
            ctx, MIR_RESOURCE_ABI_SECURE_SLOT, inner, "Claim");
        if (claim_fn == NULL)
            return false;
        slot_name = mir_instruction_destructure_binding_name_at(inst, 0);
        token_name = mir_instruction_destructure_binding_name_at(inst, 1);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "PgyToken_%s %s;\n", inner, token_name);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf,
            "PgySecureSlot_%s %s = %s(&%s);\n",
            inner, slot_name, claim_fn, token_name);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "(void)%s;\n", slot_name);
        register_slot_var(ctx, slot_name, inner, true, false);
        set_slot_token_name(ctx, slot_name, token_name);
        if (!transpiler_mir_destructure_format_type(
                typed_tok, sizeof(typed_tok), "Token", inner)
            || !transpiler_mir_destructure_format_type(
                typed_slot, sizeof(typed_slot), "SecureSlot", inner)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: ClaimSecureSlot destructuring type name is too long");
            return false;
        }
        register_typed_var(ctx, token_name, typed_tok);
        register_typed_var(ctx, slot_name, typed_slot);
        transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
        transpiler_ssa_name_map_set(ssa_map_out, token_name, token_name);
        return true;
    }

    if (binding_count == 1
        && pgy_codegen_type_name_is_slot(claim_abi_type_name)) {
        char claim_inner_buf[128];
        const char *inner = claim_inner_buf;
        const char *slot_name;
        const char *claim_fn;
        char typed_slot[64];

        if (!slot_inner_type_name_copy(claim_abi_type_name,
                claim_inner_buf, sizeof(claim_inner_buf))
            || claim_inner_buf[0] == '\0'
            || strcmp(claim_inner_buf, "Unknown") == 0) {
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "ClaimSlot destructuring requires concrete generic type");
            }
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: ClaimSlot destructuring requires concrete Slot<T> metadata");
            return false;
        }
        claim_fn = transpiler_mir_destructure_slot_runtime_fn(
            ctx, MIR_RESOURCE_ABI_SLOT, inner, "Claim");
        if (claim_fn == NULL)
            return false;
        slot_name = mir_instruction_destructure_binding_name_at(inst, 0);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "PgySlot_%s %s = %s();\n",
                      inner, slot_name, claim_fn);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "(void)%s;\n", slot_name);
        register_slot_var(ctx, slot_name, inner, false, false);
        if (!transpiler_mir_destructure_format_type(
                typed_slot, sizeof(typed_slot), "Slot", inner)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: ClaimSlot destructuring type name is too long");
            return false;
        }
        register_typed_var(ctx, slot_name, typed_slot);
        transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
        return true;
    }

    init_type_name = infer_expression_type_name(ctx, init);
    if ((init_type_name == NULL || strcmp(init_type_name, "Unknown") == 0)
        && init != NULL
        && init->type == AST_IDENTIFIER
        && ast_identifier_name(init) != NULL) {
        const char *resolved = lookup_typed_var(ctx, ast_identifier_name(init));
        if (resolved != NULL)
            init_type_name = resolved;
    }
    if (transpiler_require_type_name_c_type_copy(ctx, init_type_name,
            "MIR destructuring initializer", c_init_type_buf,
            sizeof(c_init_type_buf))) {
        c_init_type = c_init_type_buf;
    }

    if (init_type_name != NULL && init_type_name[0] == '(') {
        char elem_names[8][64];
        size_t arity = 0;
        char *rhs_t;
        int tmp_id;

        {
            size_t ti = 1;
            size_t tlen = strlen(init_type_name);
            while (ti < tlen && init_type_name[ti] != ')' && arity < 8) {
                size_t eo = 0;
                int depth = 0;
                while (ti < tlen
                       && (init_type_name[ti] == ' '
                           || init_type_name[ti] == '\t')) {
                    ti++;
                }
                while (ti < tlen && eo + 1 < sizeof(elem_names[0])) {
                    char c = init_type_name[ti];
                    if (depth == 0 && (c == ',' || c == ')'))
                        break;
                    if (c == '<' || c == '(')
                        depth++;
                    if (c == '>' || c == ')')
                        depth--;
                    elem_names[arity][eo++] = c;
                    ti++;
                }
                elem_names[arity][eo] = '\0';
                while (eo > 0
                       && (elem_names[arity][eo - 1] == ' '
                           || elem_names[arity][eo - 1] == '\t')) {
                    elem_names[arity][--eo] = '\0';
                }
                arity++;
                if (ti < tlen && init_type_name[ti] == ',')
                    ti++;
            }
        }
        if (arity != binding_count) {
            transpiler_set_mir_topology_invalid(
                ctx,
                "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                (unsigned long long) binding_count,
                (unsigned long long) arity);
            return false;
        }
        rhs_t = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
        if (rhs_t == NULL)
            return false;
        tmp_id = ++ctx->tmp_counter;
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                      c_init_type, tmp_id, rhs_t);
        free(rhs_t);
        for (size_t dn = 0; dn < binding_count; dn++) {
            const char *bname =
                mir_instruction_destructure_binding_name_at(inst, dn);
            char ssa_versioned_tmp[192];
            char *ssa_versioned;
            char *ssa_lhs;

            if (bname == NULL)
                continue;
            if (!transpiler_mir_destructure_ssa_local(
                    ssa_versioned_tmp, sizeof(ssa_versioned_tmp), bname)) {
                transpiler_set_mir_topology_invalid(
                    ctx,
                    "tuple destructuring SSA binding name is too long");
                return false;
            }
            ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
            if (ssa_versioned == NULL)
                return false;
            ssa_lhs = transpiler_render_ssa_name(ctx, ssa_versioned);
            if (ssa_lhs == NULL) {
                free(ssa_versioned);
                continue;
            }
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = _pgy_destr_%d.f%zu;\n",
                          ssa_lhs, tmp_id, dn);
            free(ssa_lhs);
            register_typed_var(ctx, bname, elem_names[dn]);
            transpiler_ssa_name_map_set(ssa_map_out, bname, ssa_versioned);
        }
        return true;
    }

    if (transpiler_type_name_is_array_or_slice(init_type_name)) {
        if (slot_inner_type_name_copy(init_type_name, elem_inner_buf,
                sizeof(elem_inner_buf)))
            elem_inner = elem_inner_buf;
        if (transpiler_require_type_name_c_type_copy(ctx, elem_inner,
                "MIR array destructuring element", elem_c_type_buf,
                sizeof(elem_c_type_buf))) {
            elem_c_type = elem_c_type_buf;
        }
    }
    if (c_init_type == NULL || elem_c_type == NULL || elem_inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot lower destructuring initializer of type '%s' to a concrete array element type",
            init_type_name != NULL ? init_type_name : "(unknown)");
        return false;
    }

    {
        char *rhs = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
        int tmp_id;

        if (rhs == NULL) {
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                         "MIR block %llu emission failed: unable to render destructuring initializer",
                         (unsigned long long) block->id);
            }
            return false;
        }
        tmp_id = ++ctx->tmp_counter;
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                      c_init_type, tmp_id, rhs);
        free(rhs);
        for (size_t dn = 0; dn < binding_count; dn++) {
            const char *bname =
                mir_instruction_destructure_binding_name_at(inst, dn);
            char ssa_versioned_tmp[192];
            char *ssa_versioned;
            char *ssa_lhs;

            if (bname == NULL)
                continue;
            if (!transpiler_mir_destructure_ssa_local(
                    ssa_versioned_tmp, sizeof(ssa_versioned_tmp), bname)) {
                transpiler_set_mir_topology_invalid(
                    ctx,
                    "array destructuring SSA binding name is too long");
                return false;
            }
            ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
            if (ssa_versioned == NULL)
                return false;
            ssa_lhs = transpiler_render_ssa_name(ctx, ssa_versioned);
            if (ssa_lhs == NULL) {
                free(ssa_versioned);
                continue;
            }
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = _pgy_destr_%d.data[%zu];\n",
                          ssa_lhs, tmp_id, dn);
            free(ssa_lhs);
            register_typed_var(ctx, bname, elem_inner);
            transpiler_ssa_name_map_set(ssa_map_out, bname, ssa_versioned);
        }
    }
    return true;
}
