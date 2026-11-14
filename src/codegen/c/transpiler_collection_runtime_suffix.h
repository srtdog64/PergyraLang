#ifndef PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H
#define PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H

#include <stdbool.h>
#include <stddef.h>

bool collection_runtime_suffix_copy(const char *inner_type,
                                    char *out,
                                    size_t out_size);

#endif /* PGY_TRANSPILER_COLLECTION_RUNTIME_SUFFIX_H */
