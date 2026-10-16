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

static const char *const SWE_WINDOW_BUILTINS[] = {
    "CharCode", "CharAtN", "SubstringWithLen", "SubEqualsWithLen",
    "SubContainsWithLen", "SubIndexOfWithLen", "SubStartsWithLen"
};

int
swe_compare_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

int
swe_compare_routine(const void *a, const void *b)
{
    return swe_compare_u32(&((const SweRoutine *)a)->decl_id,
                           &((const SweRoutine *)b)->decl_id);
}

int
swe_compare_let(const void *a, const void *b)
{
    return swe_compare_u32(&((const SweLet *)a)->decl_id,
                           &((const SweLet *)b)->decl_id);
}

SweRoutine *
swe_find_routine(StringWindowExtentStore *store, uint32_t decl_id)
{
    SweRoutine key;
    key.decl_id = decl_id;
    return decl_id == 0 ? NULL : bsearch(&key, store->routines,
        store->routine_count, sizeof(*store->routines), swe_compare_routine);
}

SweRoutine *
swe_find_routine_node(StringWindowExtentStore *store, const ASTNode *node)
{
    return node != NULL ? swe_find_routine(store, ast_node_stable_id(node)) : NULL;
}

static ASTNode *
swe_find_let(const StringWindowExtentStore *store, uint32_t decl_id)
{
    SweLet key;
    key.decl_id = decl_id;
    const SweLet *found = decl_id == 0 ? NULL : bsearch(&key, store->lets,
        store->let_count, sizeof(*store->lets), swe_compare_let);
    return found != NULL ? found->node : NULL;
}

static bool
swe_id_in(const uint32_t *items, size_t count, uint32_t decl_id)
{
    return bsearch(&decl_id, items, count, sizeof(*items),
                   swe_compare_u32) != NULL;
}

static bool
swe_stable(const StringWindowExtentStore *store, const ASTNode *node)
{
    if (node == NULL || node->type != AST_IDENTIFIER
        || ast_identifier_binding_is_host_field(node))
        return false;
    uint32_t id = ast_identifier_binding_syntax_id(node);
    return id != 0
        && (swe_id_in(store->parameters, store->parameter_count, id)
            || swe_find_let(store, id) != NULL)
        && !swe_id_in(store->mutated, store->mutated_count, id)
        && !swe_id_in(store->inout_passed, store->inout_passed_count, id);
}

/* The builtin a call reaches by its bare spelling, or NULL when a program
 * declaration or a local value owns the callee. */
const char *
swe_bare_builtin_name(const ASTNode *call)
{
    ASTNode *callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_call_semantic_callee_decl_id(call) != 0
        || ast_call_semantic_callee_value_binding_id(call) != 0)
        return NULL;
    return ast_identifier_name(callee);
}

bool
swe_is_window_call(const ASTNode *call)
{
    const char *name = swe_bare_builtin_name(call);
    if (name == NULL || ast_call_arg_count(call) < 2)
        return false;
    for (size_t i = 0; i < sizeof(SWE_WINDOW_BUILTINS) / sizeof(SWE_WINDOW_BUILTINS[0]); i++)
        if (strcmp(name, SWE_WINDOW_BUILTINS[i]) == 0)
            return true;
    return false;
}

/* The argument of `StringLength(x)` reaching the builtin, or NULL. */
static ASTNode *
swe_string_length_argument(const ASTNode *node)
{
    if (node == NULL || node->type != AST_CALL || ast_call_arg_count(node) != 1)
        return NULL;
    const char *name = swe_bare_builtin_name(node);
    return name != NULL && strcmp(name, "StringLength") == 0
        ? ast_call_argument(node, 0) : NULL;
}

/* An Int literal keeps its value in the double field; only a Long literal
 * carries the exact 64-bit value. A value that is not integral or not
 * exactly representable is no literal bound. */
static bool
swe_integer_literal(const ASTNode *node, int64_t *value_out)
{
    if (node == NULL || node->type != AST_NUMBER || ast_number_is_float(node)
        || ast_number_is_duration(node))
        return false;
    if (ast_number_is_long(node)) {
        *value_out = ast_number_exact_long_value(node);
        return true;
    }
    double value = ast_number_value(node);
    if (!(value >= -9007199254740992.0 && value <= 9007199254740992.0))
        return false;
    int64_t integral = (int64_t)value;
    if ((double)integral != value)
        return false;
    *value_out = integral;
    return true;
}

