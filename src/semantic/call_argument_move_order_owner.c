/* Move order inside one call's argument list; see the header. Symbol
 * pointers live only for one call's argument check. */

#include <stdlib.h>
#include <string.h>
#include "call_argument_move_order_owner.h"
#include "diag_codes.h"
#include "type_checker/type_checker_internal.h"
#include "slot_summary.h"
#include "collection_owned_element_requirement_owner.h"
#include "owned_string_transfer_owner.h"
#include "function_param_flow/function_param_publication_owner.h"

static Symbol *
argument_root_binding(ASTNode *argument, SemanticContext *ctx)
{
    while (argument != NULL && (argument->type == AST_MEMBER_ACCESS
            || argument->type == AST_ARRAY_ACCESS))
        argument = argument->type == AST_MEMBER_ACCESS
            ? ast_member_object(argument) : ast_array_access_array(argument);
    if (argument == NULL || argument->type != AST_IDENTIFIER)
        return NULL;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(argument));
    return binding != NULL && binding->decl_syntax_id != 0
        && binding->decl_syntax_id == ast_identifier_binding_syntax_id(argument)
        ? binding : NULL;
}

void
semantic_call_argument_move_begin(CallArgumentMoveFrame *frame)
{
    if (frame != NULL)
        memset(frame, 0, sizeof(*frame));
}

bool
semantic_call_argument_move_record(CallArgumentMoveFrame *frame,
    ASTNode *argument, SemanticContext *ctx)
{
    if (frame == NULL || ctx == NULL)
        return true;
    Symbol *binding = argument_root_binding(argument, ctx);
    if (binding == NULL || binding->is_consumed)
        return true;
    if (frame->count == frame->capacity) {
        size_t capacity = frame->capacity == 0 ? 4 : frame->capacity * 2;
        Symbol **bindings = realloc(frame->bindings, capacity * sizeof(*bindings));
        if (bindings != NULL) frame->bindings = bindings;
        ASTNode **arguments = realloc(frame->arguments, capacity * sizeof(*arguments));
        if (arguments != NULL) frame->arguments = arguments;
        if (bindings == NULL || arguments == NULL) {
            semantic_error(ctx, argument, "Call argument move order record allocation failed");
            return false;
        }
        frame->capacity = capacity;
    }
    frame->bindings[frame->count] = binding;
    frame->arguments[frame->count] = argument;
    frame->count++;
    return true;
}

bool
semantic_call_argument_move_check(CallArgumentMoveFrame *frame,
    SemanticContext *ctx)
{
    bool ok = true;
    for (size_t i = 0; frame != NULL && ctx != NULL && i < frame->count; i++) {
        Symbol *binding = frame->bindings[i];
        if (binding == NULL || !binding->is_consumed)
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            frame->arguments[i],
            "'%s' is passed to this call and moved by a later argument of the same call.\n"
            "Reason:\n"
            "- every argument is evaluated before the callee runs\n"
            "- the later argument releases the storage this argument hands the callee\n"
            "Fix:\n"
            "- evaluate the moving call in a statement before this call\n"
            "- or pass an independent Clone to one of the two",
            binding->name != NULL ? binding->name : "<value>");
        frame->bindings[i] = NULL;
        ok = false;
    }
    return ok;
}

static bool
call_argument_pending_add(CallArgumentSiblingPending row, SemanticContext *ctx)
{
    if (ctx->call_argument_sibling_count == ctx->call_argument_sibling_capacity) {
        size_t capacity = ctx->call_argument_sibling_capacity == 0
            ? 8 : ctx->call_argument_sibling_capacity * 2;
        CallArgumentSiblingPending *rows = realloc(ctx->call_argument_sibling_pending,
            capacity * sizeof(*rows));
        if (rows == NULL) {
            semantic_error(ctx, row.reader, "Call argument release record allocation failed");
            return false;
        }
        ctx->call_argument_sibling_pending = rows;
        ctx->call_argument_sibling_capacity = capacity;
    }
    ctx->call_argument_sibling_pending[ctx->call_argument_sibling_count++] = row;
    return true;
}

