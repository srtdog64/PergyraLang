#include "transpiler_domain_constructor_emit.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
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
    char *result = NULL;

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
    char **named_values = named && argc > 0
        ? calloc(argc, sizeof(*named_values)) : NULL;
    if (named && argc > 0 && named_values == NULL) {
        transpiler_set_backend_error(ctx,
            "C named constructor operand allocation failed");
        codebuf_destroy(fields);
        return NULL;
    }
    size_t emitted = 0;
    for (size_t source = 0; source < (named ? argc : field_count); source++) {
        size_t i = source;
        if (named) {
            const char *name = ast_call_argument_name(call, source);
            for (i = 0; i < field_count; i++) {
                const char *candidate =
                    transpiler_hosted_field_view_name(&field_view, i);
                if (name != NULL && candidate != NULL &&
                    strcmp(name, candidate) == 0)
                    break;
            }
            if (i == field_count) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing named constructor field identity for '%s'",
                    name != NULL ? name : "(unnamed)");
                goto cleanup;
            }
        }
        const char *field_name =
            transpiler_hosted_field_view_name(&field_view, i);
        ASTNode *arg_node = named
            ? ast_call_argument(call, source)
            : (i < argc ? ast_call_argument(call, i) : NULL);
        const char *field_type_name =
            transpiler_hosted_field_view_type_name(&field_view, i);
        char *arg;
        if (arg_node == NULL && named) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing named constructor operand at index %zu",
                source);
            goto cleanup;
        }
        if (arg_node == NULL)
            continue;
        if (field_type_name == NULL || field_type_name[0] == '\0') {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing class constructor field type-name metadata for '%s' index %zu",
                decl_name != NULL ? decl_name : "(anonymous-class)", i);
            goto cleanup;
        }
        arg = transpiler_emit_ctor_arg_with_expected_type_name(
            ctx, field_type_name, field_name, arg_node);
        if (arg == NULL) {
            goto cleanup;
        }
        if (named) {
            named_values[source] = arg;
            continue;
        }
        if (emitted > 0)
            codebuf_write(fields, ", ");
        codebuf_write(fields, ".%s = %s",
            field_name != NULL ? field_name : "field",
            arg);
        free(arg);
        emitted++;
    }

    if (named) {
        char temporary[64];
        bool collision;
        /* Gensym against rendered operands too: source identifiers can use
         * compiler-looking prefixes. This is C name hygiene, not a fact read. */
        do {
            snprintf(temporary, sizeof(temporary), "_pgy_record_value_%d",
                     ++ctx->tmp_counter);
            collision = false;
            for (size_t i = 0; i < argc; i++)
                if (named_values[i] != NULL &&
                    strstr(named_values[i], temporary) != NULL)
                    collision = true;
        } while (collision);
        codebuf_write(fields, "({ %s %s = {0}; ", ctor_type, temporary);
        for (size_t i = 0; i < argc; i++)
            codebuf_write(fields, "%s.%s = (%s); ", temporary,
                ast_call_argument_name(call, i), named_values[i]);
        codebuf_write(fields, "%s; })", temporary);
        /* Keep fresh identity construction addressable in the caller's block. */
        result = strdup_fmt("((%s[]){ %s })[0]", ctor_type, fields->data);
    } else if (fields->len > 0)
        result = strdup_fmt("(%s){ %s }", ctor_type, fields->data);
    else
        result = strdup_fmt("(%s){0}", ctor_type);
cleanup:
    for (size_t i = 0; named_values != NULL && i < argc; i++)
        free(named_values[i]);
    free(named_values);
    codebuf_destroy(fields);

    /* Route a class with destructure slot fields through its claim helper so the
     * built object's slots are live before any method writes to them. */
    if (result != NULL
        && (transpiler_active_has_mir(ctx)
            ? mir_decl_header_field_claim_count(
                transpiler_active_decl_header_of_type(
                    ctx, AST_CLASS_DECL, decl_name)) > 0
            : ast_class_field_destructure_count(class_decl) > 0)) {
        char *claimed = strdup_fmt("%s__pgy_field_slot_init(%s)",
                                   ctor_type, result);
        free(result);
        result = claimed;
        const MIRDeclHeader *header = transpiler_active_decl_header_of_type(
            ctx, AST_CLASS_DECL, decl_name);
        if (mir_decl_header_nominal_kind_or(header, NOMINAL_DECL_CLASS)
                == NOMINAL_DECL_SUBJECT) {
            /* Keep even a claim-initialized fresh identity addressable in the
             * caller's block; do not return a pointer to an inner temporary. */
            result = strdup_fmt("((%s[]){ %s })[0]", ctor_type, claimed);
            free(claimed);
        }
    }
    return result;
}
