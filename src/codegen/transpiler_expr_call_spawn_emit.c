#include "transpiler_expr_call_spawn_emit.h"

#include "../parser/ast_api.h"
#include "../semantic/builtin_kind.h"

#include "transpiler_call_constructor_result_emit.h"
#include "transpiler_call_result_option_builtin_emit.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_event_builtin_emit.h"
#include "transpiler_expr_builtin_dispatch.h"
#include "transpiler_expr_call_member_emit.h"
#include "transpiler_expr_call_user_emit.h"
#include "transpiler_expr_stdlib_builtin.h"

char *
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = ast_call_callee(call);
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        const char *callee_name = ast_identifier_name(callee);
        bk = builtin_resolve(callee_name);
        if ((bk == BUILTIN_BOX || bk == BUILTIN_RC_NEW)
            && find_class_decl(ctx, callee_name) != NULL) {
            bk = BUILTIN_NOT_BUILTIN;
        }
    }

    bool handled = false;
    char *result = emit_call_builtin_dispatch(call, bk, ctx, &handled);
    if (handled)
        return result;

    result = emit_call_domain_constructor(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_result_option_builtin(call, callee, ctx, &handled);
    if (handled)
        return result;
    result = emit_call_stdlib_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_event_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_member_style(call, callee, ctx);
    if (result != NULL)
        return result;
    return emit_call_user_function(call, callee, ctx);
}
