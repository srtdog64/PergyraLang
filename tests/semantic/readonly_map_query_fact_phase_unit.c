/* Raw owner-protocol faults, not proof that forged AST/Type rows own storage.
 * Link against the existing native semantic/parser objects, excluding main and
 * test mains. The shared gate owns that build; this unit does not alter headers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "semantic/type_checker.h"
#include "semantic/type_checker_internal.h"
#include "semantic/type_system.h"
#include "semantic/diag_codes.h"
#include "semantic/function_param_publication_owner.h"
#include "semantic/typed_map_query_fact_owner.h"
#include "semantic/callable_capability_inference.h"
#include "semantic/slot_analyzer_internal.h"
#include "semantic/type_checker_ownership_internal.h"
#include "semantic/function_param_flow_summary_internal.h"
#include "semantic/indexed_string_borrow_owner.h"

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "fact-phase fault line %d: %s\n", __LINE__, #x); } } while (0)

typedef struct {
    Lexer *lexer;
    Parser *parser;
    ASTNode *program, *reader, *caller, *map_call, *forward_call;
    SemanticContext *ctx;
    Type *map_type, *reader_type, *caller_type;
} Fixture;

static Fixture fixture(void)
{
    Fixture f = {0};
    f.lexer = lexer_create(
        "func ScalarRead(ref rows: HashMap<String, Int>) -> Int { return MapGet(rows, \"key\"); }\n"
        "func ScalarForward(ref rows: HashMap<String, Int>) -> Int { return ScalarRead(rows); }\n");
    f.parser = parser_create(f.lexer);
    f.program = parser_parse_program(f.parser);
    CHECK(f.program != NULL && !parser_has_error(f.parser));
    CHECK(ast_assign_stable_ids(f.program));
    f.reader = ast_program_statement(f.program, 0);
    f.caller = ast_program_statement(f.program, 1);
    f.map_call = ast_return_value(ast_block_statement(ast_func_body(f.reader), 0));
    f.forward_call = ast_return_value(ast_block_statement(ast_func_body(f.caller), 0));
    CHECK(f.map_call != NULL && f.map_call->type == AST_CALL);
    CHECK(f.forward_call != NULL && f.forward_call->type == AST_CALL);
    CHECK(ast_call_set_semantic_callee_builtin_kind(f.map_call, BUILTIN_NOT_BUILTIN));
    CHECK(ast_call_set_semantic_callee_is_stdlib(f.map_call, true));
    CHECK(ast_call_set_semantic_callee_decl_id(f.forward_call, ast_node_stable_id(f.reader)));
    Type *args[] = {TYPE_STRING, TYPE_INT};
    f.map_type = type_create_constructed(TYPE_HASHMAP, args, 2);
    Type *parameters[] = {f.map_type};
    f.reader_type = type_create_function(parameters, 1, TYPE_INT);
    f.caller_type = type_create_function(parameters, 1, TYPE_INT);
    type_function_set_param_mode(f.reader_type, 0, PARAM_MODE_REF);
    type_function_set_param_mode(f.caller_type, 0, PARAM_MODE_REF);
    /* A deliberately supplied body-complete marker is a phase fault-control
     * input, not a substitute for the source typecheck tests below. */
    type_function_set_body_summary(f.reader_type, 0);
    type_function_set_body_summary(f.caller_type, 0);
    f.ctx = semantic_context_create();
    f.ctx->program_root = f.program;
    CHECK(semantic_build_host_decl_index(f.ctx, f.program));
    CHECK(semantic_param_publication_begin(f.ctx));
    CHECK(semantic_param_publication_pending(f.ctx));
    CHECK(!semantic_param_publication_inputs_ready(f.ctx));
    CHECK(!semantic_param_publication_ready(f.ctx));
    /* Raw protocol baseline, not a substitute for actual source typing. */
    ASTNode *reader_argument = ast_call_argument(f.map_call, 0);
    ASTNode *caller_argument = ast_call_argument(f.forward_call, 0);
    ast_identifier_set_binding_syntax_id(reader_argument, ast_func_param_stable_id(ast_func_param(f.reader, 0)));
    ast_identifier_set_binding_syntax_id(caller_argument, ast_func_param_stable_id(ast_func_param(f.caller, 0)));
    CHECK(ast_identifier_binding_syntax_id(reader_argument) == ast_func_param_stable_id(ast_func_param(f.reader, 0)));
    CHECK(ast_identifier_binding_syntax_id(caller_argument) == ast_func_param_stable_id(ast_func_param(f.caller, 0)));
    CHECK(semantic_param_publication_record_value(f.ctx, reader_argument, f.map_type, false));
    CHECK(semantic_param_publication_record_value(f.ctx, ast_call_argument(f.map_call, 1), TYPE_STRING, false));
    CHECK(semantic_param_publication_record_value(f.ctx, caller_argument, f.map_type, false));
    CHECK(semantic_param_publication_record_checked_call(f.ctx, f.map_call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(f.forward_call, BUILTIN_NOT_BUILTIN));
    CHECK(semantic_param_publication_record_checked_call(f.ctx, f.forward_call));
    return f;
}

static bool register_reader(Fixture *f)
{
    Type *parameters[] = {f->map_type};
    return semantic_param_publication_record_routine(f->ctx, f->reader, 1, parameters, f->reader_type);
}
static bool register_caller(Fixture *f)
{
    Type *parameters[] = {f->map_type};
    return semantic_param_publication_record_routine(f->ctx, f->caller, 1, parameters, f->caller_type);
}
static bool register_map(Fixture *f)
{
    return semantic_typed_map_query_record(f->ctx, f->map_call, PGY_TYPED_MAP_QUERY_INT, f->map_type, TYPE_INT);
}
static bool register_body_dependency(Fixture *f)
{
    f->ctx->current_function_decl = f->caller;
    f->ctx->tracking_function_effects = true;
    Type *parameters[] = {f->map_type};
    CallableCapabilityRoutine *previous = callable_capability_enter(f->ctx, f->caller, parameters, 1);
    CHECK(callable_capability_current_declaration(f->ctx) == f->caller);
    bool recorded = semantic_param_publication_record_body_call(f->ctx, f->forward_call);
    callable_capability_leave(f->ctx, previous, f->caller_type, 0, 0);
    return recorded;
}
static bool register_borrow_call(Fixture *f, ASTNode *site, ASTNode *callee, size_t ordinal)
{
    f->ctx->current_function_decl = f->caller;
    return semantic_param_publication_record_call(f->ctx, f->forward_call, site, callee, ordinal,
        f->map_type, "rows", "ScalarRead", "retain original ref boundary");
}
static void dispose(Fixture *f)
{
    semantic_context_destroy(f->ctx);
    ast_destroy(f->program);
    parser_destroy(f->parser);
    lexer_destroy(f->lexer);
}

static void fact_faults(void)
{
    Fixture f = fixture();
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0)); /* missing */
    CHECK(register_map(&f));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0)); /* pending */
    CHECK(register_reader(&f));
    CHECK(register_caller(&f));
    CHECK(register_body_dependency(&f));
    CHECK(semantic_param_publication_finalize(f.ctx));
    CHECK(semantic_param_publication_ready(f.ctx));
    CHECK(semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 1));
    Fixture foreign = fixture();
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, foreign.map_call, 0));
    dispose(&foreign);
    uint32_t saved = f.map_call->stable_id;
    f.map_call->stable_id++;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    f.map_call->stable_id = saved;
    ASTNode *target = ast_call_callee(f.map_call);
    ASTNode *receiver = ast_call_argument(f.map_call, 0);
    saved = target->stable_id;
    target->stable_id++;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    target->stable_id = saved;
    saved = receiver->stable_id;
    receiver->stable_id++;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    receiver->stable_id = saved;
    ASTNode *replacement = ast_create_identifier("rows");
    replacement->stable_id = receiver->stable_id;
    f.map_call->data.call.arguments[0] = replacement;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    f.map_call->data.call.arguments[0] = receiver;
    ast_destroy(replacement);
    replacement = ast_create_identifier("MapGet");
    replacement->stable_id = target->stable_id;
    f.map_call->data.call.callee = replacement;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    f.map_call->data.call.callee = target;
    ast_destroy(replacement);
    CHECK(ast_call_set_semantic_callee_is_stdlib(f.map_call, false));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_is_stdlib(f.map_call, true));
    CHECK(ast_call_set_semantic_callee_decl_id(f.map_call, ast_node_stable_id(f.reader)));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.map_call, 123));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.map_call, 0));
    semantic_typed_map_query_destroy(f.ctx);
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    dispose(&f);

    f = fixture();
    Type *strings[] = {TYPE_STRING, TYPE_STRING};
    Type *string_map = type_create_constructed(TYPE_HASHMAP, strings, 2);
    CHECK(!semantic_typed_map_query_record(f.ctx, f.map_call, PGY_TYPED_MAP_QUERY_INT, string_map, TYPE_STRING));
    CHECK(!semantic_typed_map_query_record(f.ctx, f.map_call, PGY_TYPED_MAP_QUERY_INT, f.map_type, TYPE_UNKNOWN));
    CHECK(!semantic_typed_map_query_record(f.ctx, f.map_call, PGY_TYPED_MAP_QUERY_INT, TYPE_INT, TYPE_INT));
    CHECK(!semantic_typed_map_query_record(f.ctx, f.map_call, (PgyTypedMapQueryKind)99, f.map_type, TYPE_INT));
    CHECK(!semantic_typed_map_query_record(f.ctx, f.map_call, PGY_TYPED_MAP_QUERY_KEYS, f.map_type, TYPE_INT));
    size_t arity = f.map_call->data.call.arg_count;
    f.map_call->data.call.arg_count = 1;
    CHECK(!register_map(&f));
    f.map_call->data.call.arg_count = arity;
    CHECK(ast_call_set_semantic_callee_decl_id(f.map_call, ast_node_stable_id(f.reader)));
    CHECK(!register_map(&f));
    CHECK(ast_call_set_semantic_callee_decl_id(f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.map_call, 12));
    CHECK(!register_map(&f));
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_builtin_kind(f.map_call, 999));
    CHECK(!register_map(&f));
    CHECK(ast_call_set_semantic_callee_builtin_kind(f.map_call, BUILTIN_NOT_BUILTIN));
    CHECK(ast_replace_identifier_name_copy(ast_call_callee(f.map_call), "UnknownMapReader"));
    CHECK(!register_map(&f));
    dispose(&f);
}

static void phase_faults(void)
{
    for (unsigned fault = 0; fault < 10; fault++) {
        Fixture f = fixture();
        CHECK(register_map(&f));
        CHECK(register_reader(&f));
        CHECK(register_caller(&f));
        CHECK(register_body_dependency(&f));
        ASTNode *site = ast_call_argument(f.forward_call, 0);
        CHECK(register_borrow_call(&f, site, f.reader, 0));
        uint32_t saved_id = 0;
        ParamMode saved_mode = ast_func_param(f.reader, 0)->mode;
        if (fault == 0) { saved_id = f.program->stable_id; f.program->stable_id++; }
        if (fault == 1) { saved_id = f.forward_call->stable_id; f.forward_call->stable_id++; }
        if (fault == 2) { saved_id = site->stable_id; site->stable_id++; }
        if (fault == 3) { saved_id = f.reader->stable_id; f.reader->stable_id++; }
        if (fault == 4) { saved_id = f.caller->stable_id; f.caller->stable_id++; }
        if (fault == 5) { ast_call_set_semantic_callee_decl_id(f.forward_call, 0); }
        if (fault == 6) { ast_call_set_semantic_callee_value_binding_id(f.forward_call, 77); }
        if (fault == 7) { ast_func_param(f.reader, 0)->mode = PARAM_MODE_DEFAULT; }
        if (fault == 8) { type_function_set_param_mode(f.reader_type, 0, PARAM_MODE_DEFAULT); }
        if (fault == 9) { f.ctx->program_root = f.reader; }
        CHECK(!semantic_param_publication_finalize(f.ctx));
        CHECK(!semantic_param_publication_inputs_ready(f.ctx));
        CHECK(!semantic_param_publication_ready(f.ctx));
        if (fault == 0) f.program->stable_id = saved_id;
        if (fault == 1) f.forward_call->stable_id = saved_id;
        if (fault == 2) site->stable_id = saved_id;
        if (fault == 3) f.reader->stable_id = saved_id;
        if (fault == 4) f.caller->stable_id = saved_id;
        ast_func_param(f.reader, 0)->mode = saved_mode;
        f.ctx->program_root = f.program;
        dispose(&f);
    }
    Fixture f = fixture();
    CHECK(register_caller(&f));
    CHECK(register_body_dependency(&f));
    CHECK(!semantic_param_publication_finalize(f.ctx)); /* demanded target not registered */
    CHECK(!semantic_param_publication_inputs_ready(f.ctx));
    dispose(&f);
    f = fixture();
    Type *parameters[] = {f.map_type};
    f.reader_type = type_create_function(parameters, 1, TYPE_INT);
    type_function_set_param_mode(f.reader_type, 0, PARAM_MODE_REF);
    CHECK(register_reader(&f));
    CHECK(!semantic_param_publication_finalize(f.ctx)); /* body not typed */
    CHECK(!semantic_param_publication_inputs_ready(f.ctx));
    dispose(&f);
    f = fixture();
    CHECK(register_reader(&f));
    CHECK(ast_program_append_statement(f.program, ast_create_function("AddedAfterBegin")));
    CHECK(!semantic_param_publication_pending(f.ctx));
    CHECK(!semantic_param_publication_finalize(f.ctx));
    CHECK(!semantic_param_publication_ready(f.ctx));
    dispose(&f);
    f = fixture();
    CHECK(register_reader(&f)); CHECK(register_caller(&f)); CHECK(register_map(&f));
    ASTNode *original_site = ast_call_argument(f.forward_call, 0);
    CHECK(register_borrow_call(&f, original_site, f.reader, 0));
    ASTNode *foreign_site = ast_create_identifier("foreign");
    foreign_site->stable_id = original_site->stable_id;
    f.forward_call->data.call.arguments[0] = foreign_site;
    CHECK(!semantic_param_publication_finalize(f.ctx));
    CHECK(!semantic_param_publication_inputs_ready(f.ctx));
    f.forward_call->data.call.arguments[0] = original_site;
    ast_destroy(foreign_site);
    dispose(&f);
    f = fixture();
    ASTNode *site = ast_call_argument(f.forward_call, 0);
    CHECK(!register_borrow_call(&f, ast_call_callee(f.forward_call), f.reader, 0));
    CHECK(!register_borrow_call(&f, site, f.reader, 1));
    CHECK(!register_borrow_call(&f, site, f.caller, 0));
    ast_func_param(f.reader, 0)->mode = PARAM_MODE_DEFAULT;
    CHECK(!register_borrow_call(&f, site, f.reader, 0));
    dispose(&f);
}

static void produced_unknown_fault(void)
{
    Fixture f = fixture();
    Lexer *lexer = lexer_create("func ConcreteUnknown(ref rows: HashMap<String, Unknown>) -> Int { return 0; }");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    ASTNode *decl = ast_program_statement(program, 0);
    SemanticContext *produced = semantic_context_create();
    CHECK(type_check_program(program, produced));
    Type *resolved = semantic_host_resolve_type_ref(ast_func_param(decl, 0)->type, produced);
    /* The admitted metadata has no Unknown argument binding: the entire
     * parameter is unresolved, not a produced HashMap with a concrete V. */
    CHECK(resolved == TYPE_UNKNOWN);
    CHECK(!semantic_param_publication_routine_no_output_header_ready(produced, decl));
    bool anchored = false;
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(produced, decl, 0, &anchored));
    CHECK(!semantic_typed_map_query_record(f.ctx, f.map_call, PGY_TYPED_MAP_QUERY_INT, resolved, TYPE_INT));
    /* Explicit owner-protocol control only, NOT produced source evidence.
     * Key snapshot ownership does not depend on the map value being known. */
    Type *map_args[] = {TYPE_STRING, TYPE_UNKNOWN};
    Type *map = type_create_constructed(TYPE_HASHMAP, map_args, 2);
    CHECK(ast_replace_identifier_name_copy(ast_call_callee(f.map_call), "MapKeys"));
    size_t count = f.map_call->data.call.arg_count;
    f.map_call->data.call.arg_count = 1;
    Type *keys_args[] = {TYPE_STRING};
    Type *keys = type_create_constructed(TYPE_ARRAY, keys_args, 1);
    CHECK(semantic_typed_map_query_record(f.ctx, f.map_call, PGY_TYPED_MAP_QUERY_KEYS, map, keys));
    f.map_call->data.call.arg_count = count;
    Type *generic = type_create_generic("T");
    CHECK(generic != NULL && generic->kind == TYPE_KIND_GENERIC && generic != TYPE_UNKNOWN);
    semantic_context_destroy(produced);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    dispose(&f);
}

static void source_diagnostic(const char *label, const char *source, const char *code, const char *cause)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticResult *result = semantic_analyze(program);
    bool exact = false;
    CHECK(result != NULL && result->error_count > 0);
    if (result != NULL) for (size_t d = 0; d < result->diagnostic_count; d++) {
        const Diagnostic *diagnostic = result->diagnostics[d];
        if (diagnostic->code != NULL && diagnostic->cause_ir != NULL &&
            strcmp(diagnostic->code, code) == 0 && strcmp(diagnostic->cause_ir, cause) == 0) exact = true;
    }
    if (!exact) fprintf(stderr, "source diagnostic %s: expected %s / %s, actual errors=%zu\n",
        label, code, cause, result == NULL ? 0 : result->error_count);
    CHECK(exact);
    semantic_result_destroy(result);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
}

static bool context_has_diagnostic(const SemanticContext *ctx, const char *code, const char *cause)
{
    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        const Diagnostic *d = ctx->diagnostics[i];
        if (d->code != NULL && d->cause_ir != NULL &&
            strcmp(d->code, code) == 0 && strcmp(d->cause_ir, cause) == 0) return true;
    }
    return false;
}
static void checked_result_source(const char *source, size_t statement, bool returned,
    const char *expected, bool seed, bool key_error)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *stmt = ast_block_statement(ast_func_body(decl), statement);
    ASTNode *call = returned ? ast_return_value(stmt) : ast_let_initializer(stmt);
    CHECK(call != NULL && call->type == AST_CALL);
    if (seed) CHECK(ast_call_set_semantic_value_type_name_copy(call, "stale"));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx) == (expected != NULL));
    const char *actual = ast_call_semantic_value_type_name(call);
    CHECK(expected == NULL ? actual == NULL : actual != NULL && strcmp(actual, expected) == 0);
    if (expected == NULL) CHECK(context_has_diagnostic(ctx,
        key_error ? PGY_CODE_SEM_TYPE_MISMATCH : PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        key_error ? PGY_CAUSE_ASSIGNABILITY_CHECK : PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}
static void checked_result_lifecycle(void)
{
    const char *valid = "func Main()->Void { let rows:HashMap<String,Int> = MapNew(); let value=MapGet(rows,\"key\"); }";
    checked_result_source(valid, 1, false, "Int", false, false);
    checked_result_source(valid, 1, false, "Int", true, false);
    checked_result_source("func Main()->Void { let rows:HashMap<String,Long> = MapNew(); let value=MapGet(rows,\"key\"); }", 1, false, "Long", true, false);
    checked_result_source("func Main()->Void { let rows:HashMap<String,Int> = MapNew(); let value=MapGet(rows); }", 1, false, NULL, true, false);
    checked_result_source("func Main()->Void { let rows:Int=1; let value=MapGet(rows,\"key\"); }", 1, false, NULL, true, false);
    checked_result_source("func Main()->Void { let rows:HashMap<String,Int> = MapNew(); let value=MapGet(rows,1); }", 1, false, NULL, true, true);
    checked_result_source("func Query(ref rows:HashMap<String,Unknown>)->Int{return MapGet(rows,\"key\");} func Main()->Void{}", 0, true, NULL, true, false);
    Lexer *lexer = lexer_create(valid);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 1));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    semantic_context_destroy(ctx);
    CHECK(ast_call_semantic_value_type_name(call) != NULL && strcmp(ast_call_semantic_value_type_name(call), "Int") == 0);
    size_t count = call->data.call.arg_count;
    call->data.call.arg_count = 1;
    ctx = semantic_context_create();
    CHECK(!type_check_program(program, ctx));
    CHECK(ast_call_semantic_value_type_name(call) == NULL);
    CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH));
    call->data.call.arg_count = count;
    semantic_context_destroy(ctx);
    CHECK(ast_call_clear_semantic_value_type_name(call));
    CHECK(ast_call_clear_semantic_value_type_name(call));
    CHECK(!ast_call_set_semantic_value_type_name_copy(call, NULL));
    CHECK(!ast_call_set_semantic_value_type_name_copy(call, ""));
    CHECK(ast_call_semantic_value_type_name(call) == NULL);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* These are actual checked producer inputs, not raw publication grants. */
