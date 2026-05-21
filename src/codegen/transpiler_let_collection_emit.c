/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Option/List/HashMap/Queue let-declaration lowering.
 */

#include "transpiler_let_collection_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

typedef enum TranspilerLetOptionCtorOp {
    TRANS_LET_OPTION_CTOR_NONE = 0,
    TRANS_LET_OPTION_CTOR_NONE_VALUE,
    TRANS_LET_OPTION_CTOR_SOME,
} TranspilerLetOptionCtorOp;

typedef struct TranspilerLetOptionCtorSpec {
    const char *name;
    TranspilerLetOptionCtorOp op;
} TranspilerLetOptionCtorSpec;

static int
transpiler_let_option_ctor_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerLetOptionCtorSpec *spec =
        (const TranspilerLetOptionCtorSpec *)entry;

    return strcmp(name, spec->name);
}

static TranspilerLetOptionCtorOp
transpiler_let_option_ctor_lookup(const char *callee_name)
{
    static const TranspilerLetOptionCtorSpec kTranspilerLetOptionCtorSpecs[] = {
        { "None", TRANS_LET_OPTION_CTOR_NONE_VALUE },
        { "Some", TRANS_LET_OPTION_CTOR_SOME },
    };
    const TranspilerLetOptionCtorSpec *match;

    if (callee_name == NULL)
        return TRANS_LET_OPTION_CTOR_NONE;

    match = (const TranspilerLetOptionCtorSpec *)bsearch(&callee_name,
        kTranspilerLetOptionCtorSpecs,
        sizeof(kTranspilerLetOptionCtorSpecs)
            / sizeof(kTranspilerLetOptionCtorSpecs[0]),
        sizeof(kTranspilerLetOptionCtorSpecs[0]),
        transpiler_let_option_ctor_compare);
    return match != NULL ? match->op : TRANS_LET_OPTION_CTOR_NONE;
}

typedef enum TranspilerLetCollectionCtorOp {
    TRANS_LET_COLLECTION_CTOR_NONE = 0,
    TRANS_LET_COLLECTION_CTOR_LIST,
    TRANS_LET_COLLECTION_CTOR_MAP,
    TRANS_LET_COLLECTION_CTOR_QUEUE,
} TranspilerLetCollectionCtorOp;

typedef struct TranspilerLetCollectionCtorSpec {
    const char *name;
    const char *annotation_type;
    const char *collection;
    const char *runtime_prefix;
    TranspilerLetCollectionCtorOp op;
} TranspilerLetCollectionCtorSpec;

static int
transpiler_let_collection_ctor_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerLetCollectionCtorSpec *spec =
        (const TranspilerLetCollectionCtorSpec *)entry;

    return strcmp(name, spec->name);
}

static const TranspilerLetCollectionCtorSpec *
transpiler_let_collection_ctor_lookup(const char *callee_name)
{
    static const TranspilerLetCollectionCtorSpec
        kTranspilerLetCollectionCtorSpecs[] = {
            { "ListNew", "List", "List", "pgy_list_new",
              TRANS_LET_COLLECTION_CTOR_LIST },
            { "MapNew", "HashMap", "Map", "pgy_map_new",
              TRANS_LET_COLLECTION_CTOR_MAP },
            { "QueueNew", "Queue", "Queue", "pgy_queue_new",
              TRANS_LET_COLLECTION_CTOR_QUEUE },
        };

    if (callee_name == NULL)
        return NULL;

    return (const TranspilerLetCollectionCtorSpec *)bsearch(&callee_name,
        kTranspilerLetCollectionCtorSpecs,
        sizeof(kTranspilerLetCollectionCtorSpecs)
            / sizeof(kTranspilerLetCollectionCtorSpecs[0]),
        sizeof(kTranspilerLetCollectionCtorSpecs[0]),
        transpiler_let_collection_ctor_compare);
}

