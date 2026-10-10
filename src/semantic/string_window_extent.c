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
#include "string_window_extent.h"

#include <stdlib.h>
#include <string.h>

#include "builtin_kind.h"
#include "diag_codes.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

typedef struct
{
    size_t source;
    size_t extent;
} SweRequirement;

typedef struct
{
    const char *source;
    const char *extent;
} SweFieldPair;

typedef struct
{
    uint32_t decl_id;
    ASTNode *node;
    bool is_method;
    SweRequirement *requirements;
    size_t requirement_count;
    size_t requirement_capacity;
} SweRoutine;

typedef struct
{
    ASTNode *call;
    ASTNode *routine;
} SweCall;

typedef struct
{
    ASTNode *call;
    ASTNode *routine;
    ASTNode *decl;
} SweConstructor;

typedef struct
{
    uint32_t decl_id;
    ASTNode *node;
} SweLet;

typedef struct
{
    ASTNode *identifier;
    uint32_t decl_id;
} SweFunctionValue;

typedef struct
{
    const char *name;
    ASTNode *owner;   /* the record declaring the field; NULL when unknown */
    ASTNode *site;
} SweFieldWrite;

struct StringWindowExtentStore
{
    SweRoutine *routines;
    size_t routine_count;
    size_t routine_capacity;
    SweCall *calls;
    size_t call_count;
    size_t call_capacity;
    SweConstructor *constructors;
    size_t constructor_count;
    size_t constructor_capacity;
    SweLet *lets;
    size_t let_count;
    size_t let_capacity;
    uint32_t *mutated;
    size_t mutated_count;
    size_t mutated_capacity;
    uint32_t *inout_passed;
    size_t inout_passed_count;
    size_t inout_passed_capacity;
    uint32_t *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    SweFieldWrite *field_writes;
    size_t field_write_count;
    size_t field_write_capacity;
    SweFieldPair *field_requirements;
    size_t field_requirement_count;
    size_t field_requirement_capacity;
    SweFunctionValue *function_values;
    size_t function_value_count;
    size_t function_value_capacity;
    bool allocation_failed;
};

typedef enum
{
    SWE_UNPROVEN = 0,
    SWE_PROVEN,
    SWE_REQUIREMENT,
    SWE_FIELD_REQUIREMENT
} SweVerdict;

static const char *const SWE_WINDOW_BUILTINS[] = {
    "CharCode", "CharAtN", "SubstringWithLen", "SubEqualsWithLen",
    "SubContainsWithLen", "SubIndexOfWithLen", "SubStartsWithLen"
};

/* Grow a record array by one slot; false leaves it unchanged. */
static bool
swe_reserve(void **items, size_t *capacity, size_t count, size_t item_size)
{
    if (count < *capacity)
        return true;
    size_t next = *capacity == 0 ? 16 : *capacity * 2;
    void *grown = realloc(*items, next * item_size);
    if (grown == NULL)
        return false;
    *items = grown;
    *capacity = next;
    return true;
}

static void swe_push_id(StringWindowExtentStore *store, uint32_t **items,
                        size_t *count, size_t *capacity, uint32_t decl_id);

static StringWindowExtentStore *
swe_store(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->string_window_extents : NULL;
}

void
semantic_string_window_extent_destroy(StringWindowExtentStore *store)
{
    if (store == NULL)
        return;
    for (size_t i = 0; i < store->routine_count; i++)
        free(store->routines[i].requirements);
    free(store->routines);
    free(store->calls);
    free(store->constructors);
    free(store->lets);
    free(store->mutated);
    free(store->inout_passed);
    free(store->parameters);
    free(store->field_writes);
    free(store->field_requirements);
    free(store->function_values);
    free(store);
}

bool
semantic_string_window_extent_begin(SemanticContext *ctx)
{
    if (ctx == NULL)
        return false;
    semantic_string_window_extent_destroy(ctx->string_window_extents);
    ctx->string_window_extents = calloc(1, sizeof(*ctx->string_window_extents));
    return ctx->string_window_extents != NULL;
}