static void reached_producer_source(const char *source, bool expected)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticResult *result = semantic_analyze(program);
    CHECK(result != NULL);
    if (result != NULL && result->success != expected)
        fprintf(stderr, "producer admission mismatch expected=%d source=%s\n", expected, source);
    CHECK(result != NULL && result->success == expected);
    if (!expected && strstr(source, "func Leak(") != NULL) {
        bool borrowed_return = false;
        if (result != NULL) for (size_t i = 0; i < result->diagnostic_count; i++) {
            const Diagnostic *d = result->diagnostics[i];
            borrowed_return |= d->code != NULL && d->cause_ir != NULL &&
                strcmp(d->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0 &&
                strcmp(d->cause_ir, PGY_CAUSE_BORROW_ESCAPE) == 0;
        }
        CHECK(borrowed_return);
    }
    if (expected && strstr(source, "use money;") != NULL) {
        /* Recheck the already composed graph without loading imports again. */
        uint32_t id = ast_node_stable_id(ast_program_statement(program, 1));
        SemanticContext *ctx = semantic_context_create();
        CHECK(type_check_program(program, ctx));
        CHECK(ast_node_stable_id(ast_program_statement(program, 1)) == id);
        semantic_context_destroy(ctx);
    }
    semantic_result_destroy(result);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_generic_member_source(const char *source, bool copy)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *function = ast_program_statement(program, 1);
    ASTNode *call = ast_block_statement(ast_func_body(function), 0);
    CHECK(call != NULL && call->type == AST_CALL);
    ASTNode *member = ast_call_argument(call, 0);
    CHECK(member != NULL && member->type == AST_MEMBER_ACCESS);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, member);
    CHECK(value.ok && value.copy_escape == copy && !value.meta);
    CHECK(value.category == OWNERSHIP_TYPE_COPY_ONLY);
    CHECK(value.origin_ready == copy); /* String field copy landing is still OPEN. */
    CHECK(semantic_param_publication_local_call(ctx, call, 0));
    uint32_t builtin = 0;
    CHECK(ast_call_semantic_callee_builtin_kind(call, &builtin));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_NOT_BUILTIN));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, builtin));
    CHECK(semantic_param_publication_local_call(ctx, call, 0));
    /* Protocol drift is not source shadowing proof: either identity change
     * must invalidate the previously checked canonical-output seal. */
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, ast_node_stable_id(function)));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(function)));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
    CHECK(semantic_param_publication_local_call(ctx, call, 0));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void reached_producer_faults(void)
{
    /* General typed admission cannot require the optional explicit-header
     * scalar-return refinement used by the strict query. Original regressions. */
    reached_producer_source("func add(a:Int,b:Int){return a+b;} func Main(){let sum=add(2,3);Log(sum);}", true);
    reached_producer_source("event OnHit(damage:Int); extern \"C\"{func SDL_Quit();} func Helper()->Int{return 0;} let boot=Helper();", true);
    reached_producer_source("within BattleZone{subject Hero{let hp:Int; action Guard(self) authorized by self{return;}}} zone BattleZone{subject slot hero:Hero authority hero}", true);
    reached_producer_source("use money; func Main()->Void { let amount:Money = MoneyOf(100,\"KRW\"); Log(RenderMoney(amount)); }", true);
    reached_producer_source("func Main()->Void { let ch:Channel<Int> = Channel(1); }", true);
    reached_producer_source("func Main()->Void { let ch:Channel<Int> = Channel(\"bad\"); }", false);
    reached_producer_source("func Main()->Void { let ch:Channel<Int> = Channel(); }", false);
    reached_producer_source("func Main()->Void { let ch:Channel<Int> = Channel(1,2); }", false);
    checked_generic_member_source("class Box<T=Int>{let value:T;} func Inspect(ref b:Box<Int>)->Void { Log(b.value); }", true);
    checked_generic_member_source("class Box<T=Int>{let value:T;} func Inspect(ref b:Box<String>)->Void { Log(b.value); }", false);
    reached_producer_source("async func Worker()->Int{return 1;} async func Main()->Void { let pending:Future<Int> = spawn Worker(); for i in 0..0 { Log(i+await pending); } Log(await pending); }", true);
    reached_producer_source("subject Player { var hp:Int; action Guard(self)->Void within Arena authorized by self{return;} } zone Arena { subject slot hero:Player authority hero } intent Patrol(arena:Arena,hero:Player){step Verify{on:hero.Guard();expect:true;}}", true);
    reached_producer_source("subject Player { var hp:Int; action Guard(self)->Void within Arena authorized by self{return;} } zone Arena { subject slot hero:Player authority hero } intent Patrol(arena:Arena,hero:Player){step Verify{using:missing;on:hero.Guard();expect:true;}}", false);
    reached_producer_source("class Box<T=Int>{let value:T;} func ReadText(text:String)->Int{return StringLength(text);} func Inspect(ref b:Box<String>)->Void { Log(ReadText(b.value)); }", true);
}

static void checked_allocating_reader(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *call = ast_let_initializer(ast_block_statement(
        ast_func_body(ast_program_statement(program, 0)), 0));
    SemanticPublishedValue value = semantic_param_publication_value(ctx, call);
    uint32_t kind = 0;
    CHECK(value.ok && value.category == OWNERSHIP_TYPE_COPY_ONLY);
    CHECK(value.origin_ready && value.copy_escape);
    CHECK(ast_call_semantic_callee_builtin_kind(call, &kind) && kind == BUILTIN_NOT_BUILTIN);
    CHECK(semantic_indexed_string_independent_result(call));
    CHECK(ast_call_set_semantic_callee_is_stdlib(call, false));
    CHECK(!semantic_indexed_string_independent_result(call));
    CHECK(ast_call_set_semantic_callee_is_stdlib(call, true));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_TEXT_BUILDER_FINISH));
    CHECK(!semantic_indexed_string_independent_result(call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    call->data.call.semantic_callee_builtin_kind_set = false;
    CHECK(!semantic_indexed_string_independent_result(call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(ast_program_statement(program, 0))));
    CHECK(!semantic_indexed_string_independent_result(call));
    CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 1));
    CHECK(!semantic_indexed_string_independent_result(call));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 0));
    size_t arity = call->data.call.arg_count;
    call->data.call.arg_count = 1;
    CHECK(!semantic_indexed_string_independent_result(call));
    call->data.call.arg_count = arity;
    uint32_t id = call->stable_id;
    call->stable_id = 0;
    CHECK(!semantic_indexed_string_independent_result(call));
    call->stable_id = id;
    CHECK(semantic_indexed_string_independent_result(call));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_reader_reserved_name(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(!type_check_program(program, ctx));
    CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_REDECLARATION,
        PGY_CAUSE_FUNCTION_BUILTIN_NAME_RESERVED));
    ASTNode *call = ast_let_initializer(ast_block_statement(
        ast_func_body(ast_program_statement(program, 1)), 0));
    /* These spellings are reserved; this is not successful lexical shadowing.
     * Raw identity-drift controls above separately test callee spoofing. */
    CHECK(!semantic_param_publication_ready(ctx));
    CHECK(!semantic_indexed_string_independent_result(call));
    SemanticPublishedValue value = semantic_param_publication_value(ctx, call);
    CHECK(!value.ok && !value.origin_ready && !value.copy_escape);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_input_reader(const char *source, bool result_not_fresh)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 1);
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    unsigned summary = semantic_callable_param_escape_summary(decl, 0, ctx);
    /* Slot access bits and input-retention contracts are distinct facts. */
    CHECK(summary == SLOT_PARAM_SUMMARY_NONE);
    CHECK((summary & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE |
        SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    for (size_t i = 0; i < ast_call_arg_count(call); i++)
        CHECK(semantic_param_publication_local_call(ctx, call, i));
    if (result_not_fresh) {
        SemanticPublishedValue value = semantic_param_publication_value(ctx, call);
        CHECK(value.ok && !value.origin_ready && !value.copy_escape);
        CHECK(!semantic_indexed_string_independent_result(call));
    }
    uint32_t kind = 0;
    CHECK(ast_call_semantic_callee_builtin_kind(call, &kind));
    CHECK(ast_call_set_semantic_callee_is_stdlib(call, false));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_is_stdlib(call, true));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_TEXT_BUILDER_FINISH));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(decl)));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 1));
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 0));
    size_t arity = call->data.call.arg_count;
    call->data.call.arg_count = arity == 1 ? 0 : 1;
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    call->data.call.arg_count = arity;
    CHECK(!semantic_param_publication_local_call(ctx, call, arity));
    ASTNode *argument = ast_call_argument(call, 0);
    uint32_t id = argument->stable_id;
    argument->stable_id = 0;
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    argument->stable_id = id;
    CHECK(semantic_param_publication_local_call(ctx, call, 0));
    call->data.call.semantic_callee_builtin_kind_set = false;
    CHECK(!semantic_param_publication_local_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    CHECK(!semantic_param_publication_begin_value(ctx, call)); /* completed immutable generation */
    CHECK(semantic_param_publication_local_call(ctx, call, 0));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_scalar_return_boundary(const char *source, size_t declaration, bool eligible)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    CHECK(semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl) == eligible);
    if (declaration == 2 && eligible) {
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
        SemanticPublishedValue result = semantic_param_publication_value(ctx, call);
        CHECK(result.ok && !result.origin_ready && !result.copy_escape);
        CHECK(!semantic_indexed_string_independent_result(call));
    }
    if (eligible) {
        unsigned mask = semantic_callable_param_escape_summary(decl, 0, ctx);
        CHECK((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
        CHECK((mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        FuncParam *formal = ast_func_param(decl, 0);
        ParamMode mode = formal->mode;
        formal->mode = PARAM_MODE_MUT_REF;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        formal->mode = mode;
        uint32_t id = formal->stable_id;
        formal->stable_id = 0;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        formal->stable_id = id;
        ASTNode *formal_type = formal->type;
        formal->type = NULL;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        formal->type = formal_type;
        ASTNode *body = decl->data.func_decl.body;
        decl->data.func_decl.body = NULL;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        decl->data.func_decl.body = body;
        ASTNode *result = decl->data.func_decl.return_type;
        decl->data.func_decl.return_type = NULL;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        decl->data.func_decl.return_type = result;
        bool async = decl->is_async_decl;
        decl->is_async_decl = !async;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        decl->is_async_decl = async;
        id = decl->stable_id;
        decl->stable_id = 0;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        decl->stable_id = id;
        id = program->stable_id;
        program->stable_id++;
        CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        program->stable_id = id;
        CHECK(semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    }
    CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, NULL));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void retained_backing_source(const char *body, bool alias, bool slot)
{
    char source[2048];
    int size = snprintf(source, sizeof(source),
        "struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} "
        "extern \"c\"{func Keep(text:String)->Void;} func Retain(text:String)->Void{Keep(text);} "
        "func ReadRecord(ref record:TextRecord,ref output:Slot<String>)->Bool{%s return true;} "
        "func Publish(ref output:Slot<String>)->Bool{let record=TextRecord(Concat(\"heap\",\" text\"));return ReadRecord(record,output);} "
        "func Main()->Void{let output:Slot<String> = ClaimSlot();Log(Publish(output));Log(Read(output));Release(output);}", body);
    CHECK(size > 0 && (size_t)size < sizeof(source));
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(!type_check_program(program, ctx));
    CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    ASTNode *decl = ast_program_statement(program, 4);
    ASTNode *statement = ast_block_statement(ast_func_body(decl), 0);
    ASTNode *observed = alias ? ast_let_initializer(statement) :
        ast_call_argument(statement, slot ? 1 : 0);
    ASTNode *member = ast_call_argument(observed, 0);
    ASTNode *root = ast_member_object(member);
    CHECK(root != NULL && ast_identifier_binding_syntax_id(root) ==
        ast_func_param_stable_id(ast_func_param(decl, 0)));
    CHECK(ast_func_param_stable_id(ast_func_param(decl, 0)) !=
        ast_func_param_stable_id(ast_func_param(decl, 1)));
    SemanticPublishedValue result = semantic_param_publication_value(ctx, observed);
    CHECK(!result.origin_ready && !result.copy_escape);
    CHECK(!semantic_indexed_string_independent_result(observed));
    /* Refusal concerns the original returned backing, not output WRITE alone. */
    unsigned mask = semantic_callable_param_escape_summary(decl, 0, ctx);
    CHECK((mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
        SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) != 0);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_slot_input_contract(void)
{
    Lexer *lexer = lexer_create("func Inspect(ref output:Slot<String>)->Void{Write(output,\"literal\");let text=Read(output);Log(text);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *body = ast_func_body(decl);
    ASTNode *call = ast_block_statement(body, 0);
    ASTNode *receiver = ast_call_argument(call, 0);
    CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    CHECK(!semantic_param_publication_string_argument_read_only(ctx, call, 0));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 1));
    CHECK(semantic_param_publication_binding(ctx, receiver) == ast_func_param_stable_id(ast_func_param(decl, 0)));
    uint32_t binding = ast_identifier_binding_syntax_id(receiver);
    ast_identifier_set_binding_syntax_id(receiver, binding + 1);
    CHECK(semantic_param_publication_binding(ctx, receiver) == 0);
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    ast_identifier_set_binding_syntax_id(receiver, binding);
    ASTNode *local_decl = ast_block_statement(body, 1);
    ASTNode *read = ast_let_initializer(local_decl);
    CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, read, 0));
    SemanticPublishedValue result = semantic_param_publication_value(ctx, read);
    CHECK(result.ok && !result.origin_ready && !result.copy_escape);
    ASTNode *local_use = ast_call_argument(ast_block_statement(body, 2), 0);
    CHECK(semantic_param_publication_binding(ctx, local_use) == ast_node_stable_id(local_decl));
    binding = ast_identifier_binding_syntax_id(local_use);
    ast_identifier_set_binding_syntax_id(local_use, 0);
    CHECK(semantic_param_publication_binding(ctx, local_use) == 0);
    ast_identifier_set_binding_syntax_id(local_use, binding);
    uint32_t kind = 0;
    CHECK(ast_call_semantic_callee_builtin_kind(call, &kind));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_NOT_BUILTIN));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(decl)));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 1));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 0));
    size_t arity = call->data.call.arg_count;
    call->data.call.arg_count = 1;
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    call->data.call.arg_count = arity;
    uint32_t id = receiver->stable_id;
    receiver->stable_id = 0;
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    receiver->stable_id = id;
    CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 1));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void retained_slot_helper_source(void)
{
    const char *source = "struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} "
        "func StoreText(ref output:Slot<String>,ref text:String)->Void{Write(output,text);} "
        "func ReadRecord(ref record:TextRecord,ref output:Slot<String>)->Bool{let observed=Observe(record.text);StoreText(output,observed);return true;} "
        "func Publish(ref output:Slot<String>)->Bool{let record=TextRecord(Concat(\"heap\",\" text\"));return ReadRecord(record,output);} "
        "func Main()->Void{let output:Slot<String> = ClaimSlot();Log(Publish(output));Log(Read(output));Release(output);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(!type_check_program(program, ctx));
    CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    ASTNode *store = ast_program_statement(program, 2);
    CHECK((semantic_callable_param_escape_summary(store, 1, ctx) & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    ASTNode *caller = ast_program_statement(program, 3);
    CHECK((semantic_callable_param_escape_summary(caller, 0, ctx) &
        (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) != 0);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_secure_slot_input_contract(void)
{
    Lexer *lexer = lexer_create("subject Vec2{let x:Int;let y:Int;} func Consume(own s:SecureSlot<Vec2>)->Void{Write(s,Vec2(1,2),s_token);Release(s,s_token);} func Main()->Void{let s:SecureSlot<Vec2> = Vec2(3,7);Consume(s);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 1);
    ASTNode *call = ast_block_statement(ast_func_body(decl), 0);
    ASTNode *token = ast_call_argument(call, 2);
    CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 1));
    CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 3));
    CHECK(semantic_param_publication_value(ctx, token).ok);
    /* The typed call seal is not token authority or fresh backing. Do not
     * invent a syntactic declaration ID for an implicit token issuer. */
    uint32_t binding = ast_identifier_binding_syntax_id(token);
    CHECK(semantic_param_publication_binding(ctx, token) == binding);
    ast_identifier_set_binding_syntax_id(token, binding + 1);
    CHECK(!semantic_param_publication_value(ctx, token).ok);
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    ast_identifier_set_binding_syntax_id(token, binding);
    uint32_t id = token->stable_id;
    token->stable_id = 0;
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    token->stable_id = id;
    ASTNodeType shape = token->type;
    token->type = AST_STRING;
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    token->type = shape;
    id = call->stable_id;
    call->stable_id = 0;
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    call->stable_id = id;
    uint32_t kind = 0;
    CHECK(ast_call_semantic_callee_builtin_kind(call, &kind));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_NOT_BUILTIN));
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    size_t arity = call->data.call.arg_count;
    call->data.call.arg_count = 2;
    CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    call->data.call.arg_count = arity;
    CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 2));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void retained_inout_source(const char *setter, const char *body, bool complete)
{
    char source[3072];
    int length = snprintf(source, sizeof(source),
        "struct TextRecord{text:String} struct Held{text:String} "
        "func Observe(ref text:String)->String{return text;} %s "
        "func ReadRecord(ref record:TextRecord,ref retained:Slot<String>)->Bool{%s return true;} "
        "func Publish(ref retained:Slot<String>)->Bool{let record=TextRecord(Concat(\"heap\",\" text\"));return ReadRecord(record,retained);} "
        "func Main()->Void{let retained:Slot<String> = ClaimSlot();Log(Publish(retained));Log(Read(retained));Release(retained);}", setter, body);
    CHECK(length > 0 && (size_t)length < sizeof(source));
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(!type_check_program(program, ctx));
    CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    bool admitted_call = strstr(body, "var staged:String=\"literal\"") == NULL;
    if (!admitted_call) {
        /* Invalid-program publication robustness, not legal shadowing or an
         * independent ownership falsifier: bind-once remains authoritative. */
        CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_SCOPE_DUPLICATE_SYMBOL));
    }
    ASTNode *store = ast_program_statement(program, 3);
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, store));
    if (admitted_call)
        CHECK((semantic_callable_param_escape_summary(store, 1, ctx) & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    ASTNode *caller = ast_program_statement(program, ast_program_statement_count(program) - 3);
    CHECK((semantic_callable_param_escape_summary(caller, 0, ctx) &
        (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) != 0);
    /* Exact source and caller-owned output identities are distinct; the
     * output's WRITE bit alone does not establish source retention safety. */
    CHECK(ast_func_param_stable_id(ast_func_param(caller, 0)) !=
        ast_func_param_stable_id(ast_func_param(caller, 1)));
    ASTNode *initializer = ast_let_initializer(ast_block_statement(ast_func_body(caller), 0));
    ASTNode *call = ast_block_statement(ast_func_body(caller),
        initializer != NULL && initializer->type == AST_CALL ? 2 : 1);
    if (call != NULL && call->type == AST_IF_STMT) {
        ASTNode *first = ast_block_statement(ast_if_then_branch(call), 0);
        call = first != NULL && first->type == AST_CALL ? first :
            ast_block_statement(ast_func_body(caller), 3);
    }
    CHECK(call != NULL && call->type == AST_CALL);
    SlotFunctionLookup lookup = {ctx, program};
    bool dependent = false;
    CHECK(semantic_param_publication_checked_argument(ctx, call, 0) == admitted_call);
    CHECK(semantic_param_publication_checked_argument(ctx, call, 1) == admitted_call);
    CHECK((semantic_param_publication_callee(ctx, call, 1) != NULL) == admitted_call);
    bool actual_complete = function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent);
    if (actual_complete != complete || (complete && !dependent)) {
        ASTNode *callee = semantic_param_publication_callee(ctx, call, 1);
        FunctionParamFlowSummaryStore *store_view = ctx->function_param_flow_summaries;
        const FunctionParamFlowSummaryEntry *entry = NULL;
        if (store_view != NULL) for (size_t i = 0; i < store_view->count; i++) {
            if (store_view->entries[i].function_id == ast_call_semantic_callee_decl_id(call) &&
                store_view->entries[i].param_index == 1) { entry = &store_view->entries[i]; break; }
        }
        fprintf(stderr, "inout-case setter=%s body=%s call=%u callee=%u source-arg=%u source-binding=%u output-arg=%u output-binding=%u complete=%d expected=%d dependent=%d solving=%d failed=%d\n",
            setter, body, ast_node_stable_id(call), ast_call_semantic_callee_decl_id(call),
            ast_node_stable_id(ast_call_argument(call, 1)),
            semantic_param_publication_binding(ctx, ast_call_argument(call, 1)),
            ast_node_stable_id(ast_call_argument(call, 0)),
            semantic_param_publication_binding(ctx, ast_call_argument(call, 0)),
            actual_complete, complete, dependent,
            ctx->function_param_flow_summaries != NULL && ctx->function_param_flow_summaries->solving,
            ctx->function_param_flow_summaries != NULL && ctx->function_param_flow_summaries->failed);
        fprintf(stderr, "inout-facts checked-source=%d checked-output=%d source-value=%d output-value=%d callee-pointer=%p callee-id=%u entry-state=%d output-count=%zu output-ready=%d output0=%d\n",
            semantic_param_publication_checked_argument(ctx, call, 1),
            semantic_param_publication_checked_argument(ctx, call, 0),
            semantic_param_publication_value(ctx, ast_call_argument(call, 1)).ok,
            semantic_param_publication_value(ctx, ast_call_argument(call, 0)).ok,
            (void *)callee, ast_node_stable_id(callee), entry != NULL ? (int)entry->state : -1,
            entry != NULL ? entry->output_count : 0,
            entry != NULL && entry->output_dependencies_ready,
            entry != NULL && entry->output_count > 0 && entry->output_dependencies != NULL && entry->output_dependencies[0]);
    }
    CHECK(actual_complete == complete);
    if (complete) CHECK(dependent);
    else if (admitted_call) CHECK((semantic_callable_param_escape_summary(store, 1, ctx) & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    else CHECK(semantic_param_publication_callee(ctx, call, 1) == NULL);
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 2, 0, &dependent));
    ASTNode *source_argument = ast_call_argument(call, 1);
    uint32_t argument_id = source_argument->stable_id;
    source_argument->stable_id = 0;
    CHECK(!semantic_param_publication_checked_argument(ctx, call, 1));
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
    source_argument->stable_id = argument_id;
    if (source_argument->type == AST_IDENTIFIER) {
        uint32_t binding = ast_identifier_binding_syntax_id(source_argument);
        ast_identifier_set_binding_syntax_id(source_argument, binding + 1);
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
        ast_identifier_set_binding_syntax_id(source_argument, binding);
    }
    if (initializer != NULL && initializer->type == AST_CALL) {
        ASTNode *root = ast_member_object(ast_call_argument(initializer, 0));
        uint32_t binding = ast_identifier_binding_syntax_id(root);
        CHECK(binding == ast_func_param_stable_id(ast_func_param(caller, 0)));
        CHECK(semantic_param_publication_binding(ctx, root) == binding);
        ast_identifier_set_binding_syntax_id(root, binding + 1);
        CHECK(semantic_param_publication_binding(ctx, root) == 0);
        ast_identifier_set_binding_syntax_id(root, binding);
    }
    uint32_t generation = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, caller));
    CHECK(!semantic_param_publication_checked_argument(ctx, call, 0));
    CHECK(!semantic_param_publication_checked_argument(ctx, call, 1));
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
    program->stable_id = generation;
    dependent = false;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) == complete);
    if (complete) CHECK(dependent);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void disjoint_inout_source(void)
{
    Lexer *lexer = lexer_create("func SetText(inout destination:String,ref text:String,ref unused:String)->Void{destination=text;} func Main()->Void{var destination:String=\"\";SetText(destination,\"literal\",\"unused\");Log(destination);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 1);
    SlotFunctionLookup lookup = {ctx, program};
    bool dependent = false;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
    dependent = true;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 2, 0, &dependent));
    CHECK(!dependent);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void high_ordinal_inout_relations(void)
{
    /* One checked source crosses both source and output word boundaries;
     * unrelated columns must not inherit a neighbouring bit's dependency. */
    char source[8192];
    size_t used = 0;
    int added = snprintf(source, sizeof(source), "func Cross(");
    CHECK(added > 0); used = (size_t)added;
    for (size_t i = 0; i < 68; i++) {
        added = snprintf(source + used, sizeof(source) - used, "%s%s p%zu:String",
            i ? "," : "", i == 0 || i == 65 ? "inout" : "ref", i);
        CHECK(added > 0 && (size_t)added < sizeof(source) - used);
        used += (size_t)added;
    }
    added = snprintf(source + used, sizeof(source) - used,
        ")->Void{p0=p66;p65=p1;} func Main()->Void{var first:String=\"\";var second:String=\"\";Cross(");
    CHECK(added > 0 && (size_t)added < sizeof(source) - used); used += (size_t)added;
    for (size_t i = 0; i < 68; i++) {
        added = snprintf(source + used, sizeof(source) - used, "%s%s", i ? "," : "",
            i == 0 ? "first" : i == 65 ? "second" : "\"literal\"");
        CHECK(added > 0 && (size_t)added < sizeof(source) - used); used += (size_t)added;
    }
    added = snprintf(source + used, sizeof(source) - used, ");Log(first);Log(second);}");
    CHECK(added > 0 && (size_t)added < sizeof(source) - used);
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 2);
    CHECK(call != NULL && call->type == AST_CALL);
    SlotFunctionLookup lookup = {ctx, program};
    bool dependent = false;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 66, 0, &dependent) && dependent);
    dependent = true;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 66, 65, &dependent) && !dependent);
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 65, &dependent) && dependent);
    dependent = true;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && !dependent);
    dependent = true;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 64, 0, &dependent) && !dependent);
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 68, 0, &dependent));
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 66, 68, &dependent));
    ASTNode *argument = ast_call_argument(call, 66);
    uint32_t argument_id = argument->stable_id;
    argument->stable_id = 0;
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 66, 0, &dependent));
    argument->stable_id = argument_id;
    FuncParam *param = ast_func_param(decl, 66);
    uint32_t param_id = ast_func_param_stable_id(param);
    param->stable_id = 0;
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 66, 0, &dependent));
    param->stable_id = param_id;
    uint32_t generation = program->stable_id;
    program->stable_id++;
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 66, 0, &dependent));
    program->stable_id = generation;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 66, 0, &dependent) && dependent);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_map_result_projection(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
    ASTNode *receiver = ast_call_argument(call, 0);
    CHECK(semantic_typed_map_query_result_independent(ctx, call));
    uint32_t kind = 0;
    CHECK(ast_call_semantic_callee_builtin_kind(call, &kind));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_TEXT_BUILDER_FINISH));
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    call->data.call.semantic_callee_builtin_kind_set = false;
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    uint32_t binding = ast_identifier_binding_syntax_id(receiver);
    CHECK(binding == ast_func_param_stable_id(ast_func_param(decl, 0)));
    ast_identifier_set_binding_syntax_id(receiver, binding + 1);
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    ast_identifier_set_binding_syntax_id(receiver, binding);
    uint32_t id = call->stable_id;
    call->stable_id = 0;
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    call->stable_id = id;
    id = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    program->stable_id = id;
    CHECK(semantic_typed_map_query_result_independent(ctx, call));
    CHECK(!semantic_typed_map_query_result_independent(ctx, NULL));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_parameter_source_kind(const char *source, size_t declaration, bool expected_anchor)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    unsigned original_mask = semantic_callable_param_escape_summary(decl, 0, ctx);
    bool anchored = !expected_anchor;
    CHECK(semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    CHECK(anchored == expected_anchor);
    CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == original_mask);
    if (expected_anchor) CHECK((original_mask & SLOT_PARAM_SUMMARY_READ) != 0);
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, NULL, 0, &anchored));
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 1, &anchored));
    FuncParam *formal = ast_func_param(decl, 0);
    ParamMode mode = formal->mode;
    formal->mode = PARAM_MODE_OWN;
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    formal->mode = mode;
    uint32_t id = formal->stable_id;
    formal->stable_id = 0;
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    formal->stable_id = id;
    ASTNode *type = formal->type;
    formal->type = NULL;
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    formal->type = type;
    ASTNode *body = decl->data.func_decl.body;
    decl->data.func_decl.body = NULL;
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    decl->data.func_decl.body = body;
    id = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    program->stable_id = id;
    CHECK(semantic_param_publication_parameter_is_anchored_handle(ctx, decl, 0, &anchored));
    CHECK(anchored == expected_anchor);
    CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == original_mask);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_readonly_mutual_recursion(void)
{
    Lexer *lexer = lexer_create("func First(ref values:HashMap<String,Int>,depth:Int)->Int{if depth<=0{return MapGet(values,\"entry\");}return Second(values,depth-1);} func Second(ref values:HashMap<String,Int>,depth:Int)->Int{if depth<=0{return MapGet(values,\"entry\");}return First(values,depth-1);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    CHECK(semantic_param_publication_ready(ctx));
    ASTNode *first = ast_program_statement(program, 0);
    ASTNode *second = ast_program_statement(program, 1);
    unsigned first_mask = semantic_callable_param_escape_summary(first, 0, ctx);
    unsigned second_mask = semantic_callable_param_escape_summary(second, 0, ctx);
    unsigned escapes = SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    CHECK((first_mask & escapes) == 0 && (second_mask & escapes) == 0);
    ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(first), 1));
    SlotFunctionLookup lookup = {ctx, program};
    CHECK(semantic_param_publication_callee(ctx, call, 0) == second);
    CHECK(function_param_flow_summary_for_call(&lookup, call, 0) == second_mask);
    uint32_t id = program->stable_id;
    program->stable_id++;
    CHECK(function_param_flow_summary_for_call(&lookup, call, 0) == SLOT_PARAM_SUMMARY_ALL);
    program->stable_id = id;
    CHECK(function_param_flow_summary_for_call(&lookup, call, 0) == second_mask);
    CHECK(semantic_callable_param_escape_summary(first, 0, ctx) == first_mask);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}


