#include "pgy_runtime_linkage.h"
/*
 * Copyright (c) 2026 Pergyra Language Project
 *
 * pgy_runtime_io_result_inline.h -- TryReadFile / TryWriteFile runtime ABI.
 *
 * ReadFile and WriteFile drop the typed failure that pgy_try_read_file_result
 * and pgy_try_write_file_result return. These two entries keep it: each
 * returns true on success and otherwise reports the builtin IoError variant
 * ordinal (pgy_runtime_io_error.def) through out_error. Compilers build the
 * Result<String, IoError> / Result<Bool, IoError> value at the call site, so
 * the ABI carries no struct across the C/LLVM boundary. The LLVM runtime
 * library defines the same two entries in pgy_runtime_lib_io_string_exports.h.
 */
#ifndef PGY_RUNTIME_IO_RESULT_INLINE_H
#define PGY_RUNTIME_IO_RESULT_INLINE_H

#include <stdbool.h>
#include <stdint.h>

#include "pgy_runtime_io_status.h"
#include "pgy_runtime_panic_contract.h"

PGY_RT_DECL bool
pgy_try_read_file_export(const char *path, char **out_text,
                         int32_t *out_error)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyRuntimeIoStringResult result = pgy_try_read_file_result(path);
    int32_t ordinal;

    if (result.tag == PGY_RUNTIME_IO_RESULT_OK) {
        *out_text = result.ok;
        *out_error = 0;
        return true;
    }
    ordinal = pgy_runtime_io_error_ordinal(result.err.status);
    if (ordinal < 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "read-file status has no IoError variant");
    *out_text = NULL;
    *out_error = ordinal;
    return false;
}
#else
;
#endif

PGY_RT_DECL bool
pgy_try_write_file_export(const char *path, const char *data,
                          int32_t *out_error)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyRuntimeIoVoidResult result = pgy_try_write_file_result(path, data);
    int32_t ordinal;

    if (result.tag == PGY_RUNTIME_IO_RESULT_OK) {
        *out_error = 0;
        return true;
    }
    ordinal = pgy_runtime_io_error_ordinal(result.err.status);
    if (ordinal < 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "write-file status has no IoError variant");
    *out_error = ordinal;
    return false;
}
#else
;
#endif

#endif /* PGY_RUNTIME_IO_RESULT_INLINE_H */
