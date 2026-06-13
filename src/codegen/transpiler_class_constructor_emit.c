#include "transpiler_domain_constructor_emit.h"

#include <stdlib.h>

#include "parser/ast_api.h"
#include "transpiler_constructor_channel_guard.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_constructor_internal.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"

char *
transpiler_emit_class_constructor_with_type(ASTNode *call,
                                            ASTNode *class_decl,
                                            const char *ctor_type,
                                            TranspilerCtx *ctx)
{
    size_t argc;
    const char *decl_name;
    TranspilerHostedFieldView field_view;
    size_t field_count;
    CodeBuf *fields;
    char *result;

    if (call == NULL || class_decl == NULL || ctor_type == NULL)
        return NULL;
    {
        const char *channel_field =
            transpiler_constructor_find_channel_field(ctx, class_decl);
        if (channel_field != NULL) {
            transpiler_constructor_reject_channel_field(ctx, channel_field);
            return NULL;
        }
    }

    argc = ast_call_arg_count(call);
    fields = codebuf_create();
    decl_name = transpiler_decl_name_local(class_decl);
    field_view = transpiler_hosted_class_field_view_from_decl(
        ctx, decl_name, class_decl);
    field_count = field_view.count;
    if (transpiler_hosted_field_view_missing_mir_metadata(&field_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing class-field declaration metadata for constructor '%s'",
            decl_name != NULL ? decl_name : "(anonymous-class)");
        codebuf_destroy(fields);
        return NULL;
    }

    bool named = ast_call_has_named_arguments(call);
    size_t emitted = 0;
    for (size_t i = 0; i < field_count; i++) {
        ASTNode *field_type =
            transpiler_hosted_field_view_type(&field_view, i);
        const char *field_name =
            transpiler_hosted_field_view_name(&field_view, i);
        ASTNode *arg_node = named
            ? ast_call_find_named_argument(call, field_name)
            : (i < argc ? ast_call_argument(call, i) : NULL);
        char *arg;
        if (arg_node == NULL)
            continue;
        arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field_type, field_name, arg_node);
        if (arg == NULL) {
            codebuf_destroy(fields);
            return NULL;
        }
        if (emitted > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
        emitted++;
    }

    if (fields->len > 0)
        result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
    else
        result = strdup_fmt("(%s){0}", ctor_type);
    codebuf_destroy(fields);

    /* Route a class with destructure slot fields through its claim helper so the
     * built object's slots are live before any method writes to them. */
    if (result != NULL && ast_class_field_destructure_count(class_decl) > 0) {
        char *claimed = strdup_fmt("%s__pgy_field_slot_init(%s)",
                                   ctor_type, result);
        free(result);
        result = claimed;
    }
    return result;
}