static void checked_no_output_header(const char *source, size_t declaration)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    CHECK(semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    SlotFunctionLookup lookup = {ctx, program};
    unsigned backing = function_param_flow_summary_demand(&lookup, decl, 0);
    unsigned mask = semantic_callable_param_escape_summary(decl, 0, ctx);
    /* Callable permission consumes the same raw truth, not a Type projection
       zero. Nonphysical results retain their returned-backing dependence. */
    CHECK(mask == backing);
    CHECK((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
    CHECK((backing & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
    ASTNode *value = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
    CHECK(!semantic_param_publication_value(ctx, value).origin_ready);
    CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, value));
    FuncParam *formal = ast_func_param(decl, 0);
    ParamMode mode = formal->mode;
    formal->mode = PARAM_MODE_OWN;
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    formal->mode = mode;
    uint32_t id = formal->stable_id;
    formal->stable_id = 0;
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    formal->stable_id = id;
    ASTNode *type = formal->type;
    formal->type = NULL;
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    formal->type = type;
    ASTNode *body = decl->data.func_decl.body;
    decl->data.func_decl.body = NULL;
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    decl->data.func_decl.body = body;
    ASTNode *result = decl->data.func_decl.return_type;
    decl->data.func_decl.return_type = NULL;
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    decl->data.func_decl.return_type = result;
    id = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    program->stable_id = id;
    CHECK(semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == mask);
    CHECK(function_param_flow_summary_demand(&lookup, decl, 0) == backing);
    CHECK(!semantic_param_publication_routine_no_output_header_ready(ctx, NULL));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_scalar_local_store(void)
{
    Lexer *lexer = lexer_create("func CountText(text:String)->Bool{var cursor:Int=0;while cursor<StringLength(text){cursor=cursor+1;}return cursor>0;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    CHECK(semantic_param_publication_routine_no_output_header_ready(ctx, decl));
    CHECK(semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    ASTNode *body = ast_func_body(decl);
    ASTNode *local = ast_block_statement(body, 0);
    ASTNode *loop = ast_block_statement(body, 1);
    ASTNode *assignment = ast_block_statement(ast_while_body(loop), 0);
    ASTNode *target = ast_assignment_target(assignment);
    ASTNode *value = ast_assignment_value(assignment);
    CHECK(semantic_param_publication_binding(ctx, target) == ast_node_stable_id(local));
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx, target));
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx, value));
    SlotFunctionLookup lookup = {ctx, program};
    (void)function_param_flow_summary_demand(&lookup, decl, 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(index != NULL && !index->mutable_output_required);
    CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, NULL));
    uint32_t binding = ast_identifier_binding_syntax_id(target);
    ast_identifier_set_binding_syntax_id(target, binding + 1);
    CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, target));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    ast_identifier_set_binding_syntax_id(target, binding);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    /* Descendant mutations leave the top-level statement identity unchanged;
     * the cached local-store certificate must still reject stale evidence. */
    ASTNodeType local_kind = local->type;
    local->type = AST_RETURN;
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    local->type = local_kind;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    ASTNodeType value_kind = value->type;
    value->type = AST_STRING;
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    value->type = value_kind;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    uint32_t id = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, value));
    program->stable_id = id;
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx, value));
    CHECK((semantic_callable_param_escape_summary(decl, 0, ctx) &
        (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_local_assignment_replacement(const char *source, size_t declaration)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    ASTNode *body = ast_func_body(decl);
    ASTNode *local = ast_block_statement(body, 0);
    ASTNode *assignment = ast_block_statement(body, 1);
    ASTNode *target = ast_assignment_target(assignment), *value = ast_assignment_value(assignment);
    CHECK(semantic_param_publication_assignment_replacement(ctx, assignment));
    SlotFunctionLookup lookup = {ctx, program};
    (void)function_param_flow_summary_demand(&lookup, decl, 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(index != NULL && !index->mutable_output_required);
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    uint32_t binding = ast_identifier_binding_syntax_id(target);
    ASTNodeType target_shape = target->type, value_shape = value->type, local_shape = local->type;
    PublishedNode *published = NULL;
    for (size_t p = 0; p < ctx->function_param_publications->node_capacity; p++)
        if (ctx->function_param_publications->nodes[p].node == assignment) published = &ctx->function_param_publications->nodes[p];
    CHECK(published != NULL);
    char *typed_name = assignment->data.assignment.semantic_binding_type_name;
    for (unsigned fault = 0; fault < 10; fault++) {
        if (fault == 0) local->data.let_decl.is_alias = true;
        if (fault == 1) local->type = AST_RETURN;
        if (fault == 2) ast_identifier_set_binding_syntax_id(target, ast_func_param_stable_id(ast_func_param(decl, 0)));
        if (fault == 3) target->type = AST_ARRAY_ACCESS;
        if (fault == 4) value->type = AST_CHANNEL_SEND;
        if (fault == 5 && published != NULL) published->assignment_replacement = false;
        if (fault == 6) assignment->stable_id++;
        if (fault == 7) assignment->data.assignment.semantic_binding_type_name = "Slot<Int>";
        if (fault == 8) value->stable_id++;
        if (fault == 9) target->stable_id++;
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
        CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        local->type = local_shape; local->data.let_decl.is_alias = false;
        ast_identifier_set_binding_syntax_id(target, binding);
        target->type = target_shape; value->type = value_shape;
        if (published != NULL) published->assignment_replacement = true;
        if (fault == 6) assignment->stable_id--;
        assignment->data.assignment.semantic_binding_type_name = typed_name;
        if (fault == 8) value->stable_id--;
        if (fault == 9) target->stable_id--;
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
        CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_slot_assignment_operation(bool pin_view)
{
    const char *source = pin_view ?
        "func Probe(text:String)->Void{let slot:Slot<Int> =ClaimSlot();pin slot as view:WriteView<Int>{view=1;}Release(slot);}" :
        "func Probe(text:String)->Void{let slot:Slot<String> =ClaimSlot();slot=text;Release(slot);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *assignment = ast_block_statement(ast_func_body(decl), 1);
    if (pin_view) {
        CHECK(ast_let_is_alias(ast_block_statement(assignment, 0)));
        assignment = ast_block_statement(assignment, 1);
    }
    CHECK(assignment != NULL && assignment->type == AST_ASSIGNMENT);
    CHECK(!semantic_param_publication_assignment_replacement(ctx, assignment));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_shared_backing_validation(void)
{
    const char *source =
        "func Leaf(ref text:String)->Bool{return StringLength(text)>0;}"
        "func Left(ref text:String)->Bool{return Leaf(text);}"
        "func Right(ref text:String)->Bool{return Leaf(text);}"
        "func Diamond(ref text:String)->Bool{return Left(text)&&Right(text);}"
        "func CycleA(ref text:String,depth:Int)->Bool{if depth==0{return Leaf(text);}return CycleB(text,depth-1);}"
        "func CycleB(ref text:String,depth:Int)->Bool{if depth==0{return Leaf(text);}return CycleA(text,depth-1);}"
        "func WideA(ref text:String)->Bool{return Diamond(text)&&CycleA(text,1);}"
        "func WideB(ref text:String)->Bool{return WideA(text)&&Diamond(text);}"
        "func WideC(ref text:String)->Bool{return WideB(text)&&CycleB(text,1);}"
        "func Root(ref text:String)->Bool{return WideC(text)&&WideB(text);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    SlotFunctionLookup lookup = {ctx, program};
    for (size_t r = 0; r < 10; r++) {
        ASTNode *decl = ast_program_statement(program, r);
        (void)function_param_flow_summary_demand(&lookup, decl, 0);
        unsigned effects = 0;
        bool outputs[2] = {false, false}, pending = true;
        CHECK(function_param_flow_returned_backing_effects(ctx->function_param_flow_summaries,
            decl, 0, &effects, outputs, ast_func_param_count(decl), &pending, false));
        CHECK(!pending);
    }
    ASTNode *leaf = ast_program_statement(program, 0);
    ASTNode *diamond = ast_program_statement(program, 3);
    ASTNode *cycle = ast_program_statement(program, 4);
    ASTNode *root = ast_program_statement(program, 9);
    ASTNode *leaf_argument = ast_call_argument(ast_binary_left(ast_return_value(
        ast_block_statement(ast_func_body(leaf), 0))), 0);
    uint32_t binding = ast_identifier_binding_syntax_id(leaf_argument);
    for (unsigned query = 0; query < 2; query++) {
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, root));
        ast_identifier_set_binding_syntax_id(leaf_argument, binding + 1);
        CHECK(!function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, diamond));
        CHECK(!function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, cycle));
        CHECK(!function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, root));
        ast_identifier_set_binding_syntax_id(leaf_argument, binding);
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, diamond));
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, cycle));
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, root));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_zero_argument_output_graph(void)
{
    Lexer *lexer = lexer_create("func Zero()->Int{return 0;} func Probe(ref text:String)->Bool{return StringLength(text)>Zero();}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *zero = ast_program_statement(program, 0);
    ASTNode *decl = ast_program_statement(program, 1);
    ASTNode *call = ast_binary_right(ast_return_value(ast_block_statement(ast_func_body(decl), 0)));
    CHECK(semantic_param_publication_direct_callee(ctx, call) == zero);
    CHECK(semantic_param_publication_callee(ctx, call, 0) == NULL);
    SlotFunctionLookup lookup = {ctx, program};
    CHECK((function_param_flow_summary_demand(&lookup, decl, 0) &
        (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    uint32_t declaration = ast_call_semantic_callee_decl_id(call);
    ASTNode *body = ast_func_body(zero), *return_type = ast_func_return_type(zero);
    ASTNode *target = ast_call_callee(call);
    for (unsigned fault = 0; fault < 7; fault++) {
        if (fault == 0) ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(decl));
        if (fault == 1) ast_call_set_semantic_callee_value_binding_id(call, 1);
        if (fault == 2) call->data.call.arg_count = 1;
        if (fault == 3) target->stable_id++;
        if (fault == 4) zero->data.func_decl.body = ast_func_body(decl);
        if (fault == 5) zero->data.func_decl.return_type = NULL;
        if (fault == 6) ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_STRING_LENGTH);
        CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        ast_call_set_semantic_callee_decl_id(call, declaration);
        ast_call_set_semantic_callee_value_binding_id(call, 0);
        call->data.call.arg_count = 0;
        if (fault == 3) target->stable_id--;
        zero->data.func_decl.body = body;
        zero->data.func_decl.return_type = return_type;
        ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_NOT_BUILTIN);
        CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    }
    FunctionParamPublicationStore *publication = ctx->function_param_publications;
    PublishedNode *value = NULL;
    for (size_t i = 0; i < publication->node_capacity; i++)
        if (publication->nodes[i].node == call) value = &publication->nodes[i];
    CHECK(value != NULL && value->physical_scalar);
    if (value != NULL) {
        value->physical_scalar = false;
        CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        value->physical_scalar = true;
        CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        value->value_present = false;
        CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        value->value_present = true;
        CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_unclosed_output_graph(const char *source, size_t declaration)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    SlotFunctionLookup lookup = {ctx, program};
    (void)function_param_flow_summary_demand(&lookup, decl, 0);
    CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_dead_output_subtree(void)
{
    Lexer *lexer = lexer_create("struct Wrapped{text:String} func Inspect(text:String)->Void{return;let held=Wrapped(text);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 1);
    ASTNode *dead = ast_block_statement(ast_func_body(decl), 1);
    bool reachable = true;
    CHECK(semantic_param_publication_execution(ctx, dead, &reachable) && !reachable);
    CHECK(!semantic_param_publication_checked_call(ctx, ast_let_initializer(dead)));
    SlotFunctionLookup lookup = {ctx, program};
    CHECK((function_param_flow_summary_demand(&lookup, decl, 0) &
        (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(index != NULL && !index->mutable_output_required);
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    PublishedNode *execution = NULL;
    FunctionParamPublicationStore *publication = ctx->function_param_publications;
    for (size_t i = 0; i < publication->node_capacity; i++)
        if (publication->nodes[i].node == dead) execution = &publication->nodes[i];
    CHECK(execution != NULL);
    if (execution != NULL) {
        execution->reachable = true;
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
        CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        execution->reachable = false;
        execution->execution_present = false;
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
        CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        execution->execution_present = true;
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
        CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_nonlocal_scalar_store(const char *source, size_t declaration)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    SlotFunctionLookup lookup = {ctx, program};
    (void)function_param_flow_summary_demand(&lookup, decl, 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(index != NULL && index->mutable_output_required);
    /* A physical value is not proof its destination is an owned scalar local. */
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_copy_category_landing(void)
{
    Lexer *lexer = lexer_create("class Box<T=Int>{let value:T;} func Leak(ref b:Box<String>)->String { return b.value; }");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    bool compiled = type_check_program(program, ctx);
    ASTNode *member = ast_return_value(ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 0));
    SemanticPublishedValue value = semantic_param_publication_value(ctx, member);
    /* Positive D1 category; not a rule that plain String field returns are
     * illegal. This stage has no fresh copy landing/loan lifetime certificate. */
    CHECK(value.ok && value.category == OWNERSHIP_TYPE_COPY_ONLY);
    CHECK(!value.origin_ready && !value.copy_escape);
    CHECK(!compiled && context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_loop_execution_source(const char *source, bool reachable)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *loop = ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 0);
    ASTNode *body = ast_for_body(loop);
    bool actual = !reachable;
    /* A proven dead body is the cut; a live body checks its real statement. */
    CHECK(semantic_param_publication_execution(ctx,
        reachable ? ast_block_statement(body, 0) : body, &actual));
    CHECK(actual == reachable);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_publication_faults(void)
{
    Fixture f = fixture();
    ASTNode *argument = ast_call_argument(f.map_call, 0);
    CHECK(!semantic_param_publication_value(f.ctx, argument).ok); /* pending */
    CHECK(!semantic_param_publication_checked_argument(f.ctx, f.map_call, 0));
    /* Reclassification protocol: the producer entry invalidates prior meta
     * and primitive-copy facts before the same node's new checked result. */
    CHECK(semantic_param_publication_record_value(f.ctx, f.reader, TYPE_INT, true));
    CHECK(semantic_param_publication_begin_value(f.ctx, f.reader));
    CHECK(!semantic_param_publication_record_value(f.ctx, f.reader, TYPE_UNKNOWN, false));
    CHECK(!semantic_param_publication_record_value(f.ctx, f.reader, NULL, false));
    CHECK(semantic_param_publication_record_value(f.ctx, f.reader, TYPE_STRING, false));
    ASTNode *dead = ast_block_statement(ast_func_body(f.reader), 0);
    CHECK(semantic_param_publication_record_execution(f.ctx, dead, false));
    CHECK(register_map(&f) && register_reader(&f) && register_caller(&f));
    CHECK(register_body_dependency(&f));
    CHECK(semantic_param_publication_finalize(f.ctx));
    SemanticPublishedValue value = semantic_param_publication_value(f.ctx, argument);
    CHECK(value.ok && !value.copy_escape && !value.meta); /* HashMap is tracked */
    value = semantic_param_publication_value(f.ctx, ast_call_argument(f.map_call, 1));
    CHECK(value.ok && value.copy_escape && value.origin_ready); /* Literal issuer, not String category alone. */
    CHECK(value.category == OWNERSHIP_TYPE_COPY_ONLY);
    ASTNode *literal = ast_call_argument(f.map_call, 1);
    ASTNodeType shape = literal->type;
    literal->type = AST_ARRAY_ACCESS;
    CHECK(!semantic_param_publication_value(f.ctx, literal).ok);
    CHECK(!semantic_param_publication_checked_argument(f.ctx, f.map_call, 1));
    literal->type = shape;
    CHECK(semantic_param_publication_value(f.ctx, literal).origin_ready);
    value = semantic_param_publication_value(f.ctx, f.reader);
    CHECK(value.ok && !value.copy_escape && !value.meta);
    CHECK(!semantic_param_publication_value(f.ctx, f.caller).ok); /* missing */
    bool reachable = true;
    CHECK(semantic_param_publication_execution(f.ctx, dead, &reachable) && !reachable);
    CHECK(!semantic_param_publication_execution(f.ctx, f.reader, &reachable));
    CHECK(semantic_param_publication_checked_argument(f.ctx, f.map_call, 0));
    CHECK(!semantic_param_publication_checked_argument(f.ctx, f.map_call, 2));
    CHECK(!semantic_param_publication_local_call(f.ctx, f.map_call, 0)); /* MapGet is not that contract */
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == f.reader);
    CHECK(semantic_param_publication_callee(f.ctx, f.map_call, 0) == NULL);
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 1) == NULL);
    FuncParam *formal = ast_func_param(f.reader, 0);
    ParamMode mode = formal->mode;
    formal->mode = PARAM_MODE_OWN;
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    formal->mode = mode;
    uint32_t formal_id = formal->stable_id;
    formal->stable_id++;
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    formal->stable_id = formal_id;
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == f.reader);
    ASTNode *body = f.reader->data.func_decl.body;
    f.reader->data.func_decl.body = NULL;
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    f.reader->data.func_decl.body = body;
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.forward_call, ast_node_stable_id(f.caller)));
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.forward_call, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(f.forward_call, ast_node_stable_id(f.caller)));
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    CHECK(ast_call_set_semantic_callee_decl_id(f.forward_call, ast_node_stable_id(f.reader)));
    size_t param_count = f.reader->data.func_decl.param_count;
    f.reader->data.func_decl.param_count = 0;
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    f.reader->data.func_decl.param_count = param_count;
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == f.reader);
    SlotFunctionLookup lookup = {f.ctx, f.program};
    CHECK(function_param_flow_summary_for_call(&lookup, f.forward_call, 1) == SLOT_PARAM_SUMMARY_ALL);
    uint32_t saved = f.program->stable_id;
    f.program->stable_id++;
    CHECK(!semantic_param_publication_value(f.ctx, argument).ok);
    CHECK(!semantic_param_publication_checked_argument(f.ctx, f.map_call, 0));
    CHECK(semantic_param_publication_callee(f.ctx, f.forward_call, 0) == NULL);
    f.program->stable_id = saved;
    saved = argument->stable_id;
    argument->stable_id++;
    CHECK(!semantic_param_publication_value(f.ctx, argument).ok);
    CHECK(!semantic_param_publication_checked_argument(f.ctx, f.map_call, 0));
    argument->stable_id = saved;
    CHECK(semantic_param_publication_checked_argument(f.ctx, f.map_call, 0));
    dispose(&f);
}

static void storage_root_faults(void)
{
    /* Raw AST protocol controls only. The supplied IDs do not establish that
     * this tree came from source typing or that any storage is exclusively owned. */
    Fixture f = fixture();
    ASTNode *identifier = ast_call_argument(f.map_call, 0);
    uint32_t binding = ast_identifier_binding_syntax_id(identifier);
    ast_identifier_set_binding_syntax_id(identifier,
        ast_func_param_stable_id(ast_func_param(f.reader, 0)));
    CHECK(ast_identifier_binding_syntax_id(identifier) != 0);
    const ASTNode *root = NULL;
    CHECK(slot_call_argument_storage_root(identifier, &root) && root == identifier);
    ASTNode *member = ast_create_member_access(identifier, "entries");
    ASTNode *array = ast_create_array_access(member, ast_create_identifier("index"));
    CHECK(slot_call_argument_storage_root(member, &root) && root == identifier);
    CHECK(slot_call_argument_storage_root(array, &root) && root == identifier);
    ASTNode *deep = array;
    for (size_t i = 0; i < 5000; i++) deep = ast_create_member_access(deep, "next");
    CHECK(slot_call_argument_storage_root(deep, &root) && root == identifier);
    while (deep != array) {
        ASTNode *next = deep->data.member.object;
        deep->data.member.object = NULL;
        ast_destroy(deep);
        deep = next;
    }
    f.map_call->data.call.arguments[0] = array;
    SlotFunctionLookup lookup = {f.ctx, f.program};
    SlotSummaryOrigin origin = {f.reader, 0, "rows", NULL, 0};
    ASTNode *points[] = {f.map_call};
    CHECK(!slot_call_argument_storage_root(NULL, &root) && root == NULL);
    f.map_call->data.call.arguments[0] = NULL;
    CHECK(slot_param_summary_in_program_points(points, 1, "rows", &lookup, &origin) == SLOT_PARAM_SUMMARY_ALL);
    f.map_call->data.call.arguments[0] = array;
    uint32_t id = identifier->stable_id;
    identifier->stable_id = 0;
    CHECK(!slot_call_argument_storage_root(array, &root) && root == NULL);
    CHECK(slot_param_summary_in_program_points(points, 1, "rows", &lookup, &origin) == SLOT_PARAM_SUMMARY_ALL);
    identifier->stable_id = id;
    ast_identifier_set_binding_syntax_id(identifier, 0);
    CHECK(ast_identifier_binding_syntax_id(identifier) == 0);
    CHECK(!slot_call_argument_storage_root(array, &root) && root == NULL);
    CHECK(slot_param_summary_in_program_points(points, 1, "rows", &lookup, &origin) == SLOT_PARAM_SUMMARY_ALL);
    ast_identifier_set_binding_syntax_id(identifier,
        ast_func_param_stable_id(ast_func_param(f.reader, 0)));
    CHECK(ast_identifier_binding_syntax_id(identifier) != 0);
    member->data.member.object = NULL;
    CHECK(!slot_call_argument_storage_root(array, &root) && root == NULL);
    CHECK(slot_param_summary_in_program_points(points, 1, "rows", &lookup, &origin) == SLOT_PARAM_SUMMARY_ALL);
    member->data.member.object = array;
    CHECK(!slot_call_argument_storage_root(array, &root) && root == NULL);
    CHECK(slot_param_summary_in_program_points(points, 1, "rows", &lookup, &origin) == SLOT_PARAM_SUMMARY_ALL);
    member->data.member.object = identifier;
    CHECK(slot_call_argument_storage_root(array, &root) && root == identifier);
    f.map_call->data.call.arguments[0] = identifier;
    member->data.member.object = NULL;
    ast_destroy(array);
    ast_identifier_set_binding_syntax_id(identifier, binding);
    CHECK(ast_identifier_binding_syntax_id(identifier) == binding);
    dispose(&f);
}

static void checked_cast_and_consumed_edges(void)
{
    Lexer *lexer = lexer_create("func CastRead(value:Int)->Long{let other:Int=value;return value as Long;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *cast = ast_return_value(ast_block_statement(ast_func_body(decl), 1));
    ASTNode *operand = ast_cast_operand(cast);
    ASTNode *other = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    CHECK(cast != NULL && cast->type == AST_CAST);
    CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
    FunctionParamFlowProgramPointIndex *index = NULL;
    FunctionParamFlowProgramPointIndex *rechecked = NULL;
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK(ctx->function_param_flow_summaries != NULL);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    if (ctx->function_param_flow_summaries == NULL || index == NULL) {
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
        return;
    }
    CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, NULL));
    uint32_t cast_id = cast->stable_id;
    cast->stable_id++;
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
    cast->stable_id = cast_id;
    CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
    char *target = cast->data.cast.target_type;
    cast->data.cast.target_type = "Int";
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
    cast->data.cast.target_type = target;
    CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
    cast->data.cast.operand = other;
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
    cast->data.cast.operand = operand;
    CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
    uint32_t id = operand->stable_id;
    operand->stable_id++;
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
    operand->stable_id = id;
    CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
    uint32_t binding = ast_identifier_binding_syntax_id(operand);
    ast_identifier_set_binding_syntax_id(operand, binding + 1);
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
    ast_identifier_set_binding_syntax_id(operand, binding);
    CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
    id = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
    program->stable_id = id;
    CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
    CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);

    lexer = lexer_create("func Main()->Void{let text=\"value\";let observed=text as String;Log(observed);}");
    parser = parser_create(lexer); program = parser_parse_program(parser);
    ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    cast = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 1));
    CHECK(!semantic_param_publication_checked_numeric_cast(ctx, cast));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);

    const char *sources[] = {
        "func ConsumeValue(value:Int)->Void{let channel:Channel<Int> = Channel(1);channel <- value;}",
        "func ConsumeValue(value:Int)->Int{let rows:Array<Int> = [value,1];return ArrayLength(rows);}",
        "func ConsumeValue(value:Int)->Int{let rows=(value,1);return value;}"
    };
    for (size_t shape = 0; shape < 3; shape++) {
        lexer = lexer_create(sources[shape]); parser = parser_create(lexer);
        program = parser_parse_program(parser); ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        CHECK(type_check_program(program, ctx));
        decl = ast_program_statement(program, 0);
        ASTNode *body = ast_func_body(decl);
        ASTNode *parent = shape == 0 ? ast_block_statement(body, 1)
            : ast_let_initializer(ast_block_statement(body, 0));
        index = NULL;
        (void)function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK(ctx->function_param_flow_summaries != NULL);
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
        if (ctx->function_param_flow_summaries == NULL || index == NULL) {
            semantic_context_destroy(ctx);
            ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
            continue;
        }
        CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
        uint32_t parent_id = parent->stable_id;
        ASTNode **edge = shape == 0 ? &parent->data.channel_send.value
            : shape == 1 ? &parent->data.array_literal.elements[0]
                         : &parent->data.tuple_literal.elements[0];
        ASTNode *saved = *edge;
        *edge = shape == 0 ? ast_channel_send_channel(parent)
            : shape == 1 ? ast_array_literal_element(parent, 1)
                         : ast_tuple_literal_element(parent, 1);
        CHECK(parent->stable_id == parent_id);
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
        *edge = saved;
        CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
        if (shape == 0) {
            ASTNode *channel = parent->data.channel_send.channel;
            parent->data.channel_send.channel = saved;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
            parent->data.channel_send.channel = channel;
        } else {
            size_t *count = shape == 1 ? &parent->data.array_literal.count
                                      : &parent->data.tuple_literal.count;
            size_t saved_count = *count;
            *count = saved_count - 1;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
            *count = saved_count;
        }
        CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_nested_call_currentness(void)
{
    const char *sources[] = {
        "struct TextRecord{text:String;} struct TextPayload{text:String;} func MakeText(ref record:TextRecord)->String{return Concat(\"stable\",\" value\");} func Inspect(ref record:TextRecord)->Bool{let payload=TextPayload(MakeText(record));return StringLength(payload.text)>0;}",
        "struct TextRecord{text:String;} func MakeText(ref record:TextRecord)->String{return Concat(\"stable\",\" value\");} func Inspect(ref record:TextRecord)->Bool{let observed=MakeText(record);return StringLength(observed)>0;}",
        "func Touch()->Void{} func Inspect(ref text:String)->Bool{Touch();return StringLength(text)>0;}",
        "struct TextPayload{text:String;count:Int;} func Inspect(ref values:HashMap<String,String>)->Bool{let payload=TextPayload(MapGet(values,\"key\"),3);return StringLength(payload.text)>0;}"
    };
    for (size_t c = 0; c < 4; c++) {
        Lexer *lexer = lexer_create(sources[c]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        CHECK(type_check_program(program, ctx));
        if (program == NULL || ctx->has_error) goto cleanup;
        ASTNode *routine = ast_program_statement(program, c == 0 ? 3 : c == 1 ? 2 : 1);
        ASTNode *parent = c == 0 || c == 3 ? ast_let_initializer(ast_block_statement(ast_func_body(routine), 0)) : NULL;
        ASTNode *call = parent != NULL ? ast_call_argument(parent, 0) :
            c == 1 ? ast_let_initializer(ast_block_statement(ast_func_body(routine), 0)) : ast_block_statement(ast_func_body(routine), 0);
        CHECK(call != NULL && call->type == AST_CALL);
        if (call == NULL || call->type != AST_CALL) goto cleanup;
        bool depends = false, retains = true;
        if (c == 3) CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
        else CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
        if (parent != NULL) CHECK(semantic_param_publication_checked_result_relation(ctx, parent, 0, &depends, &retains) && depends && !retains);
        CHECK(semantic_param_publication_value(ctx, call).ok);
        unsigned baseline = function_param_flow_summary_for_param(ctx, routine, 0);
        CHECK(baseline == SLOT_PARAM_SUMMARY_NONE);
        FunctionParamFlowProgramPointIndex *index = NULL, *rechecked = NULL;
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &index));
        if (index == NULL) goto cleanup;
        ASTNode *target = ast_call_callee(call), *argument = ast_call_argument(call, 0);
        for (unsigned fault = 0; fault < 14; fault++) {
            if (argument == NULL && fault >= 7 && fault <= 9) continue;
            /* Only an issued result relation seals this AST result-name slot.
               Nonrelation user/Void calls still require their actual call seal. */
            if (fault == 10 && parent == NULL) continue;
            uint32_t decl = call->data.call.semantic_callee_decl_id;
            uint32_t kind = call->data.call.semantic_callee_builtin_kind;
            uint32_t binding = call->data.call.semantic_callee_value_binding_id;
            uint32_t target_id = target->stable_id, program_id = program->stable_id;
            uint32_t target_binding = ast_identifier_binding_syntax_id(target);
            ASTNodeType target_shape = target->type;
            uint32_t argument_id = argument == NULL ? 0 : argument->stable_id;
            uint32_t argument_binding = ast_identifier_binding_syntax_id(argument);
            char *target_name = target->data.identifier.name;
            size_t argc = call->data.call.arg_count;
            const char *current_name = ast_call_semantic_value_type_name(call);
            char *result_name = current_name == NULL ? NULL : strdup(current_name);
            if (current_name != NULL && result_name == NULL) { CHECK(false); continue; }
            if (fault == 0) call->data.call.semantic_callee_decl_id++;
            if (fault == 1) call->data.call.semantic_callee_builtin_kind = BUILTIN_CLONE;
            if (fault == 2) call->data.call.semantic_callee_value_binding_id++;
            if (fault == 3) target->data.identifier.name = "ForeignReader";
            if (fault == 4) target->stable_id++;
            if (fault == 5) call->data.call.callee = NULL;
            if (fault == 6) call->data.call.arg_count++;
            if (fault == 7) call->data.call.arguments[0] = target;
            if (fault == 8) argument->stable_id++;
            if (fault == 9) ast_identifier_set_binding_syntax_id(argument, argument_binding + 1);
            if (fault == 10) CHECK(ast_call_set_semantic_value_type_name_copy(call, "Int"));
            if (fault == 11) program->stable_id++;
            if (fault == 12) ast_identifier_set_binding_syntax_id(target, target_binding + 1);
            if (fault == 13) target->type = AST_NUMBER;
            if (fault != 10) CHECK(!semantic_param_publication_checked_call(ctx, call));
            if (fault != 10) CHECK(!semantic_param_publication_value(ctx, call).ok);
            if (parent != NULL) CHECK(!semantic_param_publication_checked_result_relation(ctx, parent, 0, &depends, &retains));
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &rechecked));
            unsigned changed = function_param_flow_summary_for_param(ctx, routine, 0);
            if (changed != SLOT_PARAM_SUMMARY_ALL) fprintf(stderr, "nested call case=%zu fault=%u mask=%u expected=ALL\n", c, fault, changed);
            CHECK(changed == SLOT_PARAM_SUMMARY_ALL);
            call->data.call.semantic_callee_decl_id = decl;
            call->data.call.semantic_callee_builtin_kind = kind;
            call->data.call.semantic_callee_value_binding_id = binding;
            target->type = target_shape;
            target->data.identifier.name = target_name;
            target->stable_id = target_id;
            ast_identifier_set_binding_syntax_id(target, target_binding);
            call->data.call.callee = target;
            call->data.call.arg_count = argc;
            if (argument != NULL) {
                call->data.call.arguments[0] = argument;
                argument->stable_id = argument_id;
                ast_identifier_set_binding_syntax_id(argument, argument_binding);
            }
            program->stable_id = program_id;
            if (fault == 10) {
                if (result_name != NULL) CHECK(ast_call_set_semantic_value_type_name_copy(call, result_name));
                else CHECK(ast_call_clear_semantic_value_type_name(call));
            }
            free(result_name);
            CHECK(semantic_param_publication_checked_call(ctx, call));
            CHECK(semantic_param_publication_value(ctx, call).ok);
            if (parent != NULL) CHECK(semantic_param_publication_checked_result_relation(ctx, parent, 0, &depends, &retains) && depends && !retains);
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &rechecked) && rechecked == index);
            CHECK(function_param_flow_summary_for_param(ctx, routine, 0) == baseline);
        }
