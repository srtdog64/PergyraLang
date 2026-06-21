/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM ability vtable forward declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_forward_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_internal.h"
#include "../compiler/mir_decl_headers.h"

typedef struct LLVMAbilityMethodView
{
    const MIRDeclHeader *decl_header;
    size_t              count;
    bool                uses_mir_metadata;
    bool                requires_mir_metadata;
} LLVMAbilityMethodView;

static LLVMAbilityMethodView
llvm_ability_method_view_from_decl(const LLVMGenCtx *ctx,
                                   const char *ability_name)
{
    LLVMAbilityMethodView view;

    view.decl_header = NULL;
    view.count = 0;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = true;

    if (llvm_active_has_mir(ctx)) {
        view.decl_header = llvm_find_decl_header_in_context_of_type(
            ctx, AST_ABILITY_DECL, ability_name);
        if (view.decl_header != NULL) {
            view.count = mir_decl_header_method_count(view.decl_header);
            view.uses_mir_metadata = true;
        }
        return view;
    }

    return view;
}

static bool
llvm_ability_method_view_missing_mir_metadata(
    const LLVMAbilityMethodView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && !view->uses_mir_metadata;
}

static const MIRDeclMethod *
llvm_ability_method_view_metadata(const LLVMAbilityMethodView *view,
                                  size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_method(view->decl_header, index);
}

static bool
llvm_require_ability_method_view_rows(LLVMGenCtx *ctx,
                                      const LLVMAbilityMethodView *view,
                                      const char *ability_name)
{
    if (llvm_ability_method_view_missing_mir_metadata(view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing ability vtable method metadata for '%s'",
            ability_name != NULL ? ability_name : "(anonymous-ability)");
        return false;
    }
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        if (view->uses_mir_metadata
            && llvm_ability_method_view_metadata(view, i) == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid ability vtable method metadata row for '%s'",
                ability_name != NULL ? ability_name : "(anonymous-ability)");
            return false;
        }
    }
    return true;
}

void
llvm_emit_domain_ability_vtables(LLVMGenCtx *ctx,
                                 ASTNode **abilities,
                                 size_t ability_count)
{
    if (ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *stmt = abilities[i];
        const char *ab_name;
        LLVMAbilityMethodView methods;
        LLVMTypeRef *vt_fields;
        char vt_name[256];
        LLVMTypeRef vt_struct;
        LLVMClassTypeEntry *entry;

        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        ab_name = ast_ability_name(stmt);
        methods = llvm_ability_method_view_from_decl(ctx, ab_name);
        if (!llvm_require_ability_method_view_rows(ctx, &methods, ab_name))
            return;
        if (mir_decl_header_name(methods.decl_header) != NULL) {
            ab_name = mir_decl_header_name(methods.decl_header);
        }
        vt_fields = pgy_arena_calloc(&ctx->scratch,
            (methods.count > 0 ? methods.count : 1) * sizeof(LLVMTypeRef));
        if (vt_fields == NULL) {
            llvm_set_error(ctx,
                "LLVM ability vtable field allocation failed for '%s'",
                ab_name != NULL ? ab_name : "(anonymous)");
            return;
        }
        for (size_t j = 0; j < methods.count; j++) {
            const MIRDeclMethod *method_meta =
                llvm_ability_method_view_metadata(&methods, j);
            LLVMTypeRef ret;
            size_t pc;
            size_t user_pc = 0;
            LLVMTypeRef *ptypes;
            size_t pidx = 1;
            LLVMTypeRef fn_type;

            if (method_meta == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing ability vtable method metadata row for '%s'",
                    ab_name != NULL ? ab_name : "(anonymous-ability)");
                return;
            }

            {
                const char *mname =
                    llvm_domain_method_name_metadata_first(
                        method_meta, NULL, false);
                const char *return_type_name =
                    llvm_domain_method_return_type_name_metadata_first(
                        method_meta, NULL, false);

                if (mname == NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing ability vtable method name metadata for '%s'",
                        ab_name != NULL ? ab_name : "(anonymous-ability)");
                    return;
                }
                if (!llvm_mir_decl_method_metadata_complete_for(ctx,
                        method_meta,
                        ab_name,
                        mname,
                        LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                        "MIR-only LLVM path missing ability vtable return type-name metadata for '%s.%s'",
                        "MIR-only LLVM path missing ability vtable parameter type-name metadata for '%s.%s'")) {
                    return;
                }
                ret = ctx->type_void;
                if (return_type_name != NULL) {
                    ret = pergyra_type_to_llvm(ctx, return_type_name);
                    if (ctx->has_error || ret == NULL)
                        return;
                }

                pc = llvm_domain_method_param_count_metadata_first(
                    method_meta, NULL, false);
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p =
                        llvm_domain_method_param_metadata_first(
                            method_meta, NULL, k, false);
                    if (!llvm_param_is_implicit_self_local(p))
                        user_pc++;
                }
                ptypes = pgy_arena_calloc(&ctx->scratch,
                    (user_pc + 1) * sizeof(LLVMTypeRef));
                if (ptypes == NULL) {
                    llvm_set_error(ctx,
                        "LLVM ability method parameter allocation failed for '%s.%s'",
                        ab_name != NULL ? ab_name : "(anonymous)",
                        mname != NULL ? mname : "(anonymous)");
                    return;
                }
                ptypes[0] = ctx->type_i8ptr;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p =
                        llvm_domain_method_param_metadata_first(
                            method_meta, NULL, k, false);
                    const char *param_type_name =
                        llvm_domain_method_param_type_name_metadata_first(
                            method_meta, NULL, k, false);
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    LLVMTypeRef pt = NULL;
                    if (param_type_name != NULL) {
                        pt = pergyra_type_to_llvm(ctx, param_type_name);
                    }
                    if (ctx->has_error || pt == NULL)
                        return;
                    ptypes[pidx++] = pt;
                }
            }

            fn_type = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
        }

        if (!llvm_domain_forward_suffix_name(vt_name, sizeof(vt_name),
                ab_name, "vtable")) {
            llvm_set_error(ctx,
                "LLVM ability vtable name is too long for '%s'", ab_name);
            return;
        }
        vt_struct = LLVMStructCreateNamed(ctx->context, vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)methods.count, 0);
        entry = llvm_register_class(ctx, pergyra_strdup(vt_name),
                                    vt_struct, false, false);
        if (entry != NULL) {
            for (size_t j = 0; j < methods.count; j++) {
                const MIRDeclMethod *method_meta =
                    llvm_ability_method_view_metadata(&methods, j);
                const char *mname =
                    llvm_domain_method_name_metadata_first(
                        method_meta, NULL, false);
                if (mname != NULL)
                    llvm_class_add_field(entry,
                        mname,
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }
}

#endif
