#ifndef PGY_RUNTIME_ARTIFACT_TRANSACTION_INLINE_H
#define PGY_RUNTIME_ARTIFACT_TRANSACTION_INLINE_H

/* Generated-C facade.  The normal runtime owns path resolution, capability
 * checks, and platform setup; pgy_runtime_io_qubit_inline.h includes the one
 * canonical transaction core.  Keeping this facade as a named include lets
 * the self-host emitter select the capability without cloning that setup. */
#include "pgy_runtime.h"

#endif /* PGY_RUNTIME_ARTIFACT_TRANSACTION_INLINE_H */
