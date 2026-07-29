#ifndef PGY_LLVM_REGISTRATION_NAMES_H
#define PGY_LLVM_REGISTRATION_NAMES_H

#include <stdbool.h>
#include <stddef.h>

typedef struct LLVMGenCtx LLVMGenCtx;

bool llvm_register_join_name(LLVMGenCtx *ctx,
                             char *out,
                             size_t out_size,
                             const char *left,
                             const char *sep,
                             const char *right,
                             const char *surface);
bool llvm_register_payload_field_name(LLVMGenCtx *ctx,
                                      char *out,
                                      size_t out_size,
                                      size_t index);

#endif /* PGY_LLVM_REGISTRATION_NAMES_H */
