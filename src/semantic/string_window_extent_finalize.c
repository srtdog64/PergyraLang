/* String-window extent owner. Rule and proof forms P1-P8 are fixed in
 * docs/agent_work_directives/string_window_extent_closure_2026-10-10.md.
 *
 * Identity is the declaration syntax id recorded on every identifier, never
 * a spelling. A binding is stable when it is a routine parameter or a plain
 * let binding, no assignment anywhere in the program targets it, and no call
 * passes it to an inout parameter. Loop, match and destructured bindings and
 * host fields are never stable. The facts are program-wide, so a binding read from two routines
 * gets one answer. Builtins never rebind an Int or String binding: their
 * inout parameters are collection receivers.
 *
 * Reads of one element of a readonly `ref` array parameter (or a field path
 * over one) at the same stable index prove the element's extent in place
 * (P8): nothing writes that array's elements while the routine runs.
 *
 * A record field pair (f, g) is an invariant keyed by the two field names:
 * every construction of every record declaring both fields proves its pair,
 * and no write anywhere targets a field of either name. Keying by name, not
 * by record type, over-approximates and needs no expression types. */
#include "string_window_extent_internal.h"

#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

static void
swe_mark_call_mutations(SemanticContext *ctx, StringWindowExtentStore *store,
                        const ASTNode *call)
{
    uint32_t decl_id = ast_call_semantic_callee_decl_id(call);
    SweRoutine *routine = swe_find_routine(store, decl_id);
    const ASTNode *decl = routine != NULL ? routine->node : NULL;
    ASTNode *callee = ast_call_callee(call);
    if (decl == NULL && decl_id != 0 && callee != NULL
        && callee->type == AST_IDENTIFIER)
        decl = semantic_find_callable_decl_by_name(ctx,
                                                   ast_identifier_name(callee));
    bool unknown_callee = decl_id == 0
        && ast_call_semantic_callee_value_binding_id(call) != 0;
    for (size_t i = 0; i < ast_call_arg_count(call); i++) {
        bool rebinds = unknown_callee;
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && i < ast_func_param_count(decl)) {
            FuncParam *param = ast_func_param(decl, i);
            rebinds = param != NULL && param->mode == PARAM_MODE_MUT_REF;
        }
        if (!rebinds)
            continue;
        ASTNode *argument = ast_call_argument(call, i);
        ASTNode *root = swe_root_identifier(argument);
        if (root != NULL)
            swe_push_id(store, &store->inout_passed, &store->inout_passed_count,
                        &store->inout_passed_capacity,
                        ast_identifier_binding_syntax_id(root));
        swe_mark_place_fields(NULL, store, argument, (ASTNode *)call);
    }
}

static const char *
swe_callee_spelling(const ASTNode *call)
{
    ASTNode *callee = ast_call_callee(call);
    if (callee == NULL)
        return NULL;
    if (callee->type == AST_IDENTIFIER)
        return ast_identifier_name(callee);
    if (callee->type == AST_MEMBER_ACCESS)
        return ast_member_name(callee);
    return NULL;
}

/* The routine a call sits in, for the diagnostic; top-level code has none. */
static const char *
swe_site_routine_name(const ASTNode *routine)
{
    const char *name = routine != NULL ? ast_declaration_name(routine) : NULL;
    return name != NULL ? name : "top-level code";
}

static void
swe_report_window(SemanticContext *ctx, const SweCall *record)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_STRING_WINDOW_EXTENT_UNPROVEN,
        PGY_CAUSE_STRING_WINDOW_EXTENT_UNPROVEN,
        PGY_FIX_PASS_SAME_STRING_LENGTH,
        record->call,
        "%s in '%s' cannot prove that its second argument is StringLength of its first.\n"
        "Reason:\n"
        "- the builtin reads its source through that extent without measuring the string\n"
        "- a larger extent would read outside the string's storage\n"
        "Fix:\n"
        "- pass StringLength(source) of the same binding\n"
        "- or a binding declared `let n: Int = StringLength(source);` where neither is reassigned\n"
        "- or receive the source and its extent as two unchanged parameters or record fields",
        ast_identifier_name(ast_call_callee(record->call)),
        swe_site_routine_name(record->routine));
}

static void
swe_report_requirement(SemanticContext *ctx, const SweCall *record,
                       const SweRoutine *callee, SweRequirement requirement)
{
    FuncParam *source = ast_func_param(callee->node, requirement.source);
    FuncParam *extent = ast_func_param(callee->node, requirement.extent);
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_STRING_WINDOW_EXTENT_UNPROVEN,
        PGY_CAUSE_STRING_WINDOW_EXTENT_UNPROVEN,
        PGY_FIX_PASS_SAME_STRING_LENGTH,
        record->call,
        "Call to '%s' in '%s' cannot prove that argument %u is StringLength of argument %u.\n"
        "Reason:\n"
        "- '%s' reads its parameter '%s' through the extent parameter '%s'\n"
        "- each caller must prove that pair\n"
        "Fix:\n"
        "- pass StringLength(source) of the same binding as argument %u\n"
        "- or a binding declared `let n: Int = StringLength(source);` where neither is reassigned",
        ast_declaration_name(callee->node), swe_site_routine_name(record->routine),
        (unsigned)(requirement.extent + 1),
        (unsigned)(requirement.source + 1), ast_declaration_name(callee->node),
        source != NULL ? source->name : "?", extent != NULL ? extent->name : "?",
        (unsigned)(requirement.extent + 1));
}

