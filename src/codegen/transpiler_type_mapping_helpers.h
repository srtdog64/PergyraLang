/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

#ifndef PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H
#define PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H

#include "transpiler.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_type_mapping.h"

static void ensure_result_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                            const char *ok_type,
                                            const char *err_type);

#include "transpiler_type_result_mapping_helpers.h"
#include "transpiler_type_render_helpers.h"

#endif /* PERGYRA_TRANSPILER_TYPE_MAPPING_HELPERS_H */
