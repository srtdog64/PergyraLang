/* Carriers of a String element read through an aggregate's member; see the
 * header. Rows name bindings by SyntaxNodeId; AST pointers are the checked
 * program's and outlive the semantic context. */

#include <stdlib.h>
#include <string.h>

#include "member_element_borrow_carrier_owner.h"
#include "indexed_string_borrow_owner.h"
#include "slot_summary.h"
#include "diag_codes.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "function_param_flow/function_param_flow_loan_order.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "type_checker/type_checker_internal.h"

#define MEMBER_BORROW_CARRIER_MAX_DEPTH 32

typedef struct {
    SemanticContext *ctx;
    const ASTNode *definition;
    const char *carrier_name;
    uint32_t carrier;
    bool failed;
} CarrierRecord;

/* A type whose value can hold String storage. */
static bool
carrier_type(const Type *type)
{
    return type != NULL && type != TYPE_UNKNOWN
        && (type_equals((Type *)type, TYPE_STRING) || type->kind == TYPE_KIND_CLASS
            || type->kind == TYPE_KIND_CONSTRUCTED || type->kind == TYPE_KIND_TUPLE
            || type->kind == TYPE_KIND_ENUM);
}

static void
carrier_emit(CarrierRecord *rec, uint32_t root, const char *field,
    const ASTNode *via, size_t via_ordinal)
{
    SemanticContext *ctx = rec->ctx;
    if (root == 0 || root == rec->carrier || field == NULL || field[0] == '\0')
        return;
    for (size_t i = 0; i < ctx->member_borrow_carrier_count; i++) {
        const MemberBorrowCarrierRow *row = &ctx->member_borrow_carriers[i];
        if (row->definition == rec->definition && row->carrier == rec->carrier
            && row->root == root && strcmp(row->field, field) == 0
            && row->via_call == via && row->via_ordinal == via_ordinal)
            return;
    }
    if (ctx->member_borrow_carrier_count == ctx->member_borrow_carrier_capacity) {
        size_t capacity = ctx->member_borrow_carrier_capacity == 0
            ? 16 : ctx->member_borrow_carrier_capacity * 2;
        MemberBorrowCarrierRow *rows = realloc(ctx->member_borrow_carriers,
            capacity * sizeof(*rows));
        if (rows == NULL) {
            rec->failed = true;
            return;
        }
        ctx->member_borrow_carriers = rows;
        ctx->member_borrow_carrier_capacity = capacity;
    }
    ctx->member_borrow_carriers[ctx->member_borrow_carrier_count++] =
        (MemberBorrowCarrierRow){ctx->current_function_decl, rec->definition,
            rec->carrier_name, rec->carrier, root, field, via, via_ordinal};
}

/* `value` is an element read whose path passes through a member of a named
 * root: the root's binding and the path's top-level field. */
static bool
member_element_read(ASTNode *value, uint32_t *root, const char **field)
{
    if (value == NULL || value->type != AST_ARRAY_ACCESS)
        return false;
    ASTNode *path = ast_array_access_array(value);
    while (path != NULL && path->type == AST_ARRAY_ACCESS)
        path = ast_array_access_array(path);
    if (path == NULL || path->type != AST_MEMBER_ACCESS)
        return false;
    ASTNode *top = path;
    while (ast_member_object(top) != NULL
        && ast_member_object(top)->type == AST_MEMBER_ACCESS)
        top = ast_member_object(top);
    ASTNode *base = ast_member_object(top);
    *root = base != NULL && base->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(base) : 0;
    *field = ast_member_name(top);
    return *root != 0 && *field != NULL;
}

static bool
call_builds_value(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *callee = ast_call_callee(call);
    Symbol *symbol = callee != NULL && callee->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(callee)) : NULL;
    return symbol != NULL && (symbol->kind == SYMBOL_CLASS
        || symbol->kind == SYMBOL_ENUM_CONSTRUCTOR);
}