cleanup:
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_secure_view_call_tuple(void)
{
    Lexer *lexer = lexer_create("func Main()->Void{let ss:SecureSlot<Int> = ClaimSecureSlot<Int>();Write(ss,7,ss_token);if true{let srv:ReadView<Int> = ViewRead(ss);Log(ToString(Read(srv,srv_token)));}Release(ss,ss_token);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    if (program == NULL || ctx->has_error) goto cleanup;
    ASTNode *main_decl = ast_program_statement(program, 0);
    ASTNode *branch = ast_if_then_branch(ast_block_statement(ast_func_body(main_decl), 2));
    ASTNode *log = ast_block_statement(branch, 1);
    ASTNode *call = ast_call_argument(ast_call_argument(log, 0), 0);
    CHECK(call != NULL && call->type == AST_CALL && ast_call_arg_count(call) == 2);
    if (call == NULL || call->type != AST_CALL || ast_call_arg_count(call) != 2) goto cleanup;
    ASTNode *token = ast_call_argument(call, 1), *receiver = ast_call_argument(call, 0);
    CHECK(semantic_param_publication_checked_call(ctx, call));
    CHECK(semantic_param_publication_checked_argument(ctx, call, 1));
    /* This existing view-token alias has no typed VALUE producer. CALL tuple
       currentness is not authorization, origin, freshness or token pairing. */
    CHECK(!semantic_param_publication_value(ctx, token).ok);
    for (unsigned fault = 0; fault < 4; fault++) {
        uint32_t id = token->stable_id, binding = ast_identifier_binding_syntax_id(token);
        ASTNodeType shape = token->type;
        if (fault == 0) ast_identifier_set_binding_syntax_id(token, binding + 1);
        if (fault == 1) token->stable_id++;
        if (fault == 2) call->data.call.arguments[1] = receiver;
        if (fault == 3) token->type = AST_NUMBER;
        CHECK(!semantic_param_publication_checked_call(ctx, call));
        CHECK(!semantic_param_publication_checked_argument(ctx, call, 1));
        token->type = shape;
        token->stable_id = id;
        ast_identifier_set_binding_syntax_id(token, binding);
        call->data.call.arguments[1] = token;
        CHECK(semantic_param_publication_checked_call(ctx, call));
        CHECK(semantic_param_publication_checked_argument(ctx, call, 1));
        CHECK(!semantic_param_publication_value(ctx, token).ok);
    }
    /* Main has zero parameters, so this source supplies no invented parameter
       summary/index. Reached cached claims are covered by the controls above. */
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_nominal_result_relations(void)
{
    Lexer *lexer = lexer_create("struct Payload{text:String;count:Int;} func Inspect(ref text:String)->Int{let positional=Payload(text,3);let named=Payload(count:5,text:text);return StringLength(positional.text)+named.count;} struct Crate<T>{let item:T;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *nominal = ast_program_statement(program, 0);
    ASTNode *routine = ast_program_statement(program, 1);
    size_t field_count = 0;
    ClassField **fields = ast_class_fields(nominal, &field_count);
    CHECK(fields != NULL && field_count == 2);
    if (fields == NULL || field_count != 2 || ctx->has_error) goto cleanup;
    FunctionParamFlowProgramPointIndex *index = NULL, *rechecked = NULL;
    (void)function_param_flow_summary_for_param(ctx, routine, 0);
    CHECK(ctx->function_param_flow_summaries != NULL);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &index));
    if (index == NULL) goto cleanup;
    for (size_t c = 0; c < 2; c++) {
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(routine), c));
        bool depends = false, retains = true;
        CHECK(call != NULL && ast_call_arg_count(call) == 2);
        if (call == NULL || ast_call_arg_count(call) != 2) continue;
        CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
        CHECK(semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains) && depends && !retains);
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 2, &depends, &retains));
        ASTNode *target = ast_call_callee(call), *arg = ast_call_argument(call, 1);
        const char *current_result = ast_call_semantic_value_type_name(call);
        char *result = current_result == NULL ? NULL : strdup(current_result);
        CHECK(result != NULL);
        if (result == NULL) continue;
        FunctionParamPublicationStore *store = ctx->function_param_publications;
        PublishedNode *row = NULL;
        for (size_t r = 0; r < store->node_capacity; r++) if (store->nodes[r].node == call) row = &store->nodes[r];
        CHECK(row != NULL && row->nominal_result != NULL && row->value_present);
        if (row == NULL || row->nominal_result == NULL) { free(result); continue; }
        SemanticNominalResultSnapshot *snapshot = row->nominal_result;
        for (unsigned fault = 0; fault < 38; fault++) {
            uint32_t arg_id = arg->stable_id, generation = program->stable_id;
            uint32_t field_id = fields[0]->stable_id, decl_id = nominal->stable_id;
            ASTNode *field_type = fields[0]->type;
            char *field_name = fields[0]->name, *target_name = target->data.identifier.name;
            ASTNodeType arg_kind = arg->type;
            size_t argc = call->data.call.arg_count;
            uint32_t declaration = call->data.call.semantic_callee_decl_id;
            uint32_t value_binding = call->data.call.semantic_callee_value_binding_id;
            ClassField *field_pointer = fields[0];
            size_t count = nominal->data.class_decl.field_count;
            size_t destructures = nominal->data.class_decl.field_destructure_count;
            bool plain = nominal->data.class_decl.is_struct;
            GenericParams *generic = nominal->data.class_decl.generic_params;
            ASTNode *default_value = fields[0]->default_value;
            SemanticNominalResultField *snapshot_fields = snapshot->fields;
            size_t snapshot_count = snapshot->field_count, mapping = snapshot->fields[0].field_index;
            char *snapshot_name = snapshot->fields[0].field_name;
            char *snapshot_result = snapshot->result_type_name, *snapshot_decl = snapshot->decl_name;
            char *snapshot_field_type = snapshot->fields[0].field_type_name;
            char *snapshot_argument_type = snapshot->fields[0].argument_type_name;
            char *snapshot_syntax = snapshot->fields[0].type_syntax_name;
            ClassField *snapshot_field = snapshot->fields[0].field;
            ASTNode *snapshot_type = snapshot->fields[0].type_node, *snapshot_declaration = snapshot->decl;
            uint32_t type_id = field_type->stable_id;
            char *decl_name = nominal->data.class_decl.name;
            char *actual_name = row->value_type_name;
            bool value_present = row->value_present;
            char *argument_name = call->data.call.arg_names == NULL ? NULL : call->data.call.arg_names[1];
            uint32_t binding = ast_identifier_binding_syntax_id(arg);
            if (fault == 0) fields[0]->stable_id++;
            if (fault == 1) fields[0]->name = "foreign_field";
            if (fault == 2) fields[0]->type = fields[1]->type;
            if (fault == 3) nominal->stable_id++;
            if (fault == 4) target->data.identifier.name = "ForeignPayload";
            if (fault == 5) call->data.call.arguments[1] = ast_call_argument(call, 0);
            if (fault == 6) arg->stable_id++;
            if (fault == 7) arg->type = AST_BOOLEAN;
            if (fault == 8) call->data.call.arg_count = 1;
            if (fault == 9) program->stable_id++;
            if (fault == 10) CHECK(ast_call_set_semantic_value_type_name_copy(call, "Int"));
            if (fault == 11) call->data.call.semantic_callee_decl_id++;
            if (fault == 12) call->data.call.semantic_callee_value_binding_id++;
            if (fault == 13) fields[0] = fields[1];
            if (fault == 14) nominal->data.class_decl.field_count = 1;
            if (fault == 15) nominal->data.class_decl.field_destructure_count = 1;
            if (fault == 16) nominal->data.class_decl.is_struct = false;
            if (fault == 17) fields[0]->default_value = arg;
            if (fault == 18) snapshot->field_count++;
            if (fault == 19) snapshot->fields[0].field_index = snapshot->field_count;
            if (fault == 20) snapshot->fields[0].field_name = NULL;
            if (fault == 21) snapshot->fields = NULL;
            if (fault == 22) row->value_present = false;
            if (fault == 23) row->value_type_name = NULL;
            if (fault == 24) row->value_type_name = "Int";
            if (fault == 25) snapshot->result_type_name = NULL;
            if (fault == 26) snapshot->decl_name = NULL;
            if (fault == 27 && call->data.call.arg_names != NULL) call->data.call.arg_names[1] = "foreign_field";
            if (fault == 28 && arg->type == AST_IDENTIFIER) ast_identifier_set_binding_syntax_id(arg, binding + 1);
            if (fault == 29) nominal->data.class_decl.generic_params = ast_program_statement(program, 2)->data.class_decl.generic_params;
            if (fault == 30) snapshot->fields[0].field_type_name = NULL;
            if (fault == 31) snapshot->fields[0].argument_type_name = NULL;
            if (fault == 32) snapshot->fields[0].type_syntax_name = NULL;
            if (fault == 33) snapshot->fields[0].field = NULL;
            if (fault == 34) snapshot->fields[0].type_node = fields[snapshot->fields[0].field_index == 0 ? 1 : 0]->type;
            if (fault == 35) field_type->stable_id++;
            if (fault == 36) nominal->data.class_decl.name = "ForeignPayload";
            if (fault == 37) snapshot->decl = ast_program_statement(program, 2);
            if ((fault == 27 && call->data.call.arg_names == NULL) || (fault == 28 && arg->type != AST_IDENTIFIER)) continue;
            CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
            CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains));
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &rechecked));
            snapshot->result_type_name = snapshot_result;
            snapshot->decl_name = snapshot_decl;
            /* Restore borrowed test substitutions before any owned destruction. */
            snapshot->fields = snapshot_fields;
            snapshot->field_count = snapshot_count;
            snapshot->fields[0].field_index = mapping;
            snapshot->fields[0].field_name = snapshot_name;
            snapshot->fields[0].field_type_name = snapshot_field_type;
            snapshot->fields[0].argument_type_name = snapshot_argument_type;
            snapshot->fields[0].type_syntax_name = snapshot_syntax;
            snapshot->fields[0].field = snapshot_field;
            snapshot->fields[0].type_node = snapshot_type;
            snapshot->decl = snapshot_declaration;
            field_type->stable_id = type_id;
            nominal->data.class_decl.name = decl_name;
            row->value_present = value_present;
            row->value_type_name = actual_name;
            fields[0] = field_pointer;
            fields[0]->default_value = default_value;
            nominal->data.class_decl.field_count = count;
            nominal->data.class_decl.field_destructure_count = destructures;
            nominal->data.class_decl.is_struct = plain;
            nominal->data.class_decl.generic_params = generic;
            if (call->data.call.arg_names != NULL) call->data.call.arg_names[1] = argument_name;
            if (arg->type == AST_IDENTIFIER) ast_identifier_set_binding_syntax_id(arg, binding);
            fields[0]->stable_id = field_id;
            fields[0]->name = field_name;
            fields[0]->type = field_type;
            nominal->stable_id = decl_id;
            target->data.identifier.name = target_name;
            call->data.call.arguments[1] = arg;
            arg->stable_id = arg_id;
            arg->type = arg_kind;
            call->data.call.arg_count = argc;
            program->stable_id = generation;
            call->data.call.semantic_callee_decl_id = declaration;
            call->data.call.semantic_callee_value_binding_id = value_binding;
            if (fault == 10) CHECK(ast_call_set_semantic_value_type_name_copy(call, result));
            CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
            CHECK(semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains) && depends && !retains);
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &rechecked) && rechecked == index);
        }
        free(result);
    }
    {
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(routine), 0));
        bool depends = false, retains = true;
        /* Whole-store destruction retires admitted proof; begin_value is tested
           below in its legitimate pending lifecycle, not after finalization. */
        semantic_param_publication_destroy(ctx);
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_nominal_unsupported_producers(void)
{
    const char *sources[] = {
        "struct Crate<T>{let item:T;} func Main()->Void{let payload:Crate<Int> = Crate(4);}",
        "struct Payload{text:String;count:Int;} func Main()->Void{let payload=Payload(7,3);}",
        "struct Payload{text:String;count:Int;} func Main()->Void{let payload=Payload(\"literal\");}",
        "struct Payload{text:String;count:Int;} func Main()->Void{let payload=Payload(text:\"literal\",text:\"other\");}"
    };
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        Lexer *lexer = lexer_create(sources[i]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        bool accepted = type_check_program(program, ctx);
        /* Existing typing accepts duplicate named arguments; optional unique
           field mapping still must not issue a result-dependence proof. */
        CHECK(i == 0 || i == 3 ? accepted : !accepted);
        ASTNode *main_decl = ast_program_statement(program, 1);
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 0));
        bool depends = false, retains = true;
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains));
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_nominal_result_growth(void)
{
    char source[2048] = "struct Payload{text:String;count:Int;} func Main()->Void{let payload=Payload(\"literal\",3);} func Padding()->Void{";
    for (unsigned i = 0; i < 100; i++) strcat(source, "return;");
    strcat(source, "}");
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(ast_assign_stable_ids(program));
    ctx->program_root = program;
    CHECK(semantic_build_host_decl_index(ctx, program));
    CHECK(semantic_param_publication_begin(ctx));
    CHECK(type_check_class_decl(ast_program_statement(program, 0), ctx));
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 0));
    ASTNode *padding = ast_program_statement(program, 2);
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    CHECK(store != NULL && store->pending && !ctx->has_error);
    if (store == NULL || !store->pending || ctx->has_error) goto cleanup;
    CHECK(type_check_expression(call, ctx) != TYPE_UNKNOWN && !ctx->has_error);
    PublishedNode *before_reset = NULL;
    for (size_t r = 0; r < store->node_capacity; r++) if (store->nodes[r].node == call) before_reset = &store->nodes[r];
    CHECK(before_reset != NULL && before_reset->nominal_result != NULL && before_reset->value_present);
    CHECK(semantic_param_publication_begin_value(ctx, call));
    PublishedNode *after_reset = NULL;
    for (size_t r = 0; r < store->node_capacity; r++) if (store->nodes[r].node == call) after_reset = &store->nodes[r];
    CHECK(after_reset != NULL && after_reset->nominal_result == NULL && !after_reset->value_present);
    /* The ordinary routine checker rechecks the original initializer and owns
       its caller identity; no publication flags or rows are fabricated. */
    CHECK(type_check_func_decl(ast_program_statement(program, 1), ctx));
    size_t capacity = store->node_capacity, statement = 1;
    PublishedNode *previous = store->nodes;
    while (store->node_capacity == capacity && statement < ast_block_statement_count(ast_func_body(padding))) {
        CHECK(semantic_param_publication_record_execution(ctx, ast_block_statement(ast_func_body(padding), statement++), false));
    }
    CHECK(store->node_capacity == capacity * 2 && store->nodes != previous);
    CHECK(type_check_func_decl(padding, ctx));
    CHECK(semantic_param_publication_finalize(ctx));
    bool depends = false, retains = true;
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains) && depends && !retains);
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_some_unwrap_result_relations(void)
{
    Lexer *lexer = lexer_create("func ReadMaybe(ref text:String)->Bool{let alternate:String=\"literal\";let observed=Some(text);let extracted=UnwrapOption(observed);let repeated=UnwrapOption(Some(text));return StringLength(extracted)>0;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *calls[] = {
        ast_let_initializer(ast_block_statement(ast_func_body(decl), 1)),
        ast_let_initializer(ast_block_statement(ast_func_body(decl), 2))
    };
    ASTNode *alternate = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    SemanticParamPublicationView view = {0};
    CHECK(semantic_param_publication_admitted_view(ctx, &view));
    CHECK(semantic_param_publication_view_current(ctx, &view));
    CHECK(!semantic_param_publication_view_current(ctx, NULL));
    SemanticParamPublicationView missing = {0};
    CHECK(!semantic_param_publication_view_current(ctx, &missing));
    for (unsigned fault = 0; fault < 8; fault++) {
        SemanticParamPublicationView changed = view;
        if (fault == 0) changed.program = NULL;
        if (fault == 1) changed.program_id++;
        if (fault == 2) changed.routines = NULL;
        if (fault == 3) changed.routine_count++;
        if (fault == 4) changed.calls = NULL;
        if (fault == 5) changed.call_count++;
        if (fault == 6) changed.body_calls = NULL;
        if (fault == 7) changed.body_call_count++;
        /* Zero-row lists may legitimately have NULL pointers; only a changed
           pointer is a falsifier, never the spelling of an absent list. */
        if (fault == 4 && view.calls == NULL) continue;
        if (fault == 6 && view.body_calls == NULL) continue;
        CHECK(!semantic_param_publication_view_current(ctx, &changed));
        CHECK(semantic_param_publication_view_current(ctx, &view));
    }
    FunctionParamFlowProgramPointIndex *index = NULL, *rechecked = NULL;
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK(ctx->function_param_flow_summaries != NULL);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    if (ctx->function_param_flow_summaries == NULL || index == NULL) goto cleanup;
    bool depends = false, retains = true;
    CHECK(!semantic_param_publication_checked_result_relation(ctx, NULL, 0, &depends, &retains));
    for (size_t c = 0; c < 2; c++) {
        ASTNode *call = calls[c];
        CHECK(call != NULL && call->type == AST_CALL);
        if (call == NULL || call->type != AST_CALL) continue;
        CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains));
        ASTNode *target = ast_call_callee(call), *argument = ast_call_argument(call, 0);
        const char *result = ast_call_semantic_value_type_name(call);
        char *saved_result = result == NULL ? NULL : strdup(result);
        CHECK(saved_result != NULL);
        if (saved_result == NULL || target == NULL || argument == NULL) { free(saved_result); continue; }
        for (unsigned fault = 0; fault < 13; fault++) {
            ASTNodeType shape = call->type;
            uint32_t id = argument->stable_id, generation = program->stable_id;
            uint32_t declaration = call->data.call.semantic_callee_decl_id;
            uint32_t value_binding = call->data.call.semantic_callee_value_binding_id;
            uint32_t kind = call->data.call.semantic_callee_builtin_kind;
            bool stdlib = call->data.call.semantic_callee_is_stdlib;
            uint32_t binding = ast_identifier_binding_syntax_id(argument);
            char *name = target->data.identifier.name;
            size_t count = call->data.call.arg_count;
            if (fault == 0) target->data.identifier.name = "ForeignResultOperation";
            if (fault == 1) call->data.call.semantic_callee_builtin_kind = BUILTIN_NOT_BUILTIN == kind ? BUILTIN_CLONE : BUILTIN_NOT_BUILTIN;
            if (fault == 2) call->data.call.semantic_callee_is_stdlib = !stdlib;
            if (fault == 3) call->data.call.semantic_callee_decl_id = declaration + 1;
            if (fault == 4) call->data.call.semantic_callee_value_binding_id = value_binding + 1;
            if (fault == 5) call->data.call.arguments[0] = alternate;
            if (fault == 6) call->data.call.arg_count = 0;
            if (fault == 7) argument->stable_id++;
            if (fault == 8) ast_identifier_set_binding_syntax_id(argument, binding + 1);
            if (fault == 9) program->stable_id++;
            if (fault == 10) CHECK(ast_call_set_semantic_value_type_name_copy(call, "Int"));
            if (fault == 11) call->type = AST_IDENTIFIER;
            if (fault == 12) CHECK(ast_call_clear_semantic_value_type_name(call));
            CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
            target->data.identifier.name = name;
            call->data.call.semantic_callee_builtin_kind = kind;
            call->data.call.semantic_callee_is_stdlib = stdlib;
            call->data.call.semantic_callee_decl_id = declaration;
            call->data.call.semantic_callee_value_binding_id = value_binding;
            call->data.call.arguments[0] = argument;
            call->data.call.arg_count = count;
            argument->stable_id = id;
            ast_identifier_set_binding_syntax_id(argument, binding);
            program->stable_id = generation;
            call->type = shape;
            if (fault == 10 || fault == 12) CHECK(ast_call_set_semantic_value_type_name_copy(call, saved_result));
            CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
        }
        free(saved_result);
    }
    ASTNode *nested = ast_let_initializer(ast_block_statement(ast_func_body(decl), 3));
    ASTNode *nested_argument = ast_call_argument(nested, 0);
    const char *argument_type = ast_call_semantic_value_type_name(nested_argument);
    char *saved_argument_type = argument_type == NULL ? NULL : strdup(argument_type);
    CHECK(saved_argument_type != NULL);
    if (saved_argument_type != NULL) {
        CHECK(semantic_param_publication_checked_result_relation(ctx, nested, 0, &depends, &retains) && depends && !retains);
        CHECK(ast_call_set_semantic_value_type_name_copy(nested_argument, "Option<Int>"));
        CHECK(!semantic_param_publication_checked_result_relation(ctx, nested, 0, &depends, &retains));
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
        CHECK(ast_call_set_semantic_value_type_name_copy(nested_argument, saved_argument_type));
        CHECK(semantic_param_publication_checked_result_relation(ctx, nested, 0, &depends, &retains) && depends && !retains);
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
        free(saved_argument_type);
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_callable_flow_projection_authority(void)
{
    const char *sources[] = {
        "func Probe(ref text:String)->Bool{return StringLength(text)>0;} func Alternate()->Void{}",
        "func Probe(ref text:String)->Bool{let channel:Channel<String> = Channel(1);channel<-text;return true;} func Alternate()->Void{}",
        "struct TextRecord{text:String} extern \"C\"{func Keep(text:String)->Void;} func Probe(ref record:TextRecord)->Bool{Keep(record.text);return true;} func Alternate()->Void{}"
    };
    const unsigned expected[] = {SLOT_PARAM_SUMMARY_NONE, SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE, SLOT_PARAM_SUMMARY_CALL_ESCAPE};
    for (unsigned c = 0; c < 3; c++) {
        Lexer *lexer = lexer_create(sources[c]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        CHECK(type_check_program(program, ctx) == (c != 2));
        if (c == 2) CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
        ASTNode *decl = ast_program_statement(program, c == 2 ? 2 : 0);
        ASTNode *alternate = ast_program_statement(program, c == 2 ? 3 : 1);
        Symbol *symbol = scope_lookup(ctx->scope, "Probe");
        CHECK(symbol != NULL && symbol->type != NULL);
        if (symbol == NULL || symbol->type == NULL) goto cleanup;
        Type *projection = symbol->type;
        unsigned raw = function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK(raw == expected[c]);
        CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == raw);
        unsigned previous_mask = type_function_param_escape_summary(projection, 0);
        bool previous_finished = projection->data.function.has_param_escape_summary_facts;
        for (unsigned forced = 0; forced < 2; forced++) {
            type_function_set_param_escape_summary(projection, 0, forced == 0 ? SLOT_PARAM_SUMMARY_NONE : SLOT_PARAM_SUMMARY_ALL);
            type_function_finish_param_escape_summaries(projection);
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == raw);
            CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == raw);
        }
        type_function_set_param_escape_summary(projection, 0, previous_mask);
        projection->data.function.has_param_escape_summary_facts = previous_finished;
        ASTNode *use = c == 0 ? ast_call_argument(ast_binary_left(ast_return_value(ast_block_statement(ast_func_body(decl), 0))), 0)
            : c == 1 ? ast_channel_send_value(ast_block_statement(ast_func_body(decl), 1))
            : ast_member_object(ast_call_argument(ast_block_statement(ast_func_body(decl), 0), 0));
        CHECK(use != NULL && use->type == AST_IDENTIFIER);
        if (use == NULL || use->type != AST_IDENTIFIER) goto cleanup;
        FuncParam *formal = ast_func_param(decl, 0);
        for (unsigned fault = 0; fault < 4; fault++) {
            ASTNode *formal_type = formal->type, *body = ast_func_body(decl);
            uint32_t binding = ast_identifier_binding_syntax_id(use), generation = program->stable_id;
            if (fault == 0) formal->type = NULL;
            if (fault == 1) decl->data.func_decl.body = ast_func_body(alternate);
            if (fault == 2) ast_identifier_set_binding_syntax_id(use, binding + 1);
            if (fault == 3) program->stable_id++;
            unsigned changed_raw = function_param_flow_summary_for_param(ctx, decl, 0);
            unsigned changed_callable = semantic_callable_param_escape_summary(decl, 0, ctx);
            if (changed_raw != SLOT_PARAM_SUMMARY_ALL || changed_callable != SLOT_PARAM_SUMMARY_ALL)
                fprintf(stderr, "callable-flow stale case=%u fault=%u raw=%u callable=%u source=%s\n",
                    c, fault, changed_raw, changed_callable, sources[c]);
            CHECK(changed_raw == SLOT_PARAM_SUMMARY_ALL);
            CHECK(changed_callable == SLOT_PARAM_SUMMARY_ALL);
            formal->type = formal_type;
            decl->data.func_decl.body = body;
            ast_identifier_set_binding_syntax_id(use, binding);
            program->stable_id = generation;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == raw);
            CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == raw);
        }
