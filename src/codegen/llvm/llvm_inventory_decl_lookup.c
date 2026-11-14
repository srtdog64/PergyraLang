/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR-backed declaration lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "../host_decl_compat.h"
#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"
#include "../codegen_match_variant_policy.h"
#include "../../compiler/mir/mir_decl_headers.h"
#include "../../parser/ast_api.h"

ASTNode *
llvm_bind_current_host_decl(LLVMGenCtx *ctx, ASTNode *host_decl)
{
    ASTNode *saved_decl = NULL;

    if (ctx == NULL)
        return NULL;
    saved_decl = ctx->current_host_decl;
    ctx->current_host_decl = host_decl;
    ctx->current_class_name =
        host_decl != NULL ? llvm_decl_node_name(host_decl) : NULL;
    return saved_decl;
}

void
llvm_restore_current_host_decl(LLVMGenCtx *ctx, ASTNode *saved_decl)
{
    if (ctx == NULL)
        return;
    ctx->current_host_decl = saved_decl;
    ctx->current_class_name =
        saved_decl != NULL ? llvm_decl_node_name(saved_decl) : NULL;
}

void
llvm_active_inventory(const LLVMGenCtx *ctx,
                      ASTNodeType decl_type,
                      ASTNode ***nodes_out,
                      size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_inventory(ctx->mir, decl_type, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

const char *
llvm_decl_node_name(ASTNode *node)
{
    const char *host_name;

    if (node == NULL)
        return NULL;

    host_name = pgy_host_decl_compat_name(node);
    if (host_name != NULL)
        return host_name;

    switch (node->type) {
    case AST_FUNC_DECL:
        return ast_declaration_name(node);
    case AST_INTENT_DECL:
        return ast_intent_decl_name(node);
    case AST_ABILITY_DECL:
        return ast_ability_name(node);
    case AST_EVENT_DECL:
        return ast_event_name(node);
    case AST_TYPE_ALIAS:
        return ast_type_alias_name(node);
    default:
        return NULL;
    }
}

ASTNode *
llvm_find_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                   ASTNodeType decl_type,
                                   const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header_of_type(ctx->mir, decl_type, name);
        if (decl_header == NULL)
            return NULL;
    }

    llvm_active_inventory(ctx, decl_type, &nodes, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *node = nodes != NULL ? nodes[i] : NULL;
        const char *node_name;
        if (node == NULL || node->type != decl_type)
            continue;
        node_name = llvm_decl_node_name(node);
        if (node_name != NULL && strcmp(node_name, name) == 0)
            return node;
    }

    return NULL;
}

bool
llvm_decl_exists_in_context(const LLVMGenCtx *ctx,
                            ASTNodeType decl_type,
                            const char *name)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx == NULL || name == NULL)
        return false;
    if (ctx->mir != NULL) {
        return mir_find_decl_header_of_type(ctx->mir, decl_type, name)
            != NULL;
    }

    llvm_active_inventory(ctx, decl_type, &nodes, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *node = nodes != NULL ? nodes[i] : NULL;
        const char *node_name;
        if (node == NULL || node->type != decl_type)
            continue;
        node_name = llvm_decl_node_name(node);
        if (node_name != NULL && strcmp(node_name, name) == 0)
            return true;
    }
    return false;
}

bool
llvm_param_is_implicit_self(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

bool
llvm_is_host_decl_type(ASTNodeType decl_type)
{
    return pgy_host_decl_compat_is_type(decl_type);
}

const MIRDeclHeader *
llvm_find_decl_header_in_context_of_type(const LLVMGenCtx *ctx,
                                         ASTNodeType decl_type,
                                         const char *name)
{
    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    return mir_find_decl_header_of_type(ctx->mir, decl_type, name);
}

const MIRDeclHeader *
llvm_find_host_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    const ASTNodeType *host_types = NULL;
    size_t host_type_count = 0;

    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;

    host_types = pgy_host_decl_compat_types(&host_type_count);
    for (size_t i = 0; host_types != NULL && i < host_type_count; i++) {
        const MIRDeclHeader *decl_header =
            llvm_find_decl_header_in_context_of_type(ctx, host_types[i], name);
        if (decl_header != NULL)
            return decl_header;
    }
    return NULL;
}

const MIRDeclField *
llvm_find_decl_field_in_context(const LLVMGenCtx *ctx,
                                const char *host_name,
                                const char *field_name)
{
    const MIRDeclHeader *decl_header;

    if (ctx == NULL || host_name == NULL || field_name == NULL)
        return NULL;
    decl_header = llvm_find_host_decl_header_in_context(ctx, host_name);
    for (size_t i = 0; decl_header != NULL
         && i < mir_decl_header_field_count(decl_header); i++) {
        const MIRDeclField *field = mir_decl_header_field(decl_header, i);
        const char *name = mir_decl_field_name(field);
        if (name != NULL && strcmp(name, field_name) == 0)
            return field;
    }
    return NULL;
}

ASTNode *
llvm_mir_decl_field_type(const MIRDeclField *field)
{
    return mir_decl_field_type(field);
}