static bool
swe_same_value(const StringWindowExtentStore *store, const ASTNode *a,
               const ASTNode *b)
{
    if (a == NULL || b == NULL)
        return false;
    if (a->type == AST_STRING && b->type == AST_STRING)
        return ast_string_value(a) != NULL && ast_string_value(b) != NULL
            && strcmp(ast_string_value(a), ast_string_value(b)) == 0;
    return swe_stable(store, a) && swe_stable(store, b)
        && ast_identifier_binding_syntax_id(a)
            == ast_identifier_binding_syntax_id(b);
}

/* Two member paths that read the same record: identical member names down
 * to one local or parameter root that no call passes as inout. The root may
 * be reassigned; every record value satisfies the field invariant. */
static bool
swe_same_record_path(const StringWindowExtentStore *store, const ASTNode *a,
                     const ASTNode *b)
{
    while (a != NULL && b != NULL) {
        if (a->type == AST_MEMBER_ACCESS && b->type == AST_MEMBER_ACCESS) {
            const char *na = ast_member_name(a), *nb = ast_member_name(b);
            if (na == NULL || nb == NULL || strcmp(na, nb) != 0)
                return false;
            a = ast_member_object(a);
            b = ast_member_object(b);
            continue;
        }
        if (a->type != AST_IDENTIFIER || b->type != AST_IDENTIFIER
            || ast_identifier_binding_is_host_field(a)
            || ast_identifier_binding_is_host_field(b))
            return false;
        uint32_t id = ast_identifier_binding_syntax_id(a);
        return id != 0 && id == ast_identifier_binding_syntax_id(b)
            && !swe_id_in(store->inout_passed, store->inout_passed_count, id);
    }
    return false;
}

static bool
swe_param_index(const ASTNode *routine, uint32_t decl_id, size_t *index_out)
{
    if (routine == NULL || routine->type != AST_FUNC_DECL || decl_id == 0)
        return false;
    for (size_t i = 0; i < ast_func_param_count(routine); i++) {
        if (ast_func_param_stable_id(ast_func_param(routine, i)) == decl_id) {
            *index_out = i;
            return true;
        }
    }
    return false;
}

/* A readonly place: a `ref` parameter of the routine, or a field path over
 * one. No write reaches its elements while the routine runs. */
static bool
swe_readonly_path(const ASTNode *a, const ASTNode *b, const SweRoutine *routine)
{
    while (a != NULL && b != NULL && a->type == AST_MEMBER_ACCESS
           && b->type == AST_MEMBER_ACCESS) {
        const char *na = ast_member_name(a), *nb = ast_member_name(b);
        if (na == NULL || nb == NULL || strcmp(na, nb) != 0)
            return false;
        a = ast_member_object(a);
        b = ast_member_object(b);
    }
    if (a == NULL || b == NULL || a->type != AST_IDENTIFIER
        || b->type != AST_IDENTIFIER || routine == NULL
        || ast_identifier_binding_is_host_field(a)
        || ast_identifier_binding_is_host_field(b))
        return false;
    uint32_t id = ast_identifier_binding_syntax_id(a);
    size_t index = 0;
    if (id == 0 || id != ast_identifier_binding_syntax_id(b)
        || !swe_param_index(routine->node, id, &index))
        return false;
    FuncParam *param = ast_func_param(routine->node, index);
    return param != NULL && param->mode == PARAM_MODE_REF;
}

/* P8: two reads of one element of a readonly array, at the same stable index
 * binding or the same integer literal. */
static bool
swe_same_element(const StringWindowExtentStore *store, const ASTNode *a,
                 const ASTNode *b, const SweRoutine *routine)
{
    if (a == NULL || b == NULL || a->type != AST_ARRAY_ACCESS
        || b->type != AST_ARRAY_ACCESS)
        return false;
    const ASTNode *ia = ast_array_access_index(a);
    const ASTNode *ib = ast_array_access_index(b);
    int64_t va = 0, vb = 0;
    bool same_index = (swe_integer_literal(ia, &va)
                       && swe_integer_literal(ib, &vb) && va == vb)
        || (swe_stable(store, ia) && swe_stable(store, ib)
            && ast_identifier_binding_syntax_id(ia)
                == ast_identifier_binding_syntax_id(ib));
    return same_index && swe_readonly_path(ast_array_access_array(a),
                                           ast_array_access_array(b), routine);
}