cleanup:
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    /* Honest pending owner lifecycle: no status flags are forged. A projection
       finish/zero cannot grant admission before typed-input finalization. */
    Lexer *lexer = lexer_create(sources[0]);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(ast_assign_stable_ids(program));
    ctx->program_root = program;
    CHECK(semantic_build_host_decl_index(ctx, program));
    CHECK(semantic_param_publication_begin(ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    CHECK(type_check_func_decl(decl, ctx));
    Symbol *symbol = scope_lookup(ctx->scope, "Probe");
    CHECK(symbol != NULL && symbol->type != NULL);
    if (symbol != NULL && symbol->type != NULL) {
        type_function_set_param_escape_summary(symbol->type, 0, SLOT_PARAM_SUMMARY_NONE);
        type_function_finish_param_escape_summaries(symbol->type);
        CHECK(semantic_param_publication_pending(ctx) && !semantic_param_publication_inputs_ready(ctx));
        CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == SLOT_PARAM_SUMMARY_ALL);
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_physical_return_retaining_effects(const char *source, size_t declaration, unsigned required_effect, bool accepted)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx) == accepted);
    if (!accepted) CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    ASTNode *decl = ast_program_statement(program, declaration);
    CHECK(semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    unsigned mask = semantic_callable_param_escape_summary(decl, 0, ctx);
    if ((mask & required_effect) == 0)
        fprintf(stderr, "physical-return effect case: mask=%u required=%u source=%s\n", mask, required_effect, source);
    CHECK((mask & required_effect) != 0);
    CHECK((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
    /* Eligibility is a header fact, never permission to erase retaining
       children. Default source type and original physical result stay sealed. */
    FuncParam *formal = ast_func_param(decl, 1);
    ASTNode *type_node = formal->type;
    formal->type = NULL;
    CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    formal->type = type_node;
    CHECK(semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    mask = semantic_callable_param_escape_summary(decl, 0, ctx);
    if ((mask & required_effect) == 0)
        fprintf(stderr, "physical-return restored effect case: mask=%u required=%u source=%s\n", mask, required_effect, source);
    CHECK((mask & required_effect) != 0);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_physical_return_mutable_output(const char *source, size_t declaration)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, declaration);
    CHECK(!semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
    (void)function_param_flow_summary_for_param(ctx, decl, 1);
    unsigned effects = 0;
    bool outputs[2] = {false, false}, pending = true;
    CHECK(function_param_flow_returned_backing_effects(ctx->function_param_flow_summaries,
        decl, 1, &effects, outputs, 2, &pending, false));
    CHECK(!pending && outputs[0]);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_string_map_get_result_relation(void)
{
    Lexer *lexer = lexer_create("func ReadMaybe(ref values:HashMap<String,String>,ref key:String)->Bool{let alternate:String=\"other\";let observed=MapGet(values,key);return StringLength(observed)>0;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(decl), 1));
    ASTNode *key = ast_call_argument(call, 1);
    ASTNode *receiver = ast_call_argument(call, 0);
    ASTNode *alternate = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    bool depends = false, retains = true;
    CHECK(ast_call_arg_count(call) == 2);
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains) && !depends && !retains);
    CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 2, &depends, &retains));
    FunctionParamFlowProgramPointIndex *index = NULL, *rechecked = NULL;
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    if (ctx->function_param_flow_summaries == NULL || index == NULL || key == NULL) goto cleanup;
    for (unsigned fault = 0; fault < 9; fault++) {
        uint32_t id = key->stable_id;
        ASTNodeType kind = key->type;
        uint32_t key_binding = ast_identifier_binding_syntax_id(key);
        uint32_t receiver_id = receiver->stable_id;
        ASTNodeType receiver_kind = receiver->type;
        uint32_t receiver_binding = ast_identifier_binding_syntax_id(receiver);
        size_t count = call->data.call.arg_count;
        if (fault == 0) call->data.call.arguments[1] = alternate;
        if (fault == 1) call->data.call.arg_count = 1;
        if (fault == 2) key->type = AST_NUMBER;
        if (fault == 3) key->stable_id++;
        if (fault == 4) call->data.call.arguments[0] = alternate;
        if (fault == 5) receiver->type = AST_NUMBER;
        if (fault == 6) receiver->stable_id++;
        if (fault == 7) ast_identifier_set_binding_syntax_id(receiver, receiver_binding + 1);
        if (fault == 8) ast_identifier_set_binding_syntax_id(key, key_binding + 1);
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains));
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
        call->data.call.arguments[1] = key;
        call->data.call.arg_count = count;
        key->type = kind;
        key->stable_id = id;
        ast_identifier_set_binding_syntax_id(key, key_binding);
        call->data.call.arguments[0] = receiver;
        receiver->type = receiver_kind;
        receiver->stable_id = receiver_id;
        ast_identifier_set_binding_syntax_id(receiver, receiver_binding);
        CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
        CHECK(semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains) && !depends && !retains);
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_result_relation_reallocation(void)
{
    char source[2048] = "func Main()->Void{let observed=Some(\"literal\");} func Padding()->Void{";
    for (unsigned i = 0; i < 100; i++) strcat(source, "return;");
    strcat(source, "}");
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(ast_assign_stable_ids(program));
    ctx->program_root = program;
    CHECK(semantic_build_host_decl_index(ctx, program));
    CHECK(semantic_param_publication_begin(ctx));
    ASTNode *main_decl = ast_program_statement(program, 0);
    ASTNode *padding = ast_program_statement(program, 1);
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 0));
    ASTNode *argument = ast_call_argument(call, 0);
    /* Types come from the actual checked original expressions, not fabricated
       row eligibility. The routine check then seals the actual caller. */
    Type *argument_type = type_check_expression(argument, ctx);
    Type *result_type = type_check_expression(call, ctx);
    CHECK(argument_type != NULL && result_type != NULL && !ctx->has_error);
    CHECK(type_check_func_decl(main_decl, ctx));
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    CHECK(store != NULL && store->pending && store->node_capacity != 0);
    if (store == NULL || !store->pending || store->node_capacity == 0) goto cleanup;
    /* These original statements follow an unconditional return and are really
       unreachable. Execution-only rows grant no value/result eligibility. */
    size_t statement = 1;
    while (store->node_count < store->node_capacity / 2 && statement < ast_block_statement_count(ast_func_body(padding))) {
        CHECK(semantic_param_publication_record_execution(ctx,
            ast_block_statement(ast_func_body(padding), statement++), false));
    }
    CHECK(store->node_count == store->node_capacity / 2);
    size_t capacity = store->node_capacity;
    PublishedNode *previous_rows = store->nodes;
    CHECK(semantic_param_publication_record_checked_result_relation(ctx, call, argument_type, result_type, 0, 0));
    CHECK(store->node_capacity == capacity * 2 && store->nodes != previous_rows);
    bool current_argument = false;
    for (size_t i = 0; i < store->node_capacity; i++) if (store->nodes[i].node == argument) {
        current_argument = store->nodes[i].value_present && store->nodes[i].value_type_name != NULL &&
            strcmp(store->nodes[i].value_type_name, argument_type->name) == 0;
    }
    CHECK(current_argument);
    CHECK(type_check_func_decl(padding, ctx));
    CHECK(semantic_param_publication_finalize(ctx));
    bool depends = false, retains = true;
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_string_map_get_payload_source(void)
{
    Lexer *lexer = lexer_create("func Returned(values:HashMap<String,String>,ref key:String)->String{return MapGet(values,key);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
    bool depends = false, retains = true;
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains) && !depends && !retains);
    SlotFunctionLookup lookup = {ctx, program};
    unsigned receiver = function_param_flow_summary_demand(&lookup, decl, 0);
    unsigned key = function_param_flow_summary_demand(&lookup, decl, 1);
    CHECK((receiver & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
    CHECK((key & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
    CHECK((key & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    CHECK(!semantic_param_publication_value(ctx, call).origin_ready);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_nested_option_result_relations(void)
{
    Lexer *lexer = lexer_create("func Main()->Void{let observed=Some(Some(\"literal\"));let extracted=UnwrapOption(observed);Log(IsSome(extracted));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *some = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    ASTNode *unwrap = ast_let_initializer(ast_block_statement(ast_func_body(decl), 1));
    bool depends = false, retains = true;
    const char *some_name = ast_call_semantic_value_type_name(some);
    const char *unwrap_name = ast_call_semantic_value_type_name(unwrap);
    CHECK(some_name != NULL && strcmp(some_name, "Option<Option<String>>") == 0);
    CHECK(unwrap_name != NULL && strcmp(unwrap_name, "Option<String>") == 0);
    CHECK(semantic_param_publication_checked_result_relation(ctx, some, 0, &depends, &retains) && depends && !retains);
    CHECK(semantic_param_publication_checked_result_relation(ctx, unwrap, 0, &depends, &retains) && depends && !retains);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);

    /* Compare nested Unknown to the existing None checker branch, not to a
       invented allocation/publication error or a freshness permission. */
    bool baseline_success = false, baseline_error = false;
    for (unsigned nested = 0; nested < 2; nested++) {
        lexer = lexer_create(nested == 0
            ? "func Main()->Void{let observed=None();}"
            : "func Main()->Void{let observed=Some(None());}");
        parser = parser_create(lexer);
        program = parser_parse_program(parser);
        ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        bool success = type_check_program(program, ctx);
        if (nested == 0) { baseline_success = success; baseline_error = ctx->has_error; }
        else { CHECK(success == baseline_success); CHECK(ctx->has_error == baseline_error); }
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 0));
        CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
        for (size_t d = 0; d < ctx->diagnostic_count; d++) {
            const Diagnostic *diagnostic = ctx->diagnostics[d];
            CHECK(diagnostic == NULL || diagnostic->message == NULL ||
                (strstr(diagnostic->message, "result relation publication failed") == NULL &&
                 strstr(diagnostic->message, "allocation") == NULL));
        }
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_static_string_backing(void)
{
    Lexer *lexer = lexer_create("func Main()->Void{let literal=\"static\";let dynamic=Concat(\"heap\",\" text\");let alias=literal;Log(alias);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *body = ast_func_body(ast_program_statement(program, 0));
    ASTNode *literal = ast_let_initializer(ast_block_statement(body, 0));
    ASTNode *dynamic = ast_let_initializer(ast_block_statement(body, 1));
    ASTNode *alias = ast_let_initializer(ast_block_statement(body, 2));
    CHECK(semantic_param_publication_value_has_static_string_backing(ctx, literal));
    CHECK(semantic_param_publication_value(ctx, dynamic).copy_escape);
    CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, dynamic));
    CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, alias));
    ASTNode *read = ast_block_statement(body, 3);
    CHECK(semantic_param_publication_string_argument_read_only(ctx, read, 0));
    CHECK(!semantic_param_publication_string_argument_read_only(ctx, read, 1));
    ASTNode *read_argument = ast_call_argument(read, 0);
    uint32_t read_binding = ast_identifier_binding_syntax_id(read_argument);
    ast_identifier_set_binding_syntax_id(read_argument, read_binding + 1);
    CHECK(!semantic_param_publication_string_argument_read_only(ctx, read, 0));
    ast_identifier_set_binding_syntax_id(read_argument, read_binding);
    CHECK(semantic_param_publication_string_argument_read_only(ctx, read, 0));
    PublishedNode *row = NULL;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    for (size_t i = 0; store != NULL && i < store->node_capacity; i++)
        if (store->nodes[i].node == literal) row = &store->nodes[i];
    CHECK(row != NULL);
    if (row != NULL) {
        row->value_present = false;
        CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, literal));
        row->value_present = true;
        row->string_value = false;
        CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, literal));
        row->string_value = true;
        row->copy_escape = false;
        CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, literal));
        row->copy_escape = true;
        literal->type = AST_NUMBER;
        CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, literal));
        literal->type = AST_STRING;
        program->stable_id++;
        CHECK(!semantic_param_publication_value_has_static_string_backing(ctx, literal));
        CHECK(!semantic_param_publication_string_argument_read_only(ctx, read, 0));
        program->stable_id--;
        CHECK(semantic_param_publication_value_has_static_string_backing(ctx, literal));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_failed_no_store_selection_epoch(void)
{
    Lexer *lexer = lexer_create("func Pure(ref value:Int)->Int{return value;}func Next(ref value:Int)->Int{return value;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    if (program == NULL || parser_has_error(parser)) goto cleanup;
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (store == NULL || index == NULL) goto cleanup;
    bool original = index->mutable_output_required;
    /* Inject a conservative owner verdict, never a storage permission. */
    index->mutable_output_required = true;
    store->solving = true; store->demand_epoch++; store->admitted_no_store_count = 0; store->work_units = 0;
    CHECK(!function_param_flow_no_store_graph(store, decl));
    size_t charged = store->work_units;
    CHECK(charged != 0);
    CHECK(!function_param_flow_no_store_graph(store, decl));
    CHECK(store->work_units == charged);
    index->mutable_output_required = original;
    CHECK(!function_param_flow_no_store_graph(store, decl));
    CHECK(store->work_units == charged);
    store->solving = false;
    CHECK(function_param_flow_no_store_graph(store, decl));
    CHECK(store->work_units == charged); /* Fresh validation owns its budget. */
    store->solving = true; store->demand_epoch++; store->admitted_no_store_count = 0;
    CHECK(function_param_flow_no_store_graph(store, decl));
    charged = store->work_units;
    index->mutable_output_required = true;
    CHECK(!function_param_flow_no_store_graph(store, decl));
    index->mutable_output_required = original;
    store->demand_epoch++; store->admitted_no_store_count = 0;
    CHECK(function_param_flow_no_store_graph(store, decl));
    store->solving = false;
    /* Overflow must start an uncached demand, not reuse a completed row. */
    function_param_flow_summary_store_destroy(ctx);
    store = function_param_flow_store_get(&(SlotFunctionLookup){ctx, program});
    CHECK(store != NULL);
    if (store == NULL) goto cleanup;
    store->demand_epoch = SIZE_MAX;
    CHECK(function_param_flow_summary_for_param(ctx, ast_program_statement(program, 1), 0) == SLOT_PARAM_SUMMARY_ALL);
    CHECK(store->failed);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_admitted_no_store_proof(void)
{
    Lexer *lexer = lexer_create("func Leaf(ref value:Int)->Int{return value;}func Root(ref value:Int)->Int{return Leaf(value);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    if (program == NULL || parser_has_error(parser)) goto cleanup;
    CHECK(type_check_program(program, ctx));
    ASTNode *leaf = ast_program_statement(program, 0), *root = ast_program_statement(program, 1);
    (void)function_param_flow_summary_for_param(ctx, root, 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *leaf_index = NULL, *root_index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, leaf, &leaf_index));
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, root, &root_index));
    if (store == NULL || leaf_index == NULL || root_index == NULL) goto cleanup;
    store->solving = true; store->demand_epoch++; store->admitted_no_store_count = 0; store->work_units = 0;
    CHECK(function_param_flow_no_store_graph(store, root));
    size_t charged = store->work_units;
    CHECK(charged == 2 && store->admitted_no_store_count == 2);
    CHECK(function_param_flow_no_store_graph(store, leaf));
    CHECK(function_param_flow_no_store_graph(store, root));
    CHECK(store->work_units == charged && store->admitted_no_store_count == 2);
    bool original = leaf_index->mutable_output_required;
    leaf_index->mutable_output_required = true;
    CHECK(!function_param_flow_no_store_graph(store, root));
    leaf_index->mutable_output_required = original;
    ASTNode *value = ast_return_value(ast_block_statement(ast_func_body(leaf), 0));
    value->stable_id++;
    CHECK(!function_param_flow_no_store_graph(store, root));
    value->stable_id--;
    ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(root), 0));
    ASTNode *argument = ast_call_argument(call, 0);
    uint32_t binding = ast_identifier_binding_syntax_id(argument);
    ast_identifier_set_binding_syntax_id(argument, binding + 1);
    CHECK(!function_param_flow_no_store_graph(store, root));
    ast_identifier_set_binding_syntax_id(argument, binding);
    size_t position = leaf_index->admitted_no_store_position;
    leaf_index->admitted_no_store_position = 0;
    CHECK(!function_param_flow_no_store_graph(store, root));
    leaf_index->admitted_no_store_position = position;
    root_index->mutable_output_required = true;
    CHECK(!function_param_flow_no_store_graph(store, root));
    root_index->mutable_output_required = false;
    CHECK(function_param_flow_no_store_graph(store, root));
    store->demand_epoch++; store->admitted_no_store_count = 0;
    leaf_index->mutable_output_required = true;
    CHECK(!function_param_flow_no_store_graph(store, root));
    leaf_index->mutable_output_required = original;
    CHECK(!function_param_flow_no_store_graph(store, root));
    store->demand_epoch++; store->admitted_no_store_count = 0;
    CHECK(function_param_flow_no_store_graph(store, root));
    CHECK(store->work_units > charged && store->admitted_no_store_count == 2);
    /* A loan query outside the solver must reconstruct current selection. */
    store->solving = false; charged = store->work_units;
    leaf_index->mutable_output_required = true;
    CHECK(!function_param_flow_no_store_graph(store, root));
    CHECK(store->work_units == charged);
    leaf_index->mutable_output_required = original;
    CHECK(function_param_flow_no_store_graph(store, root));
    size_t budget = store->work_budget;
    store->work_budget = 1; /* Two reached members exceed this fresh boundary. */
    CHECK(!function_param_flow_no_store_graph(store, root));
    CHECK(store->work_units == charged);
    store->work_budget = budget;
    CHECK(function_param_flow_no_store_graph(store, root));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_return_loan_currency(void)
{
    Lexer *lexer = lexer_create("func Echo(ref text:String)->String{return text;}func Save(ref output:Slot<String>)->Void{Write(output,Echo(\"literal\"));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) goto cleanup;
    SemanticParamPublicationView view;
    CHECK(semantic_param_publication_admitted_view(ctx, &view));
    CHECK(view.call_count == 1);
    if (view.call_count != 1) goto cleanup;
    PendingRefCall *row = &ctx->function_param_publications->calls[0];
    ASTNode *escape = NULL;
    CHECK(semantic_param_publication_ref_call_current(ctx, row));
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    PendingRefCall saved = *row;
    CHECK(!function_param_flow_return_loan_admitted(ctx, &view, &saved, &escape));
    for (unsigned fault = 0; fault < 8; fault++) {
        uint32_t program_id = program->stable_id;
        if (fault == 0) row->call_id++;
        if (fault == 1) row->site_id++;
        if (fault == 2) row->callee_id++;
        if (fault == 3) row->caller_id++;
        if (fault == 4) row->ordinal++;
        if (fault == 5) row->type = TYPE_INT;
        if (fault == 6) row->argument_count++;
        if (fault == 7) program->stable_id++;
        CHECK(!semantic_param_publication_ref_call_current(ctx, row));
        CHECK(!function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
        *row = saved; program->stable_id = program_id;
        CHECK(semantic_param_publication_ref_call_current(ctx, row));
        CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_backing_binding_growth_case(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 0);
    unsigned mask = function_param_flow_summary_for_param(ctx, decl, 1);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (store == NULL || index == NULL) goto cleanup;
    size_t count = ast_func_param_count(decl);
    bool *outputs = calloc(count, sizeof(*outputs));
    CHECK(outputs != NULL);
    if (outputs == NULL) goto cleanup;
    unsigned effects = 0;
    bool pending = true;
    CHECK(function_param_flow_returned_backing_effects(store, decl, 1, &effects, outputs, count, &pending, false));
    CHECK(!pending && outputs[0]);
    CHECK((effects & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    CHECK(index->backing_frames != NULL && index->backing_frame_count == count &&
        index->backing_frames[1] != NULL && index->backing_frames[1]->epoch != 0);
    free(outputs);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_backing_binding_growth(void)
{
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String)->Bool{output=text;return true;}");
    /* A later assignment grows a binding already read earlier in the loop. */
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{var first:String=\"\";var second:String=\"\";while flag{first=second;second=text;}output=first;return true;}");
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{using output as first;while flag{using first as second;second=text;}return true;}");
}

static void checked_backing_input_frames(void)
{
    for (unsigned reverse = 0; reverse < 2; reverse++) {
        Lexer *lexer = lexer_create("func Choose(inout output:String,left:String,right:String)->Void{output=left;}func Main()->Void{var output=\"\";Choose(output,\"left\",\"right\");}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (!typed) goto cleanup;
        /* Demand order must not make another input's empty row ready. */
        function_param_flow_summary_store_destroy(ctx);
        ASTNode *decl = ast_program_statement(program, 0);
        ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 1);
        SlotFunctionLookup lookup = {ctx, program};
        size_t first = reverse ? 2 : 1, second = reverse ? 1 : 2;
        unsigned first_mask = function_param_flow_summary_for_param(ctx, decl, first);
        CHECK(((first_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) == (first == 1));
        FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
        FunctionParamFlowProgramPointIndex *index = NULL;
        CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
        if (index == NULL || index->backing_frames == NULL) goto cleanup;
        CHECK(index->backing_frame_count == 3 && index->backing_frames[first] != NULL);
        CHECK(index->backing_frames[second] == NULL && index->backing_frames[0] == NULL);
        unsigned second_mask = function_param_flow_summary_for_param(ctx, decl, second);
        CHECK(((second_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) == (second == 1));
        CHECK(index->backing_frames[1] != index->backing_frames[2]);
        bool dependent = false;
        CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        dependent = true;
        CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 2, 0, &dependent) && !dependent);
        FunctionParamFlowBackingFact *frame = index->backing_frames[1];
        CHECK(frame != NULL && frame->origin_count == 1 && frame->destination_count == 3);
        if (frame == NULL) goto cleanup;
        for (unsigned fault = 0; fault < 7; fault++) {
            if (fault == 0) index->backing_frames[1] = NULL;
            if (fault == 1) frame->input_ordinal = 2;
            if (fault == 2) frame->input_id++;
            if (fault == 3) frame->destination_words++;
            if (fault == 4) frame->complete = false;
            if (fault == 5) frame->pending = true;
            if (fault == 6) frame->building = true;
            CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
            index->backing_frames[1] = frame;
            frame->input_ordinal = 1;
            frame->input_id = ast_func_param_stable_id(ast_func_param(decl, 1));
            frame->destination_words = 1;
            frame->complete = true; frame->pending = false; frame->building = false;
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        }
        for (size_t e = 0; e < store->count; e++) {
            FunctionParamFlowSummaryEntry *entry = &store->entries[e];
            if (entry->function_decl != decl || entry->param_index != 1) continue;
            FunctionParamFlowOutputProducer producer = entry->output_producer;
            entry->output_producer = FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED;
            CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
            entry->output_producer = producer;
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_return_loan_alias_currency(void)
{
    Lexer *lexer = lexer_create("func Echo(ref text:String)->String{return text;}func Save(ref output:Slot<String>)->Void{let held=\"static\";using held as first;using first as second;Write(output,Echo(second));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) goto cleanup;
    SemanticParamPublicationView view;
    CHECK(semantic_param_publication_admitted_view(ctx, &view));
    CHECK(view.call_count == 1);
    if (view.call_count != 1) goto cleanup;
    PendingRefCall *row = &ctx->function_param_publications->calls[0];
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, row->caller, &index));
    if (index == NULL) goto cleanup;
    ASTNode *alias = ast_block_statement(ast_func_body(row->caller), 1);
    FunctionParamFlowIndexClaim *claim = NULL;
    for (size_t c = 0; c < index->claims->count; c++)
        if (index->claims->rows[c].node == alias) claim = &index->claims->rows[c];
    CHECK(claim != NULL && claim->local_alias);
    if (claim == NULL) goto cleanup;
    ASTNode *escape = NULL;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    alias->data.let_decl.is_alias = false;
    CHECK(!function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    alias->data.let_decl.is_alias = true;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    claim->local_alias = false;
    CHECK(!function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    claim->local_alias = true;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    claim->child_ids[0]++;
    CHECK(!function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    claim->child_ids[0]--;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_local_array_push(void)
{
    Lexer *lexer = lexer_create("func Build(input:Array<String>)->Array<String>{let owned_rows:Array<String> = [];ArrayPush(owned_rows,input[0]);return owned_rows;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) {
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
        return;
    }
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *local = ast_block_statement(ast_func_body(decl), 0);
    ASTNode *initial = ast_let_initializer(local);
    ASTNode *call = ast_block_statement(ast_func_body(decl), 1);
    PublishedNode *published = NULL, *initial_value = NULL;
    for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++) {
        PublishedNode *row = &ctx->function_param_publications->nodes[i];
        if (row->node == call) published = row;
        if (row->node == initial) initial_value = row;
    }
    CHECK(published != NULL && initial_value != NULL);
    CHECK(semantic_param_publication_private_array_push(ctx, call, decl));
    unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
    CHECK((mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    if (published != NULL && initial_value != NULL && index != NULL) {
        for (unsigned fault = 0; fault < 9; fault++) {
            bool exclusive = published->private_exclusive, escaped = published->private_escaped;
            bool pending = published->private_pending, effect = call->data.call.semantic_collection_effect_set;
            uint32_t binding_id = published->private_binding_id;
            const Type *type = initial_value->string_array_type;
            if (fault == 0) published->private_exclusive = false;
            if (fault == 1) published->private_escaped = true;
            if (fault == 2) published->private_pending = true;
            if (fault == 3) local->data.let_decl.is_alias = true;
            if (fault == 4) initial->data.array_literal.count = 1;
            if (fault == 5) call->data.call.semantic_collection_effect_set = false;
            if (fault == 6) initial_value->string_array_type = NULL;
            if (fault == 7) published->private_binding_id++;
            if (fault == 8) published->private_initializer_id++;
            CHECK(!semantic_param_publication_private_array_push(ctx, call, decl));
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            published->private_exclusive = exclusive; published->private_escaped = escaped;
            published->private_pending = pending; local->data.let_decl.is_alias = false;
            initial->data.array_literal.count = 0; call->data.call.semantic_collection_effect_set = effect;
            initial_value->string_array_type = type; published->private_binding_id = binding_id;
            if (fault == 8) published->private_initializer_id--;
            CHECK(semantic_param_publication_private_array_push(ctx, call, decl));
            CHECK(function_param_flow_index_claims_current(ctx, index));
        }
        unsigned effects = 0; bool outputs[1] = {false}, pending = false;
        CHECK(!function_param_flow_returned_backing_effects(ctx->function_param_flow_summaries,
            decl, 0, &effects, outputs, 1, &pending, false));
        CHECK(function_param_flow_returned_backing_effects(ctx->function_param_flow_summaries,
            decl, 0, &effects, outputs, 1, &pending, true));
        CHECK((effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_array_push_closed_boundary(const char *source,
    size_t declaration, size_t statement, bool expected_candidate)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) {
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
        return;
    }
    ASTNode *decl = ast_program_statement(program, declaration);
    ASTNode *call = ast_block_statement(ast_func_body(decl), statement);
    CHECK(semantic_param_publication_private_array_push(ctx, call, decl) == expected_candidate);
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK(!function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
    unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_array_push_opaque_boundaries(void)
{
    checked_private_array_push_closed_boundary(
        "extern \"C\"{func Hold(rows:Array<String>)->Void;}func Build(input:Array<String>)->Int{let owned_rows:Array<String> = [];Hold(owned_rows);ArrayPush(owned_rows,input[0]);return ArrayLength(owned_rows);}", 1, 2, true);
    checked_private_array_push_closed_boundary(
        "extern \"C\"{func Hold(rows:Array<String>)->Void;}func Build(input:Array<String>)->Int{let owned_rows:Array<String> = [];ArrayPush(owned_rows,input[0]);Hold(owned_rows);return ArrayLength(owned_rows);}", 1, 1, true);
}

static void checked_empty_array_no_output(void)
{
    Lexer *lexer = lexer_create("func Inspect(ref text:String)->Bool{let rows:Array<Int> = [];return true;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *literal = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    FunctionParamFlowProgramPointIndex *index = NULL, *rechecked = NULL;
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    PublishedNode *published = NULL;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    for (size_t i = 0; store != NULL && i < store->node_capacity; i++)
        if (store->nodes[i].node == literal) published = &store->nodes[i];
    CHECK(index != NULL && published != NULL);
    if (index != NULL && published != NULL) {
        CHECK(!index->mutable_output_required);
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(!function_param_flow_mutable_output_required(ctx, decl, index, literal));
        PublishedNode saved = *published;
        uint32_t program_id = program->stable_id, literal_id = literal->stable_id;
        ASTNodeType shape = literal->type;
        for (unsigned fault = 0; fault < 9; fault++) {
            if (fault == 0) published->value_present = false;
            else if (fault == 1) published->value_shape = AST_NUMBER;
            else if (fault == 2) published->id = 0;
            else if (fault == 3) published->node = decl;
            else if (fault == 4) published->meta = true;
            else if (fault == 5) program->stable_id++;
            else if (fault == 6) literal->type = AST_TUPLE_LITERAL;
            else if (fault == 7) literal->stable_id++;
            else literal->data.array_literal.count = 1;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked));
            if (fault != 6)
                CHECK(function_param_flow_mutable_output_required(ctx, decl, index, literal));
            *published = saved;
            program->stable_id = program_id;
            literal->stable_id = literal_id;
            literal->type = shape;
            literal->data.array_literal.count = 0;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &rechecked) && rechecked == index);
            CHECK(!function_param_flow_mutable_output_required(ctx, decl, index, literal));
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);

    /* A nonempty literal still takes the conservative output path even when
     * its nested retaining call has a physical scalar result. */
    lexer = lexer_create("extern \"C\"{func Retain(value:Int)->Int;} func Inspect(value:Int)->Bool{let rows:Array<Int> = [Retain(value)];return true;}");
    parser = parser_create(lexer); program = parser_parse_program(parser);
    ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    decl = ast_program_statement(program, 1);
    index = NULL;
    (void)function_param_flow_summary_for_param(ctx, decl, 0);
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &index));
    CHECK(index != NULL && index->mutable_output_required);
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    checked_scalar_return_boundary("func Probe(ref text:String)->String{let rows:Array<Int> = [];return text;}", 0, false);
}

static void checked_named_escape_projection(void)
{
    Lexer *lexer = lexer_create(
        "extern \"C\" { func Hold(text:String)->Void; }"
        "func SelectedEcho(ref text:String)->String{return text;}"
        "func SelectedLeak(ref text:String)->Void{Hold(text);}"
        "func SelectedChannel(ref text:String)->Void{let channel:Channel<String> =Channel(1);channel<-text;}"
        "func Both(text:String,other:String)->Void{Log(SelectedEcho(text));SelectedLeak(other);SelectedChannel(text);}"
        "func Other(text:String)->Void{Log(SelectedEcho(text));}"
        "func SelectedProduce(ref text:String)->String{Hold(text);return \"literal\";}"
        "func Nested(text:String)->Void{Log(SelectedProduce(text));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    if (program == NULL || parser_has_error(parser)) {
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer); return;
    }
    SemanticContext *ctx = semantic_context_create();
    CHECK(!type_check_program(program, ctx));
    CHECK(semantic_param_publication_inputs_ready(ctx));
    ASTNode *both = semantic_find_function_decl_by_name(ctx, "Both");
    ASTNode *other = semantic_find_function_decl_by_name(ctx, "Other");
    ASTNode *nested = semantic_find_function_decl_by_name(ctx, "Nested");
    CHECK(both != NULL && other != NULL && nested != NULL);
    if (both != NULL && other != NULL && nested != NULL) {
        SlotFunctionLookup lookup = {ctx, program};
        SlotSummaryOrigin origin = {.function_decl = both, .param_index = 0, .param_name = "text"};
        ASTNode *body = ast_func_body(both);
        SlotEscapeEntry *entries = NULL;
        size_t count = 0, capacity = 0;
        bool failed = false;
        CHECK(collect_slot_escapes(body, &entries, &count, &capacity, &lookup, &origin, NULL, &failed));
        CHECK(!failed);
        const char *names[] = {"text", "other", "absent"};
        const unsigned expected[] = {SLOT_ESCAPE_RETURN | SLOT_ESCAPE_CHANNEL, SLOT_ESCAPE_CALL, SLOT_ESCAPE_NONE};
        for (size_t i = 0; i < 3; i++) {
            unsigned full = SLOT_ESCAPE_NONE;
            for (size_t row = 0; row < count; row++)
                if (strcmp(entries[row].name, names[i]) == 0) full |= entries[row].mask;
            failed = false;
            unsigned named = slot_escape_mask_in_program(body, names[i], &lookup, &origin, &failed);
            CHECK(!failed && named == full && named == expected[i]);
        }
        free(entries);
        SlotSummaryOrigin other_origin = {.function_decl = other, .param_index = 0, .param_name = "text"};
        failed = false;
        CHECK(slot_escape_mask_in_program(ast_func_body(other), "text", &lookup, &other_origin, &failed) == SLOT_ESCAPE_RETURN && !failed);
        SlotSummaryOrigin nested_origin = {.function_decl = nested, .param_index = 0, .param_name = "text"};
        ASTNode *outer = ast_block_statement(ast_func_body(nested), 0);
        const ASTNode *root = NULL;
        CHECK(outer != NULL && outer->type == AST_CALL);
        CHECK(slot_call_argument_storage_root(ast_call_argument(outer, 0), &root) && root == NULL);
        failed = false;
        CHECK(slot_escape_mask_in_program(ast_func_body(nested), "text", &lookup, &nested_origin, &failed) == SLOT_ESCAPE_CALL && !failed);
        ASTNode *unselected = ast_block_statement(body, 1);
        ASTNode *argument = ast_call_argument(unselected, 0);
        uint32_t binding = ast_identifier_binding_syntax_id(argument);
        CHECK(binding != 0);
        ast_identifier_set_binding_syntax_id(argument, 0);
        failed = false;
        CHECK(slot_escape_mask_in_program(body, "text", &lookup, &origin, &failed) ==
            (SLOT_ESCAPE_RETURN | SLOT_ESCAPE_CALL | SLOT_ESCAPE_CHANNEL) && failed);
        ast_identifier_set_binding_syntax_id(argument, binding);
        failed = false;
        CHECK(slot_escape_mask_in_program(body, "text", &lookup, &origin, &failed) == expected[0] && !failed);
        ASTNode *target = ast_call_callee(unselected);
        uint32_t identity = target->stable_id;
        target->stable_id = 0;
        failed = false;
        CHECK(slot_escape_mask_in_program(body, "text", &lookup, &origin, &failed) ==
            (SLOT_ESCAPE_RETURN | SLOT_ESCAPE_CALL | SLOT_ESCAPE_CHANNEL) && failed);
        target->stable_id = identity;
        failed = false;
        CHECK(slot_escape_mask_in_program(body, "text", &lookup, &origin, &failed) == expected[0] && !failed);
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

int main(void)
{
    checked_named_escape_projection();
    checked_failed_no_store_selection_epoch();
    checked_admitted_no_store_proof();
    checked_return_loan_currency();
    checked_return_loan_alias_currency();
    checked_backing_binding_growth();
    checked_backing_input_frames();
    checked_private_array_push_opaque_boundaries();
    checked_private_local_array_push();
    checked_empty_array_no_output();
    checked_static_string_backing();
    checked_nested_call_currentness();
    checked_secure_view_call_tuple();
    checked_nominal_result_relations();
    checked_nominal_result_growth();
    checked_nominal_unsupported_producers();
    type_system_init();
    checked_some_unwrap_result_relations();
    checked_nested_option_result_relations();
    checked_string_map_get_result_relation();
    checked_result_relation_reallocation();
    checked_string_map_get_payload_source();
    checked_callable_flow_projection_authority();
    checked_cast_and_consumed_edges();
    reached_producer_source("struct TextRecord{text:String;} extern \"C\"{func Keep(text:String)->Void;} func RetainingNumber(ref text:String)->Int{Keep(text);return 1;} func ReadRecord(ref record:TextRecord)->Bool{let observed=RetainingNumber(record.text) as Long;return observed>(0 as Long);} func Main()->Void{let record=TextRecord(Concat(\"heap\",\" text\"));Log(ReadRecord(record));}", false);
    fact_faults();
    phase_faults();
    produced_unknown_fault();
    checked_result_lifecycle();
    reached_producer_faults();
    checked_copy_category_landing();
    checked_no_output_header("func EchoText(text:String)->String{return text;}", 0);
    checked_no_output_header("struct Held{text:String} func EchoRecord(value:Held)->Held{return value;}", 1);
    checked_scalar_local_store();
    checked_dead_output_subtree();
    checked_local_assignment_replacement("func Rebind(text:String)->Bool{var held:String=text;held=\"fresh\";return StringLength(held)>0;}", 0);
    checked_local_assignment_replacement("func Rebind(text:String)->Bool{var held:Option<Int> =Some(0);held=Some(StringLength(text));return IsSome(held);}", 0);
    checked_local_assignment_replacement("struct State{count:Int} func Rebind(text:String)->Bool{var held=State(0);held=State(StringLength(text));return held.count>0;}", 1);
    checked_slot_assignment_operation(false);
    checked_slot_assignment_operation(true);
    checked_nonlocal_scalar_store("struct Cell{text:String} func Store(ref output:Cell,text:String)->Void{using output.text as target_alias;target_alias=text;}", 1);
    checked_nonlocal_scalar_store("func Store(text:String)->Void{let local_slot:Slot<String> =ClaimSlot();local_slot=text;Release(local_slot);}", 0);
    checked_shared_backing_validation();
    checked_zero_argument_output_graph();
    checked_unclosed_output_graph("func Probe(text:String)->Bool{let line=ReadLine();return StringLength(text)>0;}", 0);
    checked_unclosed_output_graph("extern \"C\"{func Opaque()->Int;} func Probe(text:String)->Bool{return StringLength(text)>Opaque();}", 1);
    checked_unclosed_output_graph("func Probe(text:String)->Bool{let indirect:func()->Int=()=>1;return StringLength(text)>indirect();}", 0);
    checked_unclosed_output_graph("extern \"C\"{func Opaque(value:Int)->Int;} func Probe(text:String)->Bool{return StringLength(text)>Opaque(1);}", 1);
    checked_unclosed_output_graph("func Probe(text:String)->Bool{let indirect:func(Int)->Int=(value:Int)=>value+StringLength(text);return StringLength(text)>indirect(1);}", 0);
    checked_unclosed_output_graph("class Box{let amount:Int;func Add(self,value:Int)->Int{return self.amount+value;}} func Probe(text:String,box:Box)->Bool{return StringLength(text)>box.Add(1);}", 1);
    checked_unclosed_output_graph("struct Cell{text:String} func Retain(inout output:Cell,text:String)->Int{output.text=text;return 1;} func Probe(inout output:Cell,text:String)->Bool{return StringLength(text)>Retain(output,text);}", 2);
    checked_nonlocal_scalar_store("struct Cell{text:String} func Store(cell:Cell,text:String)->Void{var alias=cell;alias.text=text;}", 1);
    checked_nonlocal_scalar_store("func SetNumber(inout destination:Int,value:Int)->Void{destination=value;}", 0);
    checked_nonlocal_scalar_store("struct Counter{count:Int} func Touch(ref text:String)->Bool{var cell=Counter(0);cell.count=StringLength(text);return cell.count>0;}", 1);
    checked_readonly_mutual_recursion();
    checked_parameter_source_kind("func Inspect(ref value:Slot<Int>)->Void{Log(Read(value));}", 0, true);
    checked_parameter_source_kind("func Inspect(ref value:String)->Void{Log(value);}", 0, false);
    checked_parameter_source_kind("struct TextRecord{text:String} func Inspect(ref value:TextRecord)->Void{Log(StringLength(value.text));}", 1, false);
    checked_parameter_source_kind("func Inspect(ref value:HashMap<String,Int>)->Void{Log(MapGet(value,\"key\"));}", 0, false);
    checked_parameter_source_kind("subject Hero{let hp:Int;} func Inspect(ref value:Hero)->Void{Log(value.hp);}", 1, false);
    checked_map_result_projection("func Snapshot(ref rows:HashMap<String,Int>)->Array<String>{return MapKeys(rows);}");
    checked_map_result_projection("func Snapshot(ref rows:HashMap<String,Int>)->Int{return MapGet(rows,\"key\");}");
    checked_slot_input_contract();
    checked_secure_slot_input_contract();
    retained_slot_helper_source();
    disjoint_inout_source();
    high_ordinal_inout_relations();
    const char *string_setter = "func SetText(inout destination:String,ref text:String)->Void{destination=text;}";
    const char *alias_store = "let observed=Observe(record.text);var staged:String=\"\";SetText(staged,observed);Write(retained,staged);";
    retained_inout_source(string_setter, alias_store, true);
    retained_inout_source("func SetText(inout destination:Held,ref text:String)->Void{destination.text=text;}", "let observed=Observe(record.text);var staged=Held(\"\");SetText(staged,observed);Write(retained,staged.text);", true);
    retained_inout_source(string_setter, "var staged:String=\"\";SetText(staged,Observe(record.text));Write(retained,staged);", true);
    retained_inout_source("func SetText(inout destination:String,text:String)->Void{destination=text;}", alias_store, true);
    retained_inout_source(string_setter, "let observed=Observe(record.text);var staged:String=\"\";SetText(staged,observed);SetText(staged,observed);Write(retained,staged);", true);
    retained_inout_source(string_setter, "let observed=Observe(record.text);var staged:String=\"\";if StringLength(record.text)>0{SetText(staged,observed);}Write(retained,staged);", true);
    retained_inout_source(string_setter, "let observed=Observe(record.text);var staged:String=\"\";if StringLength(record.text)>0{var staged:String=\"literal\";Log(staged);}SetText(staged,observed);Write(retained,staged);", false);
    retained_inout_source("func SetText(inout destination:String,ref text:String)->Void{if StringLength(text)>0{SetText(destination,text);}destination=text;}", alias_store, false);
    retained_inout_source("func SetText(inout destination:String,ref text:String)->Void{destination=text;} func ForwardText(inout destination:String,ref text:String)->Void{SetText(destination,text);}", "let observed=Observe(record.text);var staged:String=\"\";ForwardText(staged,observed);Write(retained,staged);", true);
    reached_producer_source("func SetText(inout destination:String,ref text:String)->Void{destination=text;} func Main()->Void{var destination:String=\"\";SetText(destination,\"literal\");Log(destination);}", true);
    reached_producer_source("func SetInt(inout destination:Int,value:Int)->Void{destination=value;} func Main()->Void{var destination:Int=0;SetInt(destination,7);Log(destination);}", true);
    reached_producer_source("func StoreText(ref output:Slot<String>,ref text:String)->Void{Write(output,text);} func Main()->Void{let output:Slot<String> = ClaimSlot();StoreText(output,\"literal\");Release(output);}", true);
    reached_producer_source("func StoreInt(ref output:Slot<Int>,value:Int)->Void{Write(output,value);} func Main()->Void{let output:Slot<Int> = ClaimSlot();StoreInt(output,7);Release(output);}", true);
    retained_backing_source("Keep(Observe(record.text));", false, false);
    retained_backing_source("let observed=Observe(record.text);Keep(observed);", true, false);
    retained_backing_source("let observed=Observe(record.text);Retain(observed);", true, false);
    retained_backing_source("Write(output,Observe(record.text));", false, true);
    retained_backing_source("let observed=Observe(record.text);Write(output,observed);", true, true);
    retained_backing_source("let observed=Observe(record.text);if StringLength(record.text)>0{let observed=\"literal\";Log(observed);}Keep(observed);", true, false);
    retained_backing_source("var observed=Observe(record.text);observed=Observe(record.text);Keep(observed);", true, false);
    retained_backing_source("var observed=Observe(record.text);if StringLength(record.text)>0{observed=\"literal\";}Keep(observed);", true, false);
    reached_producer_source("struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} extern \"c\"{func Keep(text:String)->Void;} func Retain(text:String)->Void{if StringLength(text)>0{Retain(text);}Keep(text);} func ReadRecord(ref record:TextRecord)->Bool{let observed=Observe(record.text);Retain(observed);return true;}", false);
    checked_scalar_return_boundary("struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} func ReadRecord(ref r:TextRecord)->Bool{let observed=Observe(r.text);return StringLength(observed)>0;} func Main()->Void{let r=TextRecord(Concat(\"x\",\"y\"));Log(ReadRecord(r));}", 2, true);
    checked_scalar_return_boundary("func Probe(ref text:String)->Void{Log(text);}", 0, true);
    checked_scalar_return_boundary("func Probe(text:String)->Bool{return true;}", 0, true);
    checked_scalar_return_boundary("func Probe(inout output:Int)->Bool{return true;}", 0, false);
    checked_scalar_return_boundary("struct TextRecord{text:String} func Probe(record:TextRecord)->Bool{return true;}", 1, true);
    checked_scalar_return_boundary("func Probe(ref text:String)->String{return text;}", 0, false);
    checked_scalar_return_boundary("func Probe(ref text:String)->Option<String>{return Some(\"literal\");}", 0, false);
    checked_scalar_return_boundary("struct Held{text:String} func Probe(ref text:String)->Held{return Held(\"literal\");}", 1, false);
    checked_scalar_return_boundary("func Probe(value:Int)->Bool{return value>0;}", 0, true);
    checked_physical_return_retaining_effects("struct TextRecord{text:String} extern \"C\"{func Keep(text:String)->Void;} func Probe(ref record:TextRecord,ignored:String)->Bool{Keep(record.text);return true;}", 2, SLOT_PARAM_SUMMARY_CALL_ESCAPE, false);
    checked_physical_return_retaining_effects("struct TextRecord{text:String} extern \"C\"{func Keep(text:String)->Void;} func Probe(ref record:TextRecord,ignored:TextRecord)->Int{Keep(record.text);return 1;}", 2, SLOT_PARAM_SUMMARY_CALL_ESCAPE, false);
    /* Historical c1cqhL exposed Type-cache0 versus raw CHANNEL32 for direct
       ref String. The flow-only query is tested separately above; the distinct
       direct-String borrow-diagnostic duties are not closed by that query. */
    checked_physical_return_retaining_effects("struct TextRecord{text:String} func Probe(ref record:TextRecord,ignored:String)->Bool{let channel:Channel<String> = Channel(1);channel<-record.text;return true;}", 1, SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE, false);
    checked_physical_return_retaining_effects("struct TextRecord{text:String} func Probe(ref record:TextRecord,ignored:TextRecord)->Int{let channel:Channel<String> = Channel(1);channel<-record.text;return 1;}", 1, SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE, false);
    checked_physical_return_mutable_output("func Probe(inout output:String,text:String)->Bool{output=text;return true;}", 0);
    checked_physical_return_mutable_output("struct TextRecord{text:String} func Probe(inout output:String,record:TextRecord)->Int{output=record.text;return 1;}", 1);
    reached_producer_source("struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} func LeakRecord(ref r:TextRecord)->String{return Observe(r.text);}", false);
    reached_producer_source("struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} func LeakRecord(ref r:TextRecord)->Option<String>{return Some(Observe(r.text));}", false);
    reached_producer_source("struct TextRecord{text:String} func Observe(ref text:String)->String{return text;} func LeakRecord(ref r:TextRecord)->TextRecord{return TextRecord(Observe(r.text));}", false);
    checked_allocating_reader("func Main()->Void { let text = CharAtN(\"abc\",3,1); Log(text); }");
    checked_allocating_reader("func Main()->Void { let text = SubstringWithLen(\"abc\",3,0,2); Log(text); }");
    checked_reader_reserved_name("func CharAtN(input:String,count:Int,index:Int)->String{return input;} func Main()->Void{let text=CharAtN(\"abc\",3,1);}");
    checked_reader_reserved_name("func SubstringWithLen(input:String,count:Int,start:Int,length:Int)->String{return input;} func Main()->Void{let text=SubstringWithLen(\"abc\",3,0,2);}");
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=StringIndexOf(record.text,\"x\");}", false);
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=StringContains(record.text,\"x\");}", false);
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=StringReplace(record.text,\"x\",\"y\");}", true);
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=Split(record.text,\"x\");}", true);
    checked_input_reader("struct TextRecord{ text:String, rows:HashMap<String,Int> } func Inspect(ref record:TextRecord)->Void{let result=MapHas(record.rows,record.text);}", false);
    checked_input_reader("struct Wrapped{value:Option<String>} func Inspect(ref record:Wrapped)->Void{let result=IsSome(record.value);}", false);
    checked_input_reader("struct TextRecord{text:String} func Inspect(ref record:TextRecord)->Void{let result=ToInt(record.text);}", false);
    checked_input_reader("struct TextRecord{text:String} func Inspect(ref record:TextRecord)->Void{let result=StringTrim(record.text);}", true);
    reached_producer_source("func Main()->Void{let text=CharAtN(\"abc\",1);}", false);
    reached_producer_source("func Main()->Void{let text=SubstringWithLen(\"abc\",0,1);}", false);
    checked_loop_execution_source("func Main()->Void { for i in 0..0 { Log(i); } }", false);
    checked_loop_execution_source("func Main()->Void { for i in 0..1 { Log(i); } }", true);
    checked_publication_faults();
    storage_root_faults();
    source_diagnostic("borrowed String MapGet return", "func ReadMapValue(ref values: HashMap<String, String>) -> String { return MapGet(values, \"key\"); } func Main() -> Void {}",
        PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE);
    source_diagnostic("unresolved HashMap<String, Unknown> MapGet receiver", "func Query(ref values: HashMap<String, Unknown>) -> Int { return MapGet(values, \"key\"); } func Main() -> Void {}",
        PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH);
    type_system_cleanup();
    printf("READONLY MAP FACT PHASE: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