static bool
sibling_add(CallArgumentMoveFrame *frame, CallArgumentSiblingRow row,
    SemanticContext *ctx)
{
    uint32_t binding = row.binding;
    ASTNode *argument = row.argument;
    if (binding == 0)
        return true;
    if (frame->sibling_count == frame->sibling_capacity) {
        size_t capacity = frame->sibling_capacity == 0 ? 4 : frame->sibling_capacity * 2;
        CallArgumentSiblingRow *rows = realloc(frame->siblings, capacity * sizeof(*rows));
        if (rows == NULL) {
            semantic_error(ctx, argument, "Call argument sibling record allocation failed");
            return false;
        }
        frame->siblings = rows;
        frame->sibling_capacity = capacity;
    }
    frame->siblings[frame->sibling_count++] = row;
    return true;
}

/* The binding whose storage a String-valued expression reads, or 0. */
static uint32_t
string_read_root(ASTNode *value, SemanticContext *ctx)
{
    if (value != NULL && value->type == AST_IDENTIFIER) {
        Symbol *binding = argument_root_binding(value, ctx);
        return binding != NULL && type_equals(binding->type, TYPE_STRING)
            ? binding->storage_root_binding_id : 0;
    }
    if (value == NULL || (value->type != AST_MEMBER_ACCESS && value->type != AST_ARRAY_ACCESS))
        return 0;
    Symbol *root = argument_root_binding(value, ctx);
    return root != NULL ? root->decl_syntax_id : 0;
}

bool
semantic_call_argument_sibling_record(CallArgumentMoveFrame *frame,
    ASTNode *argument, size_t ordinal, const Type *type, int param_mode,
    SemanticContext *ctx)
{
    if (frame == NULL || ctx == NULL || argument == NULL)
        return true;
    /* A binding whose parts share storage or hold borrowed String elements,
     * handed to an inout or own formal: judged against that formal's release
     * summary after Pass 2. */
    bool string_value = type_equals((Type *)type, TYPE_STRING);
    if ((param_mode == PARAM_MODE_MUT_REF || param_mode == PARAM_MODE_OWN)
        && frame->call != NULL
        && semantic_owned_string_storage_unexclusive(argument, ctx)
        && !call_argument_pending_add((CallArgumentSiblingPending){frame->call,
            ast_node_stable_id(frame->call), argument, argument, ordinal, true,
            frame->callee, param_mode, string_value}, ctx))
        return false;
    if ((param_mode == PARAM_MODE_MUT_REF || param_mode == PARAM_MODE_OWN)
        && argument->type == AST_IDENTIFIER) {
        Symbol *binding = argument_root_binding(argument, ctx);
        return binding == NULL || sibling_add(frame, (CallArgumentSiblingRow){
            binding->decl_syntax_id, argument, ordinal, false, param_mode, string_value}, ctx);
    }
    if (string_value)
        return sibling_add(frame, (CallArgumentSiblingRow){
            string_read_root(argument, ctx), argument, ordinal, true, param_mode, true}, ctx);
    if (argument->type != AST_ARRAY_LITERAL)
        return true;
    for (size_t i = 0; i < ast_array_literal_count(argument); i++)
        if (!sibling_add(frame, (CallArgumentSiblingRow){
                string_read_root(ast_array_literal_element(argument, i), ctx),
                argument, ordinal, true, param_mode, true}, ctx))
            return false;
    return true;
}