bool
transpiler_try_emit_option_let(TranspilerCtx *ctx,
                               const char *name,
                               ASTNode *init,
                               char **ann_type_name_io)
{
    char inner_buf[128];
    const char *inner = NULL;
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;

    if (ann_type_name == NULL || strncmp(ann_type_name, "Option<", 7) != 0)
        return false;

    if (slot_inner_type_name_copy(ann_type_name, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;
    if (inner == NULL || inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Option binding '%s' requires concrete Option<T> annotation",
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (init == NULL
        || init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }

    ASTNode *callee = ast_call_callee(init);
    const char *callee_name = ast_identifier_name(callee);
    TranspilerLetOptionCtorOp op =
        transpiler_let_option_ctor_lookup(callee_name);
    if (op == TRANS_LET_OPTION_CTOR_SOME
        && ast_call_arg_count(init) == 1) {
        char *arg = emit_expression(ast_call_argument(init, 0), ctx);
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyOption_%s %s = Some_%s(%s);\n",
            inner, name, inner, arg);
        register_typed_var(ctx, name, ann_type_name);
        free(arg);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (op == TRANS_LET_OPTION_CTOR_NONE_VALUE
        && ast_call_arg_count(init) == 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyOption_%s %s = None_%s();\n",
            inner, name, inner);
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }

    return false;
}

static bool
transpiler_try_emit_map_new_let(TranspilerCtx *ctx,
                                const char *name,
                                ASTNode *resolved_ann,
                                char **ann_type_name_io)
{
    GenericParams *resolved_generic_args = ast_type_generic_args(resolved_ann);
    GenericParam *key_param = ast_generic_param_at(resolved_generic_args, 0);
    GenericParam *value_param = ast_generic_param_at(resolved_generic_args, 1);
    ASTNode *key_constraint = ast_generic_param_constraint(key_param);
    ASTNode *value_constraint = ast_generic_param_constraint(value_param);
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;
    char *key = key_constraint != NULL
        ? render_type_name(key_constraint)
        : (ast_generic_param_name(key_param) != NULL
            ? pergyra_strdup(ast_generic_param_name(key_param)) : NULL);
    char *value = value_constraint != NULL
        ? render_type_name(value_constraint)
        : (ast_generic_param_name(value_param) != NULL
            ? pergyra_strdup(ast_generic_param_name(value_param)) : NULL);

    if (key == NULL || key[0] == '\0' || value == NULL || value[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: HashMap binding '%s' requires explicit concrete HashMap<K, V> annotation",
            name != NULL ? name : "<binding>");
        free(key);
        free(value);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    if (strcmp(key, "String") == 0 && value != NULL) {
        char map_c_type_buf[256];
        char suffix_buf[128];
        const char *map_c_type = NULL;
        if (pergyra_type_to_c_copy(ann_type_name, map_c_type_buf,
                sizeof(map_c_type_buf))) {
            map_c_type = map_c_type_buf;
        }
        if (map_c_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: HashMap binding '%s' annotation cannot be rendered as a stable C type",
                name != NULL ? name : "<binding>");
            free(key);
            free(value);
            free(ann_type_name);
            *ann_type_name_io = NULL;
            return true;
        }
        ensure_collection_specialization(ctx, "Map", value);
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = pgy_map_new_%s();\n",
            map_c_type, name, suffix_buf);
        register_typed_var(ctx, name, ann_type_name);
        free(key);
        free(value);
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }

    free(key);
    free(value);
    return false;
}

static bool
transpiler_try_emit_list_or_queue_new_let(TranspilerCtx *ctx,
                                          const char *name,
                                          const TranspilerLetCollectionCtorSpec *spec,
                                          const char *inner,
                                          char **ann_type_name_io)
{
    char c_type_buf[256];
    char suffix_buf[128];
    const char *c_type = NULL;
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;

    if (spec == NULL
        || (spec->op != TRANS_LET_COLLECTION_CTOR_LIST
            && spec->op != TRANS_LET_COLLECTION_CTOR_QUEUE)) {
        return false;
    }

    if (pergyra_type_to_c_copy(ann_type_name, c_type_buf, sizeof(c_type_buf)))
        c_type = c_type_buf;
    if (c_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: %s binding '%s' annotation cannot be rendered as a stable C type",
            spec->collection,
            name != NULL ? name : "<binding>");
        free(ann_type_name);
        *ann_type_name_io = NULL;
        return true;
    }
    ensure_collection_specialization(ctx, spec->collection, inner);
    collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
    write_indent(ctx);
    codebuf_write(ctx->out, "%s %s = %s_%s();\n",
        c_type, name, spec->runtime_prefix, suffix_buf);
    register_typed_var(ctx, name, ann_type_name);
    free(ann_type_name);
    *ann_type_name_io = NULL;
    return true;
}

bool
transpiler_try_emit_collection_ctor_let(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *resolved_ann,
                                        const char *resolved_ann_type_name,
                                        char **ann_type_name_io)
{
    const char *callee_name;
    const char *type_name = resolved_ann_type_name;
    const TranspilerLetCollectionCtorSpec *spec;
    char inner_buf[128];
    const char *inner = NULL;
    char *ann_type_name = ann_type_name_io != NULL ? *ann_type_name_io : NULL;

    if (ann_type_name == NULL
        || resolved_ann == NULL
        || resolved_ann->type != AST_TYPE
        || resolved_ann_type_name == NULL
        || init == NULL
        || init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }
    if (strcmp(resolved_ann_type_name, "HashMap") != 0
        && strcmp(resolved_ann_type_name, "List") != 0
        && strcmp(resolved_ann_type_name, "Queue") != 0) {
        return false;
    }

    callee_name = ast_identifier_name(ast_call_callee(init));
    spec = transpiler_let_collection_ctor_lookup(callee_name);
    if (spec == NULL || strcmp(type_name, spec->annotation_type) != 0)
        return false;
    if (slot_inner_type_name_copy(ann_type_name, inner_buf, sizeof(inner_buf)))
        inner = inner_buf;

    if (spec->op == TRANS_LET_COLLECTION_CTOR_MAP
        && ast_generic_param_count(ast_type_generic_args(resolved_ann)) == 2) {
        return transpiler_try_emit_map_new_let(ctx, name, resolved_ann,
                                               ann_type_name_io);
    }
    return transpiler_try_emit_list_or_queue_new_let(ctx,
                                                     name,
                                                     spec,
                                                     inner,
                                                     ann_type_name_io);
}
