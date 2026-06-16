/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Compile-time reflection (`reflect`) projection field folding.
 *
 * Split out of type_checker_expr_ops.c so that file stays under its size cap
 * and the reflection logic has a single owner. See docs/reflect_operator_design.md
 * and docs/reflect_domain_reflection_design.md.
 */

#include <stdio.h>
#include <string.h>

#include "type_checker_internal.h"
#include "../common/string_compat.h"

static const char *
projection_kind_label(const char *type_name, SemanticContext *ctx)
{
    ASTNode *decl = (type_name != NULL && ctx != NULL)
        ? semantic_find_class_decl_by_name(ctx, type_name) : NULL;
    if (decl != NULL && decl->type == AST_CLASS_DECL) {
        switch (ast_class_nominal_kind(decl)) {
        case NOMINAL_DECL_STRUCT:  return "struct";
        case NOMINAL_DECL_SUBJECT: return "subject";
        case NOMINAL_DECL_VESSEL:  return "vessel";
        case NOMINAL_DECL_OBJECT:  return "object";
        case NOMINAL_DECL_TOBJECT: return "tobject";
        case NOMINAL_DECL_CLASS:
        default:                   return "class";
        }
    }

    Symbol *sym = (type_name != NULL && ctx != NULL)
        ? scope_lookup(ctx->scope, type_name) : NULL;
    if (sym == NULL || sym->type == NULL)
        return "unknown";
    if (sym->type->kind == TYPE_KIND_ENUM)
        return "enum";
    if (sym->type->kind == TYPE_KIND_CLASS)
        return "class";
    return "primitive";
}

/*
 * Compile-time fold for a projection field access (the result of `reflect`).
 * Returns the folded field type, or NULL when this is not a projection field
 * so the caller keeps its normal member-access path. A projection carries its
 * reflected type name as its String representation, so `.name` is the value
 * itself, and `.kind` (direct `(reflect T).kind`) reads that name to report the
 * declared kind. `.kind` on a projection bound through a `let` is not folded
 * yet and falls through.
 */
