#ifndef PGY_TRANSPILER_LET_BOX_EMIT_H
#define PGY_TRANSPILER_LET_BOX_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

typedef struct TranspilerBoxArrayLetCtor {
    char *c_type;
    char *surface_type;
    char *rhs;
} TranspilerBoxArrayLetCtor;

bool transpiler_try_render_box_array_let_ctor(
    TranspilerCtx *ctx,
    const char *binding_name,
    ASTNode *init,
    ASTNode *ann,
    const char *ann_type_name,
    TranspilerBoxArrayLetCtor *out);

void transpiler_box_array_let_ctor_destroy(TranspilerBoxArrayLetCtor *ctor);

bool transpiler_try_emit_box_family_let(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *ann,
                                        char **ann_type_name_ptr);

#endif /* PGY_TRANSPILER_LET_BOX_EMIT_H */
