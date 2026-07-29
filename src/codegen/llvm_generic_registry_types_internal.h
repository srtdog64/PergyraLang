#ifndef PGY_LLVM_GENERIC_REGISTRY_TYPES_INTERNAL_H
#define PGY_LLVM_GENERIC_REGISTRY_TYPES_INTERNAL_H

/* Generic template entry (for lazy monomorphization) */
typedef struct
{
    const char       *name;
    ASTNode          *ast;
    const MIRRoutine *routine;
} LLVMGenericTemplate;

/* Monomorphized instance tracking */
typedef struct
{
    char *name;   /* heap-allocated, freed in ctx_destroy */
} LLVMMonoInstance;

/* Type substitution entry (T -> concrete LLVM type). */
typedef struct
{
    const char  *param_name;  /* "T" */
    LLVMTypeRef  llvm_type;   /* i32 */
    const char  *type_name;   /* "Int" */
} LLVMTypeSubst;

#endif /* PGY_LLVM_GENERIC_REGISTRY_TYPES_INTERNAL_H */