Type *
expr_ops_projection_member(ASTNode *expr, ASTNode *member_object,
                           const char *member_name, Type *object_type,
                           SemanticContext *ctx)
{
    if (object_type == NULL || object_type->name == NULL || member_name == NULL
        || member_object == NULL
        || strcmp(object_type->name, "projection") != 0)
        return NULL;

    if (strcmp(member_name, "name") == 0) {
        ASTNode object_copy = *member_object;
        *expr = object_copy;
        return TYPE_STRING;
    }

    /* `.kind` and `.effects` fold from the reflected type/decl name. */
    const char *target_name = NULL;
    if (member_object->type == AST_STRING)
        target_name = ast_string_value(member_object);
    else if (member_object->type == AST_IDENTIFIER) {
        Symbol *bound = lookup_identifier_symbol(member_object, ctx);
        if (bound != NULL)
            target_name = bound->reflect_target_name;
    }
    if (target_name == NULL)
        return NULL;

    if (strcmp(member_name, "kind") == 0) {
        ast_morph_to_string(expr, projection_kind_label(target_name, ctx));
        return TYPE_STRING;
    }

    if (strcmp(member_name, "effects") == 0) {
        Symbol *sym = scope_lookup(ctx->scope, target_name);
        char buf[256];
        effect_mask_to_string(
            (sym != NULL && sym->type != NULL)
                ? type_function_effects(sym->type) : EFFECT_NONE,
            buf, sizeof(buf));
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }

    if (strcmp(member_name, "fields") == 0) {
        ASTNode *decl = semantic_find_class_decl_by_name(ctx, target_name);
        char buf[512];
        buf[0] = '\0';
        if (decl != NULL) {
            size_t field_count = projection_source_field_count(decl);
            for (size_t fi = 0; fi < field_count; fi++) {
                ClassField *cf = projection_source_field_at(decl, fi);
                if (cf == NULL || cf->name == NULL)
                    continue;
                if (buf[0] != '\0')
                    pergyra_str_append(buf, sizeof(buf), ",");
                pergyra_str_append(buf, sizeof(buf), cf->name);
                const char *ft = cf->type != NULL
                    ? ast_type_name(cf->type) : NULL;
                if (ft != NULL) {
                    pergyra_str_append(buf, sizeof(buf), ":");
                    pergyra_str_append(buf, sizeof(buf), ft);
                }
            }
        }
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }

    if (strcmp(member_name, "methods") == 0) {
        ASTNode *decl = semantic_find_class_decl_by_name(ctx, target_name);
        char buf[512];
        buf[0] = '\0';
        if (decl != NULL) {
            size_t method_count = 0;
            ASTNode **methods = ast_class_methods(decl, &method_count);
            for (size_t mi = 0; mi < method_count; mi++) {
                const char *mname = (methods != NULL && methods[mi] != NULL)
                    ? ast_declaration_name(methods[mi]) : NULL;
                if (mname == NULL)
                    continue;
                if (buf[0] != '\0')
                    pergyra_str_append(buf, sizeof(buf), ",");
                pergyra_str_append(buf, sizeof(buf), mname);
            }
        }
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }

    if (strcmp(member_name, "params") == 0) {
        ASTNode *decl = semantic_find_function_decl_by_name(ctx, target_name);
        char buf[512];
        buf[0] = '\0';
        if (decl != NULL) {
            size_t param_count = ast_func_param_count(decl);
            for (size_t pi = 0; pi < param_count; pi++) {
                FuncParam *fp = ast_func_param(decl, pi);
                if (fp == NULL || fp->name == NULL)
                    continue;
                if (buf[0] != '\0')
                    pergyra_str_append(buf, sizeof(buf), ",");
                pergyra_str_append(buf, sizeof(buf), fp->name);
                const char *pt = fp->type != NULL
                    ? ast_type_name(fp->type) : NULL;
                if (pt != NULL) {
                    pergyra_str_append(buf, sizeof(buf), ":");
                    pergyra_str_append(buf, sizeof(buf), pt);
                }
            }
        }
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }

    if (strcmp(member_name, "returns") == 0) {
        ASTNode *decl = semantic_find_function_decl_by_name(ctx, target_name);
        const char *ret = NULL;
        if (decl != NULL) {
            ASTNode *rt = ast_func_return_type(decl);
            if (rt != NULL)
                ret = ast_type_name(rt);
        }
        ast_morph_to_string(expr, ret != NULL ? ret : "Void");
        return TYPE_STRING;
    }

    if (strcmp(member_name, "steps") == 0) {
        ASTNode *decl = semantic_find_intent_decl_by_name(ctx, target_name);
        char buf[512];
        buf[0] = '\0';
        if (decl != NULL) {
            size_t step_count = 0;
            ASTNode **steps = ast_intent_decl_steps(decl, &step_count);
            for (size_t si = 0; si < step_count; si++) {
                const char *sname = (steps != NULL && steps[si] != NULL)
                    ? ast_intent_step_name(steps[si]) : NULL;
                if (sname == NULL)
                    continue;
                if (buf[0] != '\0')
                    pergyra_str_append(buf, sizeof(buf), ",");
                pergyra_str_append(buf, sizeof(buf), sname);
            }
        }
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }

    if (strcmp(member_name, "retry") == 0) {
        ASTNode *decl = semantic_find_intent_decl_by_name(ctx, target_name);
        int retry = decl != NULL ? ast_intent_decl_retry_count(decl) : 0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", retry);
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }

    if (strcmp(member_name, "involves") == 0) {
        ASTNode *decl = semantic_find_intent_decl_by_name(ctx, target_name);
        char buf[512];
        buf[0] = '\0';
        if (decl != NULL) {
            size_t involve_count = 0;
            ASTNode **involves = ast_intent_decl_involves(decl, &involve_count);
            for (size_t ii = 0; ii < involve_count; ii++) {
                const char *alias = (involves != NULL && involves[ii] != NULL)
                    ? ast_intent_involves_alias(involves[ii]) : NULL;
                if (alias == NULL)
                    continue;
                if (buf[0] != '\0')
                    pergyra_str_append(buf, sizeof(buf), ",");
                pergyra_str_append(buf, sizeof(buf), alias);
                ASTNode *st = ast_intent_involves_subject_type(involves[ii]);
                const char *stn = st != NULL ? ast_type_name(st) : NULL;
                if (stn != NULL) {
                    pergyra_str_append(buf, sizeof(buf), ":");
                    pergyra_str_append(buf, sizeof(buf), stn);
                }
            }
        }
        ast_morph_to_string(expr, buf);
        return TYPE_STRING;
    }
    return NULL;
}
