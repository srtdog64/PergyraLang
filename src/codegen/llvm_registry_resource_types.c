/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend resource/container type-shape helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMTypeRef
llvm_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->slot_type_Int;
    case PGY_TK_LONG:   return ctx->slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->slot_type_Double;
    case PGY_TK_BOOL:   return ctx->slot_type_Bool;
    case PGY_TK_STRING: return ctx->slot_type_String;
    default: {
        LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
        if (ctx->has_error || inner_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = { inner_ty, LLVMInt1TypeInContext(ctx->context) };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    }
}

LLVMTypeRef
llvm_pinned_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->pinned_slot_type_Int;
    case PGY_TK_LONG:   return ctx->pinned_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->pinned_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->pinned_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->pinned_slot_type_Bool;
    case PGY_TK_STRING: return ctx->pinned_slot_type_String;
    default: {
        LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
        if (ctx->has_error || slot_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            LLVMPointerType(slot_ty, 0), ctx->type_i1, ctx->type_i1
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    }
}

LLVMTypeRef
llvm_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->secure_slot_type_Int;
    case PGY_TK_LONG:   return ctx->secure_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->secure_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->secure_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->secure_slot_type_Bool;
    case PGY_TK_STRING: return ctx->secure_slot_type_String;
    default: {
        LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
        if (ctx->has_error || inner_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            inner_ty, LLVMInt1TypeInContext(ctx->context), ctx->type_i64
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    }
}

LLVMTypeRef
llvm_pinned_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->pinned_secure_slot_type_Int;
    case PGY_TK_LONG:   return ctx->pinned_secure_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->pinned_secure_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->pinned_secure_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->pinned_secure_slot_type_Bool;
    case PGY_TK_STRING: return ctx->pinned_secure_slot_type_String;
    default: {
        LLVMTypeRef slot_ty = llvm_secure_slot_struct_type(ctx, inner);
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        if (ctx->has_error || slot_ty == NULL || token_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            LLVMPointerType(slot_ty, 0),
            LLVMPointerType(token_ty, 0),
            ctx->type_i1,
            ctx->type_i1
        };
        return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
    }
    }
}

LLVMTypeRef
llvm_secure_token_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->secure_token_type_Int;
    case PGY_TK_LONG:   return ctx->secure_token_type_Long;
    case PGY_TK_FLOAT:  return ctx->secure_token_type_Float;
    case PGY_TK_DOUBLE: return ctx->secure_token_type_Double;
    case PGY_TK_BOOL:   return ctx->secure_token_type_Bool;
    case PGY_TK_STRING: return ctx->secure_token_type_String;
    default: {
        LLVMTypeRef fields[] = { ctx->type_i64, ctx->type_i1, ctx->type_i1 };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    }
}

/* Map the scalar suffix of a one-level nested array element name to its named
 * nested array struct. `scalar` is the innermost element classification (the
 * T in Array<Array<T>>). Returns NULL for unsupported inner scalars so the
 * caller fails closed instead of silently producing a wrong layout. */
static LLVMTypeRef
llvm_nested_array_type_for_scalar(LLVMGenCtx *ctx, PgyTypeKind scalar)
{
    switch (scalar) {
    case PGY_TK_INT:    return ctx->array_type_Array_Int;
    case PGY_TK_LONG:   return ctx->array_type_Array_Long;
    case PGY_TK_FLOAT:  return ctx->array_type_Array_Float;
    case PGY_TK_DOUBLE: return ctx->array_type_Array_Double;
    case PGY_TK_BOOL:   return ctx->array_type_Array_Bool;
    case PGY_TK_STRING: return ctx->array_type_Array_String;
    default:            return NULL;
    }
}

/* Recognize a one-level nested-array element name in either rendered form:
 *   "Array<Int>"  (angle, from llvm_constructed_arg_name_copy)
 *   "Array_Int"   (suffix, from a PgyArray_Array_Int struct name)
 * Returns the named PgyArray_Array_<T> struct, or NULL if `inner` is not a
 * one-level array of a supported scalar. */
LLVMTypeRef
llvm_nested_array_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    const char *scalar_name = NULL;
    char scalar_buf[64];

    if (ctx == NULL || inner == NULL)
        return NULL;
    if (strncmp(inner, "Array<", 6) == 0) {
        const char *open = inner + 6;
        const char *close = strrchr(open, '>');
        size_t len;
        if (close == NULL || close <= open)
            return NULL;
        len = (size_t)(close - open);
        if (len >= sizeof(scalar_buf))
            return NULL;
        memcpy(scalar_buf, open, len);
        scalar_buf[len] = '\0';
        scalar_name = scalar_buf;
    } else if (strncmp(inner, "Array_", 6) == 0) {
        scalar_name = inner + 6;
    } else {
        return NULL;
    }
    /* Only a single nesting level: the inner scalar must itself be a scalar,
     * not another array (depth >= 3 is rejected up front by semantics). */
    return llvm_nested_array_type_for_scalar(ctx,
        pgy_classify_type(scalar_name));
}

