#include "transpiler_expr_call_member_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_generic_method_specialization.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_call_subject_arg_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_receiver_query.h"
#include "transpiler_expr_call_user_emit.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_generic_class_specialization.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_nominal.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_overlay_projection.h"
#include "transpiler_projection.h"
#include "transpiler_projection_field_path.h"
#include "transpiler_projection_method_invalidation.h"
#include "transpiler_projection_sync.h"
#include "transpiler_specialization_registry.h"
#include "codegen_type_mapping.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_slot_runtime_row.h"
#include "transpiler_symbols.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"
#include "transpiler_mangled_name.h"

static const char *
member_slot_runtime_fn(TranspilerCtx *ctx,
                       bool secure,
                       const char *inner_type,
                       const char *operation)
{
    return transpiler_slot_runtime_fn(ctx, secure, inner_type, operation);
}

char *
transpiler_emit_member_slot(ASTNode *call, ASTNode *obj,
                            const char *method, TranspilerCtx *ctx,
                            bool *handled)
{
    *handled = true;
    char inner_buf[128];
    const char *inner = NULL;
    const char *obj_name = ast_identifier_name(obj);
    bool is_secure = lookup_slot_is_secure(ctx, obj_name);
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *obj_expr = transpiler_member_call_emit_part(ctx,
        obj, method, "slot receiver");
    ctx->suppress_slot_auto_read = saved_suppress;
    if (obj_expr == NULL)
        return NULL;
    if (lookup_slot_type_copy(ctx, obj_name,
            inner_buf, sizeof(inner_buf))) {
        inner = inner_buf;
    }
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for method call on '%s'",
            obj_name != NULL
                ? obj_name
                : "<slot>");
        free(obj_expr);
        return NULL;
    }
    char *slot_ref = slot_ref_expr(ctx, obj_name, obj_expr);
    if (slot_ref == NULL) {
        free(obj_expr);
        return NULL;
    }

    if (pgy_codegen_call_name_is_write(method)
        && ast_call_arg_count(call) >= 1) {
        char *val_expr = transpiler_member_call_emit_part(ctx,
            ast_call_argument(call, 0), method, "write value");
        char *result;
        if (val_expr == NULL) {
            free(slot_ref);
            free(obj_expr);
            return NULL;
        }
        if (is_secure && ast_call_arg_count(call) >= 2) {
            char *tok = transpiler_member_call_emit_part(ctx,
                ast_call_argument(call, 1), method, "write token");
            const char *write_fn;
            if (tok == NULL) {
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            write_fn = member_slot_runtime_fn(ctx, true, inner, "Write");
            if (write_fn == NULL) {
                free(tok);
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s, %s, &%s)",
                                write_fn, slot_ref, val_expr, tok);
            free(tok);
        } else if (is_secure) {
            const char *write_fn;
            const char *token_name = require_slot_token_name(
                ctx, obj_name, "SecureSlot method Write");
            if (token_name == NULL) {
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            write_fn = member_slot_runtime_fn(ctx, true, inner, "Write");
            if (write_fn == NULL) {
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s, %s, &%s)",
                                write_fn, slot_ref, val_expr, token_name);
        } else {
            const char *write_fn =
                member_slot_runtime_fn(ctx, false, inner, "Write");
            if (write_fn == NULL) {
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s, %s)",
                                write_fn, slot_ref, val_expr);
        }
        free(val_expr);
        free(slot_ref);
        free(obj_expr);
        return result;
    } else if (pgy_codegen_call_name_is_read(method)) {
        char *result;
        if (is_secure) {
            const char *read_fn;
            const char *token_name = require_slot_token_name(
                ctx, obj_name, "SecureSlot method Read");
            if (token_name == NULL) {
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            read_fn = member_slot_runtime_fn(ctx, true, inner, "Read");
            if (read_fn == NULL) {
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s, &%s)",
                                read_fn, slot_ref, token_name);
        } else {
            const char *read_fn =
                member_slot_runtime_fn(ctx, false, inner, "Read");
            if (read_fn == NULL) {
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s)", read_fn, slot_ref);
        }
        free(slot_ref);
        free(obj_expr);
        return result;
    } else if (pgy_codegen_call_name_is_release(method)) {
        char *result;
        if (is_secure) {
            const char *release_fn;
            const char *token_name = require_slot_token_name(
                ctx, obj_name, "SecureSlot method Release");
            if (token_name == NULL) {
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            release_fn = member_slot_runtime_fn(
                ctx, true, inner, "Release");
            if (release_fn == NULL) {
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s, &%s)",
                                release_fn, slot_ref, token_name);
        } else {
            const char *release_fn =
                member_slot_runtime_fn(ctx, false, inner, "Release");
            if (release_fn == NULL) {
                free(slot_ref);
                free(obj_expr);
                return NULL;
            }
            result = strdup_fmt("%s(%s)", release_fn, slot_ref);
        }
        for (int ri = 0; ri < ctx->slot_var_count; ri++) {
            if (strcmp(ctx->slot_vars[ri].name,
                      obj_name) == 0) {
                ctx->slot_vars[ri].released = true;
                break;
            }
        }
        free(slot_ref);
        free(obj_expr);
        return result;
    }
    free(slot_ref);
    free(obj_expr);
    *handled = false;
    return NULL;
}
