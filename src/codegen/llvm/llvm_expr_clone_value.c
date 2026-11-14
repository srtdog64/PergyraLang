/* LLVM Clone owner: what a Clone copies into new storage, by type. */
#include "llvm_expr_clone_value.h"

#include <string.h>

#include "../codegen_hashmap_key_policy.h"
#include "../codegen_type_mapping.h"
#include "llvm_expr_array_calls.h"
#include "llvm_internal_api.h"
#include "llvm_internal_emit_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_type_projection_internal.h"
#include "../../compiler/mir/mir_decl_headers.h"
#include "../../parser/ast_api.h"
#include "../../semantic/diag_codes.h"

/* No program struct nests deeper; a deeper chain is refused, not cut. */
#define LLVM_CLONE_MAX_DEPTH 16

static LLVMValueRef llvm_clone_value_at(ASTNode *node, LLVMGenCtx *ctx,
    const char *type_name, LLVMValueRef value, int depth);

static LLVMValueRef
llvm_clone_unsupported(ASTNode *node, LLVMGenCtx *ctx, const char *what, const char *type_name)
{
    llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM Clone of '%s' %s", type_name != NULL ? type_name : "?", what);
    return NULL;
}

static LLVMValueRef
llvm_clone_string(ASTNode *node, LLVMGenCtx *ctx, LLVMValueRef value, int depth)
{
    /* An aggregate's receipt must not be inherited by one of its fields. */
    LLVMRegionAllocation allocation = depth == 0
        ? llvm_mir_region_allocation_scope(ctx, node, "String Clone")
        : LLVM_REGION_ALLOCATION_HEAP;
    if (allocation == LLVM_REGION_ALLOCATION_INVALID) return NULL;
    if (allocation == LLVM_REGION_ALLOCATION_ACTIVE) {
        LLVMFuncEntry *region = llvm_lookup_function(ctx, "pgy_region_string_concat_export");
        if (region == NULL || LLVMTypeOf(value) != ctx->type_i8ptr)
            return llvm_clone_unsupported(node, ctx,
                "requires its admitted region runtime and String value", "String");
        LLVMValueRef args[] = { ctx->region_alloca, value,
            LLVMBuildGlobalStringPtr(ctx->builder, "", llvm_tmp_name(ctx)) };
        return LLVMBuildCall2(ctx->builder, region->fn_type, region->fn,
            args, 3, llvm_tmp_name(ctx));
    }
    LLVMFuncEntry *concat = llvm_lookup_function(ctx, "StringConcat");
    if (concat == NULL || LLVMTypeOf(value) != ctx->type_i8ptr)
        return llvm_clone_unsupported(node, ctx, "requires runtime function 'StringConcat'", "String");
    LLVMValueRef args[] = { value,
        LLVMBuildGlobalStringPtr(ctx->builder, "", llvm_tmp_name(ctx)) };
    return LLVMBuildCall2(ctx->builder, concat->fn_type, concat->fn, args, 2, llvm_tmp_name(ctx));
}

/* An Option {i32 tag, T value} is copied with its payload when its tag is
 * Some (0); None stays None. */