SweVerdict
swe_classify(StringWindowExtentStore *store, const ASTNode *source,
             const ASTNode *extent, const SweRoutine *routine,
             SweRequirement *requirement_out, SweFieldPair *field_out)
{
    int64_t value = 0;
    if (swe_integer_literal(extent, &value)) {
        if (value == 0 || value == 1)
            return SWE_PROVEN;                                    /* P4 */
        if (value > 1 && source != NULL && source->type == AST_STRING
            && ast_string_value(source) != NULL
            && (uint64_t)value <= strlen(ast_string_value(source)))
            return SWE_PROVEN;                                    /* P3 */
        return SWE_UNPROVEN;
    }
    ASTNode *measured = swe_string_length_argument(extent);
    if (measured != NULL)
        return swe_same_value(store, measured, source)
            || swe_same_element(store, measured, source, routine)
            ? SWE_PROVEN : SWE_UNPROVEN;                          /* P1, P8 */
    if (source != NULL && extent != NULL
        && source->type == AST_MEMBER_ACCESS
        && extent->type == AST_MEMBER_ACCESS
        && swe_same_record_path(store, ast_member_object(source),
                                ast_member_object(extent))) {
        field_out->source = ast_member_name(source);
        field_out->extent = ast_member_name(extent);
        return SWE_FIELD_REQUIREMENT;                             /* P7 */
    }
    if (source != NULL && source->type == AST_ARRAY_ACCESS) {
        ASTNode *element_witness = swe_stable(store, extent)
            ? swe_find_let(store, ast_identifier_binding_syntax_id(extent)) : NULL;
        return element_witness != NULL && swe_same_element(store,
                   swe_string_length_argument(ast_let_initializer(element_witness)),
                   source, routine)
            ? SWE_PROVEN : SWE_UNPROVEN;                          /* P8 */
    }
    if (!swe_stable(store, extent) || !swe_stable(store, source))
        return SWE_UNPROVEN;
    ASTNode *let_decl = swe_find_let(store,
        ast_identifier_binding_syntax_id(extent));
    if (let_decl != NULL) {
        ASTNode *witnessed = swe_string_length_argument(
            ast_let_initializer(let_decl));
        return swe_same_value(store, witnessed, source)
            ? SWE_PROVEN : SWE_UNPROVEN;                          /* P2 */
    }
    size_t source_index = 0, extent_index = 0;
    if (routine != NULL && !routine->is_method
        && swe_param_index(routine->node,
               ast_identifier_binding_syntax_id(source), &source_index)
        && swe_param_index(routine->node,
               ast_identifier_binding_syntax_id(extent), &extent_index)) {
        requirement_out->source = source_index;
        requirement_out->extent = extent_index;
        return SWE_REQUIREMENT;                                   /* P5 */
    }
    return SWE_UNPROVEN;
}

static bool
swe_add_requirement(StringWindowExtentStore *store, SweRoutine *routine,
                    SweRequirement requirement)
{
    for (size_t i = 0; i < routine->requirement_count; i++)
        if (routine->requirements[i].source == requirement.source
            && routine->requirements[i].extent == requirement.extent)
            return false;
    if (!swe_reserve((void **)&routine->requirements,
                     &routine->requirement_capacity,
                     routine->requirement_count,
                     sizeof(*routine->requirements))) {
        store->allocation_failed = true;
        return false;
    }
    routine->requirements[routine->requirement_count++] = requirement;
    return true;
}

static bool
swe_add_field_requirement(StringWindowExtentStore *store, SweFieldPair pair)
{
    if (pair.source == NULL || pair.extent == NULL)
        return false;
    for (size_t i = 0; i < store->field_requirement_count; i++)
        if (strcmp(store->field_requirements[i].source, pair.source) == 0
            && strcmp(store->field_requirements[i].extent, pair.extent) == 0)
            return false;
    if (!swe_reserve((void **)&store->field_requirements,
                     &store->field_requirement_capacity,
                     store->field_requirement_count,
                     sizeof(*store->field_requirements))) {
        store->allocation_failed = true;
        return false;
    }
    store->field_requirements[store->field_requirement_count++] = pair;
    return true;
}

/* Record a verdict's consequence. Returns whether a requirement was new;
 * sets *unproven when the pair has no proof at all. */
bool
swe_apply_verdict(StringWindowExtentStore *store, SweRoutine *caller,
                  SweVerdict verdict, SweRequirement requirement,
                  SweFieldPair field, bool *unproven)
{
    *unproven = false;
    switch (verdict) {
    case SWE_PROVEN:
        return false;
    case SWE_REQUIREMENT:
        if (caller == NULL) {
            *unproven = true;
            return false;
        }
        return swe_add_requirement(store, caller, requirement);
    case SWE_FIELD_REQUIREMENT:
        return swe_add_field_requirement(store, field);
    case SWE_UNPROVEN:
        break;
    }
    *unproven = true;
    return false;
}

/* Mark bindings an inout parameter may rebind, and fields it may write. A
 * call through a local value has no declaration here, so each argument
 * counts. */
