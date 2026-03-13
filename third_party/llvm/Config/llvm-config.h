/*
 * Minimal stub for LLVM C API headers.
 * Only defines macros needed by llvm-c/Visibility.h.
 */

#ifndef LLVM_CONFIG_H
#define LLVM_CONFIG_H

/* We link against the shared LLVM-C library */
#define LLVM_ENABLE_LLVM_C_EXPORT_ANNOTATIONS 1

/* Not building static LLVM */
/* #undef LLVM_BUILD_STATIC */

/* Not exporting — we are consuming */
/* #undef LLVM_EXPORTS */

#endif /* LLVM_CONFIG_H */