/* The member element reads `value` may hold. A user call's result holds an
 * argument's only if the callee returns it (`via`); a builtin's result holds
 * its arguments' unless it is a proved independent result. */
static void
carrier_sources(CarrierRecord *rec, ASTNode *value, const ASTNode *via,
    size_t via_ordinal, int depth)
{
    SemanticContext *ctx = rec->ctx;
    if (value == NULL || depth > MEMBER_BORROW_CARRIER_MAX_DEPTH)
        return;
    SemanticPublishedValue published = semantic_param_publication_value(ctx, value);
    if (published.ok && !published.meta && !carrier_type(published.type))
        return;
    uint32_t root = 0;
    const char *field = NULL;
    switch (value->type) {
    case AST_ARRAY_ACCESS:
        if (member_element_read(value, &root, &field))
            carrier_emit(rec, root, field, via, via_ordinal);
        return;
    case AST_IDENTIFIER: {
        uint32_t alias = ast_identifier_binding_syntax_id(value);
        size_t count = ctx->member_borrow_carrier_count;
        for (size_t i = 0; alias != 0 && i < count; i++) {
            const MemberBorrowCarrierRow row = ctx->member_borrow_carriers[i];
            if (row.carrier == alias && row.routine == ctx->current_function_decl)
                carrier_emit(rec, row.root, row.field,
                    row.via_call != NULL ? row.via_call : via,
                    row.via_call != NULL ? row.via_ordinal : via_ordinal);
        }
        return;
    }
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(value); i++)
            carrier_sources(rec, ast_array_literal_element(value, i), via,
                via_ordinal, depth + 1);
        return;
    case AST_CALL: {
        bool user = !call_builds_value(value, ctx)
            && ast_call_semantic_callee_declared_callable(value)
            && ast_call_semantic_callee_decl_id(value) != 0;
        if (!user && !call_builds_value(value, ctx)
            && semantic_indexed_string_independent_result(value, ctx))
            return;
        for (size_t i = 0; i < ast_call_arg_count(value); i++)
            carrier_sources(rec, ast_call_argument(value, i),
                user && via == NULL ? value : via,
                user && via == NULL ? i : via_ordinal, depth + 1);
        return;
    }
    default:
        return;
    }
}

static void
carrier_record(SemanticContext *ctx, uint32_t carrier, const char *name,
    const ASTNode *definition, ASTNode *value)
{
    if (ctx == NULL || carrier == 0 || definition == NULL || value == NULL
        || ctx->current_function_decl == NULL
        || ctx->current_function_decl->type != AST_FUNC_DECL
        || ast_func_body(ctx->current_function_decl) == NULL
        || !semantic_param_publication_pending(ctx))
        return;
    CarrierRecord rec = {ctx, definition, name, carrier, false};
    carrier_sources(&rec, value, NULL, 0, 0);
    if (rec.failed)
        semantic_error(ctx, (ASTNode *)definition,
            "Member element borrow carrier record allocation failed");
}

void
semantic_member_borrow_carrier_record_binding(Symbol *binding,
    const ASTNode *definition, ASTNode *value, SemanticContext *ctx)
{
    if (binding == NULL || binding->is_parameter || binding->is_host_field
        || !carrier_type(binding->type) || definition == NULL)
        return;
    /* The name is read from the syntax tree, which outlives the scope. */
    ASTNode *target = definition->type == AST_ASSIGNMENT
        ? ast_assignment_target(definition) : NULL;
    const char *name = definition->type == AST_LET_DECL ? ast_let_name(definition)
        : target != NULL && target->type == AST_IDENTIFIER ? ast_identifier_name(target) : NULL;
    carrier_record(ctx, binding->decl_syntax_id, name, definition, value);
}

void
semantic_member_borrow_carrier_record_store(ASTNode *receiver,
    const ASTNode *call, ASTNode *value, SemanticContext *ctx)
{
    if (ctx == NULL || receiver == NULL || receiver->type != AST_IDENTIFIER)
        return;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(receiver));
    if (binding == NULL || binding->decl_syntax_id == 0
        || binding->decl_syntax_id != ast_identifier_binding_syntax_id(receiver)
        || binding->is_parameter || binding->is_host_field)
        return;
    carrier_record(ctx, binding->decl_syntax_id, ast_identifier_name(receiver), call, value);
}

