/*
 * Copyright (c) 2025 Pergyra Language Project
 * Type system implementation
 */

#ifndef PERGYRA_TYPE_SYSTEM_H
#define PERGYRA_TYPE_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "../parser/ast.h"

/* Type kinds */
typedef enum
{
    TYPE_KIND_PRIMITIVE,    /* Int, Float, Bool, etc. */
    TYPE_KIND_GENERIC,      /* T, U, etc. */
    TYPE_KIND_CONSTRUCTED,  /* Array<T>, Option<T>, etc. */
    TYPE_KIND_FUNCTION,     /* (T) -> U */
    TYPE_KIND_TUPLE,        /* (T, U, V) */
    TYPE_KIND_SLOT,         /* Slot<T>, SecureSlot<T> */
    TYPE_KIND_CLASS,        /* User-defined classes */
    TYPE_KIND_ENUM,         /* User-defined enums (integer-backed) */
    TYPE_KIND_TRAIT,        /* Interfaces/Traits */
    TYPE_KIND_ALIAS         /* Type aliases */
} TypeKind;

/* Inferred function/resource effects.
 * "local" is the zero-effect default: a function with no non-local flags
 * is treated as local-only today. */
typedef enum
{
    EFFECT_NONE             = 0,
    EFFECT_SECURE           = 1u << 0,
    EFFECT_REMOTE           = 1u << 1,
    EFFECT_NONDETERMINISTIC = 1u << 2,
    EFFECT_COLLAPSE         = 1u << 3
} EffectMask;

typedef enum
{
    SLOT_ACCESS_OWNED,
    SLOT_ACCESS_READ_VIEW,
    SLOT_ACCESS_WRITE_VIEW,
    SLOT_ACCESS_MOVE_TOKEN
} SlotAccessMode;

/* Type structure */
typedef struct Type Type;
struct Type
{
    TypeKind kind;
    char* name;
    
    union
    {
        /* Primitive type info */
        struct
        {
            size_t size;
            bool is_signed;
        } primitive;
        
        /* Generic type */
        struct
        {
            char* param_name;
            Type** constraints;    /* Trait bounds */
            size_t constraint_count;
        } generic;
        
        /* Constructed type (Array<T>, etc.) */
        struct
        {
            Type* constructor;     /* Array, Option, etc. */
            Type** args;          /* Type arguments */
            size_t arg_count;
        } constructed;
        
        /* Function type */
        struct
        {
            Type** param_types;
            size_t param_count;
            Type* return_type;
            uint32_t effect_mask;
        } function;
        
        /* Slot type */
        struct
        {
            Type* inner_type;
            bool is_secure;
            int security_level;
            SlotAccessMode access_mode;
        } slot;
    } data;
};

/* Type environment (for type checking) */
typedef struct TypeEnv TypeEnv;
struct TypeEnv
{
    TypeEnv* parent;    /* Parent scope */
    
    /* Variable types */
    struct
    {
        char* name;
        Type* type;
    }* variables;
    size_t var_count;
    
    /* Type definitions */
    struct
    {
        char* name;
        Type* type;
    }* types;
    size_t type_count;
    
    /* Generic parameters in scope */
    GenericParams* generic_params;
};

/* Type operations */
Type* type_create_primitive(const char* name, size_t size, bool is_signed);
Type* type_create_generic(const char* param_name);
Type* type_create_constructed(Type* constructor, Type** args, size_t arg_count);
Type* type_create_function(Type** params, size_t param_count, Type* return_type);
Type* type_create_slot(Type* inner_type, bool is_secure);
Type* type_create_slot_access(Type* inner_type, bool is_secure, SlotAccessMode access_mode);
Type* type_create_read_view(Type* inner_type);
Type* type_create_write_view(Type* inner_type);
uint32_t type_function_effects(const Type* type);
bool type_effect_mask_has(uint32_t mask, uint32_t effect);

/* Type checking */
bool type_equals(const Type* a, const Type* b);
bool type_is_assignable(const Type* from, const Type* to);
bool type_satisfies_constraint(const Type* type, const Type* constraint);

/* Type inference */
Type* type_infer_expression(const ASTNode* expr, TypeEnv* env);
bool type_unify(Type* a, Type* b, TypeEnv* env);

/* Type environment operations */
TypeEnv* type_env_create(TypeEnv* parent);
void type_env_destroy(TypeEnv* env);
void type_env_add_variable(TypeEnv* env, const char* name, Type* type);
Type* type_env_lookup_variable(TypeEnv* env, const char* name);
void type_env_add_type(TypeEnv* env, const char* name, Type* type);
Type* type_env_lookup_type(TypeEnv* env, const char* name);

/* Type instantiation (for generics) */
Type* type_instantiate(Type* generic_type, Type** type_args, size_t arg_count);

/* Built-in types */
extern Type* TYPE_INT;
extern Type* TYPE_LONG;
extern Type* TYPE_FLOAT;
extern Type* TYPE_DOUBLE;
extern Type* TYPE_BOOL;
extern Type* TYPE_STRING;
extern Type* TYPE_QUBIT;
extern Type* TYPE_VOID;
extern Type* TYPE_UNKNOWN;
extern Type* TYPE_ARRAY;
extern Type* TYPE_SLICE;
extern Type* TYPE_BOX;
extern Type* TYPE_RC;
extern Type* TYPE_WEAK;
extern Type* TYPE_CHANNEL;
extern Type* TYPE_FUTURE;
extern Type* TYPE_REMOTE_FUTURE;
extern Type* TYPE_DEVICE_SLOT;
extern Type* TYPE_ALLOCATOR;

void type_system_init(void);
void type_system_cleanup(void);

#endif /* PERGYRA_TYPE_SYSTEM_H */
