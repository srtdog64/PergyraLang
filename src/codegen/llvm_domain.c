/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — domain-specific passes (Party/Systemic/World, Ability,
 * Role, Event).  Extracted from llvm_backend.c to keep file sizes
 * manageable.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static const char *
llvm_operator_suffix(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "sub";
    case TOKEN_STAR: return "mul";
    case TOKEN_SLASH: return "div";
    case TOKEN_PERCENT: return "mod";
    case TOKEN_EQUAL: return "eq";
    case TOKEN_NOT_EQUAL: return "ne";
    case TOKEN_LESS: return "lt";
    case TOKEN_LESS_EQUAL: return "le";
    case TOKEN_GREATER: return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default: return NULL;
    }
}

static bool
llvm_operator_method_name_matches(TokenType op, const char *name)
{
    static const struct {
        TokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }
    return false;
}

static ASTNode *
llvm_find_role_decl(const HIRProgram *hir, const char *role_name)
{
    if (hir == NULL || role_name == NULL)
        return NULL;

    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
        if (stmt != NULL && stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_role_operator_method(const HIRProgram *hir, ASTNode *role,
                               TokenType op, int depth)
{
    if (hir == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && llvm_operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = llvm_find_role_decl(hir, inc->data.include_stmt.role_name);
        ASTNode *method = llvm_find_role_operator_method(hir, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

void
llvm_emit_domain_passes(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    /* Pass 0a: Register Party/Systemic/World struct types + methods */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **shared_fields = NULL;
        size_t shared_count = 0;
        ASTNode **methods = NULL;
        size_t method_count = 0;

        if (stmt->type == AST_PARTY_DECL) {
            decl_name    = stmt->data.party_decl.name;
            shared_fields = stmt->data.party_decl.shared_fields;
            shared_count  = stmt->data.party_decl.shared_count;
            methods       = stmt->data.party_decl.methods;
            method_count  = stmt->data.party_decl.method_count;
        } else if (stmt->type == AST_SYSTEMIC_DECL) {
            decl_name    = stmt->data.systemic_decl.name;
            shared_fields = stmt->data.systemic_decl.shared_fields;
            shared_count  = stmt->data.systemic_decl.shared_count;
            methods       = stmt->data.systemic_decl.methods;
            method_count  = stmt->data.systemic_decl.method_count;
        } else if (stmt->type == AST_WORLD_DECL) {
            decl_name    = stmt->data.world_decl.name;
            shared_fields = stmt->data.world_decl.shared_fields;
            shared_count  = stmt->data.world_decl.shared_count;
            methods       = stmt->data.world_decl.methods;
            method_count  = stmt->data.world_decl.method_count;
        } else {
            continue;
        }

        /* Count dyn role slots (for vtable pointer fields) */
        size_t dyn_slot_count = 0;
        ASTNode **role_slots = NULL;
        size_t role_count = 0;
        if (stmt->type == AST_PARTY_DECL) {
            role_slots = stmt->data.party_decl.role_slots;
            role_count = stmt->data.party_decl.role_count;
        }
        for (size_t j = 0; j < role_count; j++) {
            if (role_slots[j] != NULL
                && role_slots[j]->type == AST_ROLE_SLOT
                && role_slots[j]->data.role_slot.is_dynamic)
                dyn_slot_count++;
        }

        /* Build struct: { shared_fields..., vtable_ptrs_for_dyn_slots... } */
        size_t fc = shared_count + dyn_slot_count;
        LLVMTypeRef *ftypes = calloc(fc > 0 ? fc : 1,
                                       sizeof(LLVMTypeRef));
        for (size_t j = 0; j < shared_count; j++) {
            ASTNode *sf = shared_fields[j];
            ASTNode *sf_type = sf->data.party_shared.type;
            ftypes[j] = (sf_type != NULL)
                ? ast_type_to_llvm(ctx, sf_type)
                : ctx->type_i32;
        }
        /* dyn role slots → i8ptr (vtable pointer) */
        for (size_t j = 0; j < dyn_slot_count; j++)
            ftypes[shared_count + j] = ctx->type_i8ptr;

        LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context,
                                                        decl_name);
        LLVMStructSetBody(struct_ty, ftypes,
                           (unsigned)fc, 0);

        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            decl_name, struct_ty, false);
        if (entry != NULL) {
            for (size_t j = 0; j < shared_count; j++) {
                ASTNode *sf = shared_fields[j];
                llvm_class_add_field(entry,
                    sf->data.party_shared.name,
                    ftypes[j], (int)j);
            }
            /* Register vtable pointer fields for dyn slots */
            size_t dyn_idx = 0;
            for (size_t j = 0; j < role_count; j++) {
                ASTNode *rs = role_slots[j];
                if (rs == NULL || rs->type != AST_ROLE_SLOT
                    || !rs->data.role_slot.is_dynamic)
                    continue;
                char vt_field_buf[256];
                snprintf(vt_field_buf, sizeof(vt_field_buf), "%s_vtable",
                         rs->data.role_slot.slot_name);
                llvm_class_add_field(entry, pergyra_strdup(vt_field_buf),
                    ctx->type_i8ptr,
                    (int)(shared_count + dyn_idx));
                dyn_idx++;
            }
        }
        free(ftypes);

        /* Forward-declare methods */
        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            const char *mname = method->data.func_decl.name;
            size_t pc = method->data.func_decl.param_count;

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                user_pc++;
            }

            LLVMTypeRef *ptypes = calloc(user_pc + 1,
                                           sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr;
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                (unsigned)(user_pc + 1), 0);

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, mname);
            LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn),
                                    fn, ft, ret);
            free(ptypes);
        }
    }

    /* Pass 0b: Register ability vtable types */
    for (size_t i = 0; i < hir->ability_count; i++) {
        ASTNode *stmt = hir->abilities[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        const char *ab_name = stmt->data.ability_decl.name;
        size_t mc = stmt->data.ability_decl.method_count;

        /* Build vtable struct: { fn_ptr_1, fn_ptr_2, ... } */
        LLVMTypeRef *vt_fields = calloc(mc > 0 ? mc : 1,
                                          sizeof(LLVMTypeRef));
        for (size_t j = 0; j < mc; j++) {
            ASTNode *method = stmt->data.ability_decl.methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL) {
                vt_fields[j] = ctx->type_i8ptr;
                continue;
            }

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t pc = method->data.func_decl.param_count;
            /* Count non-self params */
            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && p->name != NULL
                    && strcmp(p->name, "self") == 0)
                    continue;
                user_pc++;
            }
            LLVMTypeRef *ptypes = calloc(user_pc + 1, sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr; /* self */
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && p->name != NULL
                    && strcmp(p->name, "self") == 0)
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef fn_type = LLVMFunctionType(ret,
                ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
            free(ptypes);
        }

        char vt_name[256];
        snprintf(vt_name, sizeof(vt_name), "%s_vtable", ab_name);
        LLVMTypeRef vt_struct = LLVMStructCreateNamed(ctx->context,
                                                        vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        free(vt_fields);

        /* Register as class type so it's findable.
         * Must strdup because vt_name is a stack local. */
        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            pergyra_strdup(vt_name), vt_struct, false);
        if (entry != NULL) {
            for (size_t j = 0; j < mc; j++) {
                ASTNode *method = stmt->data.ability_decl.methods[j];
                if (method != NULL && method->type == AST_FUNC_DECL)
                    llvm_class_add_field(entry,
                        method->data.func_decl.name,
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }

    /* Pass 0c: Forward-declare role methods + create vtable globals */
    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                const char *mname = method->data.func_decl.name;
                size_t pc = method->data.func_decl.param_count;

                LLVMTypeRef ret = ctx->type_void;
                if (method->data.func_decl.return_type != NULL)
                    ret = ast_type_to_llvm(ctx,
                        method->data.func_decl.return_type);

                /* self + user params */
                size_t user_pc = 0;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    user_pc++;
                }

                LLVMTypeRef *ptypes = calloc(user_pc + 1,
                                               sizeof(LLVMTypeRef));
                ptypes[0] = ctx->type_i8ptr;
                size_t pidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    ptypes[pidx++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                }

                LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                    (unsigned)(user_pc + 1), 0);

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, mname);
                LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                    fname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn),
                                        fn, ft, ret);
                free(ptypes);
            }
        }

        {
            TokenType ops[] = {
                TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                TOKEN_GREATER, TOKEN_GREATER_EQUAL
            };
            const char *for_type_name = NULL;
            if (stmt->data.role_decl.for_type != NULL
                && stmt->data.role_decl.for_type->type == AST_TYPE) {
                for_type_name = stmt->data.role_decl.for_type->data.type.name;
            }

            for (size_t oi = 0; for_type_name != NULL
                   && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                const char *suffix = llvm_operator_suffix(ops[oi]);
                ASTNode *method = llvm_find_role_operator_method(hir, stmt, ops[oi], 0);
                if (suffix == NULL || method == NULL)
                    continue;

                char opname[256];
                snprintf(opname, sizeof(opname), "operator_%s_%s",
                         suffix, for_type_name);
                if (llvm_lookup_function(ctx, opname) != NULL)
                    continue;

                FuncParam *rhs_param = NULL;
                size_t rhs_param_count = 0;
                for (size_t pj = 0; pj < method->data.func_decl.param_count; pj++) {
                    FuncParam *p = method->data.func_decl.params[pj];
                    if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                        rhs_param = p;
                        rhs_param_count++;
                    }
                }
                if (rhs_param_count != 1)
                    continue;

                LLVMTypeRef lhs_type = ast_type_to_llvm(ctx, stmt->data.role_decl.for_type);
                LLVMTypeRef rhs_type = (rhs_param != NULL && rhs_param->type != NULL)
                    ? ast_type_to_llvm(ctx, rhs_param->type) : ctx->type_i32;
                LLVMTypeRef ret = method->data.func_decl.return_type != NULL
                    ? ast_type_to_llvm(ctx, method->data.func_decl.return_type)
                    : ctx->type_void;
                LLVMTypeRef params[] = { lhs_type, rhs_type };
                LLVMTypeRef ft = LLVMFunctionType(ret, params, 2, 0);
                LLVMValueRef fn = LLVMAddFunction(ctx->module, opname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            }
        }
    }

    /* Pass 0e: Register event types and generate helper functions */
    for (size_t i = 0; i < hir->event_count; i++) {
        ASTNode *stmt = hir->events[i];
        if (stmt == NULL || stmt->type != AST_EVENT_DECL)
            continue;

        const char *ename = stmt->data.event_decl.name;
        int pc = (int)stmt->data.event_decl.param_count;

        /* Event struct: { [16 x ptr], i64 } → handlers + count */
        LLVMTypeRef handler_arr = LLVMArrayType(ctx->type_i8ptr,
                                                 PGY_EVENT_MAX_HANDLERS);
        LLVMTypeRef sfields[] = { handler_arr, ctx->type_i64 };
        char sname[256];
        snprintf(sname, sizeof(sname), "PgyEvent_%s", ename);
        LLVMTypeRef evt_struct = LLVMStructCreateNamed(ctx->context, sname);
        LLVMStructSetBody(evt_struct, sfields, 2, 0);

        /* Collect handler parameter types */
        LLVMTypeRef ptypes[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = stmt->data.event_decl.params[j];
            ptypes[j] = (p->data.let_decl.type != NULL)
                ? ast_type_to_llvm(ctx, p->data.let_decl.type)
                : ctx->type_i32;
        }
        llvm_register_event(ctx, ename, evt_struct, pc, ptypes);

        /* Handler function type: void(param_types...) */
        LLVMTypeRef handler_ft = LLVMFunctionType(ctx->type_void,
            ptypes, (unsigned)pc, 0);
        LLVMTypeRef handler_ptr_t = LLVMPointerTypeInContext(ctx->context, 0);
        (void)handler_ptr_t;

        /* --- Generate EventName_INIT(ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INIT", ename);
            LLVMTypeRef init_params[] = { ctx->type_i8ptr };
            LLVMTypeRef init_ft = LLVMFunctionType(ctx->type_void,
                init_params, 1, 0);
            LLVMValueRef init_fn = LLVMAddFunction(ctx->module, fname, init_ft);
            llvm_register_function(ctx, LLVMGetValueName(init_fn),
                init_fn, init_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, init_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            /* memset(e, 0, sizeof(struct)) */
            LLVMValueRef e_ptr = LLVMGetParam(init_fn, 0);
            LLVMValueRef sz = LLVMSizeOf(evt_struct);
            LLVMBuildMemSet(ctx->builder, e_ptr,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 0, 0),
                sz, 0);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_SUBSCRIBE(ptr, handler_ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", ename);
            LLVMTypeRef sub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef sub_ft = LLVMFunctionType(ctx->type_void,
                sub_params, 2, 0);
            LLVMValueRef sub_fn = LLVMAddFunction(ctx->module, fname, sub_ft);
            llvm_register_function(ctx, LLVMGetValueName(sub_fn),
                sub_fn, sub_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            LLVMValueRef e_ptr = LLVMGetParam(sub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(sub_fn, 1);

            /* count_ptr = GEP(e, 0, 1) — the i64 count field */
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* if (count < 16) { handlers[count] = h; count++; } */
            LLVMValueRef max_h = LLVMConstInt(ctx->type_i64,
                PGY_EVENT_MAX_HANDLERS, 0);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, count, max_h, "cmp");

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "then");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "end");
            LLVMBuildCondBr(ctx->builder, cmp, then_bb, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
            /* handlers_ptr = GEP(e, 0, 0, count) */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                count
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMBuildStore(ctx->builder, h_ptr, slot);

            /* count++ */
            LLVMValueRef new_count = LLVMBuildAdd(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "new_count");
            LLVMBuildStore(ctx->builder, new_count, count_ptr);
            LLVMBuildBr(ctx->builder, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_UNSUBSCRIBE(ptr, handler_ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", ename);
            LLVMTypeRef unsub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef unsub_ft = LLVMFunctionType(ctx->type_void,
                unsub_params, 2, 0);
            LLVMValueRef unsub_fn = LLVMAddFunction(ctx->module, fname, unsub_ft);
            llvm_register_function(ctx, LLVMGetValueName(unsub_fn),
                unsub_fn, unsub_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(unsub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(unsub_fn, 1);

            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* Loop: for (i = 0; i < count; i++) */
            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "loop");
            LLVMBasicBlockRef found_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "found");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "next");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "body");
            LLVMBuildCondBr(ctx->builder, cmp, body_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");
            LLVMValueRef eq = LLVMBuildICmp(ctx->builder,
                LLVMIntEQ, val, h_ptr, "eq");
            LLVMBuildCondBr(ctx->builder, eq, found_bb, next_bb);

            /* found: shift elements left, count-- */
            LLVMPositionBuilderAtEnd(ctx->builder, found_bb);
            /* Simple: set handlers[i] = handlers[count-1], count-- */
            LLVMValueRef last_idx_val = LLVMBuildSub(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "last");
            LLVMValueRef last_gep_idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                last_idx_val
            };
            LLVMValueRef last_slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, last_gep_idx, 3, "last_slot");
            LLVMValueRef last_val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, last_slot, "last_val");
            LLVMBuildStore(ctx->builder, last_val, slot);
            LLVMBuildStore(ctx->builder, last_idx_val, count_ptr);
            LLVMBuildBr(ctx->builder, done_bb);

            /* next: i++ */
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_INVOKE(ptr, params...) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", ename);
            /* params: ptr (event), then handler params */
            LLVMTypeRef *inv_params = calloc((size_t)(pc + 1),
                sizeof(LLVMTypeRef));
            inv_params[0] = ctx->type_i8ptr;
            for (int j = 0; j < pc; j++)
                inv_params[j + 1] = ptypes[j];

            LLVMTypeRef inv_ft = LLVMFunctionType(ctx->type_void,
                inv_params, (unsigned)(pc + 1), 0);
            LLVMValueRef inv_fn = LLVMAddFunction(ctx->module, fname, inv_ft);
            llvm_register_function(ctx, LLVMGetValueName(inv_fn),
                inv_fn, inv_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(inv_fn, 0);
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "loop");
            LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "call");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBuildCondBr(ctx->builder, cmp, call_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, call_bb);
            /* Load handler pointer */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef hval = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");

            /* Call handler(params...) via indirect call */
            LLVMValueRef *call_args = calloc((size_t)pc, sizeof(LLVMValueRef));
            for (int j = 0; j < pc; j++)
                call_args[j] = LLVMGetParam(inv_fn, (unsigned)(j + 1));
            LLVMBuildCall2(ctx->builder, handler_ft, hval,
                call_args, (unsigned)pc, "");
            free(call_args);

            /* i++ */
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);

            free(inv_params);
        }

        /* Create global variable for this event */
        LLVMValueRef gv = LLVMAddGlobal(ctx->module, evt_struct, ename);
        LLVMSetInitializer(gv, LLVMConstNull(evt_struct));
        LLVMSetLinkage(gv, LLVMInternalLinkage);
    }

    /* Pass 2b: Emit role method bodies + vtable globals */
    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name = impl->data.impl_ability.ability_name;

            /* Emit method bodies */
            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, method->data.func_decl.name);

                LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
                if (fentry == NULL) continue;

                LLVMValueRef fn = fentry->fn;
                LLVMTypeRef ret_type = fentry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);
                llvm_scope_push(ctx);

                /* self param */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                LLVMValueRef self_alloca = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, self_alloca);
                llvm_scope_declare(ctx, "self", self_alloca,
                                    ctx->type_i8ptr);

                /* User params */
                size_t pc = method->data.func_decl.param_count;
                unsigned lpidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    LLVMTypeRef pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                    LLVMValueRef a = llvm_create_entry_alloca(
                        ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, lpidx++), a);
                    llvm_scope_declare(ctx, p->name, a, pt);
                }

                if (method->data.func_decl.body != NULL)
                    llvm_emit_block(method->data.func_decl.body, ctx);

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last =
                        LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }

            {
                TokenType ops[] = {
                    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                    TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                    TOKEN_GREATER, TOKEN_GREATER_EQUAL
                };
                const char *for_type_name = NULL;
                if (stmt->data.role_decl.for_type != NULL
                    && stmt->data.role_decl.for_type->type == AST_TYPE) {
                    for_type_name = stmt->data.role_decl.for_type->data.type.name;
                }

                for (size_t oi = 0; for_type_name != NULL
                       && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                    const char *suffix = llvm_operator_suffix(ops[oi]);
                    ASTNode *method = llvm_find_role_operator_method(hir, stmt, ops[oi], 0);
                    if (suffix == NULL || method == NULL)
                        continue;

                    char opname[256];
                    char mname[256];
                    snprintf(opname, sizeof(opname), "operator_%s_%s",
                             suffix, for_type_name);
                    snprintf(mname, sizeof(mname), "%s_%s",
                             role_name, method->data.func_decl.name);

                    LLVMFuncEntry *op_entry = llvm_lookup_function(ctx, opname);
                    LLVMFuncEntry *method_entry = llvm_lookup_function(ctx, mname);
                    if (op_entry == NULL || method_entry == NULL)
                        continue;
                    if (LLVMCountBasicBlocks(op_entry->fn) > 0)
                        continue;

                    LLVMValueRef saved_fn = ctx->current_function;
                    LLVMTypeRef saved_ret = ctx->current_ret_type;
                    LLVMTypeRef op_ret_type = op_entry->ret_type;
                    ctx->current_function = op_entry->fn;
                    ctx->current_ret_type = op_ret_type;

                    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                        ctx->context, op_entry->fn, "entry");
                    LLVMPositionBuilderAtEnd(ctx->builder, bb);

                    LLVMTypeRef lhs_type = LLVMTypeOf(LLVMGetParam(op_entry->fn, 0));
                    LLVMValueRef lhs_alloca = llvm_create_entry_alloca(
                        ctx, lhs_type, "lhs.addr");
                    LLVMBuildStore(ctx->builder, LLVMGetParam(op_entry->fn, 0), lhs_alloca);

                    LLVMValueRef lhs_self = LLVMBuildBitCast(ctx->builder,
                        lhs_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx));
                    LLVMValueRef rhs_arg = LLVMGetParam(op_entry->fn, 1);
                    LLVMValueRef args[] = { lhs_self, rhs_arg };

                    if (op_ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, method_entry->fn_type,
                            method_entry->fn, args, 2, "");
                        LLVMBuildRetVoid(ctx->builder);
                    } else {
                        LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                            method_entry->fn_type, method_entry->fn,
                            args, 2, llvm_tmp_name(ctx));
                        LLVMBuildRet(ctx->builder, result);
                    }

                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    if (saved_fn != NULL) {
                        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
                        if (last != NULL)
                            LLVMPositionBuilderAtEnd(ctx->builder, last);
                    }
                }
            }

            /* Create vtable global constant */
            char vt_type_name[256];
            snprintf(vt_type_name, sizeof(vt_type_name),
                     "%s_vtable", ab_name);
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls != NULL) {
                size_t mc = impl->data.impl_ability.method_count;
                LLVMValueRef *vals = calloc(mc > 0 ? mc : 1,
                                              sizeof(LLVMValueRef));
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = impl->data.impl_ability.methods[j];
                    if (method == NULL || method->type != AST_FUNC_DECL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    char fname[256];
                    snprintf(fname, sizeof(fname), "%s_%s",
                             role_name, method->data.func_decl.name);
                    LLVMFuncEntry *fe = llvm_lookup_function(ctx, fname);
                    vals[j] = (fe != NULL) ? fe->fn
                        : LLVMConstNull(ctx->type_i8ptr);
                }

                LLVMValueRef vt_const = LLVMConstNamedStruct(
                    vt_cls->struct_type, vals, (unsigned)mc);

                char global_name[256];
                snprintf(global_name, sizeof(global_name),
                         "%s_%s_vtable_instance", role_name, ab_name);
                LLVMValueRef global = LLVMAddGlobal(ctx->module,
                    vt_cls->struct_type, global_name);
                LLVMSetInitializer(global, vt_const);
                LLVMSetGlobalConstant(global, 1);
                LLVMSetLinkage(global, LLVMInternalLinkage);

                free(vals);
            }
        }
    }

    /* Pass 2c: Emit Party/Systemic/World method bodies */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **methods = NULL;
        size_t method_count = 0;

        if (stmt->type == AST_PARTY_DECL) {
            decl_name    = stmt->data.party_decl.name;
            methods      = stmt->data.party_decl.methods;
            method_count = stmt->data.party_decl.method_count;
        } else if (stmt->type == AST_SYSTEMIC_DECL) {
            decl_name    = stmt->data.systemic_decl.name;
            methods      = stmt->data.systemic_decl.methods;
            method_count = stmt->data.systemic_decl.method_count;
        } else if (stmt->type == AST_WORLD_DECL) {
            decl_name    = stmt->data.world_decl.name;
            methods      = stmt->data.world_decl.methods;
            method_count = stmt->data.world_decl.method_count;
        } else {
            continue;
        }

        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, decl_name);

        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, method->data.func_decl.name);

            LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) continue;

            LLVMValueRef fn = fentry->fn;
            LLVMTypeRef ret_type = fentry->ret_type;
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ctx->current_function = fn;
            ctx->current_ret_type = ret_type;

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);
            llvm_scope_push(ctx);

            /* self param */
            LLVMValueRef self_val = LLVMGetParam(fn, 0);
            if (cls != NULL) {
                LLVMTypeRef self_ptr_t = LLVMPointerType(
                    cls->struct_type, 0);
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, self_ptr_t, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, self_ptr_t);
                llvm_register_var_class(ctx, "self", decl_name);
            } else {
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, ctx->type_i8ptr);
            }

            /* User params */
            size_t pc = method->data.func_decl.param_count;
            unsigned lpidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                LLVMTypeRef pt = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
                LLVMValueRef a = llvm_create_entry_alloca(
                    ctx, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, lpidx++), a);
                llvm_scope_declare(ctx, p->name, a, pt);
            }

            if (method->data.func_decl.body != NULL)
                llvm_emit_block(method->data.func_decl.body, ctx);

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder,
                        LLVMConstInt(ret_type, 0, 0));
            }

            llvm_scope_pop(ctx);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;

            if (saved_fn != NULL) {
                LLVMBasicBlockRef last =
                    LLVMGetLastBasicBlock(saved_fn);
                if (last != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, last);
            }
        }
    }

}

#endif /* PGY_LLVM_ENABLED */