void
semantic_member_borrow_carrier_record_inout_root(ASTNode *argument,
    SemanticContext *ctx)
{
    uint32_t root = argument != NULL && argument->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(argument) : 0;
    if (ctx == NULL || root == 0 || ctx->current_function_decl == NULL
        || !semantic_param_publication_pending(ctx))
        return;
    for (size_t i = 0; i < ctx->member_borrow_inout_root_count; i++)
        if (ctx->member_borrow_inout_roots[i].root == root
            && ctx->member_borrow_inout_roots[i].routine == ctx->current_function_decl)
            return;
    if (ctx->member_borrow_inout_root_count == ctx->member_borrow_inout_root_capacity) {
        size_t capacity = ctx->member_borrow_inout_root_capacity == 0
            ? 16 : ctx->member_borrow_inout_root_capacity * 2;
        MemberBorrowInoutRoot *rows = realloc(ctx->member_borrow_inout_roots,
            capacity * sizeof(*rows));
        if (rows == NULL) {
            semantic_error(ctx, argument, "Member element borrow inout record allocation failed");
            return;
        }
        ctx->member_borrow_inout_roots = rows;
        ctx->member_borrow_inout_root_capacity = capacity;
    }
    ctx->member_borrow_inout_roots[ctx->member_borrow_inout_root_count++] =
        (MemberBorrowInoutRoot){ctx->current_function_decl, root};
}

/* Some carrier of `routine` borrows from a binding the routine passes
 * inout. */
static bool
carrier_routine_relevant(const SemanticContext *ctx, const ASTNode *routine)
{
    for (size_t i = 0; i < ctx->member_borrow_carrier_count; i++) {
        const MemberBorrowCarrierRow *row = &ctx->member_borrow_carriers[i];
        if (row->routine != routine)
            continue;
        for (size_t j = 0; j < ctx->member_borrow_inout_root_count; j++)
            if (ctx->member_borrow_inout_roots[j].routine == routine
                && ctx->member_borrow_inout_roots[j].root == row->root)
                return true;
    }
    return false;
}

/* The row's value holds the String: no call stands between, or the callee
 * returns the argument. A callee with no published summary counts as
 * returning it. */
static bool
carrier_row_holds(SemanticContext *ctx, const MemberBorrowCarrierRow *row)
{
    if (row->via_call == NULL)
        return true;
    ASTNode *callee = semantic_param_publication_callee(ctx, row->via_call, row->via_ordinal);
    size_t formal = 0;
    if (callee == NULL || callee->type != AST_FUNC_DECL
        || !semantic_param_publication_formal_for_actual(ctx, row->via_call,
            row->via_ordinal, &formal))
        return true;
    return (function_param_flow_summary_for_param(ctx, callee, formal)
        & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0;
}

static bool
carrier_refusal(SemanticContext *ctx, const MemberBorrowCarrierRow *row,
    ASTNode *call, const ASTNode *callee)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, call,
        "'%s' holds a String borrowed from aggregate field '%s', which '%s' may release or replace before a later read.\n"
        "Reason:\n"
        "- the sealed callee summary does not preserve that field's storage\n"
        "- '%s' is read after the call\n"
        "Fix:\n"
        "- finish using the String before the call\n"
        "- or copy it into independent storage (Concat(\"\", value)) where it is read from the field",
        row->carrier_name != NULL ? row->carrier_name : "<value>", row->field,
        ast_declaration_name(callee) != NULL ? ast_declaration_name(callee) : "<callee>",
        row->carrier_name != NULL ? row->carrier_name : "<value>");
    return false;
}

/* One call of `routine` that takes `actual`'s binding by inout: a carrier
 * of that binding's field the callee does not preserve, filled before the
 * call and read after it. */
