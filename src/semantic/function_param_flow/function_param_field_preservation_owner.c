/* Field preservation of settled struct formals: whether a callee writes the
 * storage one top-level field of its formal names. The storage walk proves
 * it over the formal's settled program points, ignoring every other field's
 * paths; the verdicts are kept per function, formal and field. */
#include "function_param_flow_summary_internal.h"
#include "function_param_array_storage_walk_internal.h"
#include "../slot_summary.h"
#include "../type_checker/type_checker_internal.h"
#include "../type_checker/type_checker_call_constructor_internal.h"
#include "../../parser/ast_api.h"
#include <stdlib.h>
#include <string.h>

typedef struct FunctionParamFlowFieldVerdict {
    uint32_t function_id;
    size_t formal;
    const char *field;
    bool writes_only;
    unsigned failures;
    bool payload_read;
    bool computing;
} FunctionParamFlowFieldVerdict;

/* Verdicts are kept by index: a nested field demand of the walk may grow
 * the table. A verdict still being computed is a cycle and reads as
 * unproved. A writes_only verdict asks whether the routine writes the
 * field's storage, where the full verdict also counts its escapes. */
static unsigned
field_verdict(SemanticContext *ctx, ASTNode *decl, size_t ordinal, const char *field,
    bool writes_only, bool *payload_read)
{
    if (payload_read != NULL) *payload_read = true;
    unsigned whole = function_param_flow_array_preservation_failures(ctx, decl, ordinal);
    FunctionParamFlowSummaryStore *store = ctx != NULL ? ctx->function_param_flow_summaries : NULL;
    if (field == NULL || store == NULL || store->solving || store->failed) return whole;
    /* A preserved formal keeps every field; whether it reads an element
     * still needs the walk. */
    if (whole == 0 && payload_read == NULL) return 0;
    uint32_t id = ast_node_stable_id(decl);
    for (size_t i = 0; i < store->field_verdict_count; i++) {
        const FunctionParamFlowFieldVerdict *row = &store->field_verdicts[i];
        if (row->function_id == id && row->formal == ordinal && row->writes_only == writes_only
            && strcmp(row->field, field) == 0) {
            if (payload_read != NULL) *payload_read = row->computing || row->payload_read;
            return row->computing ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED : row->failures;
        }
    }
    if (store->field_verdict_count == store->field_verdict_capacity) {
        size_t capacity = store->field_verdict_capacity == 0 ? 16 : store->field_verdict_capacity * 2;
        FunctionParamFlowFieldVerdict *rows = realloc(store->field_verdicts, capacity * sizeof(*rows));
        if (rows == NULL) return whole;
        store->field_verdicts = rows;
        store->field_verdict_capacity = capacity;
    }
    size_t at = store->field_verdict_count++;
    store->field_verdicts[at] = (FunctionParamFlowFieldVerdict){id, ordinal, field, writes_only, whole, true, true};
    size_t entry = function_param_flow_find(store, id, ordinal);
    FunctionParamFlowProgramPointIndex *points = NULL;
    unsigned failures = whole;
    bool read = true;
    if (entry != SIZE_MAX && function_param_flow_program_point_index_build(store, decl, &points) &&
        points != NULL && ordinal < points->param_count) {
        SlotSummaryOrigin origin = {0};
        origin.function_decl = decl;
        origin.param_index = ordinal;
        origin.param_name = ast_func_param(decl, ordinal)->name;
        SlotFunctionLookup lookup = {ctx, store->program_root};
        if (!function_param_array_storage_field_unproved_in_program_points(points->dependency_roots,
                points->dependency_root_count, points->roots_by_param[ordinal], points->root_counts[ordinal],
                &origin, &lookup, store->entries[entry].mask, field, writes_only, &failures, &read)) {
            failures = whole;
            read = true;
        }
    }
    if (whole == 0)
        failures = 0;
    store->field_verdicts[at].failures = failures;
    store->field_verdicts[at].payload_read = read;
    if (payload_read != NULL) *payload_read = read;
    store->field_verdicts[at].computing = false;
    return failures;
}

unsigned
function_param_flow_field_preservation_failures(SemanticContext *ctx, ASTNode *decl, size_t ordinal,
    const char *field)
{
    return field_verdict(ctx, decl, ordinal, field, false, NULL);
}

/* The function reads an element of, writes, or hands on the storage of its
 * struct formal's `field`; a field it never names, or only reads a length
 * of, is untouched. Unknown answers touched. */
bool
function_param_flow_field_touched(SemanticContext *ctx, ASTNode *decl, size_t ordinal,
    const char *field)
{
    bool paths = false, read = true;
    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL || field == NULL
        || ordinal >= ast_func_param_count(decl)
        || !function_param_array_storage_formal_kind(decl, ordinal, &paths) || !paths)
        return true;
    return field_verdict(ctx, decl, ordinal, field, false, &read) != 0 || read;
}

/* A struct formal's field is written when its write verdict fails; the
 * whole struct formal when any field is. Any other formal (an Array) keeps
 * its full preservation verdict, which counts escapes as writes. */
bool
function_param_flow_field_written(SemanticContext *ctx, ASTNode *decl, size_t ordinal,
    const char *field)
{
    bool paths = false;
    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL
        || ordinal >= ast_func_param_count(decl))
        return true;
    if (!function_param_array_storage_formal_kind(decl, ordinal, &paths) || !paths)
        return function_param_flow_array_preservation_failures(ctx, decl, ordinal) != 0;
    if (field != NULL)
        return field_verdict(ctx, decl, ordinal, field, true, NULL) != 0;
    if (function_param_flow_array_preservation_failures(ctx, decl, ordinal) == 0)
        return false;
    FuncParam *param = ast_func_param(decl, ordinal);
    Symbol *type_symbol = param != NULL && param->type != NULL && ast_type_name(param->type) != NULL
        ? scope_lookup(ctx->scope, ast_type_name(param->type)) : NULL;
    ASTNode *struct_decl = type_symbol != NULL && type_symbol->kind == SYMBOL_CLASS
        ? semantic_host_decl_for_type(ctx, type_symbol->type) : NULL;
    if (struct_decl == NULL || struct_decl->type != AST_CLASS_DECL)
        return true;
    for (size_t i = 0; ; i++) {
        const char *name = NULL;
        (void)constructor_decl_field_resolved_type_at(struct_decl, i, &name, ctx);
        if (name == NULL) return false;
        if (field_verdict(ctx, decl, ordinal, name, true, NULL) != 0) return true;
    }
}