static LLVMValueRef
llvm_clone_option(ASTNode *node, LLVMGenCtx *ctx, const char *type_name,
                  LLVMValueRef value, int depth)
{
    char inner[256];
    LLVMTypeRef option_type = LLVMTypeOf(value);
    if (!slot_inner_type_name_copy(type_name, inner, sizeof(inner)) || inner[0] == '\0' ||
        LLVMGetTypeKind(option_type) != LLVMStructTypeKind ||
        LLVMCountStructElementTypes(option_type) != 2)
        return llvm_clone_unsupported(node, ctx, "requires a concrete {tag, value} Option layout", type_name);
    LLVMBasicBlockRef entry = LLVMGetInsertBlock(ctx->builder);
    LLVMValueRef function = LLVMGetBasicBlockParent(entry);
    LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, value, 0, llvm_tmp_name(ctx));
    LLVMValueRef some = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
        LLVMConstInt(LLVMTypeOf(tag), 0, 0), llvm_tmp_name(ctx));
    LLVMBasicBlockRef copy_block = LLVMAppendBasicBlockInContext(ctx->context, function, "clone.some");
    LLVMBasicBlockRef done_block = LLVMAppendBasicBlockInContext(ctx->context, function, "clone.done");
    LLVMBuildCondBr(ctx->builder, some, copy_block, done_block);
    LLVMPositionBuilderAtEnd(ctx->builder, copy_block);
    LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, value, 1, llvm_tmp_name(ctx));
    LLVMValueRef copy = llvm_clone_value_at(node, ctx, inner, payload, depth + 1);
    if (copy == NULL)
        return NULL;
    LLVMValueRef copied = LLVMBuildInsertValue(ctx->builder, value, copy, 1, llvm_tmp_name(ctx));
    LLVMBasicBlockRef copy_end = LLVMGetInsertBlock(ctx->builder);
    LLVMBuildBr(ctx->builder, done_block);
    LLVMPositionBuilderAtEnd(ctx->builder, done_block);
    LLVMValueRef result = LLVMBuildPhi(ctx->builder, option_type, llvm_tmp_name(ctx));
    LLVMValueRef incoming[] = { value, copied };
    LLVMBasicBlockRef blocks[] = { entry, copy_end };
    LLVMAddIncoming(result, incoming, blocks, 2);
    return result;
}

/* A HashMap with a String, Int, Long or Bool key and a String or scalar
 * value is copied slot by slot into new storage (pgy_map_clone_raw_export);
 * any other map is copied by value, and the checker keeps that Clone
 * dependent on its argument. */
static LLVMValueRef
llvm_clone_map(ASTNode *node, LLVMGenCtx *ctx, const char *type_name, LLVMValueRef value)
{
    char key[128], element[128];
    copy_constructed_arg_name_at(type_name, 0, key, sizeof(key));
    copy_constructed_arg_name_at(type_name, 1, element, sizeof(element));
    bool string_values = strcmp(element, "String") == 0;
    if (pgy_hashmap_key_kind_from_name(key) == PGY_HASHMAP_KEY_UNKNOWN ||
        !(string_values || pgy_hashmap_value_copies_by_bytes(element)))
        return value;
    LLVMTypeRef element_type = pergyra_type_to_llvm(ctx, element);
    LLVMFuncEntry *clone = llvm_lookup_function(ctx, "pgy_map_clone_raw_export");
    if (element_type == NULL || clone == NULL)
        return llvm_clone_unsupported(node, ctx, "requires runtime function 'pgy_map_clone_raw_export'", type_name);
    LLVMTypeRef map_type = LLVMTypeOf(value);
    LLVMValueRef source = llvm_create_entry_alloca(ctx, map_type, llvm_tmp_name(ctx));
    LLVMValueRef target = llvm_create_entry_alloca(ctx, map_type, llvm_tmp_name(ctx));
    if (source == NULL || target == NULL)
        return llvm_clone_unsupported(node, ctx, "could not allocate its map temporaries", type_name);
    LLVMBuildStore(ctx->builder, value, source);
    LLVMValueRef args[] = {
        LLVMBuildBitCast(ctx->builder, target, ctx->type_i8ptr, llvm_tmp_name(ctx)),
        LLVMBuildBitCast(ctx->builder, source, ctx->type_i8ptr, llvm_tmp_name(ctx)),
        llvm_sizeof_type_i64(ctx, element_type),
        LLVMConstInt(ctx->type_i32, string_values ? 1 : 0, 0)
    };
    LLVMBuildCall2(ctx->builder, clone->fn_type, clone->fn, args, 4, "");
    return LLVMBuildLoad2(ctx->builder, map_type, target, llvm_tmp_name(ctx));
}

