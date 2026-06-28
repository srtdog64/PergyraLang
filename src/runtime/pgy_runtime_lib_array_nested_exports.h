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
 * Memory model: Pergyra never frees. Storing an inner PgyArray_<T> element by
 * value (shallow struct copy, inner `data` pointer shared) is correct and
 * intended - the copy macro below is the identity, NOT a deep copy.
 * ================================================================= */

#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Int(value)    (value)
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Long(value)   (value)
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Float(value)  (value)
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Double(value) (value)
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_Bool(value)   (value)
#define PGY_ARRAY_EXPORT_COPY_VALUE_Array_String(value) (value)

PGY_DEFINE_ARRAY_EXPORTS(Array_Int,    PgyArray_Int)
PGY_DEFINE_ARRAY_EXPORTS(Array_Long,   PgyArray_Long)
PGY_DEFINE_ARRAY_EXPORTS(Array_Float,  PgyArray_Float)
PGY_DEFINE_ARRAY_EXPORTS(Array_Double, PgyArray_Double)
PGY_DEFINE_ARRAY_EXPORTS(Array_Bool,   PgyArray_Bool)
PGY_DEFINE_ARRAY_EXPORTS(Array_String, PgyArray_String)
