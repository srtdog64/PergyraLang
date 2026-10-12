/* =================================================================
 * One-level nested array operations - extern wrappers for LLVM linker.
 *
 * Array<Array<T>> for scalar T is monomorphized here by instantiating the
 * generic PGY_DEFINE_ARRAY_EXPORTS macro with the inner array struct type as
 * the element CType. This MUST be included AFTER
 * pgy_runtime_lib_array_map_exports.h so the inner PgyArray_<T> typedefs and
 * the PGY_DEFINE_ARRAY_EXPORTS / PGY_ARRAY_EXPORT_COPY_VALUE_* machinery are
 * already in scope.
 *
 * Ordinary push stores/transfers the descriptor unchanged. Explicit Clone
 * and SliceCopy instead clone each inner backing through its concrete owner.
 * The temporary descriptor evaluates the source once without mutating it.
 * ================================================================= */

#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Int(value)    pgy_array_clone_Int(&(PgyArray_Int[]){(value)}[0])
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Long(value)   pgy_array_clone_Long(&(PgyArray_Long[]){(value)}[0])
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Float(value)  pgy_array_clone_Float(&(PgyArray_Float[]){(value)}[0])
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Double(value) pgy_array_clone_Double(&(PgyArray_Double[]){(value)}[0])
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Bool(value)   pgy_array_clone_Bool(&(PgyArray_Bool[]){(value)}[0])
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_String(value) pgy_array_clone_String(&(PgyArray_String[]){(value)}[0])

PGY_DEFINE_ARRAY_EXPORTS(Array_Int,    PgyArray_Int)
PGY_DEFINE_ARRAY_EXPORTS(Array_Long,   PgyArray_Long)
PGY_DEFINE_ARRAY_EXPORTS(Array_Float,  PgyArray_Float)
PGY_DEFINE_ARRAY_EXPORTS(Array_Double, PgyArray_Double)
PGY_DEFINE_ARRAY_EXPORTS(Array_Bool,   PgyArray_Bool)
PGY_DEFINE_ARRAY_EXPORTS(Array_String, PgyArray_String)

/* Explicit pointer-out transport avoids guessing the platform's aggregate
 * return ABI. The original concrete clone remains the copy policy owner. */
#define PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(Leaf) \
void pgy_array_clone_Array_##Leaf##_into(PgyArray_Array_##Leaf *source, PgyArray_Array_##Leaf *out) \
{ \
    if (source == NULL || out == NULL || source == out) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "nested array clone into requires distinct nonnull source and output"); \
    PgyArray_Array_##Leaf copied = pgy_array_clone_Array_##Leaf(source); \
    *out = copied; \
}

PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(Int)
PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(Long)
PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(Float)
PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(Double)
PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(Bool)
PGY_DEFINE_NESTED_ARRAY_CLONE_INTO(String)
#undef PGY_DEFINE_NESTED_ARRAY_CLONE_INTO