bool
semantic_call_argument_sibling_check(CallArgumentMoveFrame *frame,
    ASTNode *call, SemanticContext *ctx)
{
    for (size_t r = 0; frame != NULL && ctx != NULL && r < frame->sibling_count; r++) {
        const CallArgumentSiblingRow *reader = &frame->siblings[r];
        if (!reader->reads)
            continue;
        for (size_t m = 0; m < frame->sibling_count; m++) {
            const CallArgumentSiblingRow *mover = &frame->siblings[m];
            if (mover->reads || mover->ordinal == reader->ordinal
                || mover->binding != reader->binding)
                continue;
            if (ctx->call_argument_sibling_count == ctx->call_argument_sibling_capacity) {
                size_t capacity = ctx->call_argument_sibling_capacity == 0
                    ? 8 : ctx->call_argument_sibling_capacity * 2;
                CallArgumentSiblingPending *rows = realloc(ctx->call_argument_sibling_pending,
                    capacity * sizeof(*rows));
                if (rows == NULL) {
                    semantic_error(ctx, reader->argument, "Call argument sibling record allocation failed");
                    return false;
                }
                ctx->call_argument_sibling_pending = rows;
                ctx->call_argument_sibling_capacity = capacity;
            }
            ctx->call_argument_sibling_pending[ctx->call_argument_sibling_count++] =
                (CallArgumentSiblingPending){call, ast_node_stable_id(call), reader->argument,
                    mover->argument, mover->ordinal, false, frame->callee, mover->mode,
                    mover->string_value};
        }
    }
    return true;
}

/* The formal of the current routine whose storage `node`'s binding (or the
 * path rooted at it) names, or SIZE_MAX. */
static size_t
current_formal_root(ASTNode *node, SemanticContext *ctx)
{
    Symbol *binding = argument_root_binding(node, ctx);
    ASTNode *routine = ctx->current_function_decl;
    if (binding == NULL || routine == NULL || routine->type != AST_FUNC_DECL)
        return SIZE_MAX;
    uint32_t root = binding->is_parameter ? binding->decl_syntax_id
        : binding->storage_root_binding_id;
    for (size_t k = 0; root != 0 && k < ast_func_param_count(routine); k++) {
        FuncParam *formal = ast_func_param(routine, k);
        if (formal != NULL && ast_func_param_stable_id(formal) == root)
            return k;
    }
    return SIZE_MAX;
}

static bool
formal_release_add(FormalReleaseRow row, ASTNode *site, SemanticContext *ctx)
{
    if (ctx->formal_release_count == ctx->formal_release_capacity) {
        size_t capacity = ctx->formal_release_capacity == 0 ? 32 : ctx->formal_release_capacity * 2;
        FormalReleaseRow *rows = realloc(ctx->formal_release_rows, capacity * sizeof(*rows));
        if (rows == NULL) {
            semantic_error(ctx, site, "Formal release record allocation failed");
            return false;
        }
        ctx->formal_release_rows = rows;
        ctx->formal_release_capacity = capacity;
    }
    ctx->formal_release_rows[ctx->formal_release_count++] = row;
    return true;
}

bool
semantic_formal_release_record_drop(ASTNode *receiver, SemanticContext *ctx)
{
    size_t formal = ctx != NULL ? current_formal_root(receiver, ctx) : SIZE_MAX;
    return formal == SIZE_MAX || formal_release_add(
        (FormalReleaseRow){ctx->current_function_decl, formal, NULL, 0, true}, receiver, ctx);
}

bool
semantic_formal_release_record_argument(ASTNode *argument, ASTNode *callee,
    size_t callee_formal, int param_mode, SemanticContext *ctx)
{
    if (ctx == NULL || callee == NULL || callee->type != AST_FUNC_DECL
        || (param_mode != PARAM_MODE_MUT_REF && param_mode != PARAM_MODE_OWN))
        return true;
    size_t formal = current_formal_root(argument, ctx);
    return formal == SIZE_MAX || formal_release_add(
        (FormalReleaseRow){ctx->current_function_decl, formal, callee, callee_formal, false},
        argument, ctx);
}

static bool
formal_releases(const SemanticContext *ctx, const ASTNode *routine, size_t formal)
{
    for (size_t i = 0; i < ctx->formal_release_count; i++) {
        const FormalReleaseRow *row = &ctx->formal_release_rows[i];
        if (row->releases && row->routine == routine && row->formal == formal)
            return true;
    }
    return false;
}

/* Close the forwarding edges: a formal handed to a releasing formal
 * releases too. */