void
semantic_string_window_extent_record_routine(SemanticContext *ctx,
                                             ASTNode *func_decl)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return;
    if (!swe_reserve((void **)&store->routines, &store->routine_capacity,
                     store->routine_count, sizeof(*store->routines))) {
        store->allocation_failed = true;
        return;
    }
    SweRoutine *routine = &store->routines[store->routine_count++];
    memset(routine, 0, sizeof(*routine));
    routine->decl_id = ast_node_stable_id(func_decl);
    routine->node = func_decl;
    /* A method's call arguments do not carry its receiver in the same
     * positions as its declaration, so a method may not carry a requirement. */
    routine->is_method = current_host_decl(ctx) != NULL
        || ctx->current_nominal_decl != NULL || ctx->current_party != NULL;
    for (size_t i = 0; i < ast_func_param_count(func_decl); i++)
        swe_push_id(store, &store->parameters, &store->parameter_count,
                    &store->parameter_capacity,
                    ast_func_param_stable_id(ast_func_param(func_decl, i)));
}

void
semantic_string_window_extent_record_let(SemanticContext *ctx,
                                         ASTNode *let_decl)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || let_decl == NULL)
        return;
    if (!swe_reserve((void **)&store->lets, &store->let_capacity,
                     store->let_count, sizeof(*store->lets))) {
        store->allocation_failed = true;
        return;
    }
    store->lets[store->let_count].decl_id = ast_node_stable_id(let_decl);
    store->lets[store->let_count].node = let_decl;
    store->let_count++;
}

static void
swe_push_id(StringWindowExtentStore *store, uint32_t **items, size_t *count,
            size_t *capacity, uint32_t decl_id)
{
    if (store == NULL || decl_id == 0)
        return;
    if (!swe_reserve((void **)items, capacity, *count, sizeof(**items))) {
        store->allocation_failed = true;
        return;
    }
    (*items)[(*count)++] = decl_id;
}

static void
swe_mark_mutated(StringWindowExtentStore *store, uint32_t decl_id)
{
    swe_push_id(store, &store->mutated, &store->mutated_count,
                &store->mutated_capacity, decl_id);
}

static void
swe_mark_field_write(StringWindowExtentStore *store, const char *name,
                     ASTNode *owner, ASTNode *site)
{
    if (store == NULL || name == NULL)
        return;
    if (!swe_reserve((void **)&store->field_writes, &store->field_write_capacity,
                     store->field_write_count, sizeof(*store->field_writes))) {
        store->allocation_failed = true;
        return;
    }
    store->field_writes[store->field_write_count].name = name;
    store->field_writes[store->field_write_count].owner = owner;
    store->field_writes[store->field_write_count].site = site;
    store->field_write_count++;
}

/* The identifier a place expression writes through, if any. */
static ASTNode *
swe_root_identifier(ASTNode *node)
{
    while (node != NULL) {
        switch (node->type) {
        case AST_IDENTIFIER:
            return node;
        case AST_MEMBER_ACCESS:
            node = ast_member_object(node);
            break;
        case AST_ARRAY_ACCESS:
            node = ast_array_access_array(node);
            break;
        default:
            return NULL;
        }
    }
    return NULL;
}

/* The record declaring the field that `object.field` names, when `object`
 * is a binding whose type is a program record; NULL otherwise. */
static ASTNode *
swe_member_owner(SemanticContext *ctx, ASTNode *object)
{
    if (ctx == NULL || object == NULL || object->type != AST_IDENTIFIER)
        return NULL;
    Symbol *sym = lookup_identifier_symbol(object, ctx);
    const char *type_name = sym != NULL && sym->type != NULL ? sym->type->name : NULL;
    return type_name != NULL ? semantic_find_class_decl_by_name(ctx, type_name) : NULL;
}

/* The field a place expression writes: its last member, or a bare host
 * field. Writing `a.b.c` replaces only field `c` of `a.b`. */
static void
swe_mark_place_fields(SemanticContext *ctx, StringWindowExtentStore *store,
                      ASTNode *place, ASTNode *site)
{
    if (place == NULL)
        return;
    if (place->type == AST_MEMBER_ACCESS) {
        swe_mark_field_write(store, ast_member_name(place),
                             swe_member_owner(ctx, ast_member_object(place)), site);
        return;
    }
    if (place->type == AST_IDENTIFIER && ast_identifier_binding_is_host_field(place))
        swe_mark_field_write(store, ast_identifier_name(place),
                             ctx != NULL ? current_host_decl(ctx) : NULL, site);
}

