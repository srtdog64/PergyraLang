#include "transpiler_mir_func_ssa_locals_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_local_type_ast_lookup.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_ssa_local_facts.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"

static bool
transpiler_mir_ssa_local_limit_fail(TranspilerCtx *ctx, const char *name)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_MIR_SSA_LIMIT,
        PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED,
        PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
        "too many MIR SSA locals while emitting function '%s'",
        name != NULL ? name : "<function>");
    return false;
}

bool
transpiler_emit_mir_func_ssa_local_decls(TranspilerCtx *ctx,
                                         ASTNode *node,
                                         const MIRRoutine *mir_routine,
                                         const char *name)
{
    const char *declared_versioned_names[4096];
    size_t declared_versioned_count = 0;

    if (mir_routine == NULL) {
        if (ctx != NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing function SSA local routine for '%s'",
                name != NULL ? name : "<function>");
        }
        return false;
    }

    if (ctx != NULL && node != NULL && node->type == AST_FUNC_DECL
        && mir_routine != NULL
        && !transpiler_mir_routine_signature_metadata_complete_for(ctx,
            mir_routine,
            node,
            0u,
            "MIR-only C path missing function SSA local signature metadata for '%s'",
            NULL,
            NULL)) {
        return false;
    }

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
        for (size_t j = 0; j < block->live_in_name_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->live_in_names[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->ssa_entry_value_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->ssa_entry_values[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->ssa_exit_value_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->ssa_exit_values[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->renamed_local_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->renamed_locals[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            for (size_t u = 0; u < inst->use_count; u++) {
                if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                        &declared_versioned_count,
                                                        4096,
                                                        inst->uses[u])) {
                    return transpiler_mir_ssa_local_limit_fail(ctx, name);
                }
            }
            if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
                && inst->result_name != NULL
                && !transpiler_versioned_name_list_add(declared_versioned_names,
                                                       &declared_versioned_count,
                                                       4096,
                                                       inst->result_name)) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
    }

    for (size_t i = 0; i < declared_versioned_count; i++) {
        const char *versioned_name = declared_versioned_names[i];
        char base[128];
        size_t version = 0;
        const char *type_name = NULL;
        char *owned_type_name = NULL;
        char normalized_type_buf[128];
        char c_type_buf[256];
        const char *c_type = NULL;
        ASTNode *type_ast = NULL;
        char *c_name = NULL;
        char *initial_expr = NULL;
        char *decl = NULL;
        bool has_param_fact = false;
        bool has_source_local_fact = false;
        bool is_destructure_binding = false;

        if (versioned_name == NULL
            || !transpiler_parse_versioned_name(versioned_name,
                                                base,
                                                sizeof(base),
                                                &version)) {
            continue;
        }
        /* A versioned SSA local (base.N with N > 0) is always a function-
         * local introduced by let-decl or assignment LHS and must get a
         * declaration here, even when the host class declares a field of
         * the same name. Skipping its declaration leaves later
         * `_pgy_ssa_base_N` references undeclared in the emitted C. The
         * `transpiler_is_implicit_field` short-circuit was only ever
         * meant for the version==0 case where the name truly resolves
         * to `self->base`. */
        if (version == 0 && transpiler_is_implicit_field(ctx, base))
            continue;
        owned_type_name = transpiler_mir_ssa_local_find_versioned_type_name(
            ctx, node, mir_routine, versioned_name);
        type_name = owned_type_name;
        if (type_name == NULL || strcmp(type_name, "Unknown") == 0)
            type_name = transpiler_find_local_type_name(ctx, node, base);
        if (type_name == NULL || strcmp(type_name, "Unknown") == 0) {
            type_name =
                transpiler_mir_ssa_local_find_receive_payload_type_name(
                ctx, node, mir_routine, base);
        }
        if (type_name != NULL
            && strchr(type_name, ':') != NULL
            && strchr(type_name, '<') == NULL) {
            if (pergyra_str_copy(normalized_type_buf,
                    sizeof(normalized_type_buf), type_name)) {
                transpiler_mir_ssa_local_trim_type_annotation_suffix(
                    normalized_type_buf);
                if (normalized_type_buf[0] != '\0')
                    type_name = normalized_type_buf;
            }
        }
        if (transpiler_type_name_is_claim_shape(type_name)) {
            free(owned_type_name);
            continue;
        }
        type_ast = NULL;
        if (type_name == NULL)
            type_ast = transpiler_find_local_event_handler_type_ast(
                ctx, node, base);
        if (type_ast != NULL) {
            c_name = transpiler_render_ssa_name(ctx, versioned_name);
            decl = pergyra_ast_typed_declarator_in_ctx(ctx, type_ast, c_name);
            if (decl == NULL) {
                free(c_name);
                free(owned_type_name);
                return false;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s = 0;\n", decl);
            write_indent(ctx);
            codebuf_write(ctx->out, "(void)%s;\n", c_name);
            free(decl);
            free(c_name);
            free(owned_type_name);
            continue;
        }
        if (type_name != NULL) {
            if (transpiler_require_type_name_c_type_copy(ctx, type_name,
                    "MIR SSA local", c_type_buf, sizeof(c_type_buf))) {
                c_type = c_type_buf;
            }
        }
        if (c_type == NULL
            || c_type[0] == '\0'
            || (type_name != NULL && strcmp(type_name, "Unknown") == 0)
            || strcmp(c_type, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot determine C type for MIR local '%s' in function '%s'",
                versioned_name,
                name != NULL ? name : "<function>");
            free(owned_type_name);
            return false;
        }
        if (version == 0) {
            has_param_fact = transpiler_mir_ssa_local_routine_has_param_name(
                mir_routine, base);
            has_source_local_fact =
                transpiler_mir_ssa_local_routine_has_source_def(mir_routine,
                    base);
            is_destructure_binding =
                transpiler_mir_ssa_local_routine_has_destructure_binding(
                    mir_routine, base);
        }
        register_typed_var(ctx, versioned_name, type_name);
        if (version == 0 && (has_param_fact || has_source_local_fact))
            transpiler_mir_ssa_local_register_base_type_fact(ctx,
                mir_routine, versioned_name, base, type_name);
        c_name = transpiler_render_ssa_name(ctx, versioned_name);
        write_indent(ctx);
        if (version == 0) {
            bool has_entry_local = false;
            if (!has_param_fact)
                has_entry_local =
                    transpiler_mir_ssa_local_entry_has_source_def(
                    mir_routine, base);
            if (has_param_fact || (has_entry_local && !is_destructure_binding))
                initial_expr = pergyra_strdup(base);
        }
        if (initial_expr != NULL) {
            codebuf_write(ctx->out, "%s %s = %s;\n",
                          c_type, c_name, initial_expr);
        } else if (transpiler_c_type_uses_scalar_zero(c_type)) {
            codebuf_write(ctx->out, "%s %s = 0;\n", c_type, c_name);
        } else {
            codebuf_write(ctx->out, "%s %s = (%s){0};\n",
                          c_type, c_name, c_type);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", c_name);
        free(initial_expr);
        free(c_name);
        free(owned_type_name);
    }

    return true;
}
