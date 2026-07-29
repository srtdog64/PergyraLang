#include "transpiler_mir_func_param_bindings.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_signature.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

bool
transpiler_register_mir_func_param_bindings(
    TranspilerCtx *ctx,
    const MIRRoutine *mir_routine,
    const char *function_name,
    bool is_method,
    bool mir_active)
{
    size_t func_param_count =
        transpiler_mir_routine_param_count(mir_routine);

    for (size_t i = 0; i < func_param_count; i++) {
        FuncParam *p = transpiler_mir_routine_param(mir_routine, i);
        bool pass_indirect =
            transpiler_mir_routine_param_passes_indirect(mir_routine, i);
        const char *type_name =
            transpiler_mir_routine_param_type_name(mir_routine, i);
        char *owned_type_name = NULL;
        if (p == NULL || p->name == NULL)
            continue;
        if (is_method && strcmp(p->name, "self") == 0 && p->type == NULL)
            continue;
        if (!mir_active && type_name == NULL && p->type != NULL) {
            owned_type_name = render_type_name_in_ctx(ctx, p->type);
            type_name = owned_type_name;
        }
        if (p->type == NULL
            && strcmp(p->name, "self") == 0
            && mir_routine != NULL
            && transpiler_mir_routine_owner_name(mir_routine) != NULL) {
            free(owned_type_name);
            owned_type_name = pergyra_strdup(
                transpiler_mir_routine_owner_name(mir_routine));
            type_name = owned_type_name;
        }
        if (type_name == NULL)
            continue;
        {
            bool boundary_slot =
                transpiler_mir_routine_param_is_boundary_resource(
                    mir_routine, i);
            register_typed_var(ctx, p->name, type_name);
            if (strcmp(p->name, "self") != 0
                && (is_pointer_self_host_type_name(ctx, type_name)
                    || pass_indirect)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_indirect_ref = true;
            }
            if (transpiler_mir_routine_param_is_slot_family(
                    mir_routine, i)) {
                char inner_buf[128];
                bool secure_slot = transpiler_mir_routine_param_is_secure_slot(
                    mir_routine, i);
                if (!slot_inner_type_name_copy(type_name, inner_buf,
                        sizeof(inner_buf))) {
                    transpiler_set_backend_error_with_hints(
                        ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "cannot register slot parameter metadata for MIR-emitted function '%s' parameter '%s'",
                        function_name != NULL ? function_name : "<function>",
                        p->name);
                    free(owned_type_name);
                    return false;
                }
                register_slot_var(ctx, p->name, inner_buf, secure_slot,
                    boundary_slot);
            } else if (transpiler_mir_routine_param_is_device_slot(
                           mir_routine, i)
                       && boundary_slot) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_indirect_ref = true;
            }
        }
        free(owned_type_name);
    }
    return true;
}