void
semantic_string_window_extent_record_assignment(SemanticContext *ctx,
                                                ASTNode *target)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || target == NULL)
        return;
    if (target->type == AST_TUPLE_LITERAL) {
        for (size_t i = 0; i < ast_tuple_literal_count(target); i++)
            semantic_string_window_extent_record_assignment(
                ctx, ast_tuple_literal_element(target, i));
        return;
    }
    ASTNode *root = swe_root_identifier(target);
    if (root == NULL)
        return;
    Symbol *sym = lookup_identifier_symbol(root, ctx);
    swe_mark_mutated(store, sym != NULL ? sym->decl_syntax_id
                                        : ast_identifier_binding_syntax_id(root));
    swe_mark_place_fields(ctx, store, target, target);
}

void
semantic_string_window_extent_record_call(SemanticContext *ctx, ASTNode *call)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || call == NULL || call->type != AST_CALL)
        return;
    if (!swe_reserve((void **)&store->calls, &store->call_capacity,
                     store->call_count, sizeof(*store->calls))) {
        store->allocation_failed = true;
        return;
    }
    store->calls[store->call_count].call = call;
    store->calls[store->call_count].routine = ctx->current_function_decl;
    store->call_count++;
}

void
semantic_string_window_extent_record_constructor(SemanticContext *ctx,
                                                 ASTNode *call, ASTNode *decl)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || call == NULL || decl == NULL
        || decl->type != AST_CLASS_DECL)
        return;
    if (!swe_reserve((void **)&store->constructors,
                     &store->constructor_capacity, store->constructor_count,
                     sizeof(*store->constructors))) {
        store->allocation_failed = true;
        return;
    }
    store->constructors[store->constructor_count].call = call;
    store->constructors[store->constructor_count].routine = ctx->current_function_decl;
    store->constructors[store->constructor_count].decl = decl;
    store->constructor_count++;
}

void
semantic_string_window_extent_record_function_value(SemanticContext *ctx,
                                                    ASTNode *identifier,
                                                    uint32_t decl_id)
{
    StringWindowExtentStore *store = swe_store(ctx);
    if (store == NULL || identifier == NULL || decl_id == 0)
        return;
    if (!swe_reserve((void **)&store->function_values,
                     &store->function_value_capacity,
                     store->function_value_count,
                     sizeof(*store->function_values))) {
        store->allocation_failed = true;
        return;
    }
    store->function_values[store->function_value_count].identifier = identifier;
    store->function_values[store->function_value_count].decl_id = decl_id;
    store->function_value_count++;
}

/* ---- finalization ---------------------------------------------------- */

static int
swe_compare_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static int
swe_compare_routine(const void *a, const void *b)
{
    return swe_compare_u32(&((const SweRoutine *)a)->decl_id,
                           &((const SweRoutine *)b)->decl_id);
}

static int
swe_compare_let(const void *a, const void *b)
{
    return swe_compare_u32(&((const SweLet *)a)->decl_id,
                           &((const SweLet *)b)->decl_id);
}

static SweRoutine *
swe_find_routine(StringWindowExtentStore *store, uint32_t decl_id)
{
    SweRoutine key;
    key.decl_id = decl_id;
    return decl_id == 0 ? NULL : bsearch(&key, store->routines,
        store->routine_count, sizeof(*store->routines), swe_compare_routine);
}

static SweRoutine *
swe_find_routine_node(StringWindowExtentStore *store, const ASTNode *node)
{
    return node != NULL ? swe_find_routine(store, ast_node_stable_id(node)) : NULL;
}

static ASTNode *
swe_find_let(StringWindowExtentStore *store, uint32_t decl_id)
{
    SweLet key;
    key.decl_id = decl_id;
    SweLet *found = decl_id == 0 ? NULL : bsearch(&key, store->lets,
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
static const char *
swe_bare_builtin_name(const ASTNode *call)
{
    ASTNode *callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_call_semantic_callee_decl_id(call) != 0
        || ast_call_semantic_callee_value_binding_id(call) != 0)
        return NULL;
    return ast_identifier_name(callee);
}

static bool
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

static SweVerdict
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
static bool
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