/* One obligation per window call or per requirement of the resolved callee.
 * In the fixed point a P5 or P7 answer adds a requirement; in the check pass
 * a pair with no proof is a refusal. */
static bool
swe_visit_call(SemanticContext *ctx, StringWindowExtentStore *store,
               const SweCall *record, bool report)
{
    bool changed = false, unproven = false;
    SweRoutine *caller = swe_find_routine_node(store, record->routine);
    SweRequirement requirement;
    SweFieldPair field;
    if (swe_is_window_call(record->call)) {
        SweVerdict verdict = swe_classify(store,
            ast_call_argument(record->call, 0),
            ast_call_argument(record->call, 1), caller, &requirement, &field);
        changed = swe_apply_verdict(store, caller, verdict, requirement, field,
                                    &unproven);
        if (unproven && report)
            swe_report_window(ctx, record);
        return changed;
    }
    SweRoutine *callee = swe_find_routine(store,
        ast_call_semantic_callee_decl_id(record->call));
    if (callee == NULL)
        return false;
    for (size_t i = 0; i < callee->requirement_count; i++) {
        SweRequirement needed = callee->requirements[i];
        if (needed.source >= ast_call_arg_count(record->call)
            || needed.extent >= ast_call_arg_count(record->call)) {
            if (report)
                swe_report_requirement(ctx, record, callee, needed);
            continue;
        }
        SweVerdict verdict = swe_classify(store,
            ast_call_argument(record->call, needed.source),
            ast_call_argument(record->call, needed.extent), caller,
            &requirement, &field);
        changed = swe_apply_verdict(store, caller, verdict, requirement, field,
                                    &unproven) || changed;
        if (unproven && report)
            swe_report_requirement(ctx, record, callee, needed);
    }
    return changed;
}

static bool
swe_decl_field_index(ASTNode *decl, const char *name, size_t *index_out)
{
    size_t count = projection_source_field_count(decl);
    for (size_t i = 0; i < count; i++) {
        PgyDeclField field = subject_host_field_at(decl, i);
        if (field.name != NULL && strcmp(field.name, name) == 0) {
            *index_out = i;
            return true;
        }
    }
    return false;
}

/* The argument a construction passes for the field at `index`, positional
 * or named; NULL when the field takes its default. */
static ASTNode *
swe_constructor_argument(const SweConstructor *record, const char *name,
                         size_t index)
{
    for (size_t i = 0; i < ast_call_arg_count(record->call); i++) {
        const char *named = ast_call_argument_name(record->call, i);
        if (named != NULL ? strcmp(named, name) == 0 : i == index)
            return ast_call_argument(record->call, i);
    }
    return NULL;
}

/* A record declaring both fields of a required pair must prove that pair
 * at each construction; an omitted field leaves the pair unproven. */
static bool
swe_visit_constructor(SemanticContext *ctx, StringWindowExtentStore *store,
                      const SweConstructor *record, bool report)
{
    bool changed = false;
    SweRoutine *caller = swe_find_routine_node(store, record->routine);
    for (size_t i = 0; i < store->field_requirement_count; i++) {
        SweFieldPair pair = store->field_requirements[i];
        size_t source_index = 0, extent_index = 0;
        if (!swe_decl_field_index(record->decl, pair.source, &source_index)
            || !swe_decl_field_index(record->decl, pair.extent, &extent_index))
            continue;
        ASTNode *source = swe_constructor_argument(record, pair.source,
                                                   source_index);
        ASTNode *extent = swe_constructor_argument(record, pair.extent,
                                                   extent_index);
        bool unproven = source == NULL || extent == NULL;
        if (!unproven) {
            SweRequirement requirement;
            SweFieldPair field;
            SweVerdict verdict = swe_classify(store, source, extent, caller,
                                              &requirement, &field);
            changed = swe_apply_verdict(store, caller, verdict, requirement,
                                        field, &unproven) || changed;
        }
        if (unproven && report)
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_CAUSE_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_FIX_PASS_SAME_STRING_LENGTH,
                record->call,
                "Construction of '%s' in '%s' cannot prove that field '%s' is StringLength of field '%s'.\n"
                "Reason:\n"
                "- a string window reads '%s' through '%s' of some '%s' value\n"
                "- every construction must prove that pair\n"
                "Fix:\n"
                "- pass StringLength(source) of the same binding for '%s'",
                ast_declaration_name(record->decl),
                swe_site_routine_name(record->routine), pair.extent,
                pair.source, pair.source, pair.extent,
                ast_declaration_name(record->decl), pair.extent);
    }
    return changed;
}

