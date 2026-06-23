#include "transpiler_let_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_constructor_emit.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_inventory_view.h"
#include "transpiler_let_box_emit.h"
#include "transpiler_let_channel_emit.h"
#include "transpiler_let_collection_emit.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_let_type_register_emit.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_signature.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_symbols.h"
#include "transpiler_type_declarator.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static char *
transpiler_let_emit_initializer(TranspilerCtx *ctx,
                                ASTNode *init,
                                const char *binding_name,
                                const char *role)
{
    char *rendered = emit_expression(init, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C let binding '%s' could not lower %s expression",
        binding_name != NULL ? binding_name : "<binding>",
        role != NULL ? role : "initializer");
    return NULL;
}

void
emit_let_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = ast_let_name(node);
    ASTNode    *init = ast_let_initializer(node);
    ASTNode    *ann  = ast_let_type(node);
    ASTNode    *resolved_ann = transpiler_active_has_mir(ctx)
        ? ann : resolve_type_alias_target(ctx, ann);
    const char *ann_node_type_name = ast_type_name(ann);
    const char *resolved_ann_type_name = ast_type_name(resolved_ann);
    char       *ann_type_name = ann != NULL
        ? render_type_name_in_ctx(ctx, ann) : NULL;
    ASTNode    *callable_type = NULL;
    ASTNode    *callable_decl = NULL;
    const char *generic_class_spec_name = NULL;
    if (ann_type_name != NULL) {
        const char *target_type_name =
            transpiler_type_alias_target_type_name_from_headers(
                ctx, ann_type_name);
        if (target_type_name != NULL) {
            char *target_copy = pergyra_strdup(target_type_name);
            if (target_copy == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: type-alias annotation allocation failed for '%s'",
                    name != NULL ? name : "<binding>");
                free(ann_type_name);
                return;
            }
            free(ann_type_name);
            ann_type_name = target_copy;
            if (strchr(ann_type_name, '<') == NULL
                && strchr(ann_type_name, '(') == NULL) {
                resolved_ann_type_name = ann_type_name;
            }
        }
    }
    if (ast_let_is_alias(node)) {
        register_alias_var(ctx, name, init);
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
        } else if (init != NULL) {
            const char *inferred = infer_expression_type_name(ctx, init);
            if (inferred != NULL)
                register_typed_var(ctx, name, inferred);
        }
        free(ann_type_name);
        return;
    }
    if (ann != NULL && ann->type == AST_TYPE
        && ann_node_type_name != NULL) {
        ASTNode *gc_decl = find_class_decl(ctx, ann_node_type_name);
        if (gc_decl != NULL && transpiler_class_has_generic_params(gc_decl)) {
            generic_class_spec_name =
                ensure_generic_class_specialization(ctx, gc_decl, ann);
            if (generic_class_spec_name == NULL)
                return;
            free(ann_type_name);
            ann_type_name = pergyra_strdup(generic_class_spec_name);
        }
    }
    if (ann != NULL && ann->type == AST_EVENT_HANDLER_TYPE) {
        callable_type = ann;
    } else if (init != NULL && init->type == AST_CALL
               && ast_call_callee(init) != NULL
               && ast_call_callee(init)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_call_callee(init)) != NULL) {
        ASTNode *decl = find_function_decl(ctx,
            ast_identifier_name(ast_call_callee(init)));
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            ASTNode *return_type = NULL;
            const MIRRoutine *routine =
                transpiler_find_mir_function(ctx, decl);
            bool generic_call =
                transpiler_mir_or_ast_function_is_generic(routine, decl);
            bool extern_func = transpiler_decl_is_extern_function(ctx, decl);
            if (!generic_call && !extern_func
                && transpiler_active_has_mir(ctx)) {
                if (routine == NULL) {
                    transpiler_set_mir_inventory_missing(ctx,
                        "MIR-only C path missing callable let return routine for '%s'",
                        ast_identifier_name(ast_call_callee(init)));
                    free(ann_type_name);
                    return;
                }
                if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                        routine, decl, 0,
                        "MIR-only C path missing callable let return signature metadata for '%s'",
                        NULL, NULL)) {
                    free(ann_type_name);
                    return;
                }
                return_type = transpiler_mir_routine_return_type(routine);
            } else if (generic_call || extern_func) {
                return_type = ast_func_return_type(decl);
            } else {
                return_type = NULL;
            }
            if (return_type != NULL
                && return_type->type == AST_EVENT_HANDLER_TYPE) {
                callable_type = return_type;
            }
        }
    } else if (init != NULL && init->type == AST_IDENTIFIER
               && ast_identifier_name(init) != NULL) {
        ASTNode *decl = find_function_decl(ctx, ast_identifier_name(init));
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            callable_decl = decl;
        }
    }
    if (transpiler_try_emit_let_slot_claim(node, ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }
    if (transpiler_try_emit_let_slot_view_or_move(ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }
    if (transpiler_try_emit_let_slot_sugar(ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }
    if (transpiler_try_emit_box_family_let(ctx, name, init, ann,
            &ann_type_name)) {
        return;
    }

    if (transpiler_try_emit_channel_let(ctx, name, init, &ann_type_name)) {
        return;
    }

    if (transpiler_try_emit_option_let(ctx, name, init, &ann_type_name)) {
        return;
    }

    if (transpiler_try_emit_collection_ctor_let(ctx,
            name,
            init,
            resolved_ann,
            resolved_ann_type_name,
            &ann_type_name)) {
        return;
    }

    if (resolved_ann != NULL
        && resolved_ann->type == AST_TYPE
        && resolved_ann_type_name != NULL
        && init != NULL
        && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && strcmp(ast_identifier_name(ast_call_callee(init)), "ToObject") == 0
        && ast_call_arg_count(init) >= 2
        && ast_call_argument(init, 1) != NULL
        && ast_call_argument(init, 1)->type == AST_IDENTIFIER) {
        ASTNode *target_decl = find_class_decl(ctx, resolved_ann_type_name);
        if (target_decl != NULL
            && target_decl->type == AST_CLASS_DECL
            && ast_class_nominal_kind(target_decl) == NOMINAL_DECL_OBJECT) {
            const char *source_name =
                ast_identifier_name(ast_call_argument(init, 1));
            register_projection_borrow_var(ctx, name,
                ann_type_name != NULL ? ann_type_name : resolved_ann_type_name,
                source_name);
            free(ann_type_name);
            return;
        }
    }

    /* Array literal: let arr = [1, 2, 3] -> PgyArray_Int arr = ({ ... }); */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        const char *array_type_name = ann_type_name != NULL
            ? ann_type_name
            : infer_expression_type_name(ctx, init);
        char array_c_type_buf[256];
        const char *array_c_type = NULL;
        const char *saved_expected_type = ctx->expected_type;
        char *init_expr;
        if (array_type_name == NULL
            || strcmp(array_type_name, "Array<Unknown>") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: empty array literal binding '%s' requires an explicit Array<T> annotation",
                name != NULL ? name : "<binding>");
            free(ann_type_name);
            return;
        }
        if (transpiler_require_type_name_c_type_copy(ctx, array_type_name,
                "array literal binding", array_c_type_buf,
                sizeof(array_c_type_buf))) {
            array_c_type = array_c_type_buf;
        }
        if (array_c_type == NULL || strcmp(array_c_type, "void*") == 0) {
            free(ann_type_name);
            return;
        }
        ctx->expected_type = array_type_name;
        init_expr = transpiler_let_emit_initializer(ctx, init,
            name, "array literal initializer");
        ctx->expected_type = saved_expected_type;
        if (init_expr == NULL) {
            free(ann_type_name);
            return;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", array_c_type, name, init_expr);
        free(init_expr);
        register_typed_var(ctx, name, array_type_name);
        free(ann_type_name);
        return;
    }

    /* Normal variable with type inference */
    char inferred_c_type_buf[256];
    char annotated_c_type_buf[256];
    const char *c_type = NULL;
    if (ann != NULL) {
        const char *ann_source_type = ann_type_name;
        if (ann_source_type != NULL
            && transpiler_require_type_name_c_type_copy(ctx, ann_source_type,
                "let annotation",
                annotated_c_type_buf,
                sizeof(annotated_c_type_buf))) {
            c_type = annotated_c_type_buf;
        }
    } else if (init != NULL) {
        const char *inferred_type = NULL;
        /* Type inference from initializer */
        if (init->type == AST_NUMBER) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL
                && transpiler_require_type_name_c_type_copy(ctx, inferred_type,
                    "numeric initializer",
                    inferred_c_type_buf, sizeof(inferred_c_type_buf))) {
                c_type = inferred_c_type_buf;
            }
        }
        else if (init->type == AST_STRING)  c_type = "char*";
        else if (init->type == AST_BOOLEAN) c_type = "bool";
        else if (init->type == AST_SPAWN_EXPR) c_type = "PgyTaskHandle";
        else if (init->type == AST_CHANNEL_RECV) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL
                && transpiler_require_type_name_c_type_copy(ctx, inferred_type,
                    "channel receive initializer",
                    inferred_c_type_buf, sizeof(inferred_c_type_buf))) {
                c_type = inferred_c_type_buf;
            }
        }
        else if (init->type == AST_CALL || init->type == AST_ARRAY_LITERAL || init != NULL) {
            inferred_type = infer_expression_type_name(ctx, init);
            if (inferred_type != NULL
                && transpiler_require_type_name_c_type_copy(ctx, inferred_type,
                    "let initializer",
                    inferred_c_type_buf, sizeof(inferred_c_type_buf))) {
                c_type = inferred_c_type_buf;
            }
        }
    }

    if (c_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine C type for let binding '%s'; explicit annotation or resolvable initializer type is required",
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        return;
    }

    /* Collection constructors: let s: Set<Int> = SetNew()
     * Emit the correct type-specific initializer from the annotation. */
    if (init != NULL && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && strcmp(ast_identifier_name(ast_call_callee(init)), "SetNew") == 0
        && transpiler_type_name_is_set(ann_type_name)) {
        char inner_buf[128];
        const char *inner = inner_buf;
        char suffix_buf[128];
        char set_c_type_buf[256];
        const char *c_type = NULL;
        const char *suffix = suffix_buf;
        slot_inner_type_name_copy(ann_type_name, inner_buf, sizeof(inner_buf));
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        if (transpiler_require_type_name_c_type_copy(ctx, ann_type_name,
                "Set binding annotation", set_c_type_buf,
                sizeof(set_c_type_buf))) {
            c_type = set_c_type_buf;
        }
        if (c_type == NULL) {
            free(ann_type_name);
            return;
        }
        ensure_collection_specialization(ctx, "Set", inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = pgy_set_new_%s();\n",
                      c_type, name, suffix);
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (init != NULL
        && init->type == AST_UNARY
        && ast_unary_operator(init).type == TOKEN_QUESTION) {
        ASTNode *operand = ast_unary_operand(init);
        const char *result_type = infer_expression_type_name(ctx, operand);
        const char *result_c_type;
        char *operand_expr;
        int try_id;
        int current_returns_result;

        if (!transpiler_type_name_is_result(result_type)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C try lowering for let binding '%s' requires Result<T,E> operand type",
                name != NULL ? name : "<binding>");
            free(ann_type_name);
            return;
        }
        current_returns_result = ctx->current_return_type[0] != '\0'
            && transpiler_type_name_is_result(ctx->current_return_type);

        char result_c_type_buf[256];
        char c_type_buf[256];
        if (transpiler_require_type_name_c_type_copy(ctx, result_type,
                "try operand Result", result_c_type_buf,
                sizeof(result_c_type_buf))) {
            result_c_type = result_c_type_buf;
        } else {
            free(ann_type_name);
            return;
        }
        if (c_type != NULL) {
            copy_capped_string(c_type_buf, sizeof(c_type_buf), c_type);
            c_type = c_type_buf;
        } else {
            c_type = "Unknown";
        }
        operand_expr = transpiler_let_emit_initializer(ctx, operand,
            name, "try operand");
        if (operand_expr == NULL) {
            free(ann_type_name);
            return;
        }
        try_id = ctx->tmp_counter++;
        const char *ok_tag =
            pgy_codegen_match_variant_c_result_tag(PGY_MATCH_VARIANT_OK);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s __try_%d = %s;\n",
                      result_c_type, try_id, operand_expr);
        write_indent(ctx);
        if (current_returns_result) {
            codebuf_write(ctx->out,
                "if (__try_%d.tag != %s) return __try_%d;\n",
                try_id, ok_tag, try_id);
        } else {
            codebuf_write(ctx->out,
                "if (__try_%d.tag != %s) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR);\n",
                try_id, ok_tag);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = __try_%d.ok;\n",
                      c_type, name, try_id);
        free(operand_expr);
        if (ann_type_name != NULL) {
            register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
        } else {
            const char *inferred = infer_expression_type_name(ctx, init);
            if (inferred != NULL)
                register_typed_var(ctx, name, inferred);
        }
        return;
    }

    /* Struct/class constructor: let p: Point = Point(...)
     * Lower positional constructor args into field-order initialization.
     * Missing fields stay zero-initialized.
     * For generic classes: callee is "Node" but ann_type_name is "Node_Int",
     * so also match against the original class name via generic_class_spec_name. */
    if (init != NULL && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && (strcmp(ast_identifier_name(ast_call_callee(init)), ann_type_name) == 0
            || (generic_class_spec_name != NULL
                && ann_node_type_name != NULL
                && strcmp(ast_identifier_name(ast_call_callee(init)), ann_node_type_name) == 0))
        ) {
        ASTNode *class_decl = find_class_decl(ctx, ann_type_name);
        /* For generic classes, find_class_decl won't find "Node_Int";
         * fall back to the original generic class declaration. */
        if (class_decl == NULL && generic_class_spec_name != NULL
            && ann_node_type_name != NULL)
            class_decl = find_class_decl(ctx, ann_node_type_name);
        if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
            char *init_expr = transpiler_emit_class_constructor_with_type(
                init, class_decl, ann_type_name, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = %s;\n",
                ann_type_name, name, init_expr != NULL ? init_expr : "0");
            free(init_expr);
        } else {
            /* Domain/runtime constructors carry internal state bits.
             * Reuse expression lowering so zone/world/relation/effect
             * literals preserve dirty/ready/runtime metadata. */
            ASTNode *domain_constructor_decl =
                transpiler_find_domain_constructor_decl_local(
                    ctx, ann_type_name);
            if (domain_constructor_decl != NULL) {
                char *init_expr = transpiler_let_emit_initializer(ctx,
                    init, name, "domain constructor");
                if (init_expr == NULL) {
                    free(ann_type_name);
                    return;
                }
                codebuf_write(ctx->out, "%s %s = %s;\n",
                    ann_type_name, name, init_expr);
                free(init_expr);
            } else {
                codebuf_write(ctx->out, "%s %s = {0};\n", ann_type_name, name);
            }
        }
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    write_indent(ctx);
    if (callable_type != NULL || callable_decl != NULL) {
        char *decl = callable_type != NULL
            ? pergyra_ast_typed_declarator_in_ctx(ctx, callable_type, name)
            : pergyra_func_pointer_declarator_from_decl_in_ctx(
                ctx, callable_decl, name);
        if (decl == NULL) {
            free(ann_type_name);
            return;
        }
        if (init != NULL) {
            const char *saved_expected_type = ctx->expected_type;
            ASTNode *saved_expected_callable_type = ctx->expected_callable_type;
            ctx->expected_type = ann_type_name;
            if (callable_type != NULL)
                ctx->expected_callable_type = callable_type;
            char *init_expr = transpiler_let_emit_initializer(ctx, init,
                name, "callable initializer");

            ctx->expected_callable_type = saved_expected_callable_type;
            ctx->expected_type = saved_expected_type;
            if (init_expr == NULL) {
                free(decl);
                free(ann_type_name);
                return;
            }
            codebuf_write(ctx->out, "%s = %s;\n", decl, init_expr);
            free(init_expr);
        } else {
            codebuf_write(ctx->out, "%s = 0;\n", decl);
        }
        free(decl);
    } else if (init != NULL) {
        ctx->expected_type = ann_type_name;
        char *init_expr = transpiler_let_emit_initializer(ctx, init,
            name, "initializer");
        ctx->expected_type = NULL;
        if (init_expr == NULL) {
            free(ann_type_name);
            return;
        }
        codebuf_write(ctx->out, "%s %s = %s;\n", c_type, name, init_expr);
        free(init_expr);
    } else if (transpiler_c_type_uses_scalar_zero(c_type)) {
        /* Scalar / pointer default. */
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    } else {
        /* Aggregate default.  Plain `= 0` is not a valid struct initializer
         * in C99, so emit a compound literal zero.  Semantic currently
         * rejects uninitialized locals before reaching this path, but this
         * keeps the fallback well-formed as defense in depth. */
        codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, name, c_type);
    }

    transpiler_register_let_type_after_emit(ctx, name, init, ann_type_name);
}
