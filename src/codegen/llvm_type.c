/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend type classification helpers split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool
llvm_nominal_uses_immutable_projection_storage(NominalDeclKind kind)
{
    return kind == NOMINAL_DECL_OBJECT || kind == NOMINAL_DECL_DTO;
}

bool
llvm_nominal_is_boundary_transfer_contract(NominalDeclKind kind)
{
    return kind == NOMINAL_DECL_DTO;
}

PgyTypeKind
pgy_classify_type(const char *type_name)
{
    if (type_name == NULL)
        return PGY_TK_VOID;

    switch (type_name[0]) {
    case 'I': if (strcmp(type_name, "Int") == 0)        return PGY_TK_INT;        break;
    case 'L': if (strcmp(type_name, "Long") == 0)       return PGY_TK_LONG;       break;
    case 'F':
        if (strcmp(type_name, "Float") == 0)            return PGY_TK_FLOAT;
        if (strncmp(type_name, "Future<", 7) == 0)      return PGY_TK_FUTURE;
        break;
    case 'D':
        if (strcmp(type_name, "Double") == 0)           return PGY_TK_DOUBLE;
        if (strncmp(type_name, "DeviceSlot<", 11) == 0) return PGY_TK_DEVICE_SLOT;
        break;
    case 'B':
        if (strcmp(type_name, "Bool") == 0)             return PGY_TK_BOOL;
        if (strncmp(type_name, "Box<", 4) == 0)         return PGY_TK_BOX;
        break;
    case 'S':
        if (strcmp(type_name, "String") == 0)           return PGY_TK_STRING;
        if (strncmp(type_name, "Slot<", 5) == 0)        return PGY_TK_SLOT;
        if (strcmp(type_name, "Slot") == 0)             return PGY_TK_SLOT;
        if (strncmp(type_name, "SecureSlot<", 11) == 0) return PGY_TK_SECURE_SLOT;
        if (strncmp(type_name, "Slice<", 6) == 0)       return PGY_TK_SLICE;
        break;
    case 'V': if (strcmp(type_name, "Void") == 0)       return PGY_TK_VOID;       break;
    case 'Q': if (strcmp(type_name, "QubitSlot") == 0)  return PGY_TK_QUBIT_SLOT; break;
    case 'R':
        if (strncmp(type_name, "ReadView<", 9) == 0)    return PGY_TK_SLOT;
        if (strncmp(type_name, "RemoteFuture<", 13) == 0) return PGY_TK_REMOTE_FUTURE;
        if (strncmp(type_name, "Result<", 7) == 0)      return PGY_TK_RESULT;
        if (strncmp(type_name, "Rc<", 3) == 0)          return PGY_TK_RC;
        break;
    case 'O':
        if (strncmp(type_name, "Option<", 7) == 0)      return PGY_TK_OPTION;
        break;
    case 'C':
        if (strncmp(type_name, "Channel<", 8) == 0)     return PGY_TK_CHANNEL;
        break;
    case 'W':
        if (strncmp(type_name, "WriteView<", 10) == 0)  return PGY_TK_SLOT;
        if (strncmp(type_name, "Weak<", 5) == 0)        return PGY_TK_WEAK;
        break;
    case 'A':
        if (strncmp(type_name, "Array<", 6) == 0)       return PGY_TK_ARRAY;
        break;
    default:
        break;
    }
    return PGY_TK_UNKNOWN;
}

LLVMTypeRef
pgy_kind_to_llvm(LLVMGenCtx *ctx, PgyTypeKind kind)
{
    switch (kind) {
    case PGY_TK_INT:           return ctx->type_i32;
    case PGY_TK_LONG:          return ctx->type_i64;
    case PGY_TK_FLOAT:         return ctx->type_f32;
    case PGY_TK_DOUBLE:        return ctx->type_f64;
    case PGY_TK_BOOL:          return ctx->type_i1;
    case PGY_TK_STRING:        return ctx->type_i8ptr;
    case PGY_TK_QUBIT_SLOT:    return ctx->type_i32;
    case PGY_TK_REMOTE_FUTURE: return ctx->type_task_handle;
    case PGY_TK_VOID:          return ctx->type_void;
    default:                   return NULL;
    }
}

const char *
pgy_kind_to_suffix(PgyTypeKind kind)
{
    switch (kind) {
    case PGY_TK_INT:        return "Int";
    case PGY_TK_LONG:       return "Long";
    case PGY_TK_FLOAT:      return "Float";
    case PGY_TK_DOUBLE:     return "Double";
    case PGY_TK_BOOL:       return "Bool";
    case PGY_TK_STRING:     return "String";
    case PGY_TK_QUBIT_SLOT: return "QubitSlot";
    default:                return NULL;
    }
}

#endif