static const SweRoutine *
swe_requirement_routine_named(const StringWindowExtentStore *store,
                              const char *name)
{
    for (size_t i = 0; name != NULL && i < store->routine_count; i++) {
        const SweRoutine *routine = &store->routines[i];
        const char *declared = ast_declaration_name(routine->node);
        if (routine->requirement_count > 0 && declared != NULL
            && strcmp(declared, name) == 0)
            return routine;
    }
    return NULL;
}

static bool
swe_failed_closed(SemanticContext *ctx)
{
    semantic_error(ctx, ctx != NULL ? ctx->program_root : NULL,
        "String-window extent analysis failed closed: its records could not be allocated");
    return false;
}

static void
swe_report_field_writes(SemanticContext *ctx,
                        const StringWindowExtentStore *store)
{
    for (size_t i = 0; i < store->field_write_count; i++) {
        const SweFieldWrite *write = &store->field_writes[i];
        for (size_t j = 0; j < store->field_requirement_count; j++) {
            const SweFieldPair *pair = &store->field_requirements[j];
            if (strcmp(write->name, pair->source) != 0
                && strcmp(write->name, pair->extent) != 0)
                continue;
            size_t ignored = 0;
            if (write->owner != NULL
                && !(swe_decl_field_index(write->owner, pair->source, &ignored)
                     && swe_decl_field_index(write->owner, pair->extent, &ignored)))
                continue;
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_CAUSE_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_FIX_PASS_SAME_STRING_LENGTH,
                write->site,
                "Field '%s' carries the string extent pair ('%s', '%s') and cannot be written after construction.\n"
                "Reason:\n"
                "- string windows read '%s' through '%s' of the same record\n"
                "Fix:\n"
                "- construct a new record with a proven pair instead",
                write->name, pair->source, pair->extent, pair->source,
                pair->extent);
            break;
        }
    }
}

bool
semantic_string_window_extent_finalize(SemanticContext *ctx)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || store->allocation_failed)
        return swe_failed_closed(ctx);
    qsort(store->routines, store->routine_count, sizeof(*store->routines),
          swe_compare_routine);
    qsort(store->lets, store->let_count, sizeof(*store->lets), swe_compare_let);
    for (size_t i = 0; i < store->call_count; i++)
        swe_mark_call_mutations(ctx, store, store->calls[i].call);
    if (store->allocation_failed)
        return swe_failed_closed(ctx);
    qsort(store->mutated, store->mutated_count, sizeof(*store->mutated),
          swe_compare_u32);
    qsort(store->inout_passed, store->inout_passed_count,
          sizeof(*store->inout_passed), swe_compare_u32);
    qsort(store->parameters, store->parameter_count,
          sizeof(*store->parameters), swe_compare_u32);

    bool changed = true;
    while (changed && !store->allocation_failed) {
        changed = false;
        for (size_t i = 0; i < store->call_count; i++)
            changed = swe_visit_call(ctx, store, &store->calls[i], false) || changed;
        for (size_t i = 0; i < store->constructor_count; i++)
            changed = swe_visit_constructor(ctx, store, &store->constructors[i],
                                            false) || changed;
    }
    if (store->allocation_failed)
        return swe_failed_closed(ctx);
    for (size_t i = 0; i < store->call_count; i++) {
        const SweCall *record = &store->calls[i];
        (void)swe_visit_call(ctx, store, record, true);
        if (ast_call_semantic_callee_decl_id(record->call) != 0
            || ast_call_semantic_callee_value_binding_id(record->call) != 0
            || swe_bare_builtin_name(record->call) != NULL)
            continue;
        const SweRoutine *named = swe_requirement_routine_named(
            store, swe_callee_spelling(record->call));
        if (named != NULL)
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_CAUSE_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_FIX_PASS_SAME_STRING_LENGTH,
                record->call,
                "Call target '%s' is not resolved, but a routine of that name requires each caller to prove a string extent",
                ast_declaration_name(named->node));
    }
    for (size_t i = 0; i < store->constructor_count; i++)
        (void)swe_visit_constructor(ctx, store, &store->constructors[i], true);
    swe_report_field_writes(ctx, store);
    for (size_t i = 0; i < store->function_value_count; i++) {
        const SweRoutine *routine = swe_find_routine(store,
            store->function_values[i].decl_id);
        if (routine != NULL && routine->requirement_count > 0)
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_CAUSE_STRING_WINDOW_EXTENT_UNPROVEN,
                PGY_FIX_PASS_SAME_STRING_LENGTH,
                store->function_values[i].identifier,
                "'%s' requires each caller to prove a string extent, so it cannot be used as a value",
                ast_declaration_name(routine->node));
    }
    return !ctx->has_error;
}