/* A struct or class declared under this name is copied field by field (a
 * generic instance is not, and is copied by value). A subject slot
 * keeps its identity and is copied by value. */
static LLVMValueRef
llvm_clone_struct(ASTNode *node, LLVMGenCtx *ctx, const char *type_name,
                  LLVMClassTypeEntry *entry, LLVMValueRef value, int depth)
{
    if (LLVMTypeOf(value) != entry->struct_type)
        return llvm_clone_unsupported(node, ctx, "is not its registered struct value", type_name);
    for (int ordinal = 0; ordinal < llvm_class_field_count(entry); ordinal++) {
        const char *field = llvm_class_field_name_at(entry, ordinal);
        int index = llvm_class_field_struct_index_at(entry, ordinal);
        if (llvm_class_field_is_subject_slot_at(entry, ordinal))
            continue;
        const MIRDeclField *row = field != NULL ? llvm_find_decl_field_in_context(ctx, type_name, field) : NULL;
        const char *field_type = row != NULL ? llvm_mir_decl_field_type_name(row) : NULL;
        if (field_type == NULL || index < 0)
            return llvm_clone_unsupported(node, ctx, "has a field with no declaration type name", type_name);
        LLVMValueRef part = LLVMBuildExtractValue(ctx->builder, value, (unsigned)index, llvm_tmp_name(ctx));
        LLVMValueRef copy = llvm_clone_value_at(node, ctx, field_type, part, depth + 1);
        if (copy == NULL)
            return NULL;
        if (copy != part)
            value = LLVMBuildInsertValue(ctx->builder, value, copy, (unsigned)index, llvm_tmp_name(ctx));
    }
    return value;
}

static LLVMValueRef
llvm_clone_value_at(ASTNode *node, LLVMGenCtx *ctx, const char *type_name,
                    LLVMValueRef value, int depth)
{
    LLVMValueRef clone = NULL;
    if (value == NULL)
        return NULL;
    if (depth > LLVM_CLONE_MAX_DEPTH)
        return llvm_clone_unsupported(node, ctx, "nests too deeply", type_name);
    /* A field Array with no runtime clone kernel is copied by value; the
     * checker keeps that Clone dependent on its argument. */
    if ((depth == 0 || llvm_array_value_suffix(ctx, value) != NULL) &&
        llvm_emit_array_clone_value(node, ctx, value, &clone))
        return clone;
    if (type_name == NULL)
        return value;
    if (strcmp(type_name, "String") == 0)
        return llvm_clone_string(node, ctx, value, depth);
    if (strncmp(type_name, "Option<", 7) == 0)
        return llvm_clone_option(node, ctx, type_name, value, depth);
    if (strncmp(type_name, "HashMap<", 8) == 0)
        return llvm_clone_map(node, ctx, type_name, value);
    ASTNode *decl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, type_name);
    LLVMClassTypeEntry *entry = decl != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
    /* A subject is copied field by field only as the Clone argument itself. */
    if (entry != NULL &&
        (ast_class_nominal_kind(decl) == NOMINAL_DECL_STRUCT || ast_class_nominal_kind(decl) == NOMINAL_DECL_CLASS ||
         (ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT && depth == 0)))
        return llvm_clone_struct(node, ctx, type_name, entry, value, depth);
    return value;
}

LLVMValueRef
llvm_emit_clone_value(ASTNode *node, LLVMGenCtx *ctx, const char *type_name, LLVMValueRef value)
{
    /* A parameter carried by address is cloned from the value it points to. */
    LLVMClassTypeEntry *entry = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
    if (entry != NULL && node != NULL && ast_call_arg_count(node) == 1)
        value = llvm_operand_value_for_storage(ctx, ast_call_argument(node, 0), value, entry->struct_type);
    return llvm_clone_value_at(node, ctx, type_name, value, 0);
}