static bool
carrier_call_admit(SemanticContext *ctx, FunctionParamFlowIndexClaims *claims,
    const ASTNode *routine, ASTNode *call, size_t actual)
{
    ASTNode *argument = ast_call_argument(call, actual);
    uint32_t root = argument != NULL && argument->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(argument) : 0;
    if (root == 0)
        return true;
    ASTNode *callee = semantic_param_publication_callee(ctx, call, actual);
    size_t formal = SIZE_MAX;
    if (callee == NULL || callee->type != AST_FUNC_DECL
        || !semantic_param_publication_checked_argument(ctx, call, actual)
        || !semantic_param_publication_formal_for_actual(ctx, call, actual, &formal)
        || formal >= ast_func_param_count(callee)
        || ast_func_param(callee, formal) == NULL
        || ast_func_param(callee, formal)->mode != PARAM_MODE_MUT_REF)
        return true;
    const ASTNode *body = ast_func_body((ASTNode *)routine);
    for (size_t r = 0; r < ctx->member_borrow_carrier_count; r++) {
        const MemberBorrowCarrierRow *row = &ctx->member_borrow_carriers[r];
        if (row->routine != routine || row->root != root
            || !function_param_flow_claim_completed_before(claims, row->definition, call, body)
            || function_param_flow_field_preservation_failures(ctx, callee, formal, row->field) == 0
            || !carrier_row_holds(ctx, row))
            continue;
        bool read_after = false;
        if (!function_param_flow_binding_read_after_call_current(
                ctx, claims, call, row->carrier, body, &read_after)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, call,
                "A String borrowed from an aggregate field has no current read-order proof at this call");
            return false;
        }
        if (read_after)
            return carrier_refusal(ctx, row, call, callee);
    }
    return true;
}

static bool
carrier_routine_admit(SemanticContext *ctx, const ASTNode *routine)
{
    if (!semantic_param_publication_routine_inputs_current(ctx, (ASTNode *)routine, true))
        return false;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *flow = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (flow == NULL || flow->failed || flow->solving || flow->validating_current
        || !function_param_flow_program_point_index_build(flow, (ASTNode *)routine, &index)
        || index == NULL || index->function_decl != routine
        || index->function_id != ast_node_stable_id(routine)
        || index->claims == NULL || !function_param_flow_index_roots_current(index)
        || !function_param_flow_index_claims_current(ctx, index))
        return false;
    FunctionParamFlowIndexClaims *claims = index->claims;
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim = &claims->rows[c];
        ASTNode *call = claim->node;
        if (claim->shape != AST_CALL || call == NULL || call->type != AST_CALL
            || !claim->semantic_promoted || !claim->snapshot_semantic_complete
            || !claim->execution_present || !claim->reachable)
            continue;
        for (size_t actual = 0; actual < ast_call_arg_count(call); actual++)
            if (!carrier_call_admit(ctx, claims, routine, call, actual))
                return false;
    }
    return true;
}

bool
semantic_member_borrow_carrier_finalize(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->member_borrow_carrier_count == 0)
        return true;
    if (ctx->has_error)
        return false;
    if (!semantic_param_publication_ready(ctx)) {
        semantic_error(ctx, (ASTNode *)ctx->member_borrow_carriers[0].definition,
            "Member element borrow carriers were judged before the publication sealed");
        return false;
    }
    for (size_t i = 0; i < ctx->member_borrow_carrier_count; i++) {
        const ASTNode *routine = ctx->member_borrow_carriers[i].routine;
        bool seen = false;
        for (size_t j = 0; j < i && !seen; j++)
            seen = ctx->member_borrow_carriers[j].routine == routine;
        if (seen || !carrier_routine_relevant(ctx, routine))
            continue;
        if (!carrier_routine_admit(ctx, routine)) {
            if (!ctx->has_error)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    (ASTNode *)ctx->member_borrow_carriers[i].definition,
                    "A String borrowed from an aggregate field has no current call-effect proof");
            return false;
        }
    }
    return true;
}