/* If `elem_type` is one of the six scalar array structs (the element type of a
 * one-level nested Array<Array<scalar>>), return the runtime/type suffix that
 * names an array-of-that-element ("Array_Int", ...). Otherwise NULL. Used to
 * route nested array construction/access through the ABI-safe raw export path
 * (the inner array struct is larger than two eightbytes, so it cannot be passed
 * to the typed runtime by value across the C/LLVM boundary). */
const char *
llvm_scalar_array_elem_suffix(LLVMGenCtx *ctx, LLVMTypeRef elem_type)
{
    if (ctx == NULL || elem_type == NULL)
        return NULL;
    if (elem_type == ctx->array_type_Int)    return "Array_Int";
    if (elem_type == ctx->array_type_Long)   return "Array_Long";
    if (elem_type == ctx->array_type_Float)  return "Array_Float";
    if (elem_type == ctx->array_type_Double) return "Array_Double";
    if (elem_type == ctx->array_type_Bool)   return "Array_Bool";
    if (elem_type == ctx->array_type_String) return "Array_String";
    return NULL;
}

/* If `type` is one of the six scalar array structs (PgyArray_Int, ...) return
 * the LLVM type of its scalar element (i32 for Int, i8ptr for String, ...).
 * Used to resolve the element type of a[i] when a[i] is itself the inner
 * Array<scalar> of a nested array (chained index a[i][j]). */
LLVMTypeRef
llvm_scalar_array_struct_element_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    if (ctx == NULL || type == NULL)
        return NULL;
    if (type == ctx->array_type_Int)    return ctx->type_i32;
    if (type == ctx->array_type_Long)   return ctx->type_i64;
    if (type == ctx->array_type_Float)  return ctx->type_f32;
    if (type == ctx->array_type_Double) return ctx->type_f64;
    if (type == ctx->array_type_Bool)   return ctx->type_i1;
    if (type == ctx->array_type_String) return ctx->type_i8ptr;
    return NULL;
}

LLVMTypeRef
llvm_array_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    LLVMTypeRef nested;

    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->array_type_Int;
    case PGY_TK_LONG:   return ctx->array_type_Long;
    case PGY_TK_FLOAT:  return ctx->array_type_Float;
    case PGY_TK_DOUBLE: return ctx->array_type_Double;
    case PGY_TK_BOOL:   return ctx->array_type_Bool;
    case PGY_TK_STRING: return ctx->array_type_String;
    default: break;
    }
    nested = llvm_nested_array_struct_type(ctx, inner);
    if (nested != NULL)
        return nested;
    {
        LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
        if (ctx->has_error || elem_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
    }
}

LLVMTypeRef
llvm_slice_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->slice_type_Int;
    case PGY_TK_LONG:   return ctx->slice_type_Long;
    case PGY_TK_FLOAT:  return ctx->slice_type_Float;
    case PGY_TK_DOUBLE: return ctx->slice_type_Double;
    case PGY_TK_BOOL:   return ctx->slice_type_Bool;
    case PGY_TK_STRING: return ctx->slice_type_String;
    default: {
        LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
        if (ctx->has_error || elem_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            LLVMPointerType(elem_ty, 0), ctx->type_i64
        };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    }
}

LLVMTypeRef
llvm_list_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    LLVMTypeRef elem_ty;

    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "List<T>: concrete element type metadata is required");
        return NULL;
    }
    elem_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || elem_ty == NULL)
        return NULL;
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
}

LLVMTypeRef
llvm_set_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Set<T>: concrete element type metadata is required");
        return NULL;
    }
    LLVMTypeRef fields[] = {
        ctx->type_i8ptr,
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
        ctx->type_i64,
        ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
}

LLVMTypeRef
llvm_queue_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    LLVMTypeRef elem_ty;

    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Queue<T>: concrete element type metadata is required");
        return NULL;
    }
    elem_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || elem_ty == NULL)
        return NULL;
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64,
        ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMTypeRef
llvm_hashmap_struct_type(LLVMGenCtx *ctx, const char *value)
{
    LLVMTypeRef value_ty;

    if (value == NULL || value[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "HashMap<K, V>: concrete value type metadata is required");
        return NULL;
    }
    value_ty = pergyra_type_to_llvm(ctx, value);
    if (ctx->has_error || value_ty == NULL)
        return NULL;
    LLVMTypeRef fields[] = {
        LLVMPointerType(ctx->type_i8ptr, 0),
        LLVMPointerType(value_ty, 0),
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
        ctx->type_i64,
        ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMValueRef
llvm_sizeof_type_i64(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    LLVMValueRef zero_ptr = LLVMConstNull(LLVMPointerType(type, 0));
    LLVMValueRef one_idx = LLVMConstInt(ctx->type_i32, 1, 0);
    LLVMValueRef gep = LLVMConstGEP2(type, zero_ptr, &one_idx, 1);
    return LLVMConstPtrToInt(gep, ctx->type_i64);
}

#endif
