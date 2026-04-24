#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
type_check_projection_call(ASTNode *call,
                           SemanticContext *ctx,
                           const char *builtin_name,
                           NominalDeclKind expected_kind,
                           const char *expected_label)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    ASTNode *target_decl;
    ASTNode *source_decl;
    Symbol *target_sym;
    Type *source_type;
    bool in_projection_context;

    if (!check_call_arity(call, 2, builtin_name, ctx))
        return TYPE_UNKNOWN;

    in_projection_context =
        ctx != NULL
        && (ctx->current_relation != NULL
            || ctx->current_effect != NULL
            || ctx->current_zone != NULL
            || ctx->current_world != NULL);

    target_arg = call->data.call.arguments[0];
    source_arg = call->data.call.arguments[1];

    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "%s requires the first argument to be a %s type name",
            builtin_name, expected_label);
        return TYPE_UNKNOWN;
    }

    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER
        || source_arg->data.identifier.name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            call,
            "%s currently requires a named subject binding as the source",
            builtin_name);
        return TYPE_UNKNOWN;
    }

    target_decl = find_named_class_decl(ctx->program_root,
        target_arg->data.identifier.name);
    if (target_decl == NULL
        || !target_decl->data.class_decl.is_struct
        || target_decl->data.class_decl.nominal_kind != expected_kind) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            target_arg,
            "%s target '%s' must be a %s declaration",
            builtin_name,
            target_arg->data.identifier.name,
            expected_label);
        return TYPE_UNKNOWN;
    }

    target_sym = scope_lookup(ctx->scope, target_arg->data.identifier.name);
    if (target_sym == NULL || target_sym->type == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
            PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_DECLARE_OR_IMPORT_TYPE,
            target_arg,
            "Unknown %s type '%s'",
            expected_label,
            target_arg->data.identifier.name);
        return TYPE_UNKNOWN;
    }

    source_type = type_check_expression(source_arg, ctx);
    if (source_type == NULL || source_type == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;

    if (source_type->kind != TYPE_KIND_CLASS || source_type->name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, source_arg,
            "%s source must be a subject binding, got '%s'",
            builtin_name,
            source_type->name != NULL ? source_type->name : "<unknown>");
        return TYPE_UNKNOWN;
    }

    source_decl = find_named_class_decl(ctx->program_root, source_type->name);
    if (!decl_is_subject_nominal(source_decl)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, source_arg,
            "%s source '%s' must be a subject declaration",
            builtin_name,
            source_type->name != NULL ? source_type->name : "<unknown>");
        return TYPE_UNKNOWN;
    }

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        Type *target_field_type;
        Type *source_field_type;
        int source_status;

        if (target_field == NULL || target_field->name == NULL
            || target_field->type == NULL) {
            continue;
        }

        source_status = resolve_projection_source_field_type_rec(
            ctx->program_root, source_decl, target_field->name, 0, ctx, &source_field_type);
        if (source_status == 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                "%s target field '%s' is ambiguous in source subject '%s'.\n"
                "Reason:\n"
                "- multiple projection source paths match field '%s'\n"
                "- automatic projection cannot choose one path safely\n"
                "Fix:\n"
                "- rename one of the source fields to make the path unique\n"
                "- or expose the desired value directly on the subject host",
                builtin_name,
                target_field->name,
                source_type->name != NULL ? source_type->name : "<unknown>",
                target_field->name);
            continue;
        }
        if (source_status == 0 || source_field_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
                "%s target field '%s' is missing from source subject '%s'",
                builtin_name,
                target_field->name,
                source_type->name != NULL ? source_type->name : "<unknown>");
            continue;
        }

        target_field_type = resolve_type_node(target_field->type, ctx);
        require_assignable(source_field_type, target_field_type, call, ctx);
    }

    if (!in_projection_context) {
        if (expected_kind == NOMINAL_DECL_TOBJECT) {
            semantic_warning(ctx, call,
                "%s is being used as a direct boundary projection outside relation/effect/zone/world context; tobject is a transfer contract, so prefer tobject slots plus publish/transport flow",
                builtin_name);
        } else {
            semantic_warning(ctx, call,
                "%s is being used as a direct internal projection outside relation/effect/zone/world context; object is a local projection contract, so prefer object slots plus refresh flow",
                builtin_name);
        }
    }

    return target_sym->type;
}

Type *
type_check_to_tobject(ASTNode *call, SemanticContext *ctx)
{
    return type_check_projection_call(call, ctx, "ToTObject",
        NOMINAL_DECL_TOBJECT, "tobject");
}

Type *
type_check_to_object(ASTNode *call, SemanticContext *ctx)
{
    return type_check_projection_call(call, ctx, "ToObject",
        NOMINAL_DECL_OBJECT, "object");
}
