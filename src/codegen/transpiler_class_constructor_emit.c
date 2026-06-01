#include "transpiler_domain_constructor_emit.h"

#include <stdlib.h>

#include "../common/string_compat.h"
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
            return pergyra_strdup("0");
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
        return pergyra_strdup("0");
    }

    for (size_t i = 0; i < argc && i < field_count; i++) {
        ASTNode *field_type =
            transpiler_hosted_field_view_type(&field_view, i);
        const char *field_name =
            transpiler_hosted_field_view_name(&field_view, i);
        char *arg = transpiler_emit_ctor_arg_with_expected_type(ctx,
            field_type, field_name, ast_call_argument(call, i));
        if (i > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg != NULL ? arg : "0");
        free(arg);
    }

    if (fields->len > 0)
        result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
    else
        result = strdup_fmt("(%s){0}", ctor_type);
    codebuf_destroy(fields);
    return result;
}
