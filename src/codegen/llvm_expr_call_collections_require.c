#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_collections_extended.h"

#include "llvm_internal_api.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_type_projection_internal.h"
#include "codegen_hashmap_key_policy.h"
#include "../compiler/mir_decl_headers.h"
#include <string.h>

bool
llvm_map_receiver_metadata_ready(LLVMGenCtx *ctx, ASTNode *receiver,
                                 LLVMMapReceiverMetadata *metadata)
{
    const char *key = NULL, *value = NULL;
    char *key_copy = NULL, *value_copy = NULL;
    LLVMTypeRef receiver_type = NULL;
    if (ctx == NULL || receiver == NULL || metadata == NULL)
        return false;
    /* The LLVM ctx destroys its persistent arena. These bounded per-query
     * copies survive operand emission and scope/registry growth; inference
     * obtains the same receiver fact without emitting an address or a call. */
    memset(metadata, 0, sizeof(*metadata));
    if (receiver->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(receiver);
        LLVMVarEntry var;
        if (!llvm_scope_lookup_snapshot(ctx, name, &var)) goto missing;
        receiver_type = var.type;
        key = llvm_lookup_map_key(ctx, name);
        value = llvm_lookup_map_value(ctx, name);
        if (key == NULL || value == NULL)
            goto missing;
        key_copy = pgy_arena_alloc(&ctx->persistent, strlen(key) + 1);
        value_copy = pgy_arena_alloc(&ctx->persistent, strlen(value) + 1);
        if (key_copy == NULL || value_copy == NULL) goto allocation;
        memcpy(key_copy, key, strlen(key) + 1);
        memcpy(value_copy, value, strlen(value) + 1);
    } else if (receiver->type == AST_MEMBER_ACCESS) {
        const char *parent = llvm_expr_custom_type_name(ast_member_object(receiver), ctx);
        const char *field_name = ast_member_name(receiver);
        LLVMClassTypeEntry *nominal = parent != NULL ? llvm_lookup_class(ctx, parent) : NULL;
        const MIRDeclField *field = llvm_find_decl_field_in_context(ctx, parent, field_name);
        const char *type = llvm_mir_decl_field_type_name(field);
        char *extra;
        if (nominal == NULL || field_name == NULL
            || llvm_class_field_index(nominal, field_name) < 0
            || field == NULL || mir_decl_field_source_syntax_id(field) == 0
            || type == NULL || strncmp(type, "HashMap<", 8) != 0
            || type[strlen(type) - 1] != '>')
            goto missing;
        receiver_type = llvm_class_field_type_at_index(nominal,
            llvm_class_field_index(nominal, field_name));
        size_t capacity = strlen(type) + 1;
        key_copy = pgy_arena_alloc(&ctx->persistent, capacity);
        value_copy = pgy_arena_alloc(&ctx->persistent, capacity);
        extra = pgy_arena_alloc(&ctx->persistent, capacity);
        if (key_copy == NULL || value_copy == NULL || extra == NULL) goto allocation;
        if (!llvm_constructed_arg_name_copy(type, 0, key_copy, capacity)
            || !llvm_constructed_arg_name_copy(type, 1, value_copy, capacity)
            || llvm_constructed_arg_name_copy(type, 2, extra, capacity))
            goto missing;
    } else {
        goto missing;
    }
    if (pgy_hashmap_key_kind_from_name(key_copy) == PGY_HASHMAP_KEY_UNKNOWN
        || value_copy[0] == '\0'
        || strcmp(value_copy, "Unknown") == 0
        || strcmp(value_copy, "<unknown>") == 0)
        goto missing;
    metadata->value_llvm_type = pergyra_type_to_llvm(ctx, value_copy);
    if (metadata->value_llvm_type == NULL || ctx->has_error) return false;
    LLVMTypeRef expected_receiver_type = llvm_hashmap_struct_type(ctx, value_copy);
    if (expected_receiver_type == NULL || ctx->has_error) return false;
    if (receiver_type != expected_receiver_type) goto missing;
    metadata->key_type = key_copy;
    metadata->value_type = value_copy;
    return true;
allocation:
    if (!ctx->has_error) llvm_set_error(ctx, "out of memory snapshotting LLVM HashMap receiver metadata");
    return false;
missing:
    if (!ctx->has_error)
        llvm_set_error_at_with_hints(ctx, receiver,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM HashMap receiver requires admitted concrete key/value metadata");
    return false;
}

LLVMTypeRef
llvm_collection_required_value_type(LLVMGenCtx *ctx, ASTNode *node,
                                    const char *collection_kind,
                                    const char *var_name,
                                    const char *type_name,
                                    LLVMValueRef *out)
{
    if (type_name == NULL || type_name[0] == '\0') {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation requires concrete element/value type metadata for '%s'",
                collection_kind != NULL ? collection_kind : "collection",
                var_name != NULL ? var_name : "<collection>");
        }
        if (out != NULL)
            *out = NULL;
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, type_name);
}

LLVMFuncEntry *
llvm_required_collection_function(LLVMGenCtx *ctx,
                                  ASTNode *node,
                                  const char *callee_name,
                                  const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM collection operation '%s' requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "collection operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

bool
llvm_collection_required_receiver_var(LLVMGenCtx *ctx,
                                      ASTNode *node,
                                      ASTNode *receiver,
                                      const char *callee_name,
                                      const char *collection_kind,
                                      LLVMVarEntry *receiver_out,
                                      LLVMValueRef *out)
{
    const char *kind;
    const char *name;
    LLVMVarEntry var;

    kind = collection_kind != NULL ? collection_kind : "collection";
    /* Storage projection is separate from collection metadata. Map consumers
     * join this exact field address with the admitted receiver projection. */
    if (receiver != NULL && receiver->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef field_type = NULL;
        LLVMValueRef field_ptr = llvm_emit_member_lvalue_ptr(receiver, ctx,
            &field_type);
        if (field_ptr == NULL || field_type == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM %s operation '%s' requires addressable field storage for its receiver",
                    kind,
                    callee_name != NULL ? callee_name : "collection operation");
            }
            if (out != NULL)
                *out = NULL;
            return false;
        }
        if (receiver_out != NULL) {
            receiver_out->name = ast_member_name(receiver);
            receiver_out->alloca = field_ptr;
            receiver_out->type = field_type;
        }
        return true;
    }
    if (receiver == NULL || receiver->type != AST_IDENTIFIER) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation '%s' requires an identifier receiver",
                kind,
                callee_name != NULL ? callee_name : "collection operation");
        }
        if (out != NULL)
            *out = NULL;
        return false;
    }

    name = ast_identifier_name(receiver);
    if (!llvm_scope_lookup_snapshot(ctx, name, &var)) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s operation '%s' requires registered %s local '%s'",
                kind,
                callee_name != NULL ? callee_name : "collection operation",
                kind,
                name != NULL ? name : "<collection>");
        }
        if (out != NULL)
            *out = NULL;
        return false;
    }
    if (receiver_out != NULL)
        *receiver_out = var;
    return true;
}

bool
llvm_collection_extended_error_out(LLVMGenCtx *ctx, ASTNode *node,
                                   LLVMValueRef *out, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM collection extended builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

#endif