const char *
llvm_mir_decl_field_type_name(const MIRDeclField *field)
{
    return mir_decl_field_type_name(field);
}

ASTNode *
llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx, const char *name)
{
    const ASTNodeType *host_types = NULL;
    size_t host_type_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    host_types = pgy_host_decl_compat_types(&host_type_count);
    for (size_t i = 0; host_types != NULL && i < host_type_count; i++) {
        ASTNode *decl = llvm_find_decl_in_active_inventory(
            ctx, host_types[i], name);
        if (decl != NULL)
            return decl;
    }

    return NULL;
}

ASTNode *
llvm_current_host_decl(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    if (ctx->current_host_decl != NULL)
        return ctx->current_host_decl;

    if (ctx->current_within_zone_name != NULL) {
        decl = llvm_find_decl_in_active_inventory(
            ctx, AST_ZONE_DECL, ctx->current_within_zone_name);
        if (decl != NULL)
            return decl;
    }

    return NULL;
}

const char *
llvm_current_host_decl_name(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    decl = llvm_current_host_decl(ctx);
    if (decl == NULL) {
        return ctx->current_within_zone_name;
    }

    return llvm_is_host_decl_type(decl->type)
        ? llvm_decl_node_name(decl)
        : NULL;
}

LLVMSelectedCall
llvm_selected_source_callable(LLVMGenCtx *ctx, ASTNode *call,
                              const char *name)
{
    LLVMSelectedCall result = {LLVM_SELECTED_CALL_INVALID, NULL, NULL};
    if (ctx == NULL || call == NULL || name == NULL
        || !ast_call_semantic_callee_declared_callable(call)
        || ast_call_semantic_callee_is_stdlib(call)
        || ast_call_semantic_callee_value_binding_id(call) != 0)
        return result;
    uint32_t id = ast_call_semantic_callee_decl_id(call);
    if (id == 0)
        return result;
    ASTNode *decl = llvm_find_decl_in_active_inventory(ctx, AST_EVENT_DECL, name);
    if (decl != NULL && ast_node_stable_id(decl) == id) {
        result.kind = LLVM_SELECTED_CALL_EVENT;
        result.decl = decl;
        return result;
    }
    decl = llvm_find_function_decl(ctx, name);
    if (decl != NULL && ast_node_stable_id(decl) == id) {
        result.kind = LLVM_SELECTED_CALL_FUNCTION;
        result.decl = decl;
        return result;
    }
    decl = llvm_find_intent_decl(ctx, name);
    if (decl != NULL && ast_node_stable_id(decl) == id) {
        result.kind = LLVM_SELECTED_CALL_INTENT;
        result.decl = decl;
        return result;
    }
    ASTNode *host = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host);
    const MIRDeclMethod *method = host_name == NULL ? NULL
        : llvm_find_host_method_metadata_in_context(ctx, host_name, name);
    if (method != NULL && mir_decl_method_source_syntax_id(method) == id) {
        result.kind = LLVM_SELECTED_CALL_HOST_METHOD;
        return result;
    }
    MIRRoutineInventory inventory;
    mir_routine_inventory_from_program(ctx->mir, &inventory);
    MIRRoutineSourceLookup alias = mir_routine_inventory_find_method_alias(
        &inventory, id, name);
    if (alias.status == MIR_ROUTINE_SOURCE_LOOKUP_UNIQUE) {
        result.kind = LLVM_SELECTED_CALL_METHOD_ALIAS;
        result.alias = alias.routine;
    }
    return result;
}

LLVMSelectedConstructor
llvm_selected_source_constructor(LLVMGenCtx *ctx, ASTNode *call,
                                 const char *name)
{
    LLVMSelectedConstructor result = {LLVM_SELECTED_CTOR_NONE, NULL};
    if (ctx == NULL || call == NULL || name == NULL
        || pgy_codegen_match_variant_is_builtin(
            pgy_codegen_match_variant_lookup(name)))
        return result;
    if (ast_call_semantic_callee_is_stdlib(call)
        && ast_call_semantic_callee_decl_id(call) == 0)
        return result;
    LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
    ASTNode *decl = variant != NULL
        ? llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL,
            variant->enum_name) : llvm_find_domain_constructor_decl(ctx, name);
    if (decl == NULL && variant == NULL)
        decl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
    if (decl == NULL && variant == NULL)
        return result;
    result.kind = LLVM_SELECTED_CTOR_INVALID;
    ASTNode *callee = ast_call_callee(call);
    uint32_t id = ast_call_semantic_callee_decl_id(call);
    uint32_t binding = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(callee) : 0;
    if (decl == NULL || id == 0 || id != ast_node_stable_id(decl)
        || (binding != 0 && binding != id)
        || ast_call_semantic_callee_declared_callable(call)
        || ast_call_semantic_callee_is_stdlib(call)
        || ast_call_semantic_callee_value_binding_id(call) != 0)
        return result;
    result.kind = variant != NULL
        ? LLVM_SELECTED_CTOR_ENUM : LLVM_SELECTED_CTOR_NOMINAL;
    result.enum_name = variant != NULL ? variant->enum_name : NULL;
    return result;
}

#endif /* PGY_LLVM_ENABLED */