static void
formal_release_close(SemanticContext *ctx)
{
    for (bool changed = true; changed;) {
        changed = false;
        for (size_t i = 0; i < ctx->formal_release_count; i++) {
            FormalReleaseRow *row = &ctx->formal_release_rows[i];
            if (row->releases || row->callee == NULL
                || !formal_releases(ctx, row->callee, row->callee_formal))
                continue;
            row->releases = true;
            changed = true;
        }
    }
}

/* After Pass 2: the callee's summary for the inout or own formal says
 * whether it releases what that argument holds. */
bool
semantic_call_argument_sibling_finalize(SemanticContext *ctx)
{
    bool ok = true;
    if (ctx != NULL)
        formal_release_close(ctx);
    for (size_t i = 0; ctx != NULL && i < ctx->call_argument_sibling_count; i++) {
        const CallArgumentSiblingPending *row = &ctx->call_argument_sibling_pending[i];
        bool current = row->call != NULL && row->call_id == ast_node_stable_id(row->call);
        ASTNode *callee = current
            ? semantic_param_publication_callee(ctx, row->call, row->mover_ordinal) : NULL;
        size_t formal = 0;
        bool published = callee != NULL && callee->type == AST_FUNC_DECL
            && semantic_param_publication_formal_for_actual(ctx, row->call, row->mover_ordinal, &formal);
        ASTNode *declared = row->declared_callee;
        bool bodyless = !published && current && declared != NULL
            && declared->type == AST_FUNC_DECL && ast_func_body(declared) == NULL;
        if (!published && !bodyless) {
            semantic_error(ctx, row->reader, "Call argument sibling check lost its callee identity");
            ok = false;
            continue;
        }
        if (bodyless && row->mover_mode == PARAM_MODE_MUT_REF && row->mover_string)
            continue;
        if (bodyless && row->mover_mode != PARAM_MODE_OWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->reader,
                "'%s' is passed to an inout parameter of '%s', which has no body: nothing proves it leaves the storage the argument's parts share or borrow.\n"
                "Reason:\n"
                "- a routine may release the parts of an inout aggregate\n"
                "- a part shared with another part, or a borrowed String element, would be freed wrongly\n"
                "Fix:\n"
                "- give each part independent owned storage (Clone, ArrayPushOwnedString) before the call",
                ast_identifier_name(row->mover) != NULL ? ast_identifier_name(row->mover) : "<value>",
                ast_declaration_name(declared) != NULL ? ast_declaration_name(declared) : "<callee>");
            ok = false;
            continue;
        }
        if (!bodyless
            && (function_param_flow_summary_for_param(ctx, callee, formal) & SLOT_PARAM_SUMMARY_RELEASE) == 0
            && !semantic_collection_owned_parameter_release_required(ctx, callee, formal)
            && !formal_releases(ctx, callee, formal))
            continue;
        if (row->storage_release) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, row->reader,
                "'%s' is passed to a routine that releases its storage, but its parts share storage or hold String elements it does not own.\n"
                "Reason:\n"
                "- the routine frees each part it releases\n"
                "- a part shared with another part is freed twice; a borrowed String element is not heap storage to free\n"
                "Fix:\n"
                "- give each part independent owned storage (Clone, ArrayPushOwnedString)\n"
                "- or keep the release with the owner of that storage",
                ast_identifier_name(row->reader));
            ok = false;
            continue;
        }
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE, row->reader,
            "A String read from '%s' is passed to a call that releases '%s'.\n"
            "Reason:\n"
            "- the callee releases the storage the String points into before or while reading it\n"
            "- it would then read freed memory through this argument\n"
            "Fix:\n"
            "- pass an independent copy (Concat(\"\", value))\n"
            "- or read the String before the call",
            ast_identifier_name(row->mover), ast_identifier_name(row->mover));
        ok = false;
    }
    return ok;
}

void
semantic_call_argument_move_finish(CallArgumentMoveFrame *frame)
{
    if (frame == NULL)
        return;
    free(frame->bindings);
    free(frame->arguments);
    free(frame->siblings);
    memset(frame, 0, sizeof(*frame));
}
