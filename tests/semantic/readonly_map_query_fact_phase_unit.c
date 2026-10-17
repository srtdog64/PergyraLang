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
#include "semantic/type_checker_flow_internal.h"
#include "semantic/type_checker_flow_universe.h"
#include "semantic/type_checker_flow_loop_summary.h"
#include "semantic/array_storage_deferred_preservation_owner.h"
#include "semantic/array_storage_element_lifetime_owner.h"
#include "semantic/array_storage_release_owner.h"
#include "semantic/type_system.h"
#include "semantic/diag_codes.h"
#include "semantic/function_param_publication_owner.h"
#include "semantic/function_param_publication_internal.h"
#include "semantic/typed_map_query_fact_owner.h"
#include "semantic/callable_capability_inference.h"
#include "semantic/slot_analyzer_internal.h"
#include "semantic/type_checker_ownership_internal.h"
#include "semantic/function_param_flow_summary_internal.h"
#include "semantic/indexed_string_borrow_owner.h"
#include "semantic/indexed_string_field_lineage_internal.h"
#include "semantic/diag_payload.h"
#include "semantic/collection_ownership_fact.h"
#include "semantic/collection_owned_element_requirement_owner.h"

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
    CHECK(ast_call_set_semantic_callee_declared_callable(f.forward_call, true));
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
    CHECK(semantic_param_publication_record_selected_callable(f.ctx, f.forward_call, f.reader));
    CHECK(semantic_param_publication_record_checked_call(f.ctx, f.forward_call));
    return f;
}

static bool register_reader(Fixture *f)
{
    Type *parameters[] = {f->map_type};
    return semantic_param_publication_record_routine(f->ctx, f->reader, 1, parameters, f->reader_type, true);
}
static bool register_caller(Fixture *f)
{
    Type *parameters[] = {f->map_type};
    return semantic_param_publication_record_routine(f->ctx, f->caller, 1, parameters, f->caller_type, true);
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
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call)); /* pending */
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
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    f.map_call->stable_id = saved;
    ASTNode *target = ast_call_callee(f.map_call);
    ASTNode *receiver = ast_call_argument(f.map_call, 0);
    saved = target->stable_id;
    target->stable_id++;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    target->stable_id = saved;
    saved = receiver->stable_id;
    receiver->stable_id++;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    receiver->stable_id = saved;
    ASTNode *replacement = ast_create_identifier("rows");
    replacement->stable_id = receiver->stable_id;
    f.map_call->data.call.arguments[0] = replacement;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    f.map_call->data.call.arguments[0] = receiver;
    ast_destroy(replacement);
    replacement = ast_create_identifier("MapGet");
    replacement->stable_id = target->stable_id;
    f.map_call->data.call.callee = replacement;
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    f.map_call->data.call.callee = target;
    ast_destroy(replacement);
    CHECK(ast_call_set_semantic_callee_is_stdlib(f.map_call, false));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    CHECK(ast_call_set_semantic_callee_is_stdlib(f.map_call, true));
    CHECK(ast_call_set_semantic_callee_decl_id(f.map_call, ast_node_stable_id(f.reader)));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    CHECK(ast_call_set_semantic_callee_decl_id(f.map_call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.map_call, 123));
    CHECK(!semantic_typed_map_query_receiver_read(f.ctx, f.map_call, 0));
    CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
    CHECK(ast_call_set_semantic_callee_value_binding_id(f.map_call, 0));
    semantic_typed_map_query_destroy(f.ctx);
    CHECK(!semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
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
        CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
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
        CHECK(semantic_typed_map_query_fact_issued(f.ctx, f.map_call));
        CHECK(!semantic_param_publication_call_analysis_unmodelled(f.ctx, f.map_call));
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
    CHECK(!type_check_program(program, produced));
    CHECK(produced->has_error);
    Type *resolved = semantic_host_resolve_type_ref(ast_func_param(decl, 0)->type, produced);
    /* The checked metadata has no Unknown argument binding: the entire
     * parameter is unresolved, not a produced HashMap with a concrete V. */
    CHECK(resolved == TYPE_UNKNOWN);
    bool required = false;
    CHECK(!semantic_param_publication_return_backing_required(produced, decl, 0, &required));
    CHECK(function_param_flow_summary_for_param(produced, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
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

static PublishedNode *published_node_for(SemanticContext *ctx, const ASTNode *node)
{
    FunctionParamPublicationStore *store = ctx != NULL ? ctx->function_param_publications : NULL;
    for (size_t i = 0; store != NULL && i < store->node_capacity; i++)
        if (store->nodes[i].node == node) return &store->nodes[i];
    return NULL;
}

/* A qualified unit variant is a namespace selection, not a runtime value
 * demanded from its qualifier. Both the direct receipt and the cached claim
 * must nevertheless reject every changed identity/payload premise. */
static void checked_unit_enum_selection_currency(void)
{
    const char *source =
        "enum Purpose{InspectOnly,Emit}"
        "func Probe(ref text:String,purpose:Purpose)->Option<Int>{"
        "let inspect=purpose==Purpose.InspectOnly;"
        "if inspect{return Some(StringLength(text));}return Some(0);}"
        "func Main()->Void{let text=Concat(\"heap\",\" value\");"
        "Log(UnwrapOption(Probe(text,Purpose.InspectOnly)));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    if (program == NULL || parser_has_error(parser)) goto cleanup;
    CHECK(type_check_program(program, ctx));
    if (ctx->has_error) goto cleanup;
    ASTNode *enum_decl = ast_program_statement(program, 0);
    ASTNode *probe = ast_program_statement(program, 1);
    ASTNode *comparison = ast_let_initializer(ast_block_statement(ast_func_body(probe), 0));
    ASTNode *member = ast_binary_right(comparison);
    ASTNode *qualifier = ast_member_object(member);
    CHECK(enum_decl != NULL && enum_decl->type == AST_ENUM_DECL);
    CHECK(member != NULL && member->type == AST_MEMBER_ACCESS);
    CHECK(qualifier != NULL && qualifier->type == AST_IDENTIFIER);
    CHECK(semantic_param_publication_checked_unit_enum_selection(ctx, member));
    PublishedNode *published = published_node_for(ctx, member);
    SemanticPublishedUnitEnumSelection *receipt =
        published != NULL ? published->unit_enum_selection : NULL;
    CHECK(published != NULL && receipt != NULL);
    FunctionParamFlowProgramPointIndex *index = NULL;
    unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
    CHECK(baseline != SLOT_PARAM_SUMMARY_ALL);
    CHECK(function_param_flow_program_point_index_build(
        ctx->function_param_flow_summaries, probe, &index));
    CHECK(index != NULL && function_param_flow_index_claims_current(ctx, index));
    if (published == NULL || receipt == NULL || index == NULL) goto cleanup;

    for (unsigned fault = 0; fault < 26; fault++) {
        uint32_t member_id = member->stable_id;
        uint32_t qualifier_id = qualifier->stable_id;
        uint32_t qualifier_binding = ast_identifier_binding_syntax_id(qualifier);
        uint32_t decl_id = enum_decl->stable_id;
        uint32_t generation = program->stable_id;
        char *member_name = member->data.member.name;
        char *qualifier_name = qualifier->data.identifier.name;
        char *decl_name = enum_decl->data.enum_decl.name;
        char *variant_name = enum_decl->data.enum_decl.variants[receipt->variant_ordinal];
        size_t variant_count = enum_decl->data.enum_decl.variant_count;
        size_t payload_count = enum_decl->data.enum_decl.variant_param_counts[receipt->variant_ordinal];
        ASTNode *member_qualifier = member->data.member.object;
        const Type *published_type = published->value_type;
        const Type *receipt_type = receipt->enum_type;
        char *type_name = ((Type *)receipt_type)->name;
        uint32_t receipt_member_id = receipt->member_id;
        uint32_t receipt_qualifier_id = receipt->qualifier_id;
        uint32_t receipt_binding = receipt->qualifier_binding_id;
        uint32_t receipt_decl_id = receipt->enum_decl_id;
        size_t receipt_ordinal = receipt->variant_ordinal;
        size_t receipt_variant_count = receipt->variant_count;
        size_t receipt_payload_count = receipt->payload_count;
        char *receipt_member_name = receipt->member_name;
        char *receipt_qualifier_name = receipt->qualifier_name;
        char *receipt_enum_name = receipt->enum_name;
        SemanticPublishedUnitEnumSelection *published_receipt = published->unit_enum_selection;
        ASTNode *replacement = NULL;
        switch (fault) {
            case 0: member->stable_id++; break;
            case 1: qualifier->stable_id++; break;
            case 2: member->data.member.name = "Emit"; break;
            case 3: qualifier->data.identifier.name = "Other"; break;
            case 4: ast_identifier_set_binding_syntax_id(qualifier,
                qualifier_binding == 0 ? 1 : 0); break;
            case 5:
                replacement = ast_create_identifier("Purpose");
                replacement->stable_id = qualifier->stable_id;
                ast_identifier_set_binding_syntax_id(replacement, qualifier_binding);
                member->data.member.object = replacement;
                break;
            case 6: published->value_type = TYPE_INT; break;
            case 7: receipt->enum_type = TYPE_INT; break;
            case 8: ((Type *)receipt_type)->name = "Other"; break;
            case 9: enum_decl->stable_id++; break;
            case 10: enum_decl->data.enum_decl.name = "Other"; break;
            case 11: receipt->variant_ordinal++; break;
            case 12: enum_decl->data.enum_decl.variants[receipt->variant_ordinal] = "Other"; break;
            case 13: enum_decl->data.enum_decl.variant_count--; break;
            case 14: enum_decl->data.enum_decl.variant_param_counts[receipt->variant_ordinal]++; break;
            case 15: receipt->payload_count++; break;
            case 16: published->unit_enum_selection = NULL; break;
            case 17: program->stable_id++; break;
            case 18: receipt->member_id++; break;
            case 19: receipt->qualifier_id++; break;
            case 20: receipt->qualifier_binding_id++; break;
            case 21: receipt->enum_decl_id++; break;
            case 22: receipt->variant_count++; break;
            case 23: receipt->member_name = "Emit"; break;
            case 24: receipt->qualifier_name = "Other"; break;
            case 25: receipt->enum_name = "Other"; break;
        }
        CHECK(!semantic_param_publication_checked_unit_enum_selection(ctx, member));
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        member->stable_id = member_id;
        qualifier->stable_id = qualifier_id;
        ast_identifier_set_binding_syntax_id(qualifier, qualifier_binding);
        enum_decl->stable_id = decl_id;
        program->stable_id = generation;
        member->data.member.name = member_name;
        qualifier->data.identifier.name = qualifier_name;
        enum_decl->data.enum_decl.name = decl_name;
        enum_decl->data.enum_decl.variant_count = variant_count;
        enum_decl->data.enum_decl.variant_param_counts[receipt_ordinal] = payload_count;
        enum_decl->data.enum_decl.variants[receipt_ordinal] = variant_name;
        member->data.member.object = member_qualifier;
        published->value_type = published_type;
        published->unit_enum_selection = published_receipt;
        receipt->enum_type = receipt_type;
        ((Type *)receipt_type)->name = type_name;
        receipt->member_id = receipt_member_id;
        receipt->qualifier_id = receipt_qualifier_id;
        receipt->qualifier_binding_id = receipt_binding;
        receipt->enum_decl_id = receipt_decl_id;
        receipt->variant_ordinal = receipt_ordinal;
        receipt->variant_count = receipt_variant_count;
        receipt->payload_count = receipt_payload_count;
        receipt->member_name = receipt_member_name;
        receipt->qualifier_name = receipt_qualifier_name;
        receipt->enum_name = receipt_enum_name;
        ast_destroy(replacement);
        CHECK(semantic_param_publication_checked_unit_enum_selection(ctx, member));
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_unit_enum_selection_boundaries(void)
{
    const char *sources[] = {
        /* A runtime member returning an enum remains ordinary evaluation. */
        "enum Purpose{InspectOnly,Emit}struct Holder{value:Purpose;}"
        "func Probe(ref holder:Holder)->Bool{"
        "return holder.value==Purpose.InspectOnly;}",
        /* Static lookup must not use a variant from another declaration via
         * the legacy bare-member fallback. */
        "enum First{One}enum Second{Two}func Probe()->Bool{return First.Two==Two;}",
        /* A payload constructor is not a unit selection. */
        "enum Parcel{Empty,Full(String)}func Probe()->Parcel{return Parcel.Full(\"x\");}",
        /* A local storage path still has its own value facts. */
        "enum Purpose{InspectOnly,Emit}func Probe()->Bool{"
        "let saved:Purpose=Purpose.InspectOnly;"
        "return saved==Purpose.InspectOnly;}"
    };
    for (size_t kind = 0; kind < sizeof(sources) / sizeof(sources[0]); kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        if (program == NULL || parser_has_error(parser))
            fprintf(stderr, "unit enum boundary parser case=%zu\n", kind);
        CHECK(program != NULL && !parser_has_error(parser));
        if (program == NULL || parser_has_error(parser)) goto boundary_cleanup;
        bool typed = type_check_program(program, ctx);
        if (!typed) fprintf(stderr, "unit enum boundary type case=%zu\n", kind);
        CHECK(typed);
        if (ctx->has_error) goto boundary_cleanup;
        ASTNode *member = NULL;
        if (kind == 0) {
            ASTNode *comparison = ast_return_value(ast_block_statement(
                ast_func_body(ast_program_statement(program, 2)), 0));
            member = ast_binary_left(comparison);
        } else if (kind == 1) {
            ASTNode *comparison = ast_return_value(ast_block_statement(
                ast_func_body(ast_program_statement(program, 2)), 0));
            member = ast_binary_left(comparison);
        } else if (kind == 2) {
            ASTNode *call = ast_return_value(ast_block_statement(
                ast_func_body(ast_program_statement(program, 1)), 0));
            member = ast_call_callee(call);
        } else {
            member = ast_let_initializer(ast_block_statement(
                ast_func_body(ast_program_statement(program, 1)), 0));
        }
        CHECK(member != NULL && member->type == AST_MEMBER_ACCESS);
        if (kind < 3)
            CHECK(!semantic_param_publication_checked_unit_enum_selection(ctx, member));
        else
            CHECK(semantic_param_publication_checked_unit_enum_selection(ctx, member));
boundary_cleanup:
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
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
    ASTNode *projection_caller = NULL, *projection_root = NULL;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member,
        &projection_caller, &projection_root));
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
    CHECK(semantic_indexed_string_independent_result(call, ctx));
    CHECK(ast_call_set_semantic_callee_is_stdlib(call, false));
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    CHECK(ast_call_set_semantic_callee_is_stdlib(call, true));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_TEXT_BUILDER_FINISH));
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    call->data.call.semantic_callee_builtin_kind_set = false;
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(ast_program_statement(program, 0))));
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 1));
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    CHECK(ast_call_set_semantic_callee_value_binding_id(call, 0));
    size_t arity = call->data.call.arg_count;
    call->data.call.arg_count = arity == 1 ? 0 : 1;
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    call->data.call.arg_count = arity;
    uint32_t id = call->stable_id;
    call->stable_id = 0;
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
    call->stable_id = id;
    CHECK(semantic_indexed_string_independent_result(call, ctx));
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
    CHECK(!semantic_indexed_string_independent_result(call, ctx));
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
        CHECK(!semantic_indexed_string_independent_result(call, ctx));
    } else if (semantic_indexed_string_independent_result(call, ctx)) {
        SemanticPublishedValue value = semantic_param_publication_value(ctx, call);
        CHECK(value.ok && value.origin_ready && value.copy_escape);
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
        CHECK(!semantic_indexed_string_independent_result(call, ctx));
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
    CHECK(!semantic_indexed_string_independent_result(observed, ctx));
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
    const char *payload_sources[] = {
        "subject BudgetLedger{let remaining:Int;let reserved:Int;}func Snapshot(ref budget:Slot<BudgetLedger>)->String{let cur:BudgetLedger=Read(budget);return \"remaining=\"+ToString(cur.remaining)+\" reserved=\"+ToString(cur.reserved);}",
        "subject TextLedger{let text:String;}extern \"C\"{func Keep(text:String)->Void;}func Snapshot(ref ledger:Slot<TextLedger>)->Void{let cur:TextLedger=Read(ledger);Keep(cur.text);}",
        "func Snapshot(ref source:Slot<String>)->String{let first=Read(source);let second=first;return second;}",
        "func Snapshot(ref source:Slot<String>,inout output:String)->Void{let first=Read(source);output=first;}",
        "subject TextLedger{let text:String;let count:Int;}func Snapshot(ref source:Slot<TextLedger>)->String{let cur=Read(source);let next=TextLedger(cur.text,cur.count+1);return next.text;}",
        "extern \"C\"{func Keep(text:String)->Void;}func Direct(ref source:ReadView<String>)->Void{Keep(Read(source));}func Alias(ref source:ReadView<String>)->Void{let first=source;let second=first;Keep(Read(second));}",
        "func Snapshot(ref source:ReadView<String>)->String{let first=source;let second=first;return Read(second);}",
        "func Snapshot(ref source:ReadView<String>,inout output:String)->Void{let text=Read(source);output=text;}",
        "extern \"C\"{func Keep(text:String)->Void;}func Snapshot(source:ReadView<String>)->Void{let text=Read(source);Keep(text);}"
    };
    for (unsigned retaining = 0; retaining < sizeof(payload_sources) / sizeof(payload_sources[0]); retaining++) {
        lexer = lexer_create(payload_sources[retaining]); parser = parser_create(lexer);
        program = parser_parse_program(parser); ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (retaining != 0) {
            CHECK(!typed && ctx->has_error);
            bool refused = false;
            for (size_t i = 0; i < ctx->diagnostic_count; i++) {
                const Diagnostic *diagnostic = ctx->diagnostics[i];
                if (diagnostic->level != DIAG_ERROR) continue;
                bool exact = diagnostic->code != NULL && diagnostic->cause_ir != NULL &&
                    strcmp(diagnostic->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0 &&
                    strcmp(diagnostic->cause_ir, PGY_CAUSE_BORROW_ESCAPE) == 0;
                CHECK(exact); refused |= exact;
            }
            CHECK(refused);
        } else {
            CHECK(typed && !ctx->has_error);
            if (typed) {
                ASTNode *snapshot = ast_program_statement(program, 1);
                ASTNode *read_call = ast_let_initializer(ast_block_statement(ast_func_body(snapshot), 0));
                SemanticPublishedValue value = semantic_param_publication_value(ctx, read_call);
                CHECK(value.ok && !value.origin_ready && !value.copy_escape);
                bool depends = false, retains = true;
                CHECK(semantic_param_publication_checked_result_relation(ctx, read_call, 0, &depends, &retains) && !depends && !retains);
                CHECK(semantic_param_publication_checked_backing_relation(ctx, read_call, 0, &depends, &retains) && depends && !retains);
                CHECK(!semantic_param_publication_checked_backing_relation(ctx, read_call, 1, &depends, &retains));
                unsigned mask = function_param_flow_summary_for_param(ctx, snapshot, 0);
                CHECK((mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
                PublishedNode *receipt = NULL, *receiver_row = NULL;
                ASTNode *read_receiver = ast_call_argument(read_call, 0);
                FunctionParamPublicationStore *publications = ctx->function_param_publications;
                for (size_t i = 0; i < publications->node_capacity; i++) {
                    if (publications->nodes[i].node == read_call) receipt = &publications->nodes[i];
                    if (publications->nodes[i].node == read_receiver) receiver_row = &publications->nodes[i];
                }
                CHECK(receipt != NULL && receiver_row != NULL);
                if (receipt != NULL && receiver_row != NULL) {
                    PublishedNode saved = *receipt, saved_receiver = *receiver_row;
                    ASTNode saved_call = *read_call;
                    for (unsigned fault = 0; fault < 6; fault++) {
                        if (fault == 0) receipt->result_relation = false;
                        if (fault == 1) receiver_row->value_type = TYPE_INT;
                        if (fault == 2) receipt->value_type = TYPE_STRING;
                        if (fault == 3) ast_call_set_semantic_callee_builtin_kind(read_call, BUILTIN_NOT_BUILTIN);
                        if (fault == 4) read_call->data.call.arg_count = 0;
                        if (fault == 5) receipt->value_present = false;
                        CHECK(!semantic_param_publication_checked_result_relation(ctx, read_call, 0, &depends, &retains));
                        CHECK(!semantic_param_publication_checked_backing_relation(ctx, read_call, 0, &depends, &retains));
                        CHECK(function_param_flow_summary_for_param(ctx, snapshot, 0) == SLOT_PARAM_SUMMARY_ALL);
                        *receipt = saved; *receiver_row = saved_receiver; *read_call = saved_call;
                        CHECK(semantic_param_publication_checked_result_relation(ctx, read_call, 0, &depends, &retains) && !depends && !retains);
                        CHECK(semantic_param_publication_checked_backing_relation(ctx, read_call, 0, &depends, &retains) && depends && !retains);
                        CHECK(function_param_flow_summary_for_param(ctx, snapshot, 0) == mask);
                    }
                }
            }
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    lexer = lexer_create("func Observe(ref source:ReadView<String>)->Void{Log(Read(source));}func Length(ref source:ReadView<String>)->Int{let first=source;let second=first;return StringLength(Read(second));}func Scalar(ref source:ReadView<Int>)->Int{return Read(source);}func NoRead(ref source:ReadView<String>)->Void{let first=source;let second=first;}");
    parser = parser_create(lexer); program = parser_parse_program(parser); ctx = semantic_context_create();
    bool views_parsed = program != NULL && !parser_has_error(parser);
    CHECK(views_parsed);
    bool views_typed = views_parsed && type_check_program(program, ctx);
    CHECK(views_typed && !ctx->has_error);
    if (views_typed) for (size_t i = 0; i < 4; i++) {
        ASTNode *view_decl = ast_program_statement(program, i);
        unsigned mask = function_param_flow_summary_for_param(ctx, view_decl, 0);
        CHECK((mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    lexer = lexer_create("subject TextLedger{let text:String;let count:Int;}func Update(ref source:Slot<TextLedger>)->Int{let rows:HashMap<String,Int> = MapNew();let cur=Read(source);let next=TextLedger(cur.text,cur.count+1);Write(source,next);return next.count;}");
    parser = parser_create(lexer); program = parser_parse_program(parser); ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (typed) {
        ASTNode *decl = ast_program_statement(program, 1), *body = ast_func_body(decl);
        ASTNode *map = ast_block_statement(body, 0), *map_call = ast_let_initializer(map);
        ASTNode *write = ast_block_statement(body, 3);
        CHECK(semantic_param_publication_empty_map_binding(ctx, map));
        CHECK(semantic_param_publication_checked_slot_write(ctx, write));
        unsigned baseline = function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK((baseline & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        PublishedNode *map_row = NULL, *write_row = NULL;
        FunctionParamPublicationStore *publications = ctx->function_param_publications;
        for (size_t i = 0; i < publications->node_capacity; i++) {
            if (publications->nodes[i].node == map) map_row = &publications->nodes[i];
            if (publications->nodes[i].node == write) write_row = &publications->nodes[i];
        }
        CHECK(map_row != NULL && write_row != NULL && write_row->arguments != NULL && write_row->argument_count >= 2);
        if (map_row != NULL && write_row != NULL && write_row->arguments != NULL && write_row->argument_count >= 2) {
            PublishedNode saved_map = *map_row, saved_write = *write_row;
            ASTNode saved_call = *write;
            ASTNode *map_target = ast_call_callee(map_call);
            char *map_name = map_target->data.identifier.name;
            uint32_t map_binding = ast_call_semantic_callee_value_binding_id(map_call);
            ASTNode *write_receiver = ast_call_argument(write, 0);
            uint32_t write_binding = ast_identifier_binding_syntax_id(write_receiver);
            PublishedCallArgument payload = write_row->arguments[1];
            for (unsigned fault = 0; fault < 8; fault++) {
                if (fault == 0) map_row->empty_map_binding = false;
                if (fault == 1) map_target->data.identifier.name = "NotMapNew";
                if (fault == 2) map_row->value_type = TYPE_INT;
                if (fault == 3) write_row->slot_write = false;
                if (fault == 4) write_row->arguments[1].type = TYPE_INT;
                if (fault == 5) write->data.call.arg_count = 1;
                if (fault == 6) ast_call_set_semantic_callee_value_binding_id(map_call, map_binding + 1);
                if (fault == 7) ast_identifier_set_binding_syntax_id(write_receiver, write_binding + 1);
                if (fault < 3 || fault == 6) CHECK(!semantic_param_publication_empty_map_binding(ctx, map));
                else CHECK(!semantic_param_publication_checked_slot_write(ctx, write));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
                *map_row = saved_map; *write_row = saved_write; *write = saved_call;
                map_target->data.identifier.name = map_name;
                ast_call_set_semantic_callee_value_binding_id(map_call, map_binding);
                ast_identifier_set_binding_syntax_id(write_receiver, write_binding);
                write_row->arguments[1] = payload;
                CHECK(semantic_param_publication_empty_map_binding(ctx, map));
                CHECK(semantic_param_publication_checked_slot_write(ctx, write));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == baseline);
            }
        }
        ASTNode *construction = ast_let_initializer(ast_block_statement(body, 2));
        PublishedNode *construction_row = NULL;
        for (size_t i = 0; i < publications->node_capacity; i++)
            if (publications->nodes[i].node == construction) construction_row = &publications->nodes[i];
        CHECK(construction_row != NULL && construction_row->nominal_result != NULL);
        if (construction_row != NULL && construction_row->nominal_result != NULL) {
            SemanticNominalResultSnapshot *mapping = construction_row->nominal_result;
            ASTNode *subject = ast_program_statement(program, 0);
            TypeNominalFlavor flavor = mapping->flavor;
            bool raw_struct = subject->data.class_decl.is_struct;
            size_t field_count = 0, arity = ast_call_arg_count(construction);
            ClassField **fields = ast_class_fields(subject, &field_count);
            CHECK(fields != NULL && field_count != 0);
            ASTNode *default_value = fields == NULL || field_count == 0 ? NULL : fields[0]->default_value;
            for (unsigned fault = 0; fault < 4; fault++) {
                if (fault == 0) mapping->flavor = TYPE_NOMINAL_CLASS;
                if (fault == 1) subject->data.class_decl.is_struct = !raw_struct;
                if (fault == 2) construction->data.call.arg_count = arity - 1;
                if (fault == 3 && fields != NULL && field_count != 0) fields[0]->default_value = ast_call_argument(construction, 0);
                CHECK(!semantic_param_publication_nominal_result_complete(ctx, construction));
                CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, construction));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
                mapping->flavor = flavor; subject->data.class_decl.is_struct = raw_struct;
                construction->data.call.arg_count = arity;
                if (fields != NULL && field_count != 0) fields[0]->default_value = default_value;
                CHECK(semantic_param_publication_nominal_result_complete(ctx, construction));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == baseline);
            }
        }
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    for (unsigned secure = 0; secure < 2; secure++) {
        lexer = lexer_create(secure == 0
            ? "func Main()->Void{let slot:Slot<String> = ClaimSlot<String>();Write(slot,Concat(\"heap\",\" text\"));let text=Read(slot);Log(text);Release(slot);}"
            : "func Main()->Void{let (slot,token)=ClaimSecureSlot<String>();Write(slot,Concat(\"heap\",\" text\"),token);let text=Read(slot,token);Log(text);Release(slot,token);}");
        parser = parser_create(lexer); program = parser_parse_program(parser); ctx = semantic_context_create();
        bool loan_parsed = program != NULL && !parser_has_error(parser);
        CHECK(loan_parsed);
        bool loan_typed = loan_parsed && type_check_program(program, ctx);
        CHECK(loan_typed && !ctx->has_error);
        if (loan_typed) {
            ASTNode *main_decl = ast_program_statement(program, 0), *main_body = ast_func_body(main_decl);
            ASTNode *birth_decl = ast_block_statement(main_body, 0);
            ASTNode *birth_call = secure == 0 ? ast_let_initializer(birth_decl) : birth_decl->data.let_destructure.initializer;
            ASTNode *read = ast_let_initializer(ast_block_statement(main_body, 2));
            ASTNode *receiver = ast_call_argument(read, 0);
            SemanticPublishedSlotBirth birth = {0};
            CHECK(semantic_param_publication_owned_slot_binding(ctx, birth_decl, &birth));
            CHECK(birth.slot_binding_id != 0 && birth.slot_type != NULL);
            CHECK((birth.token_type != NULL) == (secure != 0));
            FunctionParamPublicationStore *publications = ctx->function_param_publications;
            PendingReadPayloadLoan *loan = NULL;
            PublishedNode *read_row = NULL, *birth_row = NULL;
            for (size_t i = 0; i < publications->read_loan_count; i++)
                if (publications->read_loans[i].call == read) loan = &publications->read_loans[i];
            for (size_t i = 0; i < publications->node_capacity; i++) {
                if (publications->nodes[i].node == read) read_row = &publications->nodes[i];
                if (publications->nodes[i].node == birth_decl) birth_row = &publications->nodes[i];
            }
            CHECK(loan != NULL && read_row != NULL && birth_row != NULL);
            if (loan != NULL && read_row != NULL && birth_row != NULL) {
                uint32_t owned = 0, binding = ast_identifier_binding_syntax_id(receiver);
                CHECK(semantic_param_publication_read_payload_loan_current(ctx, loan, &owned) && owned == birth.slot_binding_id);
                SemanticParamPublicationView view = {0};
                CHECK(semantic_param_publication_admitted_view(ctx, &view));
                ASTNode *escape_site = NULL;
                CHECK(function_param_flow_read_payload_no_escape(ctx, &view, loan, &escape_site));
                PendingReadPayloadLoan saved = *loan;
                PublishedNode saved_read = *read_row, saved_birth = *birth_row;
                for (unsigned fault = 0; fault < 8; fault++) {
                    if (fault == 0) loan->call_id++;
                    if (fault == 1) loan->caller_id++;
                    if (fault == 2) loan->caller_shape = AST_PROGRAM;
                    if (fault == 3) loan->result_type = TYPE_INT;
                    if (fault == 4) loan->result_kind = TYPE_KIND_FUNCTION;
                    if (fault == 5) ast_identifier_set_binding_syntax_id(receiver, binding + 1);
                    if (fault == 6) read_row->value_present = false;
                    if (fault == 7) read_row->read_payload_loan_index = SIZE_MAX;
                    CHECK(!semantic_param_publication_read_payload_loan_current(ctx, loan, &owned));
                    CHECK(!function_param_flow_read_payload_no_escape(ctx, &view, loan, &escape_site));
                    *loan = saved; *read_row = saved_read;
                    ast_identifier_set_binding_syntax_id(receiver, binding);
                    CHECK(semantic_param_publication_read_payload_loan_current(ctx, loan, &owned) && owned == birth.slot_binding_id);
                    CHECK(function_param_flow_read_payload_no_escape(ctx, &view, loan, &escape_site));
                }
                GenericParam *generic = ast_generic_param_at(ast_call_generic_args(birth_call), 0);
                CHECK(generic != NULL);
                char *generic_name = generic == NULL ? NULL : generic->name;
                uint32_t destructured = secure == 0 ? 0 : birth_decl->data.let_destructure.local_binding_syntax_ids[0];
                for (unsigned fault = 0; fault < (secure == 0 ? 5u : 6u); fault++) {
                    if (fault == 0) birth_row->owned_slot_binding = false;
                    if (fault == 1) birth_row->slot_birth.slot_binding_id++;
                    if (fault == 2) birth_row->slot_birth_inner_type = TYPE_INT;
                    if (fault == 3) birth_row->slot_birth_secure = !saved_birth.slot_birth_secure;
                    if (fault == 4 && generic != NULL) generic->name = "Int";
                    if (fault == 5) birth_decl->data.let_destructure.local_binding_syntax_ids[0]++;
                    CHECK(!semantic_param_publication_owned_slot_binding(ctx, birth_decl, &birth));
                    CHECK(!function_param_flow_read_payload_no_escape(ctx, &view, loan, &escape_site));
                    *birth_row = saved_birth;
                    if (generic != NULL) generic->name = generic_name;
                    if (secure != 0) birth_decl->data.let_destructure.local_binding_syntax_ids[0] = destructured;
                    CHECK(semantic_param_publication_owned_slot_binding(ctx, birth_decl, &birth));
                    CHECK(function_param_flow_read_payload_no_escape(ctx, &view, loan, &escape_site));
                }
            }
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_stdlib_prefix_retention_currency(void)
{
    const char *source =
        "struct PrefixDecision{matched:Bool;}"
        "func PrefixReadingFactory()->PrefixDecision{"
        "let source=Concat(\"alpha\",\"beta\");let prefix=Concat(\"alpha\",\"\");"
        "let matched=SubStartsWithLen(source,StringLength(source),0,prefix);"
        "return PrefixDecision(matched);}"
        "func Probe(ref text:String)->Option<Int>{let decision=PrefixReadingFactory();"
        "if decision.matched{return Some(StringLength(text)+1);}return Some(StringLength(text));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (admitted) {
        ASTNode *factory = ast_program_statement(program, 1);
        ASTNode *probe = ast_program_statement(program, 2);
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(factory), 2));
        ASTNode *target = ast_call_callee(call);
        ASTNode *first = ast_call_argument(call, 0);
        ASTNode *last = ast_call_argument(call, 3);
        CHECK(call != NULL && call->type == AST_CALL && target != NULL &&
            first != NULL && last != NULL);
        for (size_t ordinal = 0; ordinal < 4; ordinal++) {
            bool borrowed = ordinal == 0 || ordinal == 3;
            CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, ordinal) == borrowed);
            CHECK(semantic_param_publication_string_argument_read_only(ctx, call, ordinal) == borrowed);
        }
        CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 4));
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        CHECK(store != NULL && function_param_flow_no_store_graph(store, factory));
        unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK((baseline & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        CHECK(function_param_flow_backing_rows_current(store, probe));

        ASTNode *concat = ast_let_initializer(ast_block_statement(ast_func_body(factory), 0));
        ASTNode *concat_target = ast_call_callee(concat);
        ASTNode *concat_first = ast_call_argument(concat, 0);
        PublishedNode *concat_row = NULL, *first_value = NULL;
        for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++) {
            PublishedNode *row = &ctx->function_param_publications->nodes[i];
            if (row->node == concat) concat_row = row;
            if (row->node == concat_first) first_value = row;
        }
        CHECK(concat != NULL && concat_target != NULL && concat_first != NULL &&
            concat_row != NULL && concat_row->argument_count == 2 &&
            concat_row->arguments != NULL && first_value != NULL);
        if (store != NULL && concat_target != NULL && concat_first != NULL &&
            concat_row != NULL && concat_row->argument_count == 2 &&
            concat_row->arguments != NULL && first_value != NULL) {
            CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, concat));
            PublishedNode saved_concat_row = *concat_row, saved_first_value = *first_value;
            PublishedCallArgument saved_first_argument = concat_row->arguments[0];
            ASTNode saved_concat = *concat;
            char *target_name = concat_target->data.identifier.name;
            uint32_t target_id = concat_target->stable_id;
            uint32_t first_id = concat_first->stable_id;
            uint32_t generation = program->stable_id;
            for (unsigned fault = 0; fault < 12; fault++) {
                if (fault == 0) concat_row->stdlib = false;
                if (fault == 1) concat_row->kind_present = false;
                if (fault == 2) concat_row->builtin = BUILTIN_STRING_LENGTH;
                if (fault == 3) concat_row->value_type = TYPE_INT;
                if (fault == 4) concat->data.call.arg_count = 1;
                if (fault == 5) concat_target->data.identifier.name = "Other";
                if (fault == 6) concat_target->stable_id++;
                if (fault == 7) concat_first->stable_id++;
                if (fault == 8) first_value->value_type = TYPE_INT;
                if (fault == 9) concat_row->arguments[0].type = TYPE_INT;
                if (fault == 10) concat_row->value_binding = 1;
                if (fault == 11) program->stable_id++;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, concat));
                bool no_store = function_param_flow_no_store_graph(store, factory);
                bool backing_current = function_param_flow_backing_rows_current(store, probe);
                if (no_store || backing_current)
                    fprintf(stderr, "prefix Concat fault=%u stale no_store=%d backing=%d\n",
                        fault, no_store, backing_current);
                CHECK(!no_store);
                CHECK(!backing_current);
                *concat_row = saved_concat_row;
                concat_row->arguments[0] = saved_first_argument;
                *first_value = saved_first_value;
                *concat = saved_concat;
                concat_target->data.identifier.name = target_name;
                concat_target->stable_id = target_id;
                concat_first->stable_id = first_id;
                program->stable_id = generation;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, concat));
                CHECK(function_param_flow_no_store_graph(store, factory));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
        }

        PublishedNode *published = NULL;
        for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++) {
            PublishedNode *row = &ctx->function_param_publications->nodes[i];
            if (row->node == call) published = row;
        }
        CHECK(published != NULL);
        if (published != NULL) {
            PublishedNode saved = *published;
            ASTNode saved_call = *call;
            uint32_t target_id = ast_node_stable_id(target);
            uint32_t first_id = ast_node_stable_id(first);
            uint32_t last_id = ast_node_stable_id(last);
            char *target_name = target->data.identifier.name;
            uint32_t generation = program->stable_id;
            for (unsigned fault = 0; fault < 10; fault++) {
                if (fault == 0) ast_call_set_semantic_callee_is_stdlib(call, false);
                if (fault == 1) ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(factory));
                if (fault == 2) ast_call_set_semantic_callee_value_binding_id(call, 1);
                if (fault == 3) call->data.call.arg_count = 3;
                if (fault == 4) target->data.identifier.name = "Other";
                if (fault == 5) target->stable_id++;
                if (fault == 6) first->stable_id++;
                if (fault == 7) last->stable_id++;
                if (fault == 8) published->stdlib = false;
                if (fault == 9) program->stable_id++;
                CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
                CHECK(!semantic_param_publication_argument_borrowed_for_call(ctx, call, 3));
                CHECK(!function_param_flow_no_store_graph(store, factory));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                *published = saved;
                *call = saved_call;
                target->data.identifier.name = target_name;
                target->stable_id = target_id;
                first->stable_id = first_id;
                last->stable_id = last_id;
                program->stable_id = generation;
                CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 0));
                CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, call, 3));
                CHECK(function_param_flow_no_store_graph(store, factory));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
        }
    }
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
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
    uint32_t kind = 0;
    CHECK(ast_call_semantic_callee_builtin_kind(call, &kind));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_TEXT_BUILDER_FINISH));
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    call->data.call.semantic_callee_builtin_kind_set = false;
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
    CHECK(ast_call_set_semantic_callee_builtin_kind(call, kind));
    uint32_t binding = ast_identifier_binding_syntax_id(receiver);
    CHECK(binding == ast_func_param_stable_id(ast_func_param(decl, 0)));
    ast_identifier_set_binding_syntax_id(receiver, binding + 1);
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
    ast_identifier_set_binding_syntax_id(receiver, binding);
    uint32_t id = call->stable_id;
    call->stable_id = 0;
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
    call->stable_id = id;
    id = program->stable_id;
    program->stable_id++;
    CHECK(!semantic_typed_map_query_result_independent(ctx, call));
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
    program->stable_id = id;
    CHECK(semantic_typed_map_query_result_independent(ctx, call));
    CHECK(semantic_typed_map_query_fact_issued(ctx, call));
    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
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

static void checked_local_owner_constructor_currency(void)
{
    const char *source =
        "func Build(text:String)->String{let allocator=AllocatorResult();let builder=TextBuilderNew(32);"
        "TextBuilderAppend(builder,text);let output=TextBuilderFinish(builder,allocator);AllocatorDestroy(allocator);return output;}"
        "func Probe(ref text:String)->Option<Int>{let output=Build(text);return Some(StringLength(output));}"
        "func LongCapacity()->Void{let builder=TextBuilderNew(32 as Long);TextBuilderDrop(builder);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *build = ast_program_statement(program, 0);
        ASTNode *probe = ast_program_statement(program, 1);
        ASTNode *calls[] = {
            ast_let_initializer(ast_block_statement(ast_func_body(build), 0)),
            ast_let_initializer(ast_block_statement(ast_func_body(build), 1))
        };
        (void)function_param_flow_summary_for_param(ctx, probe, 0);
        ASTNode *long_capacity = ast_let_initializer(ast_block_statement(
            ast_func_body(ast_program_statement(program, 2)), 0));
        CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, long_capacity));
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        for (size_t kind = 0; kind < 2; kind++) {
            ASTNode *call = calls[kind], *target = ast_call_callee(call);
            PublishedNode *claim = NULL;
            for (size_t i = 0; i < publication->node_capacity; i++)
                if (publication->nodes[i].node == call) claim = &publication->nodes[i];
            CHECK(claim != NULL);
            if (claim == NULL) continue;
            CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
            PublishedNode saved = *claim;
            uint32_t target_id = target->stable_id;
            for (unsigned fault = 0; fault < 9; fault++) {
                if (fault == 0) claim->value_type = TYPE_INT;
                if (fault == 1) claim->builtin = BUILTIN_STRING_LENGTH;
                if (fault == 2) target->stable_id++;
                if (fault == 3) claim->declaration = ast_node_stable_id(build);
                if (fault == 4) claim->value_binding = 1;
                if (fault == 5) claim->argument_count++;
                if (fault == 6) claim->meta = true;
                if (fault == 7) claim->value_present = false;
                if (fault == 8) claim->kind_present = false;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                bool current = function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe);
                if (current) fprintf(stderr, "local constructor kind=%zu fault=%u unexpectedly current\n", kind, fault);
                CHECK(!current);
                *claim = saved;
                target->stable_id = target_id;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
            }
        }
        ASTNode *capacity = ast_call_argument(calls[1], 0);
        PublishedNode *claim = NULL;
        for (size_t i = 0; i < publication->node_capacity; i++)
            if (publication->nodes[i].node == capacity) claim = &publication->nodes[i];
        CHECK(claim != NULL);
        if (claim != NULL) {
            PublishedNode saved = *claim;
            uint32_t id = capacity->stable_id;
            for (unsigned fault = 0; fault < 4; fault++) {
                if (fault == 0) claim->value_type = TYPE_STRING;
                if (fault == 1) capacity->stable_id++;
                if (fault == 2) claim->meta = true;
                if (fault == 3) claim->value_present = false;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, calls[1]));
                CHECK(!function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
                *claim = saved;
                capacity->stable_id = id;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, calls[1]));
                CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
            }
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_typed_string_result_constructor_currency(void)
{
    const char *source =
        "func SourceText()->String{return Concat(\"alph\",\"abet\");}"
        "func MakeSubstring()->String{return Substring(\"alphabet\",0,3);}"
        "func MakeReplace()->String{return StringReplace(SourceText(),\"a\",\"o\");}"
        "func MakeScalar()->String{return ToString(73);}"
        "func MakeBool()->String{return ToString(true);}"
        "func Probe(ref label:String)->Option<Int>{"
        "let first=MakeSubstring();let second=MakeReplace();"
        "let third=MakeScalar();let fourth=MakeBool();"
        "return Some(StringLength(first)+StringLength(second)+"
        "StringLength(third)+StringLength(fourth)+StringLength(label));}"
        "func Observe(ref label:String)->Void{let same=ToString(label);Log(same);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *probe = ast_program_statement(program, 5);
        ASTNode *calls[4];
        for (size_t i = 0; i < 4; i++)
            calls[i] = ast_return_value(ast_block_statement(
                ast_func_body(ast_program_statement(program, i + 1)), 0));
        ASTNode *identity = ast_let_initializer(ast_block_statement(
            ast_func_body(ast_program_statement(program, 6)), 0));
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        CHECK(store != NULL);
        unsigned summary = function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK((summary & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        CHECK(function_param_flow_backing_rows_current(store, probe));
        for (size_t i = 0; i < 4; i++) {
            CHECK(calls[i] != NULL && calls[i]->type == AST_CALL);
            CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, calls[i]));
            CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, calls[i]));
            CHECK(function_param_flow_no_store_graph(store, ast_program_statement(program, i + 1)));
            SemanticPublishedValue value = semantic_param_publication_value(ctx, calls[i]);
            /* Substring's exact allocating-reader ABI now issues an
             * independent String result; this is not Array element drop authority. */
            if (i == 0) CHECK(value.ok && value.origin_ready && value.copy_escape);
            else CHECK(value.ok && !value.origin_ready && !value.copy_escape);
        }
        CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, calls[0], 0));
        for (size_t p = 0; p < 3; p++)
            CHECK(semantic_param_publication_argument_borrowed_for_call(ctx, calls[1], p));
        CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, identity));
        CHECK(semantic_param_publication_call_analysis_unmodelled(ctx, identity));
        SemanticPublishedValue identity_value = semantic_param_publication_value(ctx, identity);
        CHECK(identity_value.ok && !identity_value.origin_ready && !identity_value.copy_escape);

        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        for (size_t i = 0; i < 4; i++) {
            ASTNode *call = calls[i];
            ASTNode *target = ast_call_callee(call);
            ASTNode *first = ast_call_argument(call, 0);
            PublishedNode *row = NULL;
            for (size_t n = 0; n < publication->node_capacity; n++)
                if (publication->nodes[n].node == call) row = &publication->nodes[n];
            CHECK(row != NULL && target != NULL && first != NULL);
            if (row == NULL || target == NULL || first == NULL) continue;
            PublishedNode saved = *row;
            PublishedCallArgument saved_first = row->arguments[0];
            ASTNode saved_call = *call, saved_target = *target, saved_arg = *first;
            uint32_t generation = program->stable_id;
            for (unsigned fault = 0; fault < 15; fault++) {
                if (fault == 0) row->value_type = TYPE_INT;
                if (fault == 1) row->value_shape = AST_IDENTIFIER;
                if (fault == 2) target->data.identifier.name = "OtherStringResult";
                if (fault == 3) ast_call_set_semantic_callee_is_stdlib(call, false);
                if (fault == 4) ast_call_set_semantic_callee_builtin_kind(call, BUILTIN_STRING_LENGTH);
                if (fault == 5) ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(probe));
                if (fault == 6) ast_call_set_semantic_callee_value_binding_id(call, 1);
                if (fault == 7) call->data.call.arg_count = 0;
                if (fault == 8) first->stable_id = 0;
                if (fault == 9) row->arguments[0].type = i >= 2 ? TYPE_STRING : TYPE_INT;
                if (fault == 10) row->arguments[0].id = 0;
                if (fault == 11) row->kind_present = false;
                if (fault == 12) row->target_shape = AST_MEMBER_ACCESS;
                if (fault == 13) program->stable_id++;
                if (fault == 14) ast_call_set_semantic_callee_declared_callable(call, true);
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                bool current = function_param_flow_backing_rows_current(store, probe);
                if (current) fprintf(stderr, "typed String result kind=%zu fault=%u unexpectedly current\n", i, fault);
                CHECK(!current);
                *row = saved; row->arguments[0] = saved_first;
                *call = saved_call; *target = saved_target; *first = saved_arg;
                program->stable_id = generation;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
            const char *result_name = ast_call_semantic_value_type_name(call);
            char *saved_result_name = result_name == NULL ? NULL : strdup(result_name);
            CHECK(result_name == NULL || saved_result_name != NULL);
            if (result_name == NULL || saved_result_name != NULL) {
                CHECK(ast_call_set_semantic_value_type_name_copy(call, "Int"));
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                if (saved_result_name != NULL)
                    CHECK(ast_call_set_semantic_value_type_name_copy(call, saved_result_name));
                else CHECK(ast_call_clear_semantic_value_type_name(call));
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
            free(saved_result_name);
            for (size_t p = 0; p < ast_call_arg_count(call); p++) {
                ASTNode *argument = ast_call_argument(call, p);
                PublishedNode *argument_row = NULL;
                for (size_t n = 0; n < publication->node_capacity; n++)
                    if (publication->nodes[n].node == argument)
                        argument_row = &publication->nodes[n];
                CHECK(argument_row != NULL);
                if (argument_row == NULL) continue;
                const Type *actual_type = argument_row->value_type;
                const Type *snapshot_type = row->arguments[p].type;
                const Type *wrong_type = actual_type == TYPE_STRING ? TYPE_INT : TYPE_STRING;
                row->arguments[p].type = wrong_type;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                row->arguments[p].type = snapshot_type;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_backing_rows_current(store, probe));
                argument_row->value_type = wrong_type;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                argument_row->value_type = actual_type;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_backing_rows_current(store, probe));
                uint32_t argument_id = argument->stable_id;
                argument->stable_id = 0;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                argument->stable_id = argument_id;
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
        }
        ASTNode *nested = ast_call_argument(calls[1], 0);
        CHECK(nested != NULL && nested->type == AST_CALL);
        if (nested != NULL && nested->type == AST_CALL) {
            const char *nested_type = ast_call_semantic_value_type_name(nested);
            char *saved_nested_type = nested_type == NULL ? NULL : strdup(nested_type);
            CHECK(nested_type == NULL || saved_nested_type != NULL);
            if (nested_type == NULL || saved_nested_type != NULL) {
                CHECK(ast_call_set_semantic_value_type_name_copy(nested, "Int"));
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, calls[1]));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                if (saved_nested_type != NULL)
                    CHECK(ast_call_set_semantic_value_type_name_copy(nested, saved_nested_type));
                else CHECK(ast_call_clear_semantic_value_type_name(nested));
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, calls[1]));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
            free(saved_nested_type);
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_selected_domain_callable_identity(void)
{
    const char *sources[2] = {
        "intent ToString(value:Int){success:value==7;failure:false;}"
        "func Main()->Void{let completed:Bool=ToString(7);Log(completed);}",
        "event ToString(value:Int);"
        "func Main()->Void{ToString(7);}",
    };
    const ASTNodeType declaration_kinds[2] = {AST_INTENT_DECL, AST_EVENT_DECL};
    for (size_t i = 0; i < 2; i++) {
        Lexer *lexer = lexer_create(sources[i]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        SemanticContext *ctx = semantic_context_create();
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (typed) {
            ASTNode *declaration = ast_program_statement(program, 0);
            ASTNode *main = ast_program_statement(program, 1);
            ASTNode *body = main == NULL ? NULL : ast_func_body(main);
            ASTNode *statement = body == NULL ? NULL : ast_block_statement(body, 0);
            ASTNode *call = i == 0 && statement != NULL
                ? ast_let_initializer(statement) : statement;
            uint32_t builtin = UINT32_MAX;
            CHECK(declaration != NULL && declaration->type == declaration_kinds[i]);
            CHECK(main != NULL && main->type == AST_FUNC_DECL);
            CHECK(call != NULL && call->type == AST_CALL);
            if (declaration != NULL && call != NULL && call->type == AST_CALL) {
                CHECK(ast_call_semantic_callee_declared_callable(call));
                CHECK(!ast_call_semantic_callee_is_stdlib(call));
                CHECK(ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(declaration));
                CHECK(ast_call_semantic_callee_value_binding_id(call) == 0);
                CHECK(ast_call_semantic_callee_builtin_kind(call, &builtin)
                    && builtin == BUILTIN_NOT_BUILTIN);
            }
        }
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_selected_namespace_callable_identity(void)
{
    Lexer *lexer = lexer_create(
        "func Tools_Render(value:Int)->Int{return value+1;}"
        "func Main()->Void{Log(Tools.Render(4));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *decl = ast_program_statement(program, 0);
        ASTNode *main = ast_program_statement(program, 1);
        ASTNode *statement = main == NULL ? NULL
            : ast_block_statement(ast_func_body(main), 0);
        ASTNode *call = statement == NULL ? NULL
            : ast_call_argument(statement, 0);
        Symbol *selected = scope_lookup(ctx->scope, "Tools_Render");
        uint32_t id = decl == NULL ? 0 : ast_node_stable_id(decl);
        CHECK(decl != NULL && decl->type == AST_FUNC_DECL &&
            strcmp(ast_declaration_name(decl), "Tools_Render") == 0);
        CHECK(call != NULL && call->type == AST_CALL);
        CHECK(selected != NULL && selected->kind == SYMBOL_FUNCTION &&
            selected->decl_syntax_id == id && id != 0);
        if (decl != NULL && call != NULL && call->type == AST_CALL &&
            selected != NULL && id != 0) {
            CHECK(ast_call_semantic_callee_decl_id(call) == id);
            CHECK(semantic_host_index_find_selected_callable(ctx,
                SYMBOL_FUNCTION, id, "Tools_Render") == decl);
            CHECK(semantic_host_index_find_selected_callable(ctx,
                SYMBOL_FUNCTION, id, "Tools.Render") == NULL);
            CHECK(semantic_host_index_find_selected_callable(ctx,
                SYMBOL_FUNCTION, id + 1, "Tools_Render") == NULL);
            CHECK(semantic_host_index_find_selected_callable(ctx,
                SYMBOL_FUNCTION, 0, "Tools_Render") == NULL);
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_selected_host_method_formal_currency(void)
{
    const char *sources[] = {
        "subject Unit{let n:Int;}"
        "zone Area{subject slot unit:Unit "
        "func Pick(self,value:Int)->Int{return value;}"
        "func Other(self,value:Int)->Int{return value+1;}}"
        "func Main()->Void{let area=Area(Unit(1));"
        "let answer=area.Pick(7);Log(answer);}",
        "zone Area{}"
        "world Domain{zone area:Area "
        "func Pick(self,value:Int)->Int{return value;}"
        "func Other(self,value:Int)->Int{return value+1;}}"
        "func Main()->Void{let domain=Domain(Area());"
        "let answer=domain.Pick(7);Log(answer);}",
        "subject Unit{let n:Int;}"
        "relation Link for source:Unit,target:Unit{"
        "func Pick(self,value:Int)->Int{return value;}"
        "func Other(self,value:Int)->Int{return value+1;}}"
        "func Main()->Void{let link=Link(Unit(1),Unit(2));"
        "let answer=link.Pick(7);Log(answer);}",
    };
    const ASTNodeType kinds[] = {AST_ZONE_DECL, AST_WORLD_DECL, AST_RELATION_DECL};
    for (size_t route = 0; route < 3; route++) {
        Lexer *lexer = lexer_create(sources[route]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        SemanticContext *ctx = semantic_context_create();
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (typed) {
            ASTNode *host = ast_program_statement(program, 1);
            ASTNode *main = ast_program_statement(program, 2);
            ASTNode *statement = main == NULL ? NULL
                : ast_block_statement(ast_func_body(main), 1);
            ASTNode *call = statement == NULL ? NULL : ast_let_initializer(statement);
            size_t count = 0, formal = SIZE_MAX;
            ASTNode **methods = semantic_host_decl_methods(host, &count);
            CHECK(host != NULL && host->type == kinds[route]);
            CHECK(methods != NULL && count == 2 && methods[0] != NULL && methods[1] != NULL);
            CHECK(call != NULL && call->type == AST_CALL);
            if (methods != NULL && count == 2 && methods[0] != NULL &&
                methods[1] != NULL && call != NULL && call->type == AST_CALL) {
                uint32_t pick_id = ast_node_stable_id(methods[0]);
                uint32_t other_id = ast_node_stable_id(methods[1]);
                CHECK(pick_id != 0 && other_id != 0 && pick_id != other_id);
                CHECK(ast_call_semantic_callee_decl_id(call) == pick_id);
                CHECK(semantic_param_publication_direct_callee(ctx, call) == methods[0]);
                CHECK(semantic_param_publication_formal_for_actual(
                    ctx, call, 0, &formal) && formal == 1);
                CHECK(semantic_param_publication_direct_callee(ctx, call) != methods[1]);
                CHECK(ast_call_set_semantic_callee_decl_id(call, other_id));
                CHECK(semantic_param_publication_direct_callee(ctx, call) == NULL);
                CHECK(!semantic_param_publication_formal_for_actual(ctx, call, 0, &formal));
                CHECK(ast_call_set_semantic_callee_decl_id(call, pick_id));
                CHECK(semantic_param_publication_direct_callee(ctx, call) == methods[0]);
                CHECK(semantic_param_publication_formal_for_actual(
                    ctx, call, 0, &formal) && formal == 1);
            }
        }
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_readonly_receiver_result_currency(void)
{
    const char *source =
        "class Formatter{let year:Int;"
        "func Format(self)->String{return ToString(year)+\"x\";}}"
        "func FormatWrapper(formatter:Formatter)->String{return formatter.Format();}"
        "func Inspect(ref label:String)->Option<Int>{let formatter=Formatter(7);"
        "let formatted=FormatWrapper(formatter);"
        "return Some(StringLength(formatted)+StringLength(label));}"
        "func Main()->Void{let label=Concat(\"q\",\"\");"
        "Log(UnwrapOption(Inspect(label)));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *host = ast_program_statement(program, 0);
        ASTNode *wrapper = ast_program_statement(program, 1);
        ASTNode *inspect = ast_program_statement(program, 2);
        ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(wrapper), 0));
        ASTNode *target = ast_call_callee(call);
        ASTNode *receiver = target == NULL ? NULL : ast_member_object(target);
        size_t method_count = 0;
        ASTNode **methods = semantic_host_decl_methods(host, &method_count);
        CHECK(methods != NULL && method_count == 1 && methods[0] != NULL);
        CHECK(call != NULL && call->type == AST_CALL && receiver != NULL);
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        CHECK(store != NULL);
        if (store != NULL && methods != NULL && method_count == 1 &&
            methods[0] != NULL && call != NULL && call->type == AST_CALL &&
            receiver != NULL) {
            (void)function_param_flow_summary_for_param(ctx, inspect, 0);
            CHECK(function_param_flow_receiver_readonly_current(store, call, true) == methods[0]);
            FunctionParamFlowProgramPointIndex *index = NULL;
            CHECK(function_param_flow_program_point_index_build(store, wrapper, &index) &&
                index != NULL && index->backing_frames != NULL &&
                index->backing_frame_count == 1 && index->backing_frames[0] != NULL);
            /* The member call is sealed while analysing wrapper formal 0;
             * a declared-result frame is only produced on separate demand. */
            FunctionParamFlowBackingFact *fact = index == NULL ||
                index->backing_frames == NULL || index->backing_frame_count != 1
                ? NULL : index->backing_frames[0];
            FunctionParamFlowBackingNode *row = NULL;
            if (fact != NULL)
                for (size_t n = 0; n < fact->node_count; n++)
                    if (fact->nodes[n].node == call) row = &fact->nodes[n];
            CHECK(row != NULL && row->receiver == receiver);
            CHECK(function_param_flow_backing_rows_current(store, wrapper));
            CHECK(function_param_flow_backing_rows_current(store, inspect));
            if (row != NULL && row->receiver == receiver) {
                uint32_t old_id = row->receiver_id;
                const Type *old_type = row->receiver_type;
                uint32_t old_callee = row->receiver_callee_id;
                uint32_t old_formal = row->receiver_formal_id;
                const ASTNode *old_receiver = row->receiver;
                for (unsigned fault = 0; fault < 5; fault++) {
                    if (fault == 0) row->receiver_id++;
                    if (fault == 1) row->receiver_type = TYPE_STRING;
                    if (fault == 2) row->receiver_callee_id++;
                    if (fault == 3) row->receiver_formal_id++;
                    if (fault == 4) row->receiver = call;
                    CHECK(!function_param_flow_backing_rows_current(store, wrapper));
                    row->receiver = old_receiver;
                    row->receiver_id = old_id;
                    row->receiver_type = old_type;
                    row->receiver_callee_id = old_callee;
                    row->receiver_formal_id = old_formal;
                    CHECK(function_param_flow_backing_rows_current(store, wrapper));
                }
            }
            size_t at = function_param_flow_find(store, ast_node_stable_id(methods[0]), 0);
            CHECK(at != SIZE_MAX);
            if (at != SIZE_MAX) {
                FunctionParamFlowSummaryEntry *entry = &store->entries[at];
                unsigned mask = entry->mask;
                bool ready = entry->output_dependencies_ready;
                FunctionParamFlowSummaryState state = entry->state;
                CHECK(entry->output_count == 1 && entry->output_dependencies != NULL);
                entry->mask |= SLOT_PARAM_SUMMARY_WRITE;
                CHECK(function_param_flow_receiver_readonly_current(store, call, false) == NULL);
                entry->mask = mask;
                CHECK(function_param_flow_receiver_readonly_current(store, call, false) == methods[0]);
                entry->output_dependencies_ready = false;
                CHECK(function_param_flow_receiver_readonly_current(store, call, false) == NULL);
                entry->output_dependencies_ready = ready;
                CHECK(function_param_flow_receiver_readonly_current(store, call, false) == methods[0]);
                if (entry->output_count == 1 && entry->output_dependencies != NULL) {
                    bool dependent = entry->output_dependencies[0];
                    entry->output_dependencies[0] = true;
                    CHECK(function_param_flow_receiver_readonly_current(store, call, false) == NULL);
                    entry->output_dependencies[0] = dependent;
                    CHECK(function_param_flow_receiver_readonly_current(store, call, false) == methods[0]);
                }
                entry->state = FUNCTION_PARAM_FLOW_COMPUTING;
                CHECK(function_param_flow_receiver_readonly_current(store, call, false) == NULL);
                entry->state = state;
                CHECK(function_param_flow_receiver_readonly_current(store, call, false) == methods[0]);
            }
            uint32_t selected = ast_call_semantic_callee_decl_id(call);
            CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
            CHECK(!function_param_flow_backing_rows_current(store, wrapper));
            CHECK(ast_call_set_semantic_callee_decl_id(call, selected));
            CHECK(function_param_flow_backing_rows_current(store, wrapper));
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_owned_string_result_receipt_currency(void)
{
    for (unsigned route = 0; route < 2; route++) {
    char source[1024];
    snprintf(source, sizeof(source),
        "func Produce()->String{let allocator=AllocatorResult();let builder=TextBuilderNew(32);"
        "TextBuilderAppend(builder,\"heap text\");let output=TextBuilderFinish(builder,allocator);AllocatorDestroy(allocator);return output;}"
        "func Forward()->String{return Produce();}"
        "func Consumer(ref text:String)->Option<Int>{let rows:Array<String> = [];ArrayPush(rows,%s());"
        "let count=ArrayLength(rows)+StringLength(text);ArrayDropOwnedStrings(rows);return Some(count);}",
        route == 0 ? "Produce" : "Forward");
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *producer = ast_program_statement(program, 0);
        ASTNode *consumer = ast_program_statement(program, 2);
        ASTNode *birth = ast_block_statement(ast_func_body(producer), 0);
        ASTNode *finish = ast_let_initializer(ast_block_statement(ast_func_body(producer), 3));
        ASTNode *push = ast_block_statement(ast_func_body(consumer), 1);
        ASTNode *produce_call = ast_call_argument(push, 1);
        PublishedNode *birth_claim = NULL, *producer_claim = NULL;
        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        for (size_t i = 0; i < publication->node_capacity; i++) {
            if (publication->nodes[i].node == birth) birth_claim = &publication->nodes[i];
            if (publication->nodes[i].node == producer) producer_claim = &publication->nodes[i];
        }
        CHECK(birth_claim != NULL);
        uint32_t producer_id = 0;
        CHECK(semantic_param_publication_checked_finish_independent(ctx, finish));
        CHECK(semantic_collection_owned_string_call_result(produce_call, ctx, &producer_id));
        CHECK(producer_id == ast_node_stable_id(ast_program_statement(program, route)));
        bool private_push = semantic_param_publication_private_array_push(ctx, push, consumer);
        CHECK(private_push);
        if (birth_claim != NULL) {
            PublishedNode saved = *birth_claim;
            semantic_param_publication_invalidate_allocator_binding(ctx, ast_call_argument(finish, 1));
            CHECK(!semantic_param_publication_checked_finish_independent(ctx, finish));
            CHECK(!semantic_collection_owned_string_call_result(produce_call, ctx, &producer_id));
            CHECK(semantic_param_publication_private_array_push(ctx, push, consumer));
            /* Raw unit restoration is not a source-level un-invalidation. */
            *birth_claim = saved;
            CHECK(semantic_param_publication_checked_finish_independent(ctx, finish));
            CHECK(semantic_collection_owned_string_call_result(produce_call, ctx, &producer_id));
            CHECK(producer_id == ast_node_stable_id(ast_program_statement(program, route)));
            CHECK(semantic_param_publication_private_array_push(ctx, push, consumer));
        }
        CHECK(producer_claim != NULL && producer_claim->owned_string_result != NULL);
        if (producer_claim != NULL && producer_claim->owned_string_result != NULL) {
            PublishedOwnedStringResult *receipt = producer_claim->owned_string_result;
            CHECK(receipt->source_count != 0 && receipt->sources != NULL);
            if (receipt->source_count != 0 && receipt->sources != NULL) {
                PublishedOwnedStringResult saved = *receipt;
                PublishedCallArgument saved_source = receipt->sources[0];
                ASTNode *saved_body = ast_func_body(producer);
                for (unsigned fault = 0; fault < 10; fault++) {
                    if (fault == 0) receipt->body_id++;
                    if (fault == 1) receipt->sources[0].id++;
                    if (fault == 2) receipt->sources[0].type = TYPE_INT;
                    if (fault == 3) receipt->sources[0].shape = AST_NUMBER;
                    if (fault == 4) producer_claim->owned_string_result = NULL;
                    if (fault == 5) receipt->syntax = NULL;
                    if (fault == 6) producer->data.func_decl.body = ast_func_body(consumer);
                    if (fault == 7) receipt->sources[0].node = produce_call;
                    if (fault == 8) receipt->source_count++;
                    if (fault == 9) receipt->sealed_source_count++;
                    bool owned = semantic_collection_owned_string_call_result(produce_call, ctx, &producer_id);
                    if (owned) fprintf(stderr, "owned String receipt route=%u fault=%u unexpectedly current\n", route, fault);
                    CHECK(!owned);
                    CHECK(semantic_param_publication_private_array_push(ctx, push, consumer));
                    producer_claim->owned_string_result = receipt;
                    *receipt = saved;
                    receipt->sources[0] = saved_source;
                    producer->data.func_decl.body = saved_body;
                    CHECK(semantic_collection_owned_string_call_result(produce_call, ctx, &producer_id));
                    CHECK(producer_id == ast_node_stable_id(ast_program_statement(program, route)));
                    CHECK(semantic_param_publication_private_array_push(ctx, push, consumer));
                }
            }
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_finish_independence_currency(void)
{
    const char *source =
        "func Build(text:String)->String{let allocator=AllocatorResult();let builder=TextBuilderNew(32);"
        "TextBuilderAppend(builder,text);let output=TextBuilderFinish(builder,allocator);AllocatorDestroy(allocator);return output;}"
        "func Probe(ref text:String)->Option<Int>{let output=Build(text);return Some(StringLength(output));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *build = ast_program_statement(program, 0);
        ASTNode *probe = ast_program_statement(program, 1);
        ASTNode *birth = ast_block_statement(ast_func_body(build), 0);
        ASTNode *initializer = ast_let_initializer(birth);
        ASTNode *finish = ast_let_initializer(ast_block_statement(ast_func_body(build), 3));
        ASTNode *actual = ast_call_argument(finish, 1);
        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        PublishedNode *initial_claim = NULL, *actual_claim = NULL, *birth_claim = NULL;
        for (size_t i = 0; i < publication->node_capacity; i++) {
            if (publication->nodes[i].node == initializer) initial_claim = &publication->nodes[i];
            if (publication->nodes[i].node == actual) actual_claim = &publication->nodes[i];
            if (publication->nodes[i].node == birth) birth_claim = &publication->nodes[i];
        }
        CHECK(initial_claim != NULL && actual_claim != NULL && birth_claim != NULL);
        (void)function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK(semantic_param_publication_checked_finish_independent(ctx, finish));
        CHECK(semantic_indexed_string_independent_result(finish, ctx));
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
        if (initial_claim != NULL && actual_claim != NULL && birth_claim != NULL) {
            PublishedNode saved_initial = *initial_claim, saved_actual = *actual_claim;
            PublishedNode saved_birth = *birth_claim;
            uint32_t birth_id = birth->stable_id, init_id = initializer->stable_id;
            uint32_t binding = ast_identifier_binding_syntax_id(actual);
            for (unsigned fault = 0; fault < 11; fault++) {
                if (fault == 0) initializer->stable_id++;
                if (fault == 1) initial_claim->value_type = TYPE_INT;
                if (fault == 2) initial_claim->builtin = BUILTIN_ALLOCATOR_POOL;
                if (fault == 3) birth->stable_id++;
                if (fault == 4) ast_identifier_set_binding_syntax_id(actual, 0);
                if (fault == 5) actual_claim->value_type = TYPE_INT;
                if (fault == 6) actual_claim->value_present = false;
                if (fault == 7) initial_claim->meta = true;
                if (fault == 8) birth_claim->allocator_initializer_id++;
                if (fault == 9) birth_claim->allocator_type = TYPE_INT;
                if (fault == 10) semantic_param_publication_invalidate_allocator_binding(ctx, actual);
                CHECK(!semantic_param_publication_checked_finish_independent(ctx, finish));
                CHECK(!semantic_indexed_string_independent_result(finish, ctx));
                bool current = function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe);
                if (current) fprintf(stderr, "Finish currency fault=%u unexpectedly current\n", fault);
                CHECK(!current);
                *initial_claim = saved_initial;
                *actual_claim = saved_actual;
                /* Raw receipt corruption controls may restore a snapshot;
                 * user programs cannot reverse monotonic invalidation. */
                *birth_claim = saved_birth;
                birth->stable_id = birth_id;
                initializer->stable_id = init_id;
                ast_identifier_set_binding_syntax_id(actual, binding);
                CHECK(semantic_param_publication_checked_finish_independent(ctx, finish));
                CHECK(semantic_indexed_string_independent_result(finish, ctx));
                CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
            }
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_local_owner_constructor_nested_effect(void)
{
    const char *source =
        "extern \"C\"{func Keep(text:String)->Int;}"
        "func Main()->Void{let text=Concat(\"heap\",\" text\");let builder=TextBuilderNew(Keep(text));TextBuilderDrop(builder);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *decl = ast_program_statement(program, 1);
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(decl), 1));
        /* The intrinsic result is independent; evaluating capacity retains.
         * Its constructor query must not pretend to own that separate effect. */
        CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        CHECK(!function_param_flow_no_store_graph(store, decl));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_local_owner_map_constructor_currency(void)
{
    const char *source =
        "struct MapScope{values:HashMap<String,Int>;}"
        "func EmptyMapScope()->MapScope{let values:HashMap<String,Int> = MapNew();return MapScope(values);}"
        "func Probe(ref text:String)->Option<Int>{let scope=EmptyMapScope();"
        "return Some(MapSize(scope.values)+StringLength(text));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (typed) {
        ASTNode *factory = ast_program_statement(program, 1);
        ASTNode *probe = ast_program_statement(program, 2);
        ASTNode *map = ast_block_statement(ast_func_body(factory), 0);
        ASTNode *call = ast_let_initializer(map);
        ASTNode *target = ast_call_callee(call);
        CHECK(semantic_param_publication_empty_map_binding(ctx, map));
        CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        CHECK(store != NULL && function_param_flow_no_store_graph(store, factory));
        unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK((baseline & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        CHECK(function_param_flow_backing_rows_current(store, probe));

        PublishedNode *map_row = NULL, *call_row = NULL;
        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        for (size_t i = 0; i < publication->node_capacity; i++) {
            if (publication->nodes[i].node == map) map_row = &publication->nodes[i];
            if (publication->nodes[i].node == call) call_row = &publication->nodes[i];
        }
        CHECK(map_row != NULL && call_row != NULL && call_row->empty_map_declaration == map);
        if (map_row != NULL && call_row != NULL) {
            PublishedNode saved_map = *map_row, saved_call = *call_row;
            ASTNode saved_call_syntax = *call;
            char *saved_target_name = target->data.identifier.name;
            uint32_t generation = ast_node_stable_id(program);
            for (unsigned fault = 0; fault < 11; fault++) {
                if (fault == 0) map_row->empty_map_binding = false;
                if (fault == 1) map_row->map_type_id++;
                if (fault == 2) map_row->target_id++;
                if (fault == 3) call_row->empty_map_declaration = NULL;
                if (fault == 4) call_row->empty_map_declaration_id++;
                if (fault == 5) call_row->stdlib = false;
                if (fault == 6) call_row->value_type = TYPE_INT;
                if (fault == 7) call_row->argument_count = 1;
                if (fault == 8) target->data.identifier.name = "NotMapNew";
                if (fault == 9) ast_call_set_semantic_callee_value_binding_id(call, 1);
                if (fault == 10) program->stable_id++;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(!function_param_flow_no_store_graph(store, factory));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                *map_row = saved_map; *call_row = saved_call; *call = saved_call_syntax;
                target->data.identifier.name = saved_target_name;
                program->stable_id = generation;
                CHECK(semantic_param_publication_empty_map_binding(ctx, map));
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_no_store_graph(store, factory));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
            for (unsigned fault = 0; fault < 4; fault++) {
                if (fault == 0) map_row->map_key_type = NULL;
                if (fault == 1) map_row->map_key_type = TYPE_INT;
                if (fault == 2) map_row->map_value_type = NULL;
                if (fault == 3) map_row->map_value_type = TYPE_STRING;
                CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(!function_param_flow_no_store_graph(store, factory));
                CHECK(!function_param_flow_backing_rows_current(store, probe));
                *map_row = saved_map;
                CHECK(semantic_param_publication_empty_map_binding(ctx, map));
                CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                CHECK(function_param_flow_no_store_graph(store, factory));
                CHECK(function_param_flow_backing_rows_current(store, probe));
            }
            size_t annotation_entry = 0;
            bool annotation_found = false;
            for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
                if (ctx->type_resolution_metadata.keys[i] == ast_let_type(map)) {
                    annotation_entry = i;
                    annotation_found = true;
                    break;
                }
            }
            CHECK(annotation_found);
            if (annotation_found) {
                void *saved_type = ctx->type_resolution_metadata.values[annotation_entry];
                for (unsigned fault = 0; fault < 2; fault++) {
                    ctx->type_resolution_metadata.values[annotation_entry] =
                        fault == 0 ? NULL : TYPE_INT;
                    CHECK(!semantic_param_publication_checked_local_owner_constructor(ctx, call));
                    CHECK(!function_param_flow_no_store_graph(store, factory));
                    CHECK(!function_param_flow_backing_rows_current(store, probe));
                    ctx->type_resolution_metadata.values[annotation_entry] = saved_type;
                    CHECK(semantic_param_publication_empty_map_binding(ctx, map));
                    CHECK(semantic_param_publication_checked_local_owner_constructor(ctx, call));
                    CHECK(function_param_flow_no_store_graph(store, factory));
                    CHECK(function_param_flow_backing_rows_current(store, probe));
                }
            }
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_declared_result_currency(void)
{
    const char *source =
        "func Empty()->Array<Int>{let rows:Array<Int> = [];return rows;}"
        "func Forward()->Array<Int>{return Empty();}"
        "func Probe(ref text:String)->Option<Int>{let rows=Forward();return Some(ArrayLength(rows)+StringLength(text));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    SemanticContext *ctx = semantic_context_create();
    CHECK(type_check_program(program, ctx));
    ASTNode *empty = ast_program_statement(program, 0);
    ASTNode *forward = ast_program_statement(program, 1);
    ASTNode *probe = ast_program_statement(program, 2);
    ASTNode *inner = ast_return_value(ast_block_statement(ast_func_body(forward), 0));
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(probe), 0));
    ASTNode *literal = ast_let_initializer(ast_block_statement(ast_func_body(empty), 0));
    ASTNode *returned = ast_block_statement(ast_func_body(empty), 1);
    ASTNode *body = ast_func_body(empty), *header = ast_func_return_type(empty);
    uint32_t callee = ast_call_semantic_callee_decl_id(inner);
    ASTNode *target = ast_call_callee(inner);
    const Type *result_type = NULL;
    CHECK(semantic_param_publication_declared_result(ctx, inner, &result_type) == empty);
    CHECK(result_type != NULL);
    CHECK(semantic_param_publication_routine_result_current(ctx, empty, result_type));
    (void)function_param_flow_summary_for_param(ctx, probe, 0);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, probe, &index));
    CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
    CHECK(index != NULL && index->backing_frames != NULL && index->backing_frames[0] != NULL);
    if (index != NULL && index->backing_frames != NULL && index->backing_frames[0] != NULL)
        CHECK(function_param_flow_declared_result_closed(ctx->function_param_flow_summaries,
            call, index->backing_frames[0]));
    FunctionParamFlowProgramPointIndex *empty_index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, empty, &empty_index));
    CHECK(empty_index != NULL && empty_index->declared_result != NULL);
    if (empty_index != NULL && empty_index->declared_result != NULL) {
        FunctionParamFlowBackingFact *saved_frame = empty_index->declared_result;
        empty_index->declared_result = NULL;
        CHECK(!function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
        empty_index->declared_result = saved_frame;
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
    }
    PublishedNode *claim = NULL, *result = NULL;
    FunctionParamPublicationStore *publication = ctx->function_param_publications;
    PendingRoutine *routine = NULL;
    for (size_t i = 0; i < publication->routine_count; i++)
        if (publication->routines[i].decl == empty) routine = &publication->routines[i];
    for (size_t i = 0; i < publication->node_capacity; i++)
    {
        if (publication->nodes[i].node == literal) claim = &publication->nodes[i];
        if (publication->nodes[i].node == inner) result = &publication->nodes[i];
    }
    CHECK(claim != NULL && result != NULL && routine != NULL);
    if (claim != NULL && result != NULL && routine != NULL) {
        PublishedNode saved = *claim;
        PublishedNode saved_result = *result;
        Type *saved_return_type = routine->return_type;
        for (unsigned fault = 0; fault < 11; fault++) {
            if (fault == 0) target->stable_id++;
            if (fault == 1) ast_call_set_semantic_callee_decl_id(inner, ast_node_stable_id(forward));
            if (fault == 2) empty->data.func_decl.body = ast_func_body(probe);
            if (fault == 3) returned->stable_id++;
            if (fault == 4) empty->data.func_decl.return_type = NULL;
            if (fault == 5) inner->data.call.arg_count = 1;
            if (fault == 6) claim->value_present = false;
            if (fault == 7) claim->value_type = TYPE_STRING;
            if (fault == 8) empty->is_async_decl = true;
            if (fault == 9) result->value_type = TYPE_STRING;
            if (fault == 10) routine->return_type = TYPE_STRING;
            bool current = function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe);
            if (current) fprintf(stderr, "declared result currency fault=%u unexpectedly current\n", fault);
            CHECK(!current);
            if (fault == 0) target->stable_id--;
            ast_call_set_semantic_callee_decl_id(inner, callee);
            empty->data.func_decl.body = body;
            if (fault == 3) returned->stable_id--;
            empty->data.func_decl.return_type = header;
            inner->data.call.arg_count = 0;
            *claim = saved;
            *result = saved_result;
            routine->return_type = saved_return_type;
            empty->is_async_decl = false;
            CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* A named String result only inherits sources from current RETURN-dependent actuals. */
static void checked_named_string_result_source_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Identity(value:String)->String{return value;}"
        "func IgnoreRows(ref ignored:Array<String>,value:String)->String{return value;}"
        "func Forward()->String{let ignored:Array<String> = [];"
        "let source=Substring(\"owned\",0,5);let named=Identity(source);"
        "return IgnoreRows(ignored,named);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *identity = ast_program_statement(program, 0);
    ASTNode *ignored_reader = ast_program_statement(program, 1);
    ASTNode *forward = ast_program_statement(program, 2);
    ASTNode *named_decl = ast_block_statement(ast_func_body(forward), 2);
    ASTNode *inner = ast_let_initializer(named_decl);
    ASTNode *outer = ast_return_value(ast_block_statement(ast_func_body(forward), 3));
    ASTNode *named_use = outer != NULL ? ast_call_argument(outer, 1) : NULL;
    CHECK(outer != NULL && outer->type == AST_CALL && inner != NULL && inner->type == AST_CALL);
    CHECK(named_use != NULL && named_use->type == AST_IDENTIFIER);
    if (inner == NULL || inner->type != AST_CALL || named_use == NULL) goto cleanup;
    uint32_t source_id = UINT32_MAX;
    CHECK(function_param_flow_string_result_source_current(ctx, inner, &source_id) && source_id == 0);
    source_id = UINT32_MAX;
    CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    size_t identity_entry = function_param_flow_find(store, ast_node_stable_id(identity), 0);
    FunctionParamFlowBackingFact *identity_result = NULL;
    for (size_t i = 0; store != NULL && i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == identity)
            identity_result = store->program_point_indexes[i].declared_result;
    CHECK(identity_entry != SIZE_MAX && store->entries[identity_entry].function_decl == identity &&
        store->entries[identity_entry].state == FUNCTION_PARAM_FLOW_COMPLETE &&
        identity_result != NULL && identity_result->epoch != 0 && store->hash != NULL);
    if (identity_entry == SIZE_MAX || identity_result == NULL || store->hash == NULL) goto cleanup;
    size_t ready_count = store->count, ready_index_count = store->program_point_index_count;
    size_t ready_demand = store->demand_epoch, ready_evaluations = store->body_evaluations;
    size_t ready_result_epoch = identity_result->epoch;
    FunctionParamFlowSummaryEntry *ready_entries = store->entries;
    FunctionParamFlowProgramPointIndex *ready_indexes = store->program_point_indexes;
    size_t *ready_hash = store->hash;
    size_t ready_hash_capacity = store->hash_capacity;
    bool ready_validating = store->validating_current;
    CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    CHECK(store->count == ready_count && store->program_point_index_count == ready_index_count &&
        store->demand_epoch == ready_demand && store->body_evaluations == ready_evaluations &&
        store->entries == ready_entries && store->program_point_indexes == ready_indexes &&
        store->hash == ready_hash && store->hash_capacity == ready_hash_capacity &&
        identity_result->epoch == ready_result_epoch && store->validating_current == ready_validating);
    /* The outer result depends on Identity's already issued input summary and
     * result frame. Currentness must not demand replacements for either. */
    for (unsigned fault = 0; fault < 4; fault++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[identity_entry];
        FunctionParamFlowSummaryState saved_state = entry->state;
        ASTNode *saved_decl = entry->function_decl;
        size_t saved_epoch = identity_result->epoch;
        if (fault == 0) entry->state = FUNCTION_PARAM_FLOW_UNSEEN;
        if (fault == 1) entry->function_decl = forward;
        if (fault == 2) identity_result->epoch = 0;
        if (fault == 3) {
            CHECK(ready_hash_capacity != 0 && ready_hash_capacity <= SIZE_MAX / sizeof(size_t));
            if (ready_hash_capacity == 0 || ready_hash_capacity > SIZE_MAX / sizeof(size_t)) goto cleanup;
            size_t *shadow = malloc(ready_hash_capacity * sizeof(size_t));
            CHECK(shadow != NULL);
            if (shadow == NULL) goto cleanup;
            memcpy(shadow, ready_hash, ready_hash_capacity * sizeof(size_t));
            size_t slot = SIZE_MAX;
            for (size_t i = 0; i < ready_hash_capacity; i++)
                if (shadow[i] == identity_entry + 1) slot = i;
            CHECK(slot != SIZE_MAX);
            if (slot == SIZE_MAX) { free(shadow); goto cleanup; }
            shadow[slot] = 0;
            store->hash = shadow;
        }
        size_t *active_hash = store->hash;
        bool current = function_param_flow_string_result_source_current(ctx, outer, &source_id);
        bool unchanged = store->count == ready_count &&
            store->program_point_index_count == ready_index_count &&
            store->demand_epoch == ready_demand && store->body_evaluations == ready_evaluations &&
            store->entries == ready_entries && store->program_point_indexes == ready_indexes &&
            store->hash == active_hash && store->hash_capacity == ready_hash_capacity &&
            identity_result->epoch == (fault == 2 ? 0 : ready_result_epoch) &&
            store->validating_current == ready_validating;
        if (current || !unchanged)
            fprintf(stderr, "nested named source fault=%u current=%d count=%zu/%zu index=%zu/%zu "
                "demand=%zu/%zu eval=%zu/%zu\n", fault, (int)current,
                store->count, ready_count, store->program_point_index_count, ready_index_count,
                store->demand_epoch, ready_demand, store->body_evaluations, ready_evaluations);
        CHECK(!current && unchanged);
        if (fault == 3) {
            if (store->hash != ready_hash) free(store->hash);
            store->hash = ready_hash;
            store->hash_capacity = ready_hash_capacity;
        }
        if (!unchanged) goto cleanup;
        entry->state = saved_state;
        entry->function_decl = saved_decl;
        identity_result->epoch = saved_epoch;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
        CHECK(store->count == ready_count && store->program_point_index_count == ready_index_count &&
            store->demand_epoch == ready_demand && store->body_evaluations == ready_evaluations &&
            store->entries == ready_entries && store->program_point_indexes == ready_indexes &&
            store->hash == ready_hash && store->hash_capacity == ready_hash_capacity &&
            identity_result->epoch == ready_result_epoch && store->validating_current == ready_validating);
    }
    ASTNode *source_use = ast_call_argument(inner, 0);
    PublishedNode *named_row = published_node_for(ctx, named_decl);
    PublishedNode *named_use_row = published_node_for(ctx, named_use);
    CHECK(source_use != NULL && named_row != NULL && named_use_row != NULL);
    if (source_use != NULL && named_row != NULL && named_use_row != NULL) {
        const ASTNode *dependency = NULL;
        CHECK(semantic_indexed_string_borrow_result_dependency_current(ctx, source_use,
            &dependency) && dependency == NULL);
        CHECK(semantic_indexed_string_borrow_result_dependency_current(ctx, named_use,
            &dependency) && dependency == inner);
        CHECK(semantic_indexed_string_borrow_source_current(source_use, ctx, &source_id)
            && source_id == 0);
        CHECK(semantic_indexed_string_borrow_source_current(named_use, ctx, &source_id)
            && source_id == 0);
        CHECK(named_row->indexed_string_let.present
            && named_row->indexed_string_let.available && named_row->indexed_string_let.dependent
            && named_use_row->indexed_string_let.present);
        IndexedStringLetLineage named_original = named_row->indexed_string_let;
        named_row->indexed_string_let.initializer_id++;
        CHECK(!semantic_indexed_string_borrow_result_dependency_current(ctx, named_use,
            &dependency));
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        named_row->indexed_string_let = named_original;
        CHECK(semantic_indexed_string_borrow_result_dependency_current(ctx, named_use,
            &dependency) && dependency == inner);
        named_row->indexed_string_let.initializer_shape = AST_BINARY;
        CHECK(!semantic_indexed_string_borrow_result_dependency_current(ctx, named_use,
            &dependency));
        named_row->indexed_string_let = named_original;
        IndexedStringLetLineage named_issued = named_use_row->indexed_string_let;
        named_use_row->indexed_string_let.initializer_id++;
        CHECK(!semantic_indexed_string_borrow_result_dependency_current(ctx, named_use,
            &dependency));
        named_use_row->indexed_string_let = named_issued;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
        named_row->indexed_string_let.available = false;
        CHECK(!semantic_indexed_string_borrow_result_dependency_current(ctx, named_use,
            &dependency));
        CHECK(!semantic_indexed_string_borrow_source_current(named_use, ctx, &source_id));
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        named_row->indexed_string_let = named_original;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    }
    uint32_t call_id = inner->stable_id;
    inner->stable_id++;
    CHECK(!function_param_flow_string_result_source_current(ctx, inner, &source_id));
    CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
    inner->stable_id = call_id;
    CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    uint32_t selected = ast_call_semantic_callee_decl_id(outer);
    CHECK(ast_call_set_semantic_callee_decl_id(outer, ast_node_stable_id(forward)));
    CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
    CHECK(ast_call_set_semantic_callee_decl_id(outer, selected));
    CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    size_t arity = outer->data.call.arg_count;
    outer->data.call.arg_count = 1;
    CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
    outer->data.call.arg_count = arity;
    CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    FunctionParamFlowSummaryEntry *aggregate = NULL, *dependent = NULL;
    for (size_t i = 0; store != NULL && i < store->count; i++) {
        if (store->entries[i].function_decl == ignored_reader && store->entries[i].param_index == 0)
            aggregate = &store->entries[i];
        if (store->entries[i].function_decl == ignored_reader && store->entries[i].param_index == 1)
            dependent = &store->entries[i];
    }
    CHECK(aggregate != NULL && dependent != NULL);
    if (aggregate != NULL && dependent != NULL) {
        CHECK((aggregate->mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
        CHECK((dependent->mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
        unsigned mask = aggregate->mask;
        aggregate->mask |= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        aggregate->mask = mask;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
        bool ready = dependent->output_dependencies_ready;
        dependent->output_dependencies_ready = false;
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        dependent->output_dependencies_ready = ready;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
        unsigned known_mask = dependent->mask;
        dependent->mask |= (unsigned)SLOT_PARAM_SUMMARY_ALL + 1u;
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        dependent->mask = known_mask;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    }
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, ignored_reader, &index));
    FunctionParamFlowBackingFact *frame = index != NULL && index->backing_frames != NULL &&
        index->backing_frame_count > 1 ? index->backing_frames[1] : NULL;
    CHECK(frame != NULL && frame->complete && frame->epoch != 0);
    if (frame != NULL) {
        bool complete = frame->complete;
        frame->complete = false;
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        frame->complete = complete;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    }
    PendingRoutine *routine = NULL;
    FunctionParamPublicationStore *publication = ctx->function_param_publications;
    for (size_t i = 0; publication != NULL && i < publication->routine_count; i++)
        if (publication->routines[i].decl == ignored_reader) routine = &publication->routines[i];
    CHECK(routine != NULL && routine->count == 2);
    if (routine != NULL && routine->count == 2) {
        uint32_t formal_id = routine->formals[1].id;
        routine->formals[1].id++;
        CHECK(!function_param_flow_string_result_source_current(ctx, outer, &source_id));
        routine->formals[1].id = formal_id;
        CHECK(function_param_flow_string_result_source_current(ctx, outer, &source_id) && source_id == 0);
    }
    CHECK(identity != NULL);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_named_string_shallow_retention_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Identity(value:String)->String{return value;}"
        "func Main()->Void{let rows:Array<String> = [];"
        "let source=Substring(\"owned\",0,5);let named=Identity(source);"
        "ArrayPush(rows,named);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *retained = NULL;
    size_t count = 0;
    for (size_t i = 0; store != NULL && i < store->node_capacity; i++) {
        if (!store->nodes[i].indexed_string_shallow_retention) continue;
        retained = &store->nodes[i];
        count++;
    }
    CHECK(retained != NULL && count != 0
        && count == store->indexed_string_shallow_retention_count);
    CHECK(semantic_indexed_string_borrow_finalize(ctx));
    if (retained != NULL) {
        retained->indexed_string_shallow_retention = false;
        CHECK(!semantic_indexed_string_borrow_finalize(ctx));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* A declared-result edge seals each actual's return dependency independently. */
static void checked_result_argument_currency(void)
{
    const char *source =
        "func ReturnInput(ref text:String,index:Int)->String{return text;}"
        "func RelayInput(ref text:String,index:Int)->String{return ReturnInput(text,index);}"
        "func FreshText(ref text:String,index:Int)->String{return text+\"!\";}"
        "func RelayFresh(ref text:String,index:Int)->String{return FreshText(text,index);}"
        "func MakeInput()->String{return RelayInput(\"known\",1);}"
        "func MakeFresh()->String{let source=\"heap\"+\" text\";return RelayFresh(source,2);}"
        "func Inspect(ref label:String)->Option<Int>{let input=MakeInput();"
        "let fresh=MakeFresh();return Some(StringLength(input)+StringLength(fresh)+StringLength(label));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *callees[] = {ast_program_statement(program, 0), ast_program_statement(program, 2)};
    ASTNode *relays[] = {ast_program_statement(program, 1), ast_program_statement(program, 3)};
    FunctionParamPublicationStore *publication = ctx->function_param_publications;
    ASTNode *inspect = ast_program_statement(program, 6);
    (void)function_param_flow_summary_for_param(ctx, inspect, 0);
    for (size_t k = 0; k < 2; k++) {
        ASTNode *callee = callees[k], *relay = relays[k];
        ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(relay), 0));
        CHECK(call != NULL && call->type == AST_CALL);
        (void)function_param_flow_summary_for_param(ctx, relay, 0);
        FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
        FunctionParamFlowProgramPointIndex *index = NULL;
        CHECK(store != NULL && function_param_flow_program_point_index_build(store, relay, &index));
        FunctionParamFlowBackingFact *frame = index != NULL ? index->declared_result : NULL;
        CHECK(frame != NULL && function_param_flow_backing_rows_current(store, relay));
        if (frame == NULL || call == NULL || store == NULL) continue;
        FunctionParamFlowBackingNode *edge = NULL;
        for (size_t n = 0; n < frame->node_count; n++)
            if (frame->nodes[n].node == call && frame->nodes[n].result_callee_id != 0)
                edge = &frame->nodes[n];
        CHECK(edge != NULL && edge->result_argument_count == 2 && edge->result_argument_kinds != NULL);
        if (edge == NULL || edge->result_argument_kinds == NULL) continue;
        unsigned char first_kind = k == 0 ? FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT :
            FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT;
        CHECK(edge->result_argument_kinds[0] == first_kind);
        CHECK(edge->result_argument_kinds[1] == FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_PHYSICAL);
        size_t saved_count = edge->result_argument_count;
        edge->result_argument_count = 1;
        CHECK(!function_param_flow_backing_rows_current(store, relay));
        edge->result_argument_count = saved_count;
        CHECK(function_param_flow_backing_rows_current(store, relay));
        unsigned char *saved_kinds = edge->result_argument_kinds;
        edge->result_argument_kinds = NULL;
        CHECK(!function_param_flow_backing_rows_current(store, relay));
        edge->result_argument_kinds = saved_kinds;
        CHECK(function_param_flow_backing_rows_current(store, relay));
        for (size_t p = 0; p < 2; p++) {
            unsigned char saved_kind = saved_kinds[p];
            saved_kinds[p] = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            saved_kinds[p] = saved_kind;
            CHECK(function_param_flow_backing_rows_current(store, relay));
        }
        saved_kinds[0] = first_kind == FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT ?
            FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT : FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT;
        CHECK(!function_param_flow_backing_rows_current(store, relay));
        saved_kinds[0] = first_kind;
        CHECK(function_param_flow_backing_rows_current(store, relay));
        saved_kinds[1] = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT;
        CHECK(!function_param_flow_backing_rows_current(store, relay));
        saved_kinds[1] = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_PHYSICAL;
        CHECK(function_param_flow_backing_rows_current(store, relay));
        size_t arity = call->data.call.arg_count;
        call->data.call.arg_count = 1;
        CHECK(!function_param_flow_backing_rows_current(store, relay));
        call->data.call.arg_count = arity;
        CHECK(function_param_flow_backing_rows_current(store, relay));
        uint32_t target = ast_call_semantic_callee_decl_id(call);
        CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(relay)));
        CHECK(!function_param_flow_backing_rows_current(store, relay));
        CHECK(ast_call_set_semantic_callee_decl_id(call, target));
        CHECK(function_param_flow_backing_rows_current(store, relay));
        PublishedNode *physical_claim = published_node_for(ctx, ast_call_argument(call, 1));
        CHECK(physical_claim != NULL && physical_claim->physical_scalar);
        if (physical_claim != NULL) {
            bool saved = physical_claim->physical_scalar;
            physical_claim->physical_scalar = false;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            physical_claim->physical_scalar = saved;
            CHECK(function_param_flow_backing_rows_current(store, relay));
        }
        FunctionParamFlowSummaryEntry *summary = NULL;
        for (size_t i = 0; i < store->count; i++)
            if (store->entries[i].function_decl == callee && store->entries[i].param_index == 0)
                summary = &store->entries[i];
        CHECK(summary != NULL);
        if (summary != NULL) {
            unsigned saved_mask = summary->mask;
            summary->mask ^= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            summary->mask = saved_mask;
            CHECK(function_param_flow_backing_rows_current(store, relay));
            CHECK(summary->output_dependencies_ready && !summary->returned_backing_unproved);
            bool saved_ready = summary->output_dependencies_ready;
            summary->output_dependencies_ready = false;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            summary->output_dependencies_ready = saved_ready;
            CHECK(function_param_flow_backing_rows_current(store, relay));
            bool saved_unproved = summary->returned_backing_unproved;
            summary->returned_backing_unproved = true;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            summary->returned_backing_unproved = saved_unproved;
            CHECK(function_param_flow_backing_rows_current(store, relay));
            FunctionParamFlowSummaryState saved_state = summary->state;
            summary->state = FUNCTION_PARAM_FLOW_COMPUTING;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            summary->state = saved_state;
            CHECK(function_param_flow_backing_rows_current(store, relay));
            FunctionParamFlowProgramPointIndex *callee_index = NULL;
            CHECK(function_param_flow_program_point_index_build(store, callee, &callee_index));
            FunctionParamFlowBackingFact *source_frame = callee_index != NULL &&
                callee_index->backing_frames != NULL && callee_index->backing_frame_count != 0 ?
                callee_index->backing_frames[0] : NULL;
            CHECK(source_frame != NULL && source_frame->complete && source_frame->epoch != 0);
            if (source_frame != NULL) {
                bool saved_complete = source_frame->complete;
                source_frame->complete = false;
                CHECK(!function_param_flow_backing_rows_current(store, relay));
                source_frame->complete = saved_complete;
                CHECK(function_param_flow_backing_rows_current(store, relay));
                size_t saved_epoch = source_frame->epoch;
                source_frame->epoch = 0;
                CHECK(!function_param_flow_backing_rows_current(store, relay));
                source_frame->epoch = saved_epoch;
                CHECK(function_param_flow_backing_rows_current(store, relay));
                if (k == 1) {
                    CHECK((summary->mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
                    CHECK(source_frame->effects != NULL &&
                        (source_frame->effects[0] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
                    if (source_frame->effects != NULL) {
                        unsigned saved_effects = source_frame->effects[0];
                        source_frame->effects[0] |= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
                        CHECK(!function_param_flow_backing_rows_current(store, relay));
                        source_frame->effects[0] = saved_effects;
                        CHECK(function_param_flow_backing_rows_current(store, relay));
                    }
                }
            }
        }
        for (size_t owner = 0; owner < 2; owner++) {
            ASTNode *decl = owner == 0 ? callee : relay;
            PendingRoutine *routine = NULL;
            for (size_t i = 0; i < publication->routine_count; i++)
                if (publication->routines[i].decl == decl) routine = &publication->routines[i];
            CHECK(routine != NULL && routine->normal_exit_closed);
            if (routine == NULL) continue;
            routine->normal_exit_closed = false;
            CHECK(!semantic_param_publication_routine_result_current(ctx, decl, TYPE_STRING));
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            routine->normal_exit_closed = true;
            CHECK(function_param_flow_backing_rows_current(store, relay));
            ASTNode *body = ast_func_body(decl);
            uint32_t body_id = body->stable_id;
            body->stable_id++;
            CHECK(!function_param_flow_backing_rows_current(store, relay));
            body->stable_id = body_id;
            CHECK(function_param_flow_backing_rows_current(store, relay));
        }
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_witnessed_physical_call_alias_currency(void)
{
    const char *source =
        "func Select(ref content:String,index:Int,inout cursor_out:Array<Int>)->String{ArraySet(cursor_out,0,index);return content;}"
        "func Probe(ref label:String)->Option<Int>{let content=Concat(\"abc\",\"\");var cursor:Array<Int> = [0];"
        "let observed=Select(content,StringLength(label),cursor);return Some(StringLength(observed)+cursor[0]);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;

    ASTNode *select = ast_program_statement(program, 0);
    ASTNode *probe = ast_program_statement(program, 1);
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(probe), 2));
    CHECK(call != NULL && call->type == AST_CALL && ast_call_arg_count(call) == 3);
    if (call == NULL || call->type != AST_CALL || ast_call_arg_count(call) != 3) goto cleanup;
    ASTNode *descriptor = ast_call_argument(call, 0);
    ASTNode *physical = ast_call_argument(call, 1);
    ASTNode *output = ast_call_argument(call, 2);
    bool required = false;
    CHECK(semantic_param_publication_return_backing_required(ctx, select, 0, &required) && required);
    CHECK(semantic_param_publication_return_backing_required(ctx, select, 1, &required) && !required);
    CHECK(semantic_param_publication_callee(ctx, call, 0) == select);
    CHECK(semantic_param_publication_callee(ctx, call, 1) == select);
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx, physical));
    unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
    CHECK(baseline != SLOT_PARAM_SUMMARY_ALL);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *probe_index = NULL, *select_index = NULL;
    CHECK(store != NULL && !store->failed &&
        function_param_flow_program_point_index_build(store, probe, &probe_index) &&
        function_param_flow_program_point_index_build(store, select, &select_index));
    if (store == NULL || probe_index == NULL || select_index == NULL ||
        probe_index->backing_frames == NULL || select_index->backing_frames == NULL) goto cleanup;
    FunctionParamFlowBackingFact *probe_frame = probe_index->backing_frames[0];
    FunctionParamFlowBackingFact *source_frame = select_index->backing_frames[0];
    CHECK(probe_frame != NULL && source_frame != NULL && source_frame->epoch != 0 &&
        source_frame->complete && !source_frame->pending);
    if (probe_frame == NULL || source_frame == NULL) goto cleanup;
    bool non_declared_alias = false;
    for (size_t n = 0; n < probe_frame->node_count; n++)
        if (probe_frame->nodes[n].node == call && probe_frame->nodes[n].result_callee_id == 0)
            non_declared_alias = true;
    CHECK(non_declared_alias && function_param_flow_backing_rows_current(store, probe));
    SlotFunctionLookup lookup = {ctx, program};
    bool output_depends = true;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 2, &output_depends) && !output_depends);

    size_t source_entry_index = SIZE_MAX;
    for (size_t e = 0; e < store->count; e++)
        if (store->entries[e].function_decl == select &&
            store->entries[e].function_id == ast_node_stable_id(select) &&
            store->entries[e].param_index == 0)
            source_entry_index = e;
    CHECK(source_entry_index != SIZE_MAX);
    if (source_entry_index == SIZE_MAX) goto cleanup;
    FunctionParamFlowSummaryEntry saved_source_entry = store->entries[source_entry_index];
    CHECK(saved_source_entry.state == FUNCTION_PARAM_FLOW_COMPLETE &&
        saved_source_entry.output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME &&
        saved_source_entry.output_dependencies_ready &&
        !saved_source_entry.returned_backing_unproved &&
        !saved_source_entry.analysis_unsupported);
    for (unsigned fault = 0; fault < 5; fault++) {
        FunctionParamFlowSummaryEntry *source_entry = &store->entries[source_entry_index];
        if (fault == 0) source_entry->output_producer = FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED;
        if (fault == 1) source_entry->state = FUNCTION_PARAM_FLOW_COMPUTING;
        if (fault == 2) source_entry->output_dependencies_ready = false;
        if (fault == 3) source_entry->returned_backing_unproved = true;
        if (fault == 4) source_entry->analysis_unsupported = true;
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 2, &output_depends));
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
        store->entries[source_entry_index] = saved_source_entry;
        CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 2, &output_depends) && !output_depends);
        CHECK(function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }

    PublishedNode *physical_claim = published_node_for(ctx, physical);
    PublishedNode *descriptor_claim = published_node_for(ctx, descriptor);
    CHECK(physical_claim != NULL && descriptor_claim != NULL);
    if (physical_claim == NULL || descriptor_claim == NULL) goto cleanup;
    PublishedNode saved_physical = *physical_claim, saved_descriptor = *descriptor_claim;
    CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, descriptor));
    descriptor_claim->physical_scalar = true;
    CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, descriptor));
    *descriptor_claim = saved_descriptor;
    for (unsigned fault = 0; fault < 4; fault++) {
        if (fault == 0) physical_claim->physical_scalar = false;
        if (fault == 1) physical_claim->value_type = TYPE_STRING;
        if (fault == 2) physical_claim->value_present = false;
        if (fault == 3) physical_claim->id++;
        CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, physical));
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
        *physical_claim = saved_physical;
        CHECK(function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }
    uint32_t physical_id = physical->stable_id, output_id = output->stable_id;
    uint32_t callee_id = ast_call_semantic_callee_decl_id(call);
    size_t arity = call->data.call.arg_count;
    for (unsigned fault = 0; fault < 4; fault++) {
        if (fault == 0) physical->stable_id++;
        if (fault == 1) call->data.call.arg_count = arity - 1;
        if (fault == 2) ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(probe));
        if (fault == 3) output->stable_id++;
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
        physical->stable_id = physical_id;
        output->stable_id = output_id;
        call->data.call.arg_count = arity;
        ast_call_set_semantic_callee_decl_id(call, callee_id);
        CHECK(function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }
    bool complete = source_frame->complete, pending = source_frame->pending;
    size_t epoch = source_frame->epoch;
    for (unsigned fault = 0; fault < 3; fault++) {
        if (fault == 0) source_frame->complete = false;
        if (fault == 1) source_frame->pending = true;
        if (fault == 2) source_frame->epoch = 0;
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 2, &output_depends));
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
        source_frame->complete = complete; source_frame->pending = pending; source_frame->epoch = epoch;
        CHECK(function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }
    bool source_index_ready = function_param_flow_program_point_index_build(store, select, &select_index);
    CHECK(source_index_ready);
    if (!source_index_ready || select_index == NULL || select_index->backing_frames == NULL ||
        select_index->backing_frames[0] != source_frame) goto cleanup;
    size_t source_index_position = (size_t)(select_index - store->program_point_indexes);
    FunctionParamFlowBackingFact **source_frames = select_index->backing_frames;
    size_t source_frame_count = select_index->backing_frame_count;
    source_frames[0] = NULL;
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 2, &output_depends));
    CHECK(!function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
    source_frames[0] = source_frame;
    CHECK(function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    select_index = &store->program_point_indexes[source_index_position];
    CHECK(select_index->function_decl == select && select_index->backing_frames == source_frames &&
        select_index->backing_frame_count == source_frame_count);
    select_index->backing_frames = NULL;
    select_index->backing_frame_count = 0;
    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 2, &output_depends));
    CHECK(!function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
    select_index = &store->program_point_indexes[source_index_position];
    select_index->backing_frames = source_frames;
    select_index->backing_frame_count = source_frame_count;
    CHECK(function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_slot_physical_scalar_forgery(void)
{
    Lexer *lexer = lexer_create("func Probe()->Void{let slot:Slot<Int> = ClaimSlot();Release(slot);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *decl = ast_program_statement(program, 0);
        ASTNode *slot = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
        PublishedNode *claim = published_node_for(ctx, slot);
        CHECK(claim != NULL && !semantic_param_publication_value_is_physical_scalar(ctx, slot));
        if (claim != NULL) {
            bool physical = claim->physical_scalar;
            claim->physical_scalar = true;
            CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, slot));
            claim->physical_scalar = physical;
            CHECK(!semantic_param_publication_value_is_physical_scalar(ctx, slot));
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_witnessed_physical_recursive_alias_currency(void)
{
    const char *source =
        "func Select(ref content:String,index:Int,inout cursor_out:Array<Int>)->String{"
        "if index>0{let observed=Select(content,index-1,cursor_out);return observed;}"
        "ArraySet(cursor_out,0,index);return content;}"
        "func Probe(ref label:String)->Option<Int>{let content=Concat(\"abc\",\"\");var cursor:Array<Int> = [0];"
        "let observed=Select(content,StringLength(label),cursor);return Some(StringLength(observed)+cursor[0]);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;

    ASTNode *select = ast_program_statement(program, 0);
    ASTNode *probe = ast_program_statement(program, 1);
    ASTNode *branch = ast_block_statement(ast_func_body(select), 0);
    ASTNode *recursive = ast_let_initializer(ast_block_statement(ast_if_then_branch(branch), 0));
    ASTNode *outer = ast_let_initializer(ast_block_statement(ast_func_body(probe), 2));
    CHECK(recursive != NULL && recursive->type == AST_CALL && ast_call_arg_count(recursive) == 3);
    CHECK(outer != NULL && outer->type == AST_CALL && ast_call_arg_count(outer) == 3);
    if (recursive == NULL || recursive->type != AST_CALL || outer == NULL || outer->type != AST_CALL) goto cleanup;
    CHECK(semantic_param_publication_callee(ctx, recursive, 0) == select &&
        semantic_param_publication_callee(ctx, recursive, 1) == select);
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx, ast_call_argument(recursive, 1)));
    bool required = false;
    CHECK(semantic_param_publication_return_backing_required(ctx, select, 0, &required) && required);
    (void)function_param_flow_summary_for_param(ctx, probe, 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && !store->failed && function_param_flow_program_point_index_build(store, select, &index));
    if (store == NULL || index == NULL || index->backing_frames == NULL || index->backing_frames[0] == NULL) goto cleanup;
    FunctionParamFlowBackingFact *frame = index->backing_frames[0];
    CHECK(frame->complete && !frame->pending && !frame->building && frame->epoch != 0 &&
        frame->pending_relations != NULL && frame->pending_relation_count != 0 &&
        frame->sealed_pending_relation_count == frame->pending_relation_count);
    size_t edge_at = 0;
    while (edge_at < frame->pending_relation_count &&
        (frame->pending_relations[edge_at].call != recursive ||
         frame->pending_relations[edge_at].ordinal != 0)) edge_at++;
    CHECK(edge_at < frame->pending_relation_count);
    if (edge_at == frame->pending_relation_count) goto cleanup;
    FunctionParamFlowPendingRelation *edge = &frame->pending_relations[edge_at];
    CHECK(edge->call_id == ast_node_stable_id(recursive) && edge->callee_id == ast_node_stable_id(select));
    size_t entry_at = 0;
    while (entry_at < store->count && (store->entries[entry_at].function_decl != select ||
        store->entries[entry_at].param_index != 0)) entry_at++;
    CHECK(entry_at < store->count);
    if (entry_at == store->count) goto cleanup;
    CHECK(function_param_flow_output_current(store, entry_at, false));
    CHECK(function_param_flow_backing_rows_current(store, select));
    CHECK(function_param_flow_backing_rows_current(store, probe));

    FunctionParamFlowPendingRelation saved_edge = *edge;
    size_t sealed = frame->sealed_pending_relation_count;
    for (unsigned fault = 0; fault < 5; fault++) {
        if (fault == 0) edge->call_id++;
        if (fault == 1) edge->callee_id = ast_node_stable_id(probe);
        if (fault == 2) edge->ordinal = 1;
        if (fault == 3) edge->call = outer;
        if (fault == 4) frame->sealed_pending_relation_count = 0;
        CHECK(!function_param_flow_output_current(store, entry_at, false));
        CHECK(!function_param_flow_backing_rows_current(store, select));
        *edge = saved_edge;
        frame->sealed_pending_relation_count = sealed;
        CHECK(function_param_flow_output_current(store, entry_at, false));
        CHECK(function_param_flow_backing_rows_current(store, select));
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* A ready body witness and a recursive pending edge can occupy one backing
 * frame. Both must be sealed before the caller consumes its return relation. */
static void checked_mixed_ready_pending_physical_alias(void)
{
    const char *source =
        "func Select(ref content:String,index:Int,inout cursor_out:Array<Int>)->String{"
        "ArraySet(cursor_out,0,index);return content;}"
        "func Recur(ref content:String,index:Int,inout cursor_out:Array<Int>)->String{"
        "let first=Select(content,index,cursor_out);"
        "if index>0{return Recur(first,index-1,cursor_out);}return first;}"
        "func Probe(ref label:String)->Option<Int>{let content=Concat(\"abc\",\"\");"
        "var cursor:Array<Int> = [0];"
        "let observed=Recur(content,StringLength(label),cursor);"
        "return Some(StringLength(observed)+cursor[0]);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;

    ASTNode *select = ast_program_statement(program, 0);
    ASTNode *recur = ast_program_statement(program, 1);
    ASTNode *probe = ast_program_statement(program, 2);
    ASTNode *ready_call = ast_let_initializer(ast_block_statement(ast_func_body(recur), 0));
    ASTNode *branch = ast_block_statement(ast_func_body(recur), 1);
    ASTNode *pending_call = ast_return_value(ast_block_statement(ast_if_then_branch(branch), 0));
    ASTNode *outer_call = ast_let_initializer(ast_block_statement(ast_func_body(probe), 2));
    CHECK(ready_call != NULL && ready_call->type == AST_CALL &&
        pending_call != NULL && pending_call->type == AST_CALL &&
        outer_call != NULL && outer_call->type == AST_CALL);
    if (ready_call == NULL || ready_call->type != AST_CALL ||
        pending_call == NULL || pending_call->type != AST_CALL ||
        outer_call == NULL || outer_call->type != AST_CALL) goto cleanup;
    CHECK(semantic_param_publication_callee(ctx, ready_call, 0) == select &&
        semantic_param_publication_callee(ctx, pending_call, 0) == recur &&
        semantic_param_publication_callee(ctx, outer_call, 0) == recur);
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx,
        ast_call_argument(ready_call, 1)) &&
        semantic_param_publication_value_is_physical_scalar(ctx,
            ast_call_argument(pending_call, 1)));
    bool required = false;
    CHECK(semantic_param_publication_return_backing_required(ctx, select, 0, &required) && required);
    CHECK(semantic_param_publication_return_backing_required(ctx, recur, 0, &required) && required);
    unsigned summary = function_param_flow_summary_for_param(ctx, probe, 0);
    CHECK(summary != SLOT_PARAM_SUMMARY_ALL);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    CHECK(store != NULL && !store->failed && function_param_flow_backing_rows_current(store, probe));
    if (store == NULL || store->failed) goto cleanup;
    FunctionParamFlowProgramPointIndex *index = NULL;
    bool indexed = function_param_flow_program_point_index_build(store, recur, &index);
    CHECK(indexed);
    if (!indexed || index == NULL || index->backing_frames == NULL ||
        index->backing_frame_count == 0 || index->backing_frames[0] == NULL) goto cleanup;
    FunctionParamFlowBackingFact *frame = index->backing_frames[0];
    CHECK(frame->complete && !frame->pending && !frame->building && frame->epoch != 0 &&
        frame->pending_relations != NULL && frame->pending_relation_count >= 2 &&
        frame->sealed_pending_relation_count == frame->pending_relation_count);
    if (frame->pending_relations == NULL) goto cleanup;
    bool saw_ready = false, saw_recursive = false;
    for (size_t e = 0; e < frame->pending_relation_count; e++) {
        const FunctionParamFlowPendingRelation *edge = &frame->pending_relations[e];
        if (edge->call == ready_call && edge->call_id == ast_node_stable_id(ready_call) &&
            edge->callee_id == ast_node_stable_id(select) && edge->ordinal == 0)
            saw_ready = true;
        if (edge->call == pending_call && edge->call_id == ast_node_stable_id(pending_call) &&
            edge->callee_id == ast_node_stable_id(recur) && edge->ordinal == 0)
            saw_recursive = true;
    }
    CHECK(saw_ready && saw_recursive);
    CHECK(function_param_flow_backing_rows_current(store, recur));
    CHECK(function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == summary);
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* Reading an Int element cannot make the local scalar alias the Array's
 * inout storage, while both array and index remain eager, current children. */
static void checked_physical_array_element_destination(void)
{
    const char *source =
        "func ReadCursorFact(ref label:String,inout cursor_out:Array<Int>)->String{"
        "let index:Int=0;let named:Int=cursor_out[index];"
        "let literal:Int=cursor_out[0];ArraySet(cursor_out,0,named+literal);"
        "return label;}"
        "func Probe(ref label:String)->Option<Int>{var cursor:Array<Int> = [1];"
        "let observed=ReadCursorFact(label,cursor);"
        "return Some(StringLength(observed)+cursor[0]);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;

    ASTNode *read = ast_program_statement(program, 0);
    ASTNode *probe = ast_program_statement(program, 1);
    ASTNode *named_local = ast_block_statement(ast_func_body(read), 1);
    ASTNode *literal_local = ast_block_statement(ast_func_body(read), 2);
    ASTNode *named = ast_let_initializer(named_local);
    ASTNode *literal = ast_let_initializer(literal_local);
    ASTNode *receiver = named != NULL && named->type == AST_ARRAY_ACCESS ?
        ast_array_access_array(named) : NULL;
    ASTNode *named_index = named != NULL && named->type == AST_ARRAY_ACCESS ?
        ast_array_access_index(named) : NULL;
    ASTNode *literal_index = literal != NULL && literal->type == AST_ARRAY_ACCESS ?
        ast_array_access_index(literal) : NULL;
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(probe), 1));
    CHECK(named != NULL && named->type == AST_ARRAY_ACCESS &&
        literal != NULL && literal->type == AST_ARRAY_ACCESS &&
        receiver != NULL && named_index != NULL && literal_index != NULL &&
        call != NULL && call->type == AST_CALL);
    if (named == NULL || named->type != AST_ARRAY_ACCESS ||
        literal == NULL || literal->type != AST_ARRAY_ACCESS || receiver == NULL ||
        named_index == NULL || literal_index == NULL || call == NULL || call->type != AST_CALL)
        goto cleanup;
    CHECK(semantic_param_publication_value_is_physical_scalar(ctx, named) &&
        semantic_param_publication_value_is_physical_scalar(ctx, literal) &&
        semantic_param_publication_value_is_physical_scalar(ctx, named_index) &&
        semantic_param_publication_value_is_physical_scalar(ctx, literal_index));

    unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
    CHECK(baseline != SLOT_PARAM_SUMMARY_ALL);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    CHECK(store != NULL && !store->failed && function_param_flow_backing_rows_current(store, probe));
    if (store == NULL || store->failed) goto cleanup;
    SlotFunctionLookup lookup = {ctx, program};
    bool depends = true;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &depends) && !depends);
    FunctionParamFlowProgramPointIndex *point = NULL;
    bool indexed = function_param_flow_program_point_index_build(store, read, &point);
    CHECK(indexed);
    if (!indexed || point == NULL || point->backing_frames == NULL ||
        point->backing_frame_count == 0 || point->backing_frames[0] == NULL) goto cleanup;
    size_t point_position = (size_t)(point - store->program_point_indexes);
    FunctionParamFlowBackingFact *frame = point->backing_frames[0];
    CHECK(frame->complete && !frame->pending && frame->epoch != 0);
    for (unsigned which = 0; which < 2; which++) {
        ASTNode *local = which == 0 ? named_local : literal_local;
        FunctionParamFlowBackingBinding *binding = NULL;
        for (size_t b = 0; b < frame->binding_count; b++)
            if (frame->bindings[b].binding == ast_node_stable_id(local)) binding = &frame->bindings[b];
        CHECK(binding != NULL && binding->local && binding->destinations != NULL &&
            binding->destinations[0] == 0);
    }

    PublishedNode *element_value = published_node_for(ctx, named);
    PublishedNode *receiver_value = published_node_for(ctx, receiver);
    PublishedNode *index_value = published_node_for(ctx, named_index);
    CHECK(element_value != NULL && receiver_value != NULL && index_value != NULL);
    if (element_value == NULL || receiver_value == NULL || index_value == NULL) goto cleanup;
    PublishedNode saved_element = *element_value, saved_receiver = *receiver_value;
    PublishedNode saved_index = *index_value;
    uint32_t named_id = ast_node_stable_id(named);
    uint32_t receiver_id = ast_node_stable_id(receiver);
    uint32_t index_id = ast_node_stable_id(named_index);
    uint32_t literal_index_id = ast_node_stable_id(literal_index);
    for (unsigned fault = 0; fault < 9; fault++) {
        if (fault == 0) element_value->physical_scalar = false;
        if (fault == 1) element_value->value_type = TYPE_STRING;
        if (fault == 2) element_value->value_present = false;
        if (fault == 3) receiver->stable_id++;
        if (fault == 4) named_index->stable_id++;
        if (fault == 5) receiver_value->value_type = TYPE_STRING;
        if (fault == 6) index_value->value_type = TYPE_STRING;
        if (fault == 7) index_value->physical_scalar = false;
        if (fault == 8) named->stable_id++;
        CHECK(!function_param_flow_index_claims_current(ctx,
            &store->program_point_indexes[point_position]));
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
        *element_value = saved_element; *receiver_value = saved_receiver;
        *index_value = saved_index;
        named->stable_id = named_id; receiver->stable_id = receiver_id;
        named_index->stable_id = index_id;
        CHECK(function_param_flow_index_claims_current(ctx,
            &store->program_point_indexes[point_position]));
        CHECK(function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }
    literal_index->stable_id++;
    CHECK(!function_param_flow_index_claims_current(ctx,
        &store->program_point_indexes[point_position]));
    CHECK(!function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
    literal_index->stable_id = literal_index_id;
    CHECK(function_param_flow_index_claims_current(ctx,
        &store->program_point_indexes[point_position]));
    CHECK(function_param_flow_backing_rows_current(store, probe));
    CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* A descriptor read from a borrowed nested Array cannot be promoted to a
 * new owned local by the physical Int element rule. */
static void checked_descriptor_array_element_escape(void)
{
    Lexer *lexer = lexer_create(
        "func ReadDescriptorFact(ref label:String,ref rows:Array<Array<Int>>)->String{"
        "let inner:Array<Int> = rows[0];return label;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    if (parsed) CHECK(!type_check_program(program, ctx) &&
        context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

/* Destination narrowing cannot turn child evaluation into dead code. A
 * receiver call and an index call must each still be visited by C. */
static void checked_array_element_call_children_eager(void)
{
    const char *source =
        "func Index(ref label:String)->Int{return StringLength(label)-StringLength(label);}"
        "func Rows(ref label:String)->Array<Int>{return [StringLength(label)];}"
        "func ReadIndex(ref label:String,inout rows:Array<Int>)->String{"
        "let value:Int=rows[Index(label)];ArraySet(rows,0,value);return label;}"
        "func ReadReceiver(ref label:String,inout rows:Array<Int>)->String{"
        "let value:Int=Rows(label)[0];ArraySet(rows,0,value);return label;}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    CHECK(store != NULL);
    if (store == NULL) goto cleanup;
    for (unsigned which = 0; which < 2; which++) {
        ASTNode *decl = ast_program_statement(program, 2 + which);
        ASTNode *element = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
        ASTNode *child = element != NULL && element->type == AST_ARRAY_ACCESS ?
            (which == 0 ? ast_array_access_index(element) : ast_array_access_array(element)) : NULL;
        CHECK(element != NULL && element->type == AST_ARRAY_ACCESS &&
            child != NULL && child->type == AST_CALL &&
            semantic_param_publication_value_is_physical_scalar(ctx, element));
        if (element == NULL || element->type != AST_ARRAY_ACCESS ||
            child == NULL || child->type != AST_CALL) continue;
        unsigned summary = function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK(summary != SLOT_PARAM_SUMMARY_ALL);
        FunctionParamFlowProgramPointIndex *point = NULL;
        bool indexed = function_param_flow_program_point_index_build(store, decl, &point);
        CHECK(indexed);
        if (!indexed || point == NULL || point->backing_frames == NULL ||
            point->backing_frame_count == 0 || point->backing_frames[0] == NULL) continue;
        size_t point_position = (size_t)(point - store->program_point_indexes);
        FunctionParamFlowBackingFact *frame = point->backing_frames[0];
        CHECK(frame->complete && !frame->pending && frame->epoch != 0 &&
            function_param_flow_backing_rows_current(store, decl));
        bool visited = false;
        for (size_t n = 0; n < frame->node_count; n++)
            if (frame->nodes[n].node == child &&
                frame->nodes[n].visit_epoch == frame->epoch) visited = true;
        CHECK(visited);
        ASTNode *target = ast_call_callee(child);
        CHECK(target != NULL);
        if (target == NULL) continue;
        uint32_t target_id = ast_node_stable_id(target);
        target->stable_id++;
        CHECK(!function_param_flow_index_claims_current(ctx,
            &store->program_point_indexes[point_position]));
        CHECK(!function_param_flow_backing_rows_current(store, decl));
        target->stable_id = target_id;
        CHECK(function_param_flow_index_claims_current(ctx,
            &store->program_point_indexes[point_position]));
        CHECK(function_param_flow_backing_rows_current(store, decl));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == summary);
    }
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_declared_physical_result_currency(void)
{
    const char *source =
        "struct Scalar{status:Int;}"
        "func Empty()->Scalar{let base=Scalar(-1);return Scalar(base.status);}"
        "func Forward()->Scalar{return Empty();}"
        "func Probe(ref text:String)->Option<Int>{let result=Forward();return Some(result.status+StringLength(text));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *empty = ast_program_statement(program, 1);
        ASTNode *probe = ast_program_statement(program, 3);
        ASTNode *ctor = ast_let_initializer(ast_block_statement(ast_func_body(empty), 0));
        ASTNode *unary = ast_call_argument(ctor, 0);
        ASTNode *result_ctor = ast_return_value(ast_block_statement(ast_func_body(empty), 1));
        ASTNode *field = ast_call_argument(result_ctor, 0);
        CHECK(unary != NULL && unary->type == AST_UNARY);
        CHECK(field != NULL && field->type == AST_MEMBER_ACCESS);
        (void)function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
        ASTNode *values[] = {unary, field};
        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        for (size_t v = 0; v < sizeof(values) / sizeof(values[0]); v++) {
            PublishedNode *claim = NULL;
            for (size_t i = 0; i < publication->node_capacity; i++)
                if (publication->nodes[i].node == values[v]) claim = &publication->nodes[i];
            CHECK(claim != NULL && claim->physical_scalar);
            if (claim == NULL) continue;
            PublishedNode saved = *claim;
            for (unsigned fault = 0; fault < 3; fault++) {
                if (fault == 0) claim->physical_scalar = false;
                if (fault == 1) claim->value_type = TYPE_STRING;
                if (fault == 2) claim->value_present = false;
                bool current = function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe);
                if (current) fprintf(stderr, "declared physical result value=%zu fault=%u unexpectedly current\n", v, fault);
                CHECK(!current);
                *claim = saved;
                CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
            }
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_declared_array_result_currency(void)
{
    const char *source =
        "func Codes()->Array<String>{return [\"known\",\"other\"];}"
        "func Probe(ref text:String)->Option<Int>{let codes=Codes();return Some(ArrayLength(codes)+StringLength(text));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    SemanticContext *ctx = semantic_context_create();
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *codes = ast_program_statement(program, 0);
        ASTNode *probe = ast_program_statement(program, 1);
        ASTNode *literal = ast_return_value(ast_block_statement(ast_func_body(codes), 0));
        (void)function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
        FunctionParamPublicationStore *publication = ctx->function_param_publications;
        for (size_t element = 0; element < 2; element++) {
            ASTNode *child = ast_array_literal_element(literal, element);
            PublishedNode *claim = NULL;
            for (size_t i = 0; i < publication->node_capacity; i++)
                if (publication->nodes[i].node == child) claim = &publication->nodes[i];
            CHECK(claim != NULL);
            if (claim == NULL) continue;
            PublishedNode saved = *claim;
            uint32_t saved_id = child->stable_id;
            for (unsigned fault = 0; fault < 3; fault++) {
                if (fault == 0) child->stable_id++;
                if (fault == 1) claim->value_type = TYPE_INT;
                if (fault == 2) claim->value_present = false;
                bool current = function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe);
                if (current) fprintf(stderr, "declared array result element=%zu fault=%u unexpectedly current\n", element, fault);
                CHECK(!current);
                *claim = saved;
                child->stable_id = saved_id;
                CHECK(function_param_flow_backing_rows_current(ctx->function_param_flow_summaries, probe));
            }
        }
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_declared_result_unclosed(void)
{
    const char *sources[] = {
        "extern \"C\"{func Keep(rows:Array<Int>)->Void;}"
        "func Produce()->Array<Int>{let rows:Array<Int> = [];Keep(rows);return rows;}"
        "func Main()->Void{let rows=Produce();Log(ArrayLength(rows));}",
        "extern \"C\"{func Foreign()->Array<Int>;}"
        "func Produce()->Array<Int>{return Foreign();}"
        "func Main()->Void{let rows=Produce();Log(ArrayLength(rows));}",
        "func Produce()->Array<Int>{return Again();}"
        "func Again()->Array<Int>{return Produce();}"
        "func Main()->Void{let rows=Produce();Log(ArrayLength(rows));}",
        "func Empty()->Array<Int>{let rows:Array<Int> = [];return rows;}"
        "func Produce()->Array<Int>{let selected=Empty;return selected();}"
        "func Main()->Void{let rows=Produce();Log(ArrayLength(rows));}",
        "extern \"C\"{func Opaque()->Int;}"
        "func Produce()->Int{return Opaque();}"
        "func Main()->Void{let result=Produce();Log(result);}",
        "extern \"C\"{func Keep(rows:Array<Int>)->Int;}"
        "func Produce()->Int{let rows:Array<Int> = [];return Keep(rows);}"
        "func Main()->Void{let result=Produce();Log(result);}",
        "extern \"C\"{func Opaque()->String;}"
        "func Produce()->Array<String>{return [\"known\",Opaque()];}"
        "func Main()->Void{let result=Produce();Log(ArrayLength(result));}",
        "extern \"C\"{func Keep(rows:Array<Int>)->String;}"
        "func Produce()->Array<String>{let rows:Array<Int> = [];return [\"known\",Keep(rows)];}"
        "func Main()->Void{let result=Produce();Log(ArrayLength(result));}"
    };
    for (size_t kind = 0; kind < sizeof(sources) / sizeof(sources[0]); kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        SemanticContext *ctx = semantic_context_create();
        bool typed = parsed && type_check_program(program, ctx);
        if (kind < 4) {
            bool refused = false;
            CHECK(!typed && ctx->has_error);
            for (size_t d = 0; d < ctx->diagnostic_count; d++) {
                const Diagnostic *diagnostic = ctx->diagnostics[d];
                if (diagnostic->level != DIAG_ERROR) continue;
                bool exact = diagnostic->code != NULL && diagnostic->cause_ir != NULL &&
                    strcmp(diagnostic->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0 &&
                    strcmp(diagnostic->cause_ir, PGY_CAUSE_BORROW_ESCAPE) == 0;
                CHECK(exact);
                refused |= exact;
            }
            CHECK(refused);
        } else {
            CHECK(typed);
        }
        if (kind >= 4 && typed) {
            ASTNode *main = ast_program_statement(program, 2);
            ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(main), 0));
            SlotFunctionLookup lookup = {ctx, program};
            FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
            FunctionParamFlowBackingFact consumer = {0};
            consumer.origin_count = consumer.origin_words = 1;
            consumer.destination_count = consumer.destination_words = 1;
            CHECK(!function_param_flow_declared_result_closed(store, call, &consumer));
            for (size_t n = 0; n < consumer.node_count; n++) free(consumer.nodes[n].origins);
            free(consumer.nodes);
        }
        semantic_context_destroy(ctx);
        ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_output_graph(const char *source, size_t declaration, bool closed)
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
    CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl) == closed);
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

static void checked_cast_after_prior_ownership_error(void)
{
    const char *prior =
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Refuse()->Void{let rows:Array<Int> = [1];let alias=rows;Consume(alias);}";
    for (unsigned route = 0; route < 3; route++) {
        char source[512];
        if (route == 0)
            snprintf(source, sizeof(source), "%sfunc CastRead(hash:Long,value:String)->Long{"
                "let modulus:Long=268435456L;let out:Long=(hash*131L+(StringLength(value) as Long))%%modulus;"
                "let i:Int=0;while i<StringLength(value){out=(out*131L+"
                "(CharCode(value,StringLength(value),i) as Long))%%modulus;i=i+1;}return out;}", prior);
        else
            snprintf(source, sizeof(source), "%sfunc CastRead(value:Int)->Long{return %s as Long;}",
                route == 1 ? prior : "", route == 1 ? "\"invalid\"" : "value");
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed == (route == 2));
        bool spurious = false;
        for (size_t d = 0; d < ctx->diagnostic_count; d++) {
            const Diagnostic *diagnostic = ctx->diagnostics[d];
            if (diagnostic != NULL && diagnostic->message != NULL)
                spurious |= strstr(diagnostic->message, "Checked numeric cast publication failed") != NULL;
        }
        if (spurious) fprintf(stderr, "Numeric cast publication cascaded after ownership error route=%u\n", route);
        CHECK(!spurious);
        if (route < 2) {
            CHECK(ctx->has_error && context_has_diagnostic(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
            if (route == 1) CHECK(context_has_diagnostic(ctx,
                PGY_CODE_SEM_TYPE_MISMATCH, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH));
        } else if (typed) {
            ASTNode *decl = ast_program_statement(program, 0);
            ASTNode *cast = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
            CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
            CHECK(semantic_param_publication_value(ctx, cast).ok);
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) != SLOT_PARAM_SUMMARY_ALL);
            CHECK(semantic_param_publication_checked_numeric_cast(ctx, cast));
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    /* A pending owner protocol probe, not reopening a finalized program. No
     * operand value was published before the independent error was raised. */
    Lexer *lexer = lexer_create("func CastRead()->Long{return StringLength(\"value\") as Long;}");
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *program = parser != NULL ? parser_parse_program(parser) : NULL;
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed && ast_assign_stable_ids(program));
    CHECK(lexer != NULL && parser != NULL && ctx != NULL);
    if (parsed && ctx != NULL) {
        ASTNode *cast = ast_return_value(ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 0));
        ctx->program_root = program;
        bool begun = semantic_param_publication_begin(ctx);
        CHECK(begun);
        if (begun) {
            semantic_error(ctx, cast, "Earlier independent semantic error for numeric-cast publication probe");
            CHECK(type_equals(type_check_expression(cast, ctx), TYPE_LONG));
            bool spurious = false;
            for (size_t d = 0; d < ctx->diagnostic_count; d++) {
                const Diagnostic *diagnostic = ctx->diagnostics[d];
                if (diagnostic != NULL && diagnostic->message != NULL)
                    spurious |= strstr(diagnostic->message, "Checked numeric cast publication failed") != NULL;
            }
            if (spurious) fprintf(stderr, "Numeric cast unpublished CALL operand cascaded after prior error\n");
            CHECK(!spurious && ctx->has_error);
        }
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
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

static void checked_tuple_literal_type_receipt(void)
{
    Lexer *lexer = lexer_create("func Main()->Void{let(first,second)=(1,2);Log(first);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    CHECK(type_check_program(program, ctx));
    if (program == NULL || ctx->has_error) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *binding = ast_block_statement(ast_func_body(decl), 0);
    CHECK(binding != NULL && binding->type == AST_LET_DESTRUCTURE);
    if (binding == NULL || binding->type != AST_LET_DESTRUCTURE) goto cleanup;
    ASTNode *tuple = binding->data.let_destructure.initializer;
    CHECK(tuple != NULL && tuple->type == AST_TUPLE_LITERAL);
    if (tuple == NULL || tuple->type != AST_TUPLE_LITERAL) goto cleanup;
    const char *issued = ast_tuple_literal_semantic_type_name(tuple);
    CHECK(issued != NULL);
    if (issued == NULL) goto cleanup;
    size_t name_length = strlen(issued);
    char *saved_name = malloc(name_length + 1);
    CHECK(saved_name != NULL);
    if (saved_name == NULL) goto cleanup;
    memcpy(saved_name, issued, name_length + 1);
    CHECK(ast_tuple_literal_set_semantic_type_name_copy(tuple, saved_name));
    CHECK(!ast_tuple_literal_set_semantic_type_name_copy(tuple, "Void"));

    ASTNode *clone = ast_clone(tuple);
    CHECK(clone != NULL && clone->stable_id == 0);
    if (clone != NULL) {
        CHECK(ast_tuple_literal_semantic_type_name(clone) == NULL);
        clone->stable_id = tuple->stable_id;
        CHECK(ast_tuple_literal_semantic_type_name(clone) == NULL);
        ast_destroy(clone);
    }
    uint32_t id = tuple->stable_id;
    tuple->stable_id++;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    tuple->stable_id = id;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    tuple->data.tuple_literal.semantic_type_syntax_id++;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    tuple->data.tuple_literal.semantic_type_syntax_id = id;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);

    ASTNode *child = tuple->data.tuple_literal.elements[0];
    uint32_t child_id = child->stable_id;
    child->stable_id++;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    child->stable_id = child_id;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    ASTNodeType child_shape = child->type;
    child->type = AST_STRING;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    child->type = child_shape;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    ASTNode *same_id = ast_clone(child);
    CHECK(same_id != NULL);
    if (same_id != NULL) {
        same_id->stable_id = child_id;
        tuple->data.tuple_literal.elements[0] = same_id;
        CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
        tuple->data.tuple_literal.elements[0] = child;
        ast_destroy(same_id);
        CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    }
    size_t count = tuple->data.tuple_literal.count;
    tuple->data.tuple_literal.count--;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    tuple->data.tuple_literal.count = count;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    tuple->data.tuple_literal.semantic_type_child_count--;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    tuple->data.tuple_literal.semantic_type_child_count = count;
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    CHECK(ast_tuple_literal_clear_semantic_type_name(tuple));
    CHECK(ast_tuple_literal_semantic_type_name(tuple) == NULL);
    CHECK(ast_tuple_literal_set_semantic_type_name_copy(tuple, saved_name));
    CHECK(ast_tuple_literal_semantic_type_name(tuple) != NULL);
    free(saved_name);
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
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
    ASTNode *view_call = ast_let_initializer(ast_block_statement(branch, 0));
    bool depends = false, retains = true;
    CHECK(semantic_param_publication_checked_result_relation(ctx, view_call, 0, &depends, &retains) && depends && !retains);
    CHECK(semantic_param_publication_checked_backing_relation(ctx, view_call, 0, &depends, &retains) && depends && !retains);
    CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
    CHECK(!semantic_param_publication_checked_backing_relation(ctx, call, 0, &depends, &retains));
    CHECK(!semantic_param_publication_slot_token_operand(ctx, token));
    uint32_t view_id = view_call->stable_id;
    view_call->stable_id++;
    CHECK(!semantic_param_publication_checked_result_relation(ctx, view_call, 0, &depends, &retains));
    CHECK(!semantic_param_publication_checked_backing_relation(ctx, view_call, 0, &depends, &retains));
    view_call->stable_id = view_id;
    CHECK(semantic_param_publication_checked_result_relation(ctx, view_call, 0, &depends, &retains) && depends && !retains);
    CHECK(semantic_param_publication_checked_backing_relation(ctx, view_call, 0, &depends, &retains) && depends && !retains);
    ASTNode *operations[] = {ast_block_statement(ast_func_body(main_decl), 1),
        ast_block_statement(ast_func_body(main_decl), 3)};
    for (size_t operation = 0; operation < 2; operation++) {
        ASTNode *actual = operations[operation];
        size_t ordinal = operation == 0 ? 2 : 1;
        ASTNode *operand = ast_call_argument(actual, ordinal), *source = ast_call_argument(actual, 0);
        CHECK(semantic_param_publication_slot_token_operand(ctx, operand));
        uint32_t operand_binding = ast_identifier_binding_syntax_id(operand);
        CHECK(operand_binding == 0);
        PublishedNode *row = NULL;
        FunctionParamPublicationStore *publications = ctx->function_param_publications;
        for (size_t i = 0; i < publications->node_capacity; i++)
            if (publications->nodes[i].node == operand) row = &publications->nodes[i];
        CHECK(row != NULL);
        if (row == NULL) continue;
        PublishedNode saved = *row;
        ASTNode saved_actual = *actual;
        uint32_t operand_id = operand->stable_id, source_binding = ast_identifier_binding_syntax_id(source);
        for (unsigned fault = 0; fault < 8; fault++) {
            if (fault == 0) operand->stable_id++;
            if (fault == 1) actual->stable_id++;
            if (fault == 2) ast_call_set_semantic_callee_builtin_kind(actual, BUILTIN_NOT_BUILTIN);
            if (fault == 3) ast_identifier_set_binding_syntax_id(source, source_binding + 1);
            if (fault == 4) row->relation_input_type = TYPE_INT;
            if (fault == 5) row->slot_token_operand = false;
            if (fault == 6) row->slot_token_ordinal = ordinal + 1;
            if (fault == 7) actual->data.call.arg_count--;
            CHECK(!semantic_param_publication_slot_token_operand(ctx, operand));
            *row = saved; *actual = saved_actual; operand->stable_id = operand_id;
            ast_identifier_set_binding_syntax_id(source, source_binding);
            CHECK(semantic_param_publication_slot_token_operand(ctx, operand));
            CHECK(ast_identifier_binding_syntax_id(operand) == operand_binding);
        }
    }
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
    lexer = lexer_create("func Main()->Void{let slot:SecureSlot<Int> = ClaimSecureSlot<Int>();Write(slot,7,slot_token);Log(Read(slot,slot_token));Release(slot,slot_token);}");
    parser = parser_create(lexer); program = parser_parse_program(parser); ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (typed) {
        ASTNode *owned_body = ast_func_body(ast_program_statement(program, 0));
        ASTNode *read = ast_call_argument(ast_block_statement(owned_body, 2), 0);
        ASTNode *read_token = ast_call_argument(read, 1);
        CHECK(ast_identifier_binding_syntax_id(read_token) == 0);
        CHECK(semantic_param_publication_slot_token_operand(ctx, read_token));
        bool dependent = true, retained = true;
        CHECK(semantic_param_publication_checked_result_relation(ctx, read, 0, &dependent, &retained) && !dependent && !retained);
        CHECK(semantic_param_publication_checked_backing_relation(ctx, read, 0, &dependent, &retained) && dependent && !retained);
        uint32_t token_id = read_token->stable_id;
        ASTNode saved_read = *read;
        for (unsigned fault = 0; fault < 3; fault++) {
            if (fault == 0) read_token->stable_id++;
            if (fault == 1) read->stable_id++;
            if (fault == 2) ast_call_set_semantic_callee_builtin_kind(read, BUILTIN_NOT_BUILTIN);
            CHECK(!semantic_param_publication_slot_token_operand(ctx, read_token));
            CHECK(!semantic_param_publication_checked_result_relation(ctx, read, 0, &dependent, &retained));
            CHECK(!semantic_param_publication_checked_backing_relation(ctx, read, 0, &dependent, &retained));
            *read = saved_read; read_token->stable_id = token_id;
            CHECK(semantic_param_publication_slot_token_operand(ctx, read_token));
            CHECK(semantic_param_publication_checked_result_relation(ctx, read, 0, &dependent, &retained) && !dependent && !retained);
            CHECK(semantic_param_publication_checked_backing_relation(ctx, read, 0, &dependent, &retained) && dependent && !retained);
            CHECK(ast_identifier_binding_syntax_id(read_token) == 0);
        }
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
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
        CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
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
            bool current0 = semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains);
            bool current1 = semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains);
            if (current0 || current1) fprintf(stderr, "nominal struct fault=%u current0=%d current1=%d\n", fault, current0, current1);
            CHECK(!current0);
            CHECK(!current1);
            CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
            bool claims_current = function_param_flow_index_claims_current(ctx, index);
            bool rebuilt = function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, routine, &rechecked);
            if (claims_current || rebuilt) fprintf(stderr, "nominal struct fault=%u claims=%d rebuilt=%d\n", fault, claims_current, rebuilt);
            CHECK(!claims_current);
            CHECK(!rebuilt);
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
            CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
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

/* Selected field identity and the nominal constructor's actual are separate
 * current receipts. Neither getter grants String storage or cleanup rights. */
static void checked_selected_string_field_constructor_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct TextRecord{text:String;other:String;}"
        "func Main()->Void{let record:TextRecord = "
        "TextRecord(other:\"x\",text:\"stable\");Log(record.text);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    bool typed = program != NULL && !parser_has_error(parser) &&
        type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *main_decl = ast_program_statement(program, 1);
    ASTNode *body = main_decl == NULL ? NULL : ast_func_body(main_decl);
    ASTNode *local = body == NULL ? NULL : ast_block_statement(body, 0);
    ASTNode *log = body == NULL ? NULL : ast_block_statement(body, 1);
    ASTNode *ctor = local == NULL ? NULL : ast_let_initializer(local);
    ASTNode *member = log == NULL ? NULL : ast_call_argument(log, 0);
    ASTNode *receiver = member == NULL ? NULL : ast_member_object(member);
    PublishedNode *member_row = published_node_for(ctx, member);
    PublishedNode *ctor_row = published_node_for(ctx, ctor);
    CHECK(ctor != NULL && ctor->type == AST_CALL &&
        ast_call_arg_count(ctor) == 2 && ctor->data.call.arg_names != NULL &&
        member != NULL && member->type == AST_MEMBER_ACCESS &&
        receiver != NULL && receiver->type == AST_IDENTIFIER &&
        member_row != NULL && ctor_row != NULL &&
        ctor_row->nominal_result != NULL &&
        ctor_row->nominal_result->field_count == 2);
    if (ctor == NULL || ctor->type != AST_CALL ||
        ast_call_arg_count(ctor) != 2 || ctor->data.call.arg_names == NULL ||
        member == NULL || member->type != AST_MEMBER_ACCESS ||
        receiver == NULL || receiver->type != AST_IDENTIFIER ||
        member_row == NULL || ctor_row == NULL ||
        ctor_row->nominal_result == NULL ||
        ctor_row->nominal_result->field_count != 2) goto cleanup;
    ASTNode *caller = NULL, *root = NULL, *actual = NULL;
    size_t ordinal = SIZE_MAX;
    SemanticPublishedSelectedField selected = {0};
    CHECK(semantic_param_publication_selected_member_field_current(ctx,
        member, &caller, &root, &selected) && caller == main_decl &&
        root == receiver && selected.field_ordinal == 0 &&
        selected.field_count == 2 && selected.field_type == TYPE_STRING);
    CHECK(semantic_param_publication_constructor_field_actual_current(ctx,
        ctor, &selected, &actual, &ordinal) && ordinal == 1 &&
        actual == ast_call_argument(ctor, 1));
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    size_t ready_count = flow == NULL ? 0 : flow->count;
    size_t ready_indexes = flow == NULL ? 0 : flow->program_point_index_count;
    size_t ready_work = flow == NULL ? 0 : flow->work_units;
    size_t ready_demand = flow == NULL ? 0 : flow->demand_epoch;
    size_t ready_evaluations = flow == NULL ? 0 : flow->body_evaluations;
    PublishedProjection projection = member_row->projection;
    SemanticNominalResultField mapped = ctor_row->nominal_result->fields[1];
    ASTNode *mapped_actual = ctor->data.call.arguments[1];
    char *mapped_name = ctor->data.call.arg_names[1];
    uint32_t member_id = member->stable_id, receiver_id = receiver->stable_id;
    uint32_t receiver_binding = ast_identifier_binding_syntax_id(receiver);
    uint32_t ctor_id = ctor->stable_id;
    uint32_t selected_decl = ast_call_semantic_callee_decl_id(ctor);
    CHECK(selected_decl != 0 &&
        ctor_row->nominal_result->fields[1].field_index == 0);
    for (unsigned fault = 0; fault < 15; fault++) {
        if (fault == 0) member_row->projection.present = false;
        if (fault == 1) member_row->projection.field_ordinal++;
        if (fault == 2) member_row->projection.field_id++;
        if (fault == 3) member_row->projection.field_type_node = NULL;
        if (fault == 4) member_row->projection.declaration_id++;
        if (fault == 5) member->stable_id++;
        if (fault == 6) receiver->stable_id++;
        if (fault == 7) ast_identifier_set_binding_syntax_id(receiver,
            receiver_binding + 1);
        if (fault == 8) ast_call_set_semantic_callee_decl_id(ctor, 0);
        if (fault == 9) ctor->data.call.arguments[1] = ctor->data.call.arguments[0];
        if (fault == 10) ctor->data.call.arg_names[1] = "other";
        if (fault == 11) ctor_row->nominal_result->fields[1].field_index = 1;
        if (fault == 12) ctor_row->nominal_result->fields[1].field_id++;
        if (fault == 13) ctor_row->nominal_result->fields[1].type_id++;
        if (fault == 14) ctor->stable_id++;
        SemanticPublishedSelectedField current = {0};
        bool member_current = semantic_param_publication_selected_member_field_current(
            ctx, member, &caller, &root, &current);
        bool constructor_current =
            semantic_param_publication_constructor_field_actual_current(
                ctx, ctor, &selected, &actual, &ordinal);
        CHECK((fault < 8 ? !member_current : member_current) &&
            !constructor_current);
        member_row->projection = projection;
        ctor_row->nominal_result->fields[1] = mapped;
        ctor->data.call.arguments[1] = mapped_actual;
        ctor->data.call.arg_names[1] = mapped_name;
        member->stable_id = member_id;
        receiver->stable_id = receiver_id;
        ast_identifier_set_binding_syntax_id(receiver, receiver_binding);
        ast_call_set_semantic_callee_decl_id(ctor, selected_decl);
        ctor->stable_id = ctor_id;
        CHECK(semantic_param_publication_selected_member_field_current(ctx,
            member, &caller, &root, &current) && caller == main_decl &&
            root == receiver);
        CHECK(semantic_param_publication_constructor_field_actual_current(ctx,
            ctor, &selected, &actual, &ordinal) && ordinal == 1 &&
            actual == mapped_actual);
    }
    CHECK(ctx->function_param_flow_summaries == flow &&
        (flow == NULL || (flow->count == ready_count &&
        flow->program_point_index_count == ready_indexes &&
        flow->work_units == ready_work && flow->demand_epoch == ready_demand &&
        flow->body_evaluations == ready_evaluations)));
cleanup:
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    lexer = lexer_create(
        "struct TextRecord{text:String;other:String;}struct Holder{record:TextRecord;}"
        "func Main()->Void{let rows:Array<TextRecord> = [TextRecord(\"array\",\"x\")];"
        "let holder:Holder = Holder(TextRecord(\"nested\",\"y\"));"
        "Log(rows[0].text);Log(holder.record.text);}");
    parser = parser_create(lexer);
    program = parser_parse_program(parser);
    ctx = semantic_context_create();
    CHECK(program != NULL && !parser_has_error(parser));
    typed = program != NULL && !parser_has_error(parser) &&
        type_check_program(program, ctx);
    CHECK(typed);
    if (typed) {
        ASTNode *routine = ast_program_statement(program, 2);
        ASTNode *body = routine == NULL ? NULL : ast_func_body(routine);
        ASTNode *array_log = body == NULL ? NULL : ast_block_statement(body, 2);
        ASTNode *nested_log = body == NULL ? NULL : ast_block_statement(body, 3);
        ASTNode *array_member = array_log == NULL ? NULL : ast_call_argument(array_log, 0);
        ASTNode *nested_member = nested_log == NULL ? NULL : ast_call_argument(nested_log, 0);
        ASTNode *array_receiver = array_member == NULL ? NULL : ast_member_object(array_member);
        ASTNode *nested_receiver = nested_member == NULL ? NULL : ast_member_object(nested_member);
        SemanticPublishedSelectedField ignored = {0};
        CHECK(array_member != NULL && array_member->type == AST_MEMBER_ACCESS &&
            array_receiver != NULL && array_receiver->type == AST_ARRAY_ACCESS);
        CHECK(nested_member != NULL && nested_member->type == AST_MEMBER_ACCESS &&
            nested_receiver != NULL && nested_receiver->type == AST_MEMBER_ACCESS);
        CHECK(!semantic_param_publication_selected_member_field_current(ctx,
            array_member, NULL, NULL, &ignored));
        CHECK(!semantic_param_publication_selected_member_field_current(ctx,
            nested_member, NULL, NULL, &ignored));
    }
    semantic_context_destroy(ctx);
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_indexed_string_field_recipe_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct TextRecord{text:String;}struct Rows{ids:Array<Int>;}"
        "func MakeRecord(text:String)->TextRecord{return TextRecord(text);}"
        "func Inspect(text:String,count:Int,ref first:Rows,ref second:Rows)->Int{"
        "return StringLength(text)+count+ArrayLength(first.ids)+ArrayLength(second.ids);}"
        "func Main()->Void{let record:TextRecord=MakeRecord(\"stable\");"
        "let first:Rows=Rows([1]);let second:Rows=Rows([2]);"
        "let no_texts:Array<String> = [];"
        "Log(Inspect(record.text,1,first,second));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    Lexer *foreign_lexer = NULL;
    Parser *foreign_parser = NULL;
    ASTNode *foreign_program = NULL;
    CHECK(program != NULL && !parser_has_error(parser));
    bool typed = program != NULL && !parser_has_error(parser) &&
        type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *main_decl = ast_program_statement(program, 4);
    ASTNode *body = main_decl == NULL ? NULL : ast_func_body(main_decl);
    ASTNode *definition = body == NULL ? NULL : ast_block_statement(body, 0);
    ASTNode *log = body == NULL ? NULL : ast_block_statement(body, 4);
    ASTNode *sink = log == NULL ? NULL : ast_call_argument(log, 0);
    ASTNode *member = sink == NULL ? NULL : ast_call_argument(sink, 0);
    PublishedNode *member_row = published_node_for(ctx, member);
    IndexedStringFieldRecipe *recipe = member_row == NULL ? NULL :
        member_row->indexed_string_field;
    CHECK(main_decl != NULL && definition != NULL && sink != NULL &&
        member != NULL && member->type == AST_MEMBER_ACCESS &&
        recipe != NULL && recipe->definition == definition &&
        recipe->producer == ast_let_initializer(definition) &&
        recipe->sink == sink && recipe->sink_ordinal == 0 &&
        recipe->return_count != 0 && recipe->dependency_count != 0 &&
        recipe->sealed_returns == recipe->return_count &&
        recipe->sealed_dependencies == recipe->dependency_count);
    if (recipe == NULL || recipe->producer == NULL ||
        recipe->producer->type != AST_CALL ||
        ast_call_arg_count(recipe->producer) == 0 ||
        recipe->return_count == 0 || recipe->dependency_count == 0 ||
        recipe->returns == NULL || recipe->dependencies == NULL) goto cleanup;
    ASTNode *literal = ast_call_argument(recipe->producer, 0);
    ASTNode *constructor = (ASTNode *)recipe->returns[0].constructor;
    PublishedNode *constructor_row = published_node_for(ctx, constructor);
    CHECK(literal != NULL && literal->type == AST_STRING &&
        constructor != NULL && constructor_row != NULL &&
        constructor_row->nominal_result == recipe->returns[0].mapping &&
        member_row->indexed_string_source_present &&
        member_row->indexed_string_source_id == UINT32_MAX);
    if (literal == NULL || constructor_row == NULL ||
        constructor_row->nominal_result == NULL) goto cleanup;
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    CHECK(flow != NULL);
    if (flow == NULL) goto cleanup;
    size_t index_slot = SIZE_MAX;
    for (size_t i = 0; i < flow->program_point_index_count; i++)
        if (flow->program_point_indexes[i].function_decl ==
            recipe->dependencies[0].decl) index_slot = i;
    CHECK(index_slot != SIZE_MAX &&
        flow->program_point_indexes[index_slot].claims ==
            recipe->dependencies[0].claims);
    if (index_slot == SIZE_MAX) goto cleanup;
    foreign_lexer = lexer_create(
        "func Foreign()->Void{let borrowed:Array<String> = [\"borrowed\"];"
        "Log(borrowed[0]);}");
    foreign_parser = parser_create(foreign_lexer);
    foreign_program = parser_parse_program(foreign_parser);
    CHECK(foreign_program != NULL && !parser_has_error(foreign_parser));
    ASTNode *foreign_decl = foreign_program == NULL ? NULL :
        ast_program_statement(foreign_program, 0);
    ASTNode *foreign_body = foreign_decl == NULL ? NULL :
        ast_func_body(foreign_decl);
    ASTNode *foreign_log = foreign_body == NULL ? NULL :
        ast_block_statement(foreign_body, 1);
    ASTNode *foreign_access = foreign_log == NULL ? NULL :
        ast_call_argument(foreign_log, 0);
    CHECK(foreign_access != NULL && foreign_access->type == AST_ARRAY_ACCESS);
    if (foreign_access == NULL || foreign_access->type != AST_ARRAY_ACCESS)
        goto cleanup;
    size_t ready_count = flow->count, ready_indexes = flow->program_point_index_count;
    size_t ready_demand = flow->demand_epoch, ready_work = flow->work_units;
    size_t ready_evaluations = flow->body_evaluations;
    FunctionParamFlowSummaryEntry *ready_entries = flow->entries;
    FunctionParamFlowProgramPointIndex *ready_points = flow->program_point_indexes;
    uint32_t source = UINT32_MAX;
    CHECK(semantic_indexed_string_field_current(ctx, recipe) &&
        semantic_indexed_string_borrow_source_current(member, ctx, &source) &&
        source == 0);
    CHECK(flow->count == ready_count &&
        flow->program_point_index_count == ready_indexes &&
        flow->demand_epoch == ready_demand &&
        flow->work_units == ready_work &&
        flow->body_evaluations == ready_evaluations &&
        flow->entries == ready_entries &&
        flow->program_point_indexes == ready_points);
    PublishedProjection projection = member_row->projection;
    SemanticNominalResultSnapshot *nominal = constructor_row->nominal_result;
    ASTNode *initializer = definition->data.let_decl.initializer;
    ASTNode *actual = recipe->producer->data.call.arguments[0];
    FunctionParamFlowIndexClaims *dependency_claims = recipe->dependencies[0].claims;
    for (unsigned fault = 0; fault < 8; fault++) {
        FunctionParamFlowProgramPointIndex *index =
            &flow->program_point_indexes[index_slot];
        if (fault == 0) member_row->indexed_string_field = NULL;
        if (fault == 1) member_row->projection.present = false;
        if (fault == 2) member_row->projection.field_id++;
        if (fault == 3) constructor_row->nominal_result = NULL;
        if (fault == 4) definition->data.let_decl.initializer = foreign_access;
        if (fault == 5) recipe->producer->data.call.arguments[0] = foreign_access;
        if (fault == 6) recipe->dependencies[0].claims = NULL;
        if (fault == 7) index->claims = NULL;
        source = UINT32_MAX;
        bool current = semantic_indexed_string_field_current(ctx, recipe);
        bool sourced = semantic_indexed_string_borrow_source_current(
            member, ctx, &source);
        CHECK(!current && (!sourced || source != 0));
        member_row->indexed_string_field = recipe;
        member_row->projection = projection;
        constructor_row->nominal_result = nominal;
        definition->data.let_decl.initializer = initializer;
        recipe->producer->data.call.arguments[0] = actual;
        recipe->dependencies[0].claims = dependency_claims;
        index = &flow->program_point_indexes[index_slot];
        index->claims = dependency_claims;
        source = UINT32_MAX;
        CHECK(semantic_indexed_string_field_current(ctx, recipe) &&
            semantic_indexed_string_borrow_source_current(member, ctx, &source) &&
            source == 0);
        CHECK(ctx->function_param_flow_summaries == flow &&
            flow->count == ready_count &&
            flow->program_point_index_count == ready_indexes &&
            flow->demand_epoch == ready_demand &&
            flow->work_units == ready_work &&
            flow->body_evaluations == ready_evaluations &&
            flow->entries == ready_entries &&
            flow->program_point_indexes == ready_points);
    }
cleanup:
    ast_destroy(foreign_program);
    parser_destroy(foreign_parser); lexer_destroy(foreign_lexer);
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
    {
        Lexer *lexer = lexer_create("class Packet{let value:Int;}func Main()->Void{let packet=Packet(1);}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (typed) {
            ASTNode *declaration = ast_program_statement(program, 0);
            ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 0));
            PublishedNode *row = NULL;
            FunctionParamPublicationStore *store = ctx->function_param_publications;
            for (size_t i = 0; i < store->node_capacity; i++)
                if (store->nodes[i].node == call) row = &store->nodes[i];
            CHECK(row != NULL && row->nominal_result != NULL);
            CHECK(semantic_param_publication_nominal_result_complete(ctx, call));
            CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
            if (row != NULL && row->nominal_result != NULL) {
                bool depends = false, retains = true;
                CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
                CHECK(row->nominal_result->flavor == TYPE_NOMINAL_CLASS);
                SemanticNominalResultSnapshot *snapshot = row->nominal_result;
                SemanticNominalResultSnapshot saved_snapshot = *snapshot;
                PublishedNode saved = *row;
                ASTNode saved_declaration = *declaration, saved_call = *call;
                for (unsigned fault = 0; fault < 9; fault++) {
                    if (fault == 0) declaration->stable_id++;
                    if (fault == 1) declaration->type = AST_FUNC_DECL;
                    if (fault == 2) snapshot->decl_id++;
                    if (fault == 3) snapshot->flavor = TYPE_NOMINAL_STRUCT;
                    if (fault == 4) row->value_type_name = "Int";
                    if (fault == 5) ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(ast_program_statement(program, 1)));
                    if (fault == 6) declaration->data.class_decl.is_struct = true;
                    if (fault == 7) snapshot->decl = ast_program_statement(program, 1);
                    if (fault == 8) row->value_present = false;
                    bool complete = semantic_param_publication_nominal_result_complete(ctx, call);
                    bool current = semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains);
                    if (complete || current) fprintf(stderr, "nominal class fault=%u complete=%d current=%d\n", fault, complete, current);
                    CHECK(!complete);
                    CHECK(!current);
                    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
                    *row = saved; *snapshot = saved_snapshot; *declaration = saved_declaration; *call = saved_call;
                    CHECK(semantic_param_publication_nominal_result_complete(ctx, call));
                    CHECK(semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) && depends && !retains);
                    CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
                }
            }
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    for (unsigned retaining = 0; retaining < 2; retaining++) {
        Lexer *lexer = lexer_create(retaining == 0
            ? "object Packet{let hp:Int;}class Holder{let packet:Packet;}func Store(ref packet:Packet)->Void{let holder=Holder(Packet(packet.hp));}"
            : "object Packet{let hp:Int;}class Holder{let packet:Packet;}func Store(ref packet:Packet)->Void{let holder=Holder(packet);}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (retaining != 0) {
            CHECK(!typed && ctx->has_error);
            bool refused = false;
            for (size_t i = 0; i < ctx->diagnostic_count; i++) {
                const Diagnostic *diagnostic = ctx->diagnostics[i];
                if (diagnostic->level != DIAG_ERROR) continue;
                bool exact = diagnostic->code != NULL && diagnostic->cause_ir != NULL &&
                    strcmp(diagnostic->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0 &&
                    strcmp(diagnostic->cause_ir, PGY_CAUSE_BORROW_ESCAPE) == 0;
                CHECK(exact); refused |= exact;
            }
            CHECK(refused);
        } else {
            CHECK(typed && !ctx->has_error);
            if (parsed && (!typed || ctx->has_error)) {
                ASTNode *decl = ast_program_statement(program, 2);
                ASTNode *outer = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
                ASTNode *inner = ast_call_argument(outer, 0);
                bool inner_depends = false, inner_retains = false, outer_depends = false, outer_retains = false;
                bool inner_relation = semantic_param_publication_checked_result_relation(ctx, inner, 0, &inner_depends, &inner_retains);
                bool outer_relation = semantic_param_publication_checked_result_relation(ctx, outer, 0, &outer_depends, &outer_retains);
                unsigned raw = function_param_flow_summary_for_param(ctx, decl, 0);
                fprintf(stderr, "object reconstruction inner complete=%d relation=%d depends=%d retains=%d outer complete=%d relation=%d depends=%d retains=%d mask=%u\n",
                    semantic_param_publication_nominal_result_complete(ctx, inner), inner_relation, inner_depends, inner_retains,
                    semantic_param_publication_nominal_result_complete(ctx, outer), outer_relation, outer_depends, outer_retains, raw);
                PublishedNode *inner_row = NULL, *argument_row = NULL;
                ASTNode *argument_node = ast_call_argument(inner, 0);
                FunctionParamPublicationStore *publications = ctx->function_param_publications;
                for (size_t i = 0; publications != NULL && i < publications->node_capacity; i++) {
                    if (publications->nodes[i].node == inner) inner_row = &publications->nodes[i];
                    if (publications->nodes[i].node == argument_node) argument_row = &publications->nodes[i];
                }
                const Type *value_type = inner_row == NULL ? NULL : inner_row->value_type;
                SemanticNominalResultSnapshot *snapshot = inner_row == NULL ? NULL : inner_row->nominal_result;
                const char *semantic_name = ast_call_semantic_value_type_name(inner);
                fprintf(stderr, "object inner row=%d present=%d type=%p kind=%d flavor=%d name=%s stored=%s semantic=%s checked=%d current=%d\n",
                    inner_row != NULL, inner_row != NULL && inner_row->value_present, (const void *)value_type,
                    value_type == NULL ? -1 : (int)value_type->kind, value_type == NULL ? -1 : (int)value_type->nominal_flavor,
                    value_type == NULL || value_type->name == NULL ? "<null>" : value_type->name,
                    inner_row == NULL || inner_row->value_type_name == NULL ? "<null>" : inner_row->value_type_name,
                    semantic_name == NULL ? "<null>" : semantic_name,
                    semantic_param_publication_checked_call(ctx, inner), semantic_param_publication_value(ctx, inner).ok);
                fprintf(stderr, "object inner snapshot=%d flavor=%d decl=%s declID=%u result=%s fields=%zu current=%d\n",
                    snapshot != NULL, snapshot == NULL ? -1 : (int)snapshot->flavor,
                    snapshot == NULL || snapshot->decl_name == NULL ? "<null>" : snapshot->decl_name,
                    snapshot == NULL ? 0 : snapshot->decl_id,
                    snapshot == NULL || snapshot->result_type_name == NULL ? "<null>" : snapshot->result_type_name,
                    snapshot == NULL ? 0 : snapshot->field_count,
                    snapshot != NULL && semantic_nominal_result_snapshot_current(ctx, inner, snapshot));
                SemanticPublishedValue argument_value = semantic_param_publication_value(ctx, argument_node);
                fprintf(stderr, "object argument snapshotType=%p publishedType=%p name=%s snapshotName=%s current=%d origin=%d binding=%u\n",
                    inner_row == NULL || inner_row->arguments == NULL || inner_row->argument_count == 0 ? NULL : (const void *)inner_row->arguments[0].type,
                    argument_row == NULL ? NULL : (const void *)argument_row->value_type,
                    argument_row == NULL || argument_row->value_type_name == NULL ? "<null>" : argument_row->value_type_name,
                    snapshot == NULL || snapshot->fields == NULL || snapshot->field_count == 0 || snapshot->fields[0].argument_type_name == NULL ? "<null>" : snapshot->fields[0].argument_type_name,
                    argument_value.ok, argument_value.origin_ready, semantic_param_publication_binding(ctx, argument_node));
                FunctionParamFlowSummaryStore *summaries = ctx->function_param_flow_summaries;
                for (size_t i = 0; summaries != NULL && i < summaries->count; i++) {
                    const FunctionParamFlowSummaryEntry *entry = &summaries->entries[i];
                    if (entry->function_decl == decl && entry->param_index == 0)
                        fprintf(stderr, "object reconstruction producer=%d unsupported=%d unproved=%d pending=%d ready=%d\n",
                            (int)entry->output_producer, entry->analysis_unsupported, entry->returned_backing_unproved,
                            entry->returned_backing_pending, entry->output_dependencies_ready);
                }
            }
            if (typed) {
                ASTNode *decl = ast_program_statement(program, 2);
                ASTNode *outer = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
                ASTNode *call = ast_call_argument(outer, 0);
                PublishedNode *row = NULL;
                FunctionParamPublicationStore *store = ctx->function_param_publications;
                for (size_t i = 0; i < store->node_capacity; i++)
                    if (store->nodes[i].node == call) row = &store->nodes[i];
                CHECK(row != NULL && row->nominal_result != NULL);
                CHECK(semantic_param_publication_nominal_result_complete(ctx, call));
                unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
                CHECK((mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
                if (row != NULL && row->nominal_result != NULL) {
                    SemanticNominalResultSnapshot *snapshot = row->nominal_result;
                    CHECK(snapshot->flavor == TYPE_NOMINAL_OBJECT);
                    ASTNode *object_decl = ast_program_statement(program, 0);
                    bool raw_struct = object_decl->data.class_decl.is_struct;
                    for (unsigned fault = 0; fault < 3; fault++) {
                        if (fault < 2) snapshot->flavor = fault == 0 ? TYPE_NOMINAL_CLASS : TYPE_NOMINAL_STRUCT;
                        if (fault == 2) object_decl->data.class_decl.is_struct = !raw_struct;
                        CHECK(!semantic_param_publication_nominal_result_complete(ctx, call));
                        CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, call));
                        CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
                        snapshot->flavor = TYPE_NOMINAL_OBJECT;
                        object_decl->data.class_decl.is_struct = raw_struct;
                        CHECK(semantic_param_publication_nominal_result_complete(ctx, call));
                        CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
                    }
                }
            }
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    for (unsigned incomplete = 0; incomplete < 2; incomplete++) {
        Lexer *lexer = lexer_create(incomplete == 0
            ? "class Packet{let value:Int;let tail:Int;}func Main()->Void{let packet=Packet(1);}"
            : "class Packet{let value:Int=1;}func Main()->Void{let packet=Packet(2);}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        bool typed = type_check_program(program, ctx);
        CHECK(typed);
        if (typed) {
            ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 0));
            bool depends = false, retains = true;
            CHECK(!semantic_param_publication_nominal_result_complete(ctx, call));
            CHECK(!semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains));
            CHECK(semantic_param_publication_call_analysis_unmodelled(ctx, call));
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    {
        Lexer *lexer = lexer_create("class Empty{}func Make()->Empty{return Empty();}func Main()->Void{let exact=Empty();let ordinary=Make();}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        bool typed = type_check_program(program, ctx);
        CHECK(typed);
        if (typed) {
            ASTNode *body = ast_func_body(ast_program_statement(program, 2));
            ASTNode *exact = ast_let_initializer(ast_block_statement(body, 0));
            ASTNode *ordinary = ast_let_initializer(ast_block_statement(body, 1));
            bool depends = false, retains = true;
            CHECK(semantic_param_publication_nominal_result_complete(ctx, exact));
            CHECK(!semantic_param_publication_checked_result_relation(ctx, exact, 0, &depends, &retains));
            CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, exact));
            CHECK(!semantic_param_publication_nominal_result_complete(ctx, ordinary));
            CHECK(!semantic_param_publication_call_analysis_unmodelled(ctx, ordinary));
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_unmodelled_map_class_result(void)
{
    Lexer *lexer = lexer_create(
        "class Packet{let value:Int;}func Snapshot(rows:HashMap<String,Packet>)->Packet{return MapGet(rows,\"key\");}func Forward(rows:HashMap<String,Packet>)->Packet{return Snapshot(rows);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (typed) {
        ASTNode *snapshot = ast_program_statement(program, 1);
        ASTNode *map_get = ast_return_value(ast_block_statement(ast_func_body(snapshot), 0));
        CHECK(!semantic_typed_map_query_fact_issued(ctx, map_get));
        CHECK(!semantic_typed_map_query_result_independent(ctx, map_get));
        CHECK(semantic_param_publication_call_analysis_unmodelled(ctx, map_get));
        CHECK(function_param_flow_summary_for_param(ctx, snapshot, 0) == SLOT_PARAM_SUMMARY_ALL);
        FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
        CHECK(store != NULL && !store->failed && !ctx->has_error);
        bool unready = false;
        for (size_t i = 0; store != NULL && i < store->count; i++) {
            FunctionParamFlowSummaryEntry *entry = &store->entries[i];
            if (entry->function_decl == snapshot && entry->param_index == 0)
                unready = entry->analysis_unsupported && entry->returned_backing_unproved &&
                    !entry->returned_backing_pending && !entry->output_dependencies_ready;
        }
        CHECK(unready);
        ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(ast_program_statement(program, 2)), 0));
        SlotFunctionLookup lookup = {ctx, program};
        bool dependent = true;
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 0, &dependent));
        CHECK(!function_param_flow_callee_relation_ready(&lookup, call, 0));
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
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
        if (raw != expected[c]) fprintf(stderr, "callable raw case=%u mask=%u expected=%u\n", c, raw, expected[c]);
        CHECK(raw == expected[c]);
        CHECK(semantic_callable_param_escape_summary(decl, 0, ctx) == raw);
        if (c == 1) {
            ASTNode *channel = ast_block_statement(ast_func_body(decl), 0);
            ASTNode *capacity = ast_call_argument(ast_let_initializer(channel), 0);
            PublishedNode *receipt = NULL, *capacity_row = NULL;
            FunctionParamPublicationStore *publications = ctx->function_param_publications;
            for (size_t i = 0; i < publications->node_capacity; i++) {
                if (publications->nodes[i].node == channel) receipt = &publications->nodes[i];
                if (publications->nodes[i].node == capacity) capacity_row = &publications->nodes[i];
            }
            CHECK(semantic_param_publication_channel_binding(ctx, channel));
            CHECK(receipt != NULL && capacity_row != NULL && receipt->arguments != NULL);
            if (receipt != NULL && capacity_row != NULL && receipt->arguments != NULL) {
                PublishedNode saved = *receipt, saved_capacity = *capacity_row;
                PublishedCallArgument argument = receipt->arguments[0];
                ASTNode *annotation = ast_let_type(channel);
                GenericParam *element = ast_generic_param_at(ast_type_generic_args(annotation), 0);
                CHECK(annotation != NULL && element != NULL);
                char *constructor_name = annotation->data.type.name;
                char *element_name = element == NULL ? NULL : element->name;
                for (unsigned fault = 0; fault < 8; fault++) {
                    if (fault == 0) receipt->channel_binding = false;
                    if (fault == 1) receipt->channel_type_id++;
                    if (fault == 2) receipt->channel_element_type = TYPE_INT;
                    if (fault == 3) capacity_row->value_present = false;
                    if (fault == 4) receipt->target_id++;
                    if (fault == 5) receipt->arguments[0].type = TYPE_STRING;
                    if (fault == 6) annotation->data.type.name = "Array";
                    if (fault == 7 && element != NULL) element->name = "Int";
                    bool current = semantic_param_publication_channel_binding(ctx, channel);
                    unsigned stale_mask = function_param_flow_summary_for_param(ctx, decl, 0);
                    if (current || stale_mask != SLOT_PARAM_SUMMARY_ALL)
                        fprintf(stderr, "channel receipt fault=%u current=%d mask=%u\n", fault, current, stale_mask);
                    CHECK(!current);
                    CHECK(stale_mask == SLOT_PARAM_SUMMARY_ALL);
                    *receipt = saved; *capacity_row = saved_capacity;
                    receipt->arguments[0] = argument;
                    annotation->data.type.name = constructor_name;
                    if (element != NULL) element->name = element_name;
                    CHECK(semantic_param_publication_channel_binding(ctx, channel));
                    CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == raw);
                }
            }
        }
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
    ASTNode *echo = ast_program_statement(program, 0);
    ASTNode *returned = ast_return_value(ast_block_statement(ast_func_body(echo), 0));
    CHECK(returned != NULL && returned->type == AST_IDENTIFIER);
    (void)function_param_flow_summary_for_param(ctx, echo, 0);
    FunctionParamFlowSummaryStore *echo_summaries = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *echo_index = NULL;
    CHECK(echo_summaries != NULL &&
        function_param_flow_program_point_index_build(echo_summaries, echo, &echo_index));
    FunctionParamFlowBackingFact *echo_frame = echo_index != NULL &&
        echo_index->backing_frames != NULL ? echo_index->backing_frames[0] : NULL;
    CHECK(echo_frame != NULL && (echo_frame->effects[0] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
    if (echo_frame != NULL && returned != NULL) {
        bool redundant_leaf = false;
        for (size_t n = 0; n < echo_frame->node_count; n++)
            redundant_leaf |= echo_frame->nodes[n].node == returned;
        CHECK(!redundant_leaf);
        CHECK(function_param_flow_backing_rows_current(echo_summaries, echo));
        uint32_t id = ast_node_stable_id(returned);
        returned->stable_id = id + 1;
        CHECK(!function_param_flow_backing_rows_current(echo_summaries, echo));
        returned->stable_id = id;
        uint32_t binding = ast_identifier_binding_syntax_id(returned);
        ast_identifier_set_binding_syntax_id(returned, 0);
        CHECK(!function_param_flow_backing_rows_current(echo_summaries, echo));
        ast_identifier_set_binding_syntax_id(returned, binding);
        PublishedNode *published = published_node_for(ctx, returned);
        CHECK(published != NULL);
        if (published != NULL) {
            bool origin_ready = published->origin_ready;
            published->origin_ready = !origin_ready;
            CHECK(!function_param_flow_backing_rows_current(echo_summaries, echo));
            published->origin_ready = origin_ready;
        }
        CHECK(function_param_flow_backing_rows_current(echo_summaries, echo));
    }
    SemanticParamPublicationView view;
    CHECK(semantic_param_publication_admitted_view(ctx, &view));
    CHECK(view.call_count == 1);
    if (view.call_count != 1) goto cleanup;
    PendingRefCall *row = &ctx->function_param_publications->calls[0];
    ASTNode *escape = NULL;
    CHECK(semantic_param_publication_ref_call_current(ctx, row));
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
    size_t diagnostic_formal = SIZE_MAX;
    CHECK(semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
    CHECK(diagnostic_formal == row->formal_ordinal);
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
    for (unsigned fault = 0; fault < 12; fault++) {
        uint32_t program_id = program->stable_id;
        char target_initial = row->target_name != NULL ? row->target_name[0] : '\0';
        if (fault == 0) row->formal_id++;
        if (fault == 1) row->formal_ordinal++;
        if (fault == 2) row->ordinal++;
        if (fault == 3) row->site_binding_id++;
        if (fault == 4) row->target_binding_id++;
        if (fault == 5) row->call_target_id++;
        if (fault == 6 && row->target_name != NULL) row->target_name[0] = 'X';
        if (fault == 7) row->callee_id++;
        if (fault == 8) row->caller_id++;
        if (fault == 9) row->callee = row->caller;
        if (fault == 10) row->caller = row->callee;
        if (fault == 11) program->stable_id++;
        diagnostic_formal = SIZE_MAX;
        CHECK(!semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
        CHECK(!semantic_param_publication_ref_call_current(ctx, row));
        if (row->target_name != NULL) row->target_name[0] = target_initial;
        *row = saved;
        program->stable_id = program_id;
        diagnostic_formal = SIZE_MAX;
        CHECK(semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
        CHECK(diagnostic_formal == row->formal_ordinal);
        CHECK(semantic_param_publication_ref_call_current(ctx, row));
    }
    PublishedNode *call_value = published_node_for(ctx, row->call);
    PublishedNode *site_value = published_node_for(ctx, row->site);
    CHECK(call_value != NULL && site_value != NULL);
    if (call_value != NULL && site_value != NULL) {
        CHECK(call_value->value_present && call_value->string_value);
        CHECK(site_value->value_present && site_value->string_value);
        uint32_t selected_id = call_value->selected_callable_id;
        call_value->selected_callable_id++;
        CHECK(!semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
        CHECK(!semantic_param_publication_ref_call_current(ctx, row));
        call_value->selected_callable_id = selected_id;
        CHECK(semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
        CHECK(semantic_param_publication_ref_call_current(ctx, row));
        for (unsigned fault = 0; fault < 4; fault++) {
            bool call_present = call_value->value_present, call_string = call_value->string_value;
            bool site_present = site_value->value_present, site_string = site_value->string_value;
            if (fault == 0) call_value->value_present = false;
            if (fault == 1) call_value->string_value = false;
            if (fault == 2) site_value->value_present = false;
            if (fault == 3) site_value->string_value = false;
            diagnostic_formal = SIZE_MAX;
            CHECK(semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
            CHECK(diagnostic_formal == row->formal_ordinal);
            CHECK(!semantic_param_publication_ref_call_current(ctx, row));
            CHECK(!function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
            call_value->value_present = call_present; call_value->string_value = call_string;
            site_value->value_present = site_present; site_value->string_value = site_string;
            CHECK(semantic_param_publication_ref_call_diagnostic_current(ctx, row, &diagnostic_formal));
            CHECK(semantic_param_publication_ref_call_current(ctx, row));
            CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
        }
    }
    FunctionParamFlowSummaryStore *summaries = ctx->function_param_flow_summaries;
    size_t callee_entry = SIZE_MAX;
    for (size_t i = 0; summaries != NULL && i < summaries->count; i++)
        if (summaries->entries[i].function_decl == row->callee && summaries->entries[i].param_index == row->ordinal)
            callee_entry = i;
    CHECK(callee_entry != SIZE_MAX);
    if (callee_entry != SIZE_MAX) {
        FunctionParamFlowSummaryEntry saved_entry = summaries->entries[callee_entry];
        for (unsigned fault = 0; fault < 2; fault++) {
            if (fault == 0) summaries->entries[callee_entry].output_dependencies_ready = false;
            if (fault == 1) summaries->entries[callee_entry].returned_backing_unproved = true;
            CHECK(!function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
            summaries->entries[callee_entry] = saved_entry;
            CHECK(function_param_flow_return_loan_admitted(ctx, &view, row, &escape));
        }
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_closed_scalar_return_receipt(void)
{
    for (unsigned kind = 0; kind < 3; kind++) {
        Lexer *lexer = lexer_create(kind == 0
            ? "func Probe(text:String)->Int{return StringLength(Echo(text));}func Echo(ref text:String)->String{return text;}func Main()->Void{Log(Probe(\"static\"));}"
            : kind == 1 ? "func Probe(text:String)->Void{Log(text);return;}func Main()->Void{Probe(\"static\");}"
            : "func Probe(text:String)->Int{if StringLength(text)>0{return Second(text);}return StringLength(Echo(text));}func Echo(ref text:String)->String{return text;}func Second(ref text:String)->Int{return Probe(text);}func Main()->Void{Log(Probe(\"static\"));}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *decl = ast_program_statement(program, 0);
        ASTNode *statement = ast_block_statement(ast_func_body(ast_program_statement(program, kind == 0 ? 2 : kind == 1 ? 1 : 3)), 0);
        ASTNode *call = kind != 1 ? ast_call_argument(statement, 0) : statement;
        SlotFunctionLookup lookup = {ctx, program};
        /* StringLength itself borrows only for the call. Its nested Echo
         * still contributes Echo's RETURN for text: the named collector
         * unconditionally traverses call arguments and projects that callee
         * bit. Echo has no output mutation and a current ref/default header,
         * so the complete Probe -> Echo no-store graph can close. */
        CHECK(function_param_flow_store_get(&lookup) != NULL);
        FunctionParamFlowProgramPointIndex *points = NULL;
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decl, &points));
        if (points == NULL) goto cleanup;
        CHECK(!points->mutable_output_required && !points->private_local_stores);
        SlotSummaryOrigin origin = {.function_decl = decl, .param_index = 0, .param_name = "text"};
        unsigned raw = slot_param_summary_in_program_points(points->roots_by_param[0],
            points->root_counts[0], "text", &lookup, &origin);
        CHECK(((raw & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) == (kind != 1));
        CHECK((raw & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        CHECK(function_param_flow_no_store_graph(ctx->function_param_flow_summaries, decl));
        CHECK(semantic_param_publication_routine_return_dependency_is_scalar(ctx, decl));
        unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK((mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE |
            SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        bool dependent = true;
        CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 0, &dependent) && !dependent);
        FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
        size_t entry_index = SIZE_MAX;
        for (size_t i = 0; i < store->count; i++)
            if (store->entries[i].function_decl == decl && store->entries[i].param_index == 0) entry_index = i;
        PendingRoutine *routine = NULL;
        for (size_t i = 0; i < ctx->function_param_publications->routine_count; i++)
            if (ctx->function_param_publications->routines[i].decl == decl)
                routine = &ctx->function_param_publications->routines[i];
        CHECK(entry_index != SIZE_MAX && routine != NULL);
        if (entry_index == SIZE_MAX || routine == NULL) goto cleanup;
        FunctionParamFlowSummaryEntry saved = store->entries[entry_index];
        CHECK(saved.output_dependencies_ready && saved.output_count == 1 &&
            saved.output_dependencies != NULL && !saved.output_dependencies[0]);
        CHECK(saved.output_producer == (kind != 1 ? FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN
            : FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR));
        CHECK(!saved.returned_backing_pending && !saved.returned_backing_unproved);
        if (kind == 2) {
            /* Re-demand both members after the complete recursive solve.
             * This checks the issued reason's lifetime, not an epoch count
             * or a claim that a particular intermediate candidate was zero. */
            ASTNode *second = ast_program_statement(program, 2);
            unsigned other = function_param_flow_summary_for_param(ctx, second, 0);
            CHECK((other & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
            CHECK(store->entries[entry_index].output_producer == FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN);
        }
        PendingRoutine saved_routine = *routine;
        ASTNode *return_node = ast_func_return_type(decl);
        uint32_t return_id = return_node->stable_id;
        for (unsigned fault = 0; fault < (kind != 1 ? 11u : 5u); fault++) {
            if (fault == 0) routine->return_type = TYPE_STRING;
            if (fault == 1) routine->return_type = TYPE_UNKNOWN;
            if (fault == 2) decl->data.func_decl.return_type = NULL;
            if (fault == 3) return_node->stable_id++;
            if (fault == 4) routine->return_node_id++;
            if (fault == 5) store->entries[entry_index].output_producer = FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED;
            if (fault == 6) store->entries[entry_index].output_dependencies_ready = false;
            if (fault == 7) store->entries[entry_index].output_dependencies = NULL;
            if (fault == 8) store->entries[entry_index].output_count++;
            if (fault == 9) store->entries[entry_index].returned_backing_pending = true;
            if (fault == 10) store->entries[entry_index].returned_backing_unproved = true;
            CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 0, &dependent));
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
            *routine = saved_routine;
            decl->data.func_decl.return_type = return_node;
            return_node->stable_id = return_id;
            store->entries[entry_index] = saved;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 0, &dependent) && !dependent);
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    /* Descriptor, private-store, mutable, owned and unknown graphs retain
     * their established proof path despite a physical final return. */
    const char *controls[] = {
        "func Probe(ref text:String)->String{return text;}",
        "struct Held{text:String} func Probe(text:String)->Held{return Held(\"literal\");}",
        "func Probe(inout output:String,text:String)->Int{output=text;return 1;}",
        "func Probe(own text:String)->Int{return StringLength(text);}",
        "func Probe(text:String)->Int{let rows:Array<String> = [];ArrayPush(rows,text);return ArrayLength(rows);}",
        "extern \"C\"{func Opaque()->Int;}func Probe(text:String)->Int{return StringLength(text)+Opaque();}"
    };
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); i++) {
        Lexer *lexer = lexer_create(controls[i]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (typed) {
            ASTNode *decl = ast_program_statement(program, i == 1 || i == 5 ? 1 : 0);
            size_t ordinal = i == 2 ? 1 : 0;
            (void)function_param_flow_summary_for_param(ctx, decl, ordinal);
            FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
            bool found = false;
            for (size_t e = 0; store != NULL && e < store->count; e++)
                if (store->entries[e].function_decl == decl && store->entries[e].param_index == ordinal) {
                    found = true;
                    CHECK(store->entries[e].output_producer != FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN);
                }
            CHECK(found);
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_builtin_string_concat_receipt(void)
{
    for (unsigned overloaded = 0; overloaded < 2; overloaded++) {
        Lexer *lexer = lexer_create(overloaded
            ? "func operator_add_String(left:String,right:String)->String{return \"overloaded\";}func Probe(text:String,other:String)->String{return text+other;}"
            : "func Probe(text:String,other:String)->String{return text+other;}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *decl = ast_program_statement(program, overloaded ? 1 : 0);
        ASTNode *binary = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
        PublishedNode *receipt = NULL, *operands[2] = {NULL, NULL};
        FunctionParamPublicationStore *store = ctx->function_param_publications;
        ASTNode *children[2] = {ast_binary_left(binary), ast_binary_right(binary)};
        for (size_t i = 0; i < store->node_capacity; i++) {
            if (store->nodes[i].node == binary) receipt = &store->nodes[i];
            for (size_t side = 0; side < 2; side++)
                if (store->nodes[i].node == children[side]) operands[side] = &store->nodes[i];
        }
        SemanticPublishedValue value = semantic_param_publication_value(ctx, binary);
        CHECK(receipt != NULL && value.ok);
        if (receipt == NULL) goto cleanup;
        if (overloaded) {
            CHECK(!receipt->builtin_string_concat && !value.origin_ready);
            CHECK(!semantic_indexed_string_independent_result(binary, ctx));
            goto cleanup;
        }
        CHECK(receipt->builtin_string_concat && value.origin_ready && receipt->argument_count == 2);
        uint32_t source_id = UINT32_MAX;
        CHECK(semantic_indexed_string_independent_result(binary, ctx));
        CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
        unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK((mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE |
            SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        CHECK(receipt->arguments != NULL && operands[0] != NULL && operands[1] != NULL);
        if (receipt->arguments == NULL || operands[0] == NULL || operands[1] == NULL) goto cleanup;
        PublishedNode saved = *receipt;
        PgyTokenType operation = binary->data.binary.op.type;
        for (unsigned fault = 0; fault < 5; fault++) {
            if (fault == 0) receipt->builtin_string_concat = false;
            if (fault == 1) receipt->origin_ready = false;
            if (fault == 2) binary->data.binary.op.type = TOKEN_MINUS;
            if (fault == 3) receipt->value_type = TYPE_INT;
            if (fault == 4) receipt->argument_count = 1;
            SemanticPublishedValue current = semantic_param_publication_value(ctx, binary);
            CHECK(!current.ok || !current.origin_ready);
            CHECK(!semantic_indexed_string_independent_result(binary, ctx));
            CHECK(!semantic_indexed_string_borrow_source_current(binary, ctx, &source_id));
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
            *receipt = saved; binary->data.binary.op.type = operation;
            CHECK(semantic_param_publication_value(ctx, binary).origin_ready);
            CHECK(semantic_indexed_string_independent_result(binary, ctx));
            CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
        }
        for (size_t side = 0; side < 2; side++) {
            ASTNode original = *children[side];
            PublishedNode publication = *operands[side];
            PublishedCallArgument argument = receipt->arguments[side];
            for (unsigned fault = 0; fault < 9; fault++) {
                if (fault == 0) children[side]->stable_id++;
                if (fault == 1) {
                    if (side == 0) binary->data.binary.left = children[1];
                    else binary->data.binary.right = children[0];
                }
                if (fault == 2) ast_identifier_set_binding_syntax_id(children[side], 0);
                if (fault == 3) operands[side]->value_present = false;
                if (fault == 4) receipt->arguments[side].id++;
                if (fault == 5) receipt->arguments[side].node = children[1 - side];
                if (fault == 6) receipt->arguments[side].binding++;
                if (fault == 7) operands[side]->value_type = TYPE_INT;
                if (fault == 8) receipt->arguments[side].type = TYPE_INT;
                CHECK(!semantic_param_publication_value(ctx, binary).ok);
                CHECK(!semantic_indexed_string_independent_result(binary, ctx));
                CHECK(!semantic_indexed_string_borrow_source_current(binary, ctx, &source_id));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
                *children[side] = original; *operands[side] = publication;
                binary->data.binary.left = children[0]; binary->data.binary.right = children[1];
                receipt->arguments[side] = argument;
                CHECK(semantic_param_publication_value(ctx, binary).origin_ready);
                CHECK(semantic_indexed_string_independent_result(binary, ctx));
                CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
            }
        }
        for (size_t side = 0; side < 2; side++) {
            ASTNode *child = children[side];
            ASTNode *same_id = ast_clone(child);
            CHECK(same_id != NULL);
            if (same_id == NULL) continue;
            same_id->stable_id = child->stable_id;
            if (side == 0) binary->data.binary.left = same_id;
            else binary->data.binary.right = same_id;
            CHECK(!semantic_indexed_string_independent_result(binary, ctx));
            CHECK(!semantic_indexed_string_borrow_source_current(binary, ctx, &source_id));
            if (side == 0) binary->data.binary.left = child;
            else binary->data.binary.right = child;
            ast_destroy(same_id);
            CHECK(semantic_indexed_string_independent_result(binary, ctx));
            CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
            if (side == 0) binary->data.binary.left = NULL;
            else binary->data.binary.right = NULL;
            CHECK(!semantic_indexed_string_independent_result(binary, ctx));
            CHECK(!semantic_indexed_string_borrow_source_current(binary, ctx, &source_id));
            if (side == 0) binary->data.binary.left = child;
            else binary->data.binary.right = child;
            CHECK(semantic_indexed_string_independent_result(binary, ctx));
            CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
            ASTNodeType shape = child->type;
            child->type = AST_NUMBER;
            CHECK(!semantic_indexed_string_independent_result(binary, ctx));
            CHECK(!semantic_indexed_string_borrow_source_current(binary, ctx, &source_id));
            child->type = shape;
            CHECK(semantic_indexed_string_independent_result(binary, ctx));
            CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_indexed_string_concat_nested_call_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Probe(text:String)->String{return Concat(\"\",text)+\"suffix\";}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *binary = ast_return_value(ast_block_statement(ast_func_body(decl), 0));
    ASTNode *nested = ast_binary_left(binary);
    ASTNode *target = ast_call_callee(nested);
    PublishedNode *row = published_node_for(ctx, nested);
    CHECK(binary != NULL && binary->type == AST_BINARY && nested != NULL &&
        nested->type == AST_CALL && target != NULL && target->type == AST_IDENTIFIER && row != NULL);
    if (binary == NULL || nested == NULL || target == NULL || row == NULL) goto cleanup;
    uint32_t source_id = UINT32_MAX;
    CHECK(semantic_indexed_string_independent_result(binary, ctx));
    CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
    ASTNode saved_call = *nested, saved_target = *target;
    PublishedNode saved_row = *row;
    for (unsigned fault = 0; fault < 7; fault++) {
        if (fault == 0) CHECK(ast_call_set_semantic_callee_is_stdlib(nested, false));
        if (fault == 1) CHECK(ast_call_set_semantic_callee_builtin_kind(nested, BUILTIN_STRING_LENGTH));
        if (fault == 2) CHECK(ast_call_set_semantic_callee_decl_id(nested, ast_node_stable_id(decl)));
        if (fault == 3) target->data.identifier.name = "Different";
        if (fault == 4) target->stable_id++;
        if (fault == 5) nested->stable_id++;
        if (fault == 6) row->value_type = TYPE_INT;
        CHECK(!semantic_indexed_string_independent_result(binary, ctx));
        CHECK(!semantic_indexed_string_borrow_source_current(binary, ctx, &source_id));
        *nested = saved_call; *target = saved_target; *row = saved_row;
        CHECK(semantic_indexed_string_independent_result(binary, ctx));
        CHECK(semantic_indexed_string_borrow_source_current(binary, ctx, &source_id) && source_id == 0);
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_conservative_callback_backing(void)
{
    for (unsigned borrowed = 0; borrowed < 2; borrowed++) {
        const char *source = borrowed
            ? "struct TextRecord{text:String;}func Apply(text:String,callback:func(String)->String)->String{return callback(text);}func Inspect(ref record:TextRecord,callback:func(String)->String)->String{return Apply(record.text,callback);}"
            : "func Apply(text:String,callback:func(String)->String)->String{return Concat(callback(text),Concat(text,\"tail\"));}func Store(inout output:String,text:String,callback:func(String)->String)->Void{output=Apply(text,callback);}func Invoke(inout output:String,text:String,callback:func(String)->String)->Void{Store(output,text,callback);}";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (borrowed) {
            CHECK(!typed && ctx->has_error);
            bool refused = false;
            for (size_t i = 0; i < ctx->diagnostic_count; i++) {
                const Diagnostic *diagnostic = ctx->diagnostics[i];
                if (diagnostic->level != DIAG_ERROR) continue;
                bool exact = diagnostic->code != NULL && diagnostic->cause_ir != NULL &&
                    strcmp(diagnostic->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0 &&
                    strcmp(diagnostic->cause_ir, PGY_CAUSE_BORROW_ESCAPE) == 0;
                CHECK(exact);
                refused |= exact;
            }
            CHECK(refused);
        } else {
            CHECK(typed && !ctx->has_error);
            if (typed) {
                ASTNode *apply = ast_program_statement(program, 0);
                CHECK(function_param_flow_summary_for_param(ctx, apply, 0) == SLOT_PARAM_SUMMARY_ALL);
                FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
                CHECK(store != NULL && !store->failed && !ctx->has_error);
                bool found = false;
                for (size_t i = 0; store != NULL && i < store->count; i++) {
                    FunctionParamFlowSummaryEntry *entry = &store->entries[i];
                    if (entry->function_decl != apply || entry->param_index != 0) continue;
                    found = true;
                    CHECK(entry->analysis_unsupported && entry->array_storage_unproved &&
                        entry->returned_backing_unproved && !entry->returned_backing_pending &&
                        !entry->output_dependencies_ready);
                }
                CHECK(found);
                FunctionParamFlowProgramPointIndex *points = NULL;
                CHECK(function_param_flow_program_point_index_build(store, apply, &points));
                CHECK(points != NULL && points->backing_frames != NULL && points->backing_frames[0] != NULL);
                if (points != NULL && points->backing_frames != NULL && points->backing_frames[0] != NULL) {
                    FunctionParamFlowBackingFact *frame = points->backing_frames[0];
                    CHECK(frame->analysis_unsupported && !frame->complete && !frame->pending);
                    CHECK(frame->unsupported_relation.call != NULL &&
                        frame->unsupported_relation.call_id == ast_node_stable_id(frame->unsupported_relation.call));
                    ASTNode *returned = ast_return_value(ast_block_statement(ast_func_body(apply), 0));
                    ASTNode *tail = ast_call_argument(returned, 1);
                    bool tail_seen = false;
                    for (size_t n = 0; n < frame->node_count; n++) tail_seen |= frame->nodes[n].node == tail;
                    CHECK(tail != NULL && !tail_seen);
                    size_t nodes = frame->node_count, work = store->work_units;
                    ReturnedBacking stopped = {.store = store, .lookup = {ctx, program}, .decl = apply, .fact = frame};
                    backing_consumers(&stopped, tail);
                    CHECK(frame->node_count == nodes && store->work_units == work);
                    CHECK(function_param_flow_backing_rows_current(store, apply));
                    FunctionParamFlowPendingRelation witness = frame->unsupported_relation;
                    ASTNode *argument = ast_call_argument(witness.call, 0);
                    ASTNode *child = ast_call_argument(tail, 1);
                    PublishedNode *child_value = NULL;
                    for (size_t n = 0; n < ctx->function_param_publications->node_capacity; n++)
                        if (ctx->function_param_publications->nodes[n].node == child)
                            child_value = &ctx->function_param_publications->nodes[n];
                    CHECK(child_value != NULL);
                    if (child_value == NULL) goto callback_cleanup;
                    PublishedNode saved_child = *child_value;
                    size_t entry_index = 0;
                    while (entry_index < store->count && (store->entries[entry_index].function_decl != apply ||
                        store->entries[entry_index].param_index != 0)) entry_index++;
                    CHECK(entry_index < store->count);
                    if (entry_index == store->count) goto callback_cleanup;
                    CHECK(function_param_flow_unproved_current(store, entry_index));
                    uint32_t argument_id = ast_node_stable_id(argument), child_id = ast_node_stable_id(child);
                    for (unsigned fault = 0; fault < 7; fault++) {
                        if (fault == 0) frame->unsupported_relation.call_id++;
                        if (fault == 1) frame->unsupported_relation.callee_id++;
                        if (fault == 2) frame->unsupported_relation.ordinal = 0;
                        if (fault == 3) argument->stable_id++;
                        if (fault == 4) child->stable_id++;
                        if (fault == 5) child_value->value_present = false;
                        if (fault == 6) child_value->value_type = TYPE_INT;
                        CHECK(!function_param_flow_backing_rows_current(store, apply));
                        CHECK(!function_param_flow_unproved_current(store, entry_index));
                        frame->unsupported_relation = witness;
                        argument->stable_id = argument_id; child->stable_id = child_id;
                        *child_value = saved_child;
                        CHECK(function_param_flow_backing_rows_current(store, apply));
                        CHECK(function_param_flow_unproved_current(store, entry_index));
                    }
                    for (unsigned mode = 0; mode < 3; mode++) {
                        unsigned effects = 0;
                        uint64_t outputs = 0;
                        FunctionParamFlowBackingFact partial = {.origin_count = 1, .origin_words = 1,
                            .destination_count = 1, .destination_words = 1, .effects = &effects,
                            .outputs = &outputs, .claims = frame->claims, .epoch = 1,
                            .complete = mode != 0, .pending = mode == 1, .building = mode == 2};
                        ReturnedBacking continuing = {.store = store, .lookup = {ctx, program},
                            .decl = apply, .fact = &partial};
                        size_t prior_work = store->work_units;
                        backing_consumers(&continuing, tail);
                        CHECK(partial.node_count != 0 && store->work_units > prior_work);
                        CHECK(!partial.analysis_unsupported);
                        for (size_t n = 0; n < partial.node_count; n++) free(partial.nodes[n].origins);
                        for (size_t b = 0; b < partial.binding_count; b++) {
                            free(partial.bindings[b].origins); free(partial.bindings[b].destinations);
                        }
                        free(partial.nodes); free(partial.bindings); free(partial.pending_relations);
                    }
                }
                ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 2)), 0);
                SlotFunctionLookup lookup = {ctx, program};
                bool dependent = true;
                CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
                CHECK(!function_param_flow_callee_relation_ready(&lookup, call, 1));
                CHECK(function_param_flow_summary_for_param(ctx, apply, 0) == SLOT_PARAM_SUMMARY_ALL);
                CHECK(store != NULL && !store->failed && !ctx->has_error);
            }
        }
callback_cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_recursive_alias_return_backing(void)
{
    Lexer *lexer = lexer_create(
        "func First(ref text:String,depth:Int)->String{if depth<=0{let first=text;let second=first;return second;}return Second(text,depth-1);}"
        "func Second(ref text:String,depth:Int)->String{if depth<=0{let first=text;let second=first;return second;}return First(text,depth-1);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decls[2] = {ast_program_statement(program, 0), ast_program_statement(program, 1)};
    unsigned masks[2];
    bool observed_pending_evidence = false;
    for (size_t side = 0; side < 2; side++) {
        masks[side] = function_param_flow_summary_for_param(ctx, decls[side], 0);
        CHECK((masks[side] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0);
        CHECK((masks[side] & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    }
    for (size_t side = 0; side < 2; side++) {
        ASTNode *call = ast_return_value(ast_block_statement(ast_func_body(decls[side]), 1));
        ASTNode *argument = ast_call_argument(call, 0);
        uint32_t callee = ast_call_semantic_callee_decl_id(call);
        uint32_t argument_id = ast_node_stable_id(argument);
        uint32_t binding = ast_identifier_binding_syntax_id(argument);
        size_t arity = call->data.call.arg_count;
        for (unsigned fault = 0; fault < 5; fault++) {
            if (fault == 0) ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(decls[side]));
            if (fault == 1) ast_call_set_semantic_callee_value_binding_id(call, 1);
            if (fault == 2) call->data.call.arg_count = 0;
            if (fault == 3) argument->stable_id++;
            if (fault == 4) ast_identifier_set_binding_syntax_id(argument, 0);
            CHECK(function_param_flow_summary_for_param(ctx, decls[0], 0) == SLOT_PARAM_SUMMARY_ALL);
            CHECK(function_param_flow_summary_for_param(ctx, decls[1], 0) == SLOT_PARAM_SUMMARY_ALL);
            ast_call_set_semantic_callee_decl_id(call, callee);
            ast_call_set_semantic_callee_value_binding_id(call, 0);
            call->data.call.arg_count = arity;
            argument->stable_id = argument_id;
            ast_identifier_set_binding_syntax_id(argument, binding);
            CHECK(function_param_flow_summary_for_param(ctx, decls[0], 0) == masks[0]);
            CHECK(function_param_flow_summary_for_param(ctx, decls[1], 0) == masks[1]);
        }
        FunctionParamFlowProgramPointIndex *index = NULL;
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, decls[side], &index));
        CHECK(index != NULL && index->backing_frames != NULL && index->backing_frames[0] != NULL);
        if (index == NULL || index->backing_frames == NULL || index->backing_frames[0] == NULL) continue;
        FunctionParamFlowBackingFact *frame = index->backing_frames[0];
        bool complete = frame->complete, building = frame->building;
        for (unsigned fault = 0; fault < 3; fault++) {
            if (fault == 0) index->backing_frames[0] = NULL;
            if (fault == 1) frame->complete = false;
            if (fault == 2) frame->building = true;
            CHECK(function_param_flow_summary_for_param(ctx, decls[0], 0) == SLOT_PARAM_SUMMARY_ALL);
            CHECK(function_param_flow_summary_for_param(ctx, decls[1], 0) == SLOT_PARAM_SUMMARY_ALL);
            index->backing_frames[0] = frame; frame->complete = complete; frame->building = building;
            CHECK(function_param_flow_summary_for_param(ctx, decls[0], 0) == masks[0]);
            CHECK(function_param_flow_summary_for_param(ctx, decls[1], 0) == masks[1]);
        }
        CHECK(frame->pending_relation_count == frame->sealed_pending_relation_count);
        if (frame->pending_relation_count != 0) {
            observed_pending_evidence = true;
            CHECK(frame->pending_relations != NULL);
            if (frame->pending_relations == NULL) continue;
            FunctionParamFlowPendingRelation *rows = frame->pending_relations;
            FunctionParamFlowPendingRelation saved = rows[0];
            size_t count = frame->pending_relation_count, sealed = frame->sealed_pending_relation_count;
            for (unsigned fault = 0; fault < 10; fault++) {
                if (fault == 0) frame->pending_relations = NULL;
                if (fault == 1) frame->pending_relation_count++;
                if (fault == 2) frame->sealed_pending_relation_count++;
                if (fault == 3) rows[0].call_id++;
                if (fault == 4) rows[0].callee_id = ast_node_stable_id(decls[side]);
                if (fault == 5) rows[0].ordinal = ast_func_param_count(decls[1 - side]);
                if (fault == 6) rows[0].call = ast_return_value(ast_block_statement(ast_func_body(decls[1 - side]), 1));
                if (fault == 7) frame->pending_relation_count = 0;
                if (fault == 8) rows[0].callee_id = UINT32_MAX;
                if (fault == 9) rows[0].call_id = 0;
                CHECK(function_param_flow_summary_for_param(ctx, decls[0], 0) == SLOT_PARAM_SUMMARY_ALL);
                CHECK(function_param_flow_summary_for_param(ctx, decls[1], 0) == SLOT_PARAM_SUMMARY_ALL);
                frame->pending_relations = rows; frame->pending_relation_count = count;
                frame->sealed_pending_relation_count = sealed; rows[0] = saved;
                CHECK(function_param_flow_summary_for_param(ctx, decls[0], 0) == masks[0]);
                CHECK(function_param_flow_summary_for_param(ctx, decls[1], 0) == masks[1]);
            }
        }
    }
    CHECK(observed_pending_evidence);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    reached_producer_source(
        "extern \"C\"{func Opaque(text:String)->String;}func First(ref text:String,depth:Int)->String{if depth<=0{return text;}return Second(text,depth-1);}func Second(ref text:String,depth:Int)->String{if depth<=0{return Opaque(text);}return First(text,depth-1);}", false);
}

static void checked_call_result_field_destination(void)
{
    const char *sources[] = {
        "struct TextRecord{text:String;}struct Held{value:String;}func ReadRecord(ref record:TextRecord)->Option<Int>{let key:String=UnwrapOption(Some(Held(record.text))).value;return Some(StringLength(key));}",
        "struct TextRecord{text:String;}struct Held{value:String;}func StoreRecord(record:TextRecord,inout output:String)->Option<Int>{let key:String=UnwrapOption(Some(Held(record.text))).value;output=key;return Some(0);}func Forward(record:TextRecord,inout output:String)->Option<Int>{return StoreRecord(record,output);}"
    };
    for (size_t kind = 0; kind < 2; kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *decl = ast_program_statement(program, 2);
        bool required = false;
        CHECK(semantic_param_publication_return_backing_required(ctx, decl, 0, &required) && required);
        unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
        CHECK((mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        CHECK(((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) == (kind == 1));
        ASTNode *projection = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
        ASTNode *unwrap = ast_member_object(projection);
        ASTNode *some = ast_call_argument(unwrap, 0);
        ASTNode *argument = ast_call_argument(some, 0);
        CHECK(projection->type == AST_MEMBER_ACCESS && unwrap->type == AST_CALL);
        CHECK(semantic_param_publication_binding(ctx, unwrap) == 0);
        bool depends = false, retains = true;
        CHECK(semantic_param_publication_checked_result_relation(ctx, unwrap, 0, &depends, &retains) && depends && !retains);
        ASTNode *call = kind == 1 ? ast_return_value(ast_block_statement(ast_func_body(ast_program_statement(program, 3)), 0)) : NULL;
        SlotFunctionLookup lookup = {ctx, program};
        bool output = false;
        if (kind == 1) CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &output) && output);
        uint32_t id = unwrap->stable_id;
        for (unsigned fault = 0; fault < 3; fault++) {
            if (fault == 0) unwrap->stable_id++;
            if (fault == 1) projection->data.member.object = some;
            if (fault == 2) some->data.call.arguments[0] = NULL;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
            if (kind == 1) CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &output));
            unwrap->stable_id = id; projection->data.member.object = unwrap;
            some->data.call.arguments[0] = argument;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
            if (kind == 1) CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &output) && output);
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_default_formal_replacement(void)
{
    const char *sources[] = {
        "func Probe(text:String,inout output:String,flag:Bool)->Void{if flag{text=\"literal\";}output=text;}func Forward(text:String,inout output:String,flag:Bool)->Void{Probe(text,output,flag);}",
        "func Probe(value:Int)->Option<Int>{value=value+1;return Some(value);}",
        "func Probe(rows:Array<Int>)->Option<Int>{rows=[];return Some(ArrayLength(rows));}",
        "func Probe(ref value:Int)->Void{value=1;}",
        "func Probe(inout value:Int)->Void{value=1;}",
        "struct Counter{count:Int;}func Probe(cell:Counter)->Void{cell.count=1;}",
        "func Probe(inout output:Int)->Void{using output as alias_value;alias_value=1;}",
        "func Probe()->Void{let slot:Slot<Int> = ClaimSlot();slot=1;Release(slot);}"
    };
    for (size_t kind = 0; kind < sizeof(sources) / sizeof(sources[0]); kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *decl = ast_program_statement(program, kind == 5 ? 1 : 0);
        ASTNode *assignment = ast_block_statement(ast_func_body(decl), kind == 6 || kind == 7 ? 1 : 0);
        if (kind == 0) assignment = ast_block_statement(ast_if_then_branch(assignment), 0);
        if (ast_func_param_count(decl) != 0) (void)function_param_flow_summary_for_param(ctx, decl, 0);
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&(SlotFunctionLookup){ctx, program});
        FunctionParamFlowProgramPointIndex *index = NULL;
        CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
        if (index == NULL) goto cleanup;
        CHECK(function_param_flow_local_replacement(ctx, index, assignment) == (kind < 3));
        if (kind < 3) CHECK(semantic_param_publication_assignment_replacement(ctx, assignment));
        if (kind == 0) {
            unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
            CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
            ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 1)), 0);
            SlotFunctionLookup lookup = {ctx, program};
            bool dependent = false;
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &dependent) && dependent);
            FuncParam *formal = ast_func_param(decl, 0);
            uint32_t id = formal->stable_id, owner_id = index->function_id;
            ParamMode mode = formal->mode;
            for (unsigned fault = 0; fault < 3; fault++) {
                if (fault == 0) formal->stable_id++;
                if (fault == 1) formal->mode = PARAM_MODE_REF;
                if (fault == 2) index->function_id++;
                CHECK(!function_param_flow_local_replacement(ctx, index, assignment));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
                CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &dependent));
                formal->stable_id = id; formal->mode = mode; index->function_id = owner_id;
                CHECK(function_param_flow_local_replacement(ctx, index, assignment));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
                CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 0, 1, &dependent) && dependent);
            }
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_local_alias_return_backing(void)
{
    const char *sources[] = {
        "func Alias(ref text:String)->String{let first=text;let second=first;return second;}",
        "func Alias(ref text:String)->String{var alias_text:String=\"literal\";if StringLength(text)>0{alias_text=text;}return alias_text;}",
        "func Alias(ref text:String)->String{let alias_text=\"literal\";return alias_text;}",
        "func Alias(ref text:String)->Int{let alias_text=text;return StringLength(alias_text);}",
        "func Alias(value:Int)->String{let alias_text=ToString(value);return alias_text;}",
        "func Alias(ref text:String){let first=text;let second=first;return second;}"
    };
    for (size_t kind = 0; kind < sizeof(sources) / sizeof(sources[0]); kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (!typed) fprintf(stderr, "alias-return baseline kind=%zu parsed=%d source=%s\n", kind, parsed, sources[kind]);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *decl = ast_program_statement(program, 0);
        bool required = false;
        CHECK(semantic_param_publication_return_backing_required(ctx, decl, 0, &required));
        CHECK(required == (kind != 3 && kind != 4));
        unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
        if (((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) != (kind < 2 || kind == 5) ||
            (mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) != 0)
            fprintf(stderr, "alias-return mask kind=%zu mask=%u required=%d source=%s\n", kind, mask, required, sources[kind]);
        CHECK(((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) == (kind < 2 || kind == 5));
        CHECK((mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
        if (kind == 0) {
            FunctionParamPublicationStore *store = ctx->function_param_publications;
            PendingRoutine *routine = NULL;
            for (size_t i = 0; i < store->routine_count; i++)
                if (store->routines[i].decl == decl) routine = &store->routines[i];
            CHECK(routine != NULL);
            if (routine == NULL) goto cleanup;
            PendingRoutine saved_routine = *routine;
            PublishedFormal saved_formal = routine->formals[0];
            Type *saved_input = routine->types[0];
            FuncParam *formal = ast_func_param(decl, 0);
            ASTNode *type_node = formal->type;
            uint32_t formal_id = formal->stable_id;
            ASTNode *result = ast_func_return_type(decl);
            uint32_t result_id = result->stable_id;
            for (unsigned fault = 0; fault < 8; fault++) {
                if (fault == 0) routine->return_type = TYPE_INT;
                if (fault == 1) decl->data.func_decl.return_type = NULL;
                if (fault == 2) result->stable_id++;
                if (fault == 3) routine->async = !routine->async;
                /* Mutate this publication's kind snapshot, never shared
                 * TYPE_STRING/TYPE_INT singleton storage. */
                if (fault == 4) routine->formals[0].kind = TYPE_KIND_FUNCTION;
                if (fault == 5) routine->types[0] = TYPE_INT;
                if (fault == 6) formal->type = NULL;
                if (fault == 7) formal->stable_id++;
                CHECK(!semantic_param_publication_return_backing_required(ctx, decl, 0, &required));
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
                *routine = saved_routine;
                routine->formals[0] = saved_formal;
                routine->types[0] = saved_input;
                formal->type = type_node; formal->stable_id = formal_id;
                decl->data.func_decl.return_type = result; result->stable_id = result_id;
                CHECK(semantic_param_publication_return_backing_required(ctx, decl, 0, &required) && required);
                CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
            }
        }
        if (kind == 4) goto cleanup;
        ASTNode *local = ast_block_statement(ast_func_body(decl), 0);
        ASTNode *initial = ast_let_initializer(local);
        PublishedNode *published = NULL;
        FunctionParamPublicationStore *publications = ctx->function_param_publications;
        for (size_t i = 0; i < publications->node_capacity; i++)
            if (publications->nodes[i].node == initial) published = &publications->nodes[i];
        CHECK(initial != NULL && published != NULL && published->value_present);
        if (initial == NULL || published == NULL) goto cleanup;
        ASTNode saved_node = *initial;
        PublishedNode saved = *published;
        /* Replay the original checked alias input, not merely the final local
         * spelling: missing/changed origin evidence must not become mask0. */
        for (unsigned fault = 0; fault < 6; fault++) {
            if (fault == 0) initial->stable_id++;
            if (fault == 1) published->value_present = false;
            if (fault == 2) published->meta = !saved.meta;
            if (fault == 3) initial->type = AST_BOOLEAN;
            if (fault == 4) CHECK(ast_let_attach_initializer(local, NULL));
            if (fault == 5) {
                if (initial->type == AST_IDENTIFIER) ast_identifier_set_binding_syntax_id(initial, 0);
                else initial->data.string.value = "different";
            }
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == SLOT_PARAM_SUMMARY_ALL);
            CHECK(ast_let_attach_initializer(local, initial));
            *initial = saved_node;
            *published = saved;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == mask);
            CHECK(((function_param_flow_summary_for_param(ctx, decl, 0) & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) == (kind < 2 || kind == 5));
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_identifier_consumer_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Echo(ref text:String)->String{return text;}"
        "func Probe(inout output:String,text:String,flag:Bool,other:Bool,ref slot:Slot<String>)->Bool{let empty:Option<Int> = None;if flag{output=text;Write(slot,Echo(\"literal\"));}return true;}"
        "func Main()->Void{let slot:Slot<String> = ClaimSlot();var output=\"\";Probe(output,\"static\",true,false,slot);Release(slot);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) {
        for (size_t d = 0; d < ctx->diagnostic_count; d++)
            fprintf(stderr, "identifier consumer baseline: %s\n", ctx->diagnostics[d]->message);
        goto cleanup;
    }
    ASTNode *decl = ast_program_statement(program, 1);
    ASTNode *empty = ast_let_initializer(ast_block_statement(ast_func_body(decl), 0));
    ASTNode *branch = ast_block_statement(ast_func_body(decl), 1);
    ASTNode *identifier = ast_if_condition(branch);
    ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 2)), 2);
    SlotFunctionLookup lookup = {ctx, program};
    unsigned mask = function_param_flow_summary_for_param(ctx, decl, 1);
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    bool dependent = false;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
    SemanticParamPublicationView view;
    CHECK(semantic_param_publication_admitted_view(ctx, &view));
    PendingRefCall *loan = NULL;
    FunctionParamPublicationStore *publications = ctx->function_param_publications;
    for (size_t i = 0; i < publications->call_count; i++)
        if (publications->calls[i].caller == decl &&
            publications->calls[i].callee == ast_program_statement(program, 0)) loan = &publications->calls[i];
    PublishedNode *published = NULL;
    for (size_t i = 0; i < publications->node_capacity; i++)
        if (publications->nodes[i].node == identifier) published = &publications->nodes[i];
    CHECK(loan != NULL && published != NULL && published->value_present);
    CHECK(identifier != NULL && identifier->type == AST_IDENTIFIER &&
        ast_identifier_binding_syntax_id(identifier) == ast_func_param_stable_id(ast_func_param(decl, 2)));
    if (loan == NULL || published == NULL || identifier == NULL) goto cleanup;
    ASTNode *escape = NULL;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
    ASTNode original = *identifier;
    PublishedNode saved = *published;
    uint32_t other = ast_func_param_stable_id(ast_func_param(decl, 3));
    CHECK(other != 0 && other != ast_identifier_binding_syntax_id(identifier));
    /* The condition consumes a Bool identifier but transfers no origin. Its
     * shared checked identity must survive the consumer-only leaf shortcut.
     * Output=text separately forces ordinary backing and its read/growth path. */
    for (unsigned fault = 0; fault < 9; fault++) {
        if (fault == 0) identifier->stable_id++;
        if (fault == 1) identifier->data.identifier.name = "other";
        if (fault == 2) identifier->type = AST_BOOLEAN;
        if (fault == 3) ast_identifier_set_binding_syntax_id(identifier, 0);
        if (fault == 4) ast_identifier_set_binding_syntax_id(identifier, other);
        if (fault == 5) published->value_present = false;
        if (fault == 6) published->value_shape = AST_NUMBER;
        if (fault == 7) published->binding_id = other;
        /* This is a checked Bool-result call with actual inout retention,
         * not a replacement scalar leaf with a guessed zero-origin grant. */
        if (fault == 8) branch->data.if_stmt.condition = call;
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
        CHECK(!function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
        branch->data.if_stmt.condition = identifier;
        *identifier = original;
        *published = saved;
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == mask);
        CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        CHECK(function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
    }
    PublishedNode *empty_publication = NULL;
    for (size_t i = 0; i < publications->node_capacity; i++)
        if (publications->nodes[i].node == empty) empty_publication = &publications->nodes[i];
    SemanticPublishedValue meta = semantic_param_publication_value(ctx, empty);
    CHECK(empty != NULL && empty->type == AST_IDENTIFIER && ast_node_stable_id(empty) != 0);
    CHECK(meta.ok && meta.meta && semantic_param_publication_binding(ctx, empty) == 0);
    CHECK(empty_publication != NULL && empty_publication->meta && empty_publication->binding_id == 0);
    CHECK(!saved.meta && saved.binding_id != 0);
    if (empty == NULL || empty_publication == NULL) goto cleanup;
    /* A current typed meta identifier is not an unresolved variable. Replay
     * the meta distinction itself, not just its coincidentally zero binding. */
    for (unsigned target = 0; target < 2; target++) {
        ASTNode *node = target == 0 ? empty : identifier;
        PublishedNode *publication = target == 0 ? empty_publication : published;
        ASTNode saved_node = *node;
        PublishedNode saved_publication = *publication;
        for (unsigned fault = 0; fault < 10; fault++) {
            if (fault == 0) publication->meta = !saved_publication.meta;
            if (fault == 1) publication->value_present = false;
            if (fault == 2) publication->value_shape = AST_NUMBER;
            if (fault == 3) node->stable_id++;
            if (fault == 4) node->data.identifier.name = "other";
            if (fault == 5) node->type = AST_BOOLEAN;
            if (fault == 6) ast_identifier_set_binding_syntax_id(node, target == 0 ? other : 0);
            if (fault == 7) publication->binding_id = other;
            if (fault == 8) {
                uint32_t forged = target == 0 ? other : 0;
                ast_identifier_set_binding_syntax_id(node, forged);
                publication->binding_id = forged;
                publication->meta = true;
            }
            if (fault == 9) publication->id++;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
            CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
            CHECK(!function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
            *node = saved_node;
            *publication = saved_publication;
            CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == mask);
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
            CHECK(function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
        }
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_pure_literal_backing_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Choose(inout output:String,text:String)->Void{let s=\"literal\";let n=9007199254740993L;let b=true;output=text;}"
        "func Echo(ref text:String)->String{return text;}"
        "func Save(ref output:Slot<String>)->Void{let s=\"literal\";let n=9007199254740993L;let b=true;Write(output,Echo(\"literal\"));}"
        "func Main()->Void{var output=\"\";Choose(output,\"static\");}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *choose = ast_program_statement(program, 0);
    ASTNode *save = ast_program_statement(program, 2);
    ASTNode *call = ast_block_statement(ast_func_body(ast_program_statement(program, 3)), 1);
    ASTNode *retaining = ast_block_statement(ast_func_body(save), 3);
    SlotFunctionLookup lookup = {ctx, program};
    SemanticParamPublicationView view;
    CHECK(semantic_param_publication_admitted_view(ctx, &view));
    PendingRefCall *loan = NULL;
    for (size_t i = 0; i < ctx->function_param_publications->call_count; i++)
        if (ctx->function_param_publications->calls[i].caller == save)
            loan = &ctx->function_param_publications->calls[i];
    CHECK(loan != NULL);
    if (loan == NULL) goto cleanup;
    unsigned mask = function_param_flow_summary_for_param(ctx, choose, 1);
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    bool dependent = false;
    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
    ASTNode *escape = NULL;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
    for (unsigned consumer = 0; consumer < 2; consumer++) {
        ASTNode *decl = consumer == 0 ? choose : save;
        for (unsigned kind = 0; kind < 3; kind++) {
            ASTNode *binding = ast_block_statement(ast_func_body(decl), kind);
            ASTNode *literal = ast_let_initializer(binding);
            PublishedNode *published = NULL;
            FunctionParamPublicationStore *publications = ctx->function_param_publications;
            for (size_t i = 0; i < publications->node_capacity; i++)
                if (publications->nodes[i].node == literal) published = &publications->nodes[i];
            CHECK(published != NULL && published->value_present);
            if (published == NULL) continue;
            ASTNode original = *literal;
            PublishedNode saved = *published;
            /* Payload, exact integer identity/flags, ID, shape and typed proof
             * remain currentness obligations even without a backing node row. */
            for (unsigned fault = 0; fault < (kind == 1 ? 9u : 5u); fault++) {
                if (fault == 0) {
                    if (kind == 0) literal->data.string.value = "different";
                    if (kind == 1) literal->data.number.value += 8;
                    if (kind == 2) literal->data.boolean.value = false;
                }
                if (fault == 1) literal->stable_id++;
                if (fault == 2) literal->type = AST_IDENTIFIER;
                if (fault == 3) published->value_present = false;
                if (fault == 4) CHECK(ast_let_attach_initializer(binding, retaining));
                if (fault == 5) literal->data.number.exact_long_value++;
                if (fault == 6) literal->data.number.is_long = !literal->data.number.is_long;
                if (fault == 7) literal->data.number.is_float = !literal->data.number.is_float;
                if (fault == 8) literal->data.number.is_duration = !literal->data.number.is_duration;
                if (consumer == 0) {
                    CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
                    CHECK(function_param_flow_summary_for_param(ctx, choose, 1) == SLOT_PARAM_SUMMARY_ALL);
                } else CHECK(!function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
                CHECK(ast_let_attach_initializer(binding, literal));
                *literal = original;
                *published = saved;
                if (consumer == 0) {
                    CHECK(function_param_flow_summary_for_param(ctx, choose, 1) == mask);
                    CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
                } else CHECK(function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
            }
        }
    }
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, choose, &index));
    if (index != NULL && index->backing_frames != NULL && index->backing_frames[1] != NULL) {
        FunctionParamFlowBackingFact *frame = index->backing_frames[1];
        for (unsigned axis = 0; axis < 2; axis++) {
            size_t *width = axis == 0 ? &frame->origin_count : &frame->origin_words;
            size_t saved = *width;
            *width = 2;
            CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
            *width = saved;
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        }
    } else CHECK(false);
    /* A literal cannot impersonate the registered CALL loan seed. */
    PendingRefCall saved_loan = *loan;
    loan->call = ast_let_initializer(ast_block_statement(ast_func_body(save), 0));
    loan->call_id = ast_node_stable_id(loan->call);
    CHECK(!semantic_param_publication_ref_call_current(ctx, loan));
    CHECK(!function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
    *loan = saved_loan;
    CHECK(function_param_flow_return_loan_admitted(ctx, &view, loan, &escape));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_backing_binding_growth_case(const char *source, bool expected_output)
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
    CHECK(!pending && outputs[0] == expected_output);
    CHECK(((effects & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) == expected_output);
    CHECK(((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) == expected_output);
    if (!expected_output) CHECK(effects == SLOT_PARAM_SUMMARY_NONE);
    CHECK(index->backing_frames != NULL && index->backing_frame_count == count &&
        index->backing_frames[1] != NULL && index->backing_frames[1]->epoch != 0);
    free(outputs);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_defer_return_backing(void)
{
    for (unsigned retaining = 0; retaining < 3; retaining++) {
        const char *source = retaining == 2
            ? "extern \"C\"{func Keep(text:String)->Void;}func Probe(ref text:String)->String{defer{Keep(text);};return \"independent\";}struct TextRecord{text:String;}func Inspect(ref record:TextRecord)->String{return Probe(record.text);}"
            : retaining == 1
                ? "extern \"C\"{func Keep(text:String)->Void;}func Probe(ref text:String)->String{defer{Keep(text);};return \"independent\";}"
                : "func Probe(text:String)->String{let result_text=\"independent\";defer{Log(\"cleanup\");};return result_text;}";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (retaining == 2) {
            CHECK(!typed && ctx->has_error);
            bool refused = false;
            for (size_t i = 0; i < ctx->diagnostic_count; i++) {
                const Diagnostic *diagnostic = ctx->diagnostics[i];
                if (diagnostic->level != DIAG_ERROR) continue;
                bool exact = diagnostic->code != NULL && diagnostic->cause_ir != NULL &&
                    strcmp(diagnostic->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0 &&
                    strcmp(diagnostic->cause_ir, PGY_CAUSE_BORROW_ESCAPE) == 0;
                CHECK(exact);
                refused |= exact;
            }
            CHECK(refused);
        } else {
            CHECK(typed && !ctx->has_error);
            if (typed) {
                ASTNode *decl = ast_program_statement(program, retaining == 1 ? 1 : 0);
                unsigned mask = function_param_flow_summary_for_param(ctx, decl, 0);
                if (retaining == 1) {
                    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
                    SlotFunctionLookup lookup = {ctx, program};
                    SlotSummaryOrigin origin = {.function_decl = decl, .param_index = 0, .param_name = "text"};
                    bool failed = false;
                    unsigned may = slot_escape_mask_in_program(ast_block_statement(ast_func_body(decl), 0),
                        "text", &lookup, &origin, &failed);
                    CHECK(!failed && (may & SLOT_ESCAPE_CALL) != 0);
                } else CHECK((mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                    SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
            }
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    /* Cleanup reads the binding after its later assignment. The first MAY
     * visit must not cache the initial empty value as the final provenance. */
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String)->Void{var staged:String=\"\";defer{output=staged;};staged=text;}", true);
}

static void checked_backing_binding_growth(void)
{
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String)->Bool{output=text;return true;}", true);
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String)->Bool{let first=text;let second=first;output=second;return true;}", true);
    /* A later assignment grows a binding already read earlier in the loop. */
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{var first:String=\"\";var second:String=\"\";while flag{first=second;second=text;}output=first;return true;}", true);
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{using output as first;while flag{using first as second;second=text;}return true;}", true);
    /* Ordinary assignments copy current values, not future variable aliases:
     * rebinding first cannot write output. The using aliases above do write it. */
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String)->Bool{var first:String=\"\";var second:String=\"\";first=second;second=output;first=text;return true;}", false);
    /* A memoized read in the first branch precedes growth in the second. */
    checked_backing_binding_growth_case(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{var first:String=\"\";if flag{output=first;}else{first=text;}return true;}", true);
}

static void checked_c_empty_preparation(void)
{
    Lexer *lexer = lexer_create(
        "func Probe(inout output:String,text:String,flag:Bool)->Void{"
        "if !flag||flag{output=text;}}");
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
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (index == NULL || index->backing_frames == NULL || index->backing_frames[1] == NULL) {
        CHECK(false); goto cleanup;
    }
    FunctionParamFlowBackingFact *frame = index->backing_frames[1];
    CHECK(frame->complete && !frame->pending && frame->outputs != NULL &&
        (frame->outputs[0] & UINT64_C(1)) != 0);
    size_t binary_rows = 0, unary_rows = 0;
    for (size_t n = 0; n < frame->node_count; n++) {
        binary_rows += frame->nodes[n].shape == AST_BINARY;
        unary_rows += frame->nodes[n].shape == AST_UNARY;
    }
    if (binary_rows != 0 || unary_rows != 0)
        fprintf(stderr, "C-empty retained binary=%zu unary=%zu per-frame rows\n", binary_rows, unary_rows);
    CHECK(binary_rows == 0 && unary_rows == 0);
    CHECK(function_param_flow_backing_rows_current(store, decl));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_selector_c_empty_preparation(void)
{
    Lexer *lexer = lexer_create(
        "struct Flag{value:Bool}"
        "func Probe(inout output:String,text:String,flag:Flag,flags:Array<Bool>)->Void{"
        "if flag.value||flags[0]{output=text;}}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 1);
    unsigned mask = function_param_flow_summary_for_param(ctx, decl, 1);
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (index == NULL || index->backing_frames == NULL || index->backing_frames[1] == NULL) {
        CHECK(false); goto cleanup;
    }
    FunctionParamFlowBackingFact *frame = index->backing_frames[1];
    CHECK(frame->complete && !frame->pending && frame->outputs != NULL
        && (frame->outputs[0] & UINT64_C(1)) != 0);
    size_t member_rows = 0, index_rows = 0;
    for (size_t n = 0; n < frame->node_count; n++) {
        member_rows += frame->nodes[n].shape == AST_MEMBER_ACCESS;
        index_rows += frame->nodes[n].shape == AST_ARRAY_ACCESS;
    }
    if (member_rows != 0 || index_rows != 0)
        fprintf(stderr, "Selector C-empty retained member=%zu index=%zu per-frame rows\n",
            member_rows, index_rows);
    CHECK(member_rows == 0 && index_rows == 0);
    CHECK(function_param_flow_backing_rows_current(store, decl));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_selector_c_unreachable(void)
{
    Lexer *lexer = lexer_create(
        "struct Flag{value:Bool}"
        "func Probe(inout output:String,text:String,flag:Flag,flags:Array<Bool>)->Void{"
        "output=text;return;flag.value;flags[0];}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 1), *body = ast_func_body(decl);
    ASTNode *dead[] = {ast_block_statement(body, 2), ast_block_statement(body, 3)};
    for (size_t i = 0; i < 2; i++) {
        bool reachable = true;
        CHECK(semantic_param_publication_execution(ctx, dead[i], &reachable) && !reachable);
    }
    CHECK(function_param_flow_summary_for_param(ctx, decl, 2) == 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(store, decl, &index));
    FunctionParamFlowBackingFact *frame = index != NULL && index->backing_frames != NULL
        ? index->backing_frames[2] : NULL;
    CHECK(frame != NULL && frame->complete && !frame->pending);
    if (frame != NULL) {
        CHECK(frame->effects != NULL && frame->effects[0] == 0
            && frame->outputs != NULL && frame->outputs[0] == 0);
        for (size_t i = 0; i < 2; i++) {
            bool seen = false;
            for (size_t n = 0; n < frame->node_count; n++) seen |= frame->nodes[n].node == dead[i];
            CHECK(seen);
        }
    }
    CHECK(function_param_flow_backing_rows_current(store, decl));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_selector_c_empty_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct Flag{value:Bool;other:Bool}enum Purpose{On,Off}"
        "extern \"C\"{func Observe(text:String)->Flag;func Choose(text:String)->Int;}"
        "func Probe(inout output:String,text:String,flag:Flag,flags:Array<Bool>,purpose:Purpose)->Void{"
        "if flag.value{output=text;}if flags[0+0]{output=text;}"
        "if Observe(text).value||flags[Choose(text)]{output=text;}"
        "if purpose==Purpose.On{output=text;}}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 3), *body = ast_func_body(decl);
    ASTNode *effectful = ast_if_condition(ast_block_statement(body, 2));
    ASTNode *targets[] = {ast_if_condition(ast_block_statement(body, 0)),
        ast_if_condition(ast_block_statement(body, 1)), ast_binary_left(effectful),
        ast_binary_right(effectful), ast_binary_right(ast_if_condition(ast_block_statement(body, 3)))};
    unsigned baseline = function_param_flow_summary_for_param(ctx, decl, 1);
    CHECK((baseline & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0 && baseline != SLOT_PARAM_SUMMARY_ALL);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(store, decl, &index));
    if (index == NULL || index->claims == NULL) goto cleanup;
    size_t ordinals[5] = {SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX};
    for (size_t c = 0; c < index->claims->count; c++)
        for (size_t t = 0; t < 5; t++) if (index->claims->rows[c].node == targets[t]) ordinals[t] = c;
    for (size_t t = 0; t < 5; t++) {
        bool empty = false;
        CHECK(ordinals[t] != SIZE_MAX);
        if (ordinals[t] == SIZE_MAX) goto cleanup;
        CHECK(function_param_flow_consumer_empty_current(ctx, index->claims, targets[t], &empty));
        CHECK(empty == (t < 2 || t == 4));
    }
    FunctionParamFlowBackingFact *frame = index->backing_frames != NULL ? index->backing_frames[1] : NULL;
    CHECK(frame != NULL && frame->complete && !frame->pending);
    if (frame == NULL) goto cleanup;
    for (size_t t = 0; t < 4; t++) {
        bool seen = false;
        for (size_t n = 0; n < frame->node_count; n++) seen |= frame->nodes[n].node == targets[t];
        CHECK(seen == (t >= 2));
    }
    for (size_t t = 0; t < 2; t++) {
        FunctionParamFlowIndexClaim *row = &index->claims->rows[ordinals[t]];
        FunctionParamFlowIndexClaim saved_row = *row;
        ASTNode *node = targets[t], saved_node = *node;
        ASTNode *child = row->children[0], saved_child = *child;
        ASTNode foreign_child = *child;
        size_t child_ordinal = row->child_claim_ordinals[0];
        uint32_t child_id = row->child_ids[0];
        uint32_t child_binding = ast_identifier_binding_syntax_id(child);
        PublishedNode *value = published_node_for(ctx, child);
        CHECK(value != NULL);
        if (value == NULL) goto cleanup;
        PublishedNode saved_value = *value;
        for (unsigned fault = 0; fault < (t == 0 ? 14u : 15u); fault++) {
            if (fault == 0) row->consumer_empty = false;
            if (fault == 1) row->child_claim_ordinals = NULL;
            if (fault == 2) row->child_claim_ordinals[0] = SIZE_MAX;
            if (fault == 3) row->children[0] = &foreign_child;
            if (fault == 4) row->id++;
            if (fault == 5) node->stable_id++;
            if (fault == 6) child->stable_id++;
            if (fault == 7) value->value_present = false;
            if (fault == 8) value->value_type = TYPE_INT;
            if (fault == 9) ast_identifier_set_binding_syntax_id(child, 0);
            if (fault == 10) {
                if (t == 0) node->data.member.object = NULL;
                else node->data.array_access.array = NULL;
            }
            if (fault == 11) row->child_count--;
            if (fault == 12) row->child_ids[0]++;
            if (fault == 13) row->semantic_promoted = false;
            if (fault == 14) {
                ASTNode *receiver = node->data.array_access.array;
                node->data.array_access.array = node->data.array_access.index;
                node->data.array_access.index = receiver;
            }
            bool empty = true;
            CHECK(!function_param_flow_consumer_empty_current(ctx, index->claims, node, &empty));
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
            *node = saved_node; *child = saved_child; *value = saved_value; *row = saved_row;
            ast_identifier_set_binding_syntax_id(child, child_binding);
            row->children[0] = child; row->child_claim_ordinals[0] = child_ordinal; row->child_ids[0] = child_id;
            CHECK(function_param_flow_consumer_empty_current(ctx, index->claims, node, &empty) && empty);
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
        }
    }
    /* A same-typed field change must not borrow the old selector receipt. */
    FunctionParamFlowIndexClaim *member_row = &index->claims->rows[ordinals[0]];
    char *member_name = targets[0]->data.member.name;
    char *claim_name = member_row->syntax_name;
    for (unsigned fault = 0; fault < 4; fault++) {
        if (fault == 0) targets[0]->data.member.name = "other";
        if (fault == 1) targets[0]->data.member.name = NULL;
        if (fault == 2) member_row->syntax_name = "other";
        if (fault == 3) member_row->syntax_name = NULL;
        bool empty = true;
        CHECK(!function_param_flow_consumer_empty_current(ctx, index->claims, targets[0], &empty));
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
        targets[0]->data.member.name = member_name;
        member_row->syntax_name = claim_name;
        CHECK(function_param_flow_consumer_empty_current(ctx, index->claims, targets[0], &empty) && empty);
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
    }
    /* Grandchild currency belongs to the full owner, not a recursive getter. */
    ASTNode *number = ast_binary_left(ast_array_access_index(targets[1]));
    uint32_t number_id = number->stable_id;
    number->stable_id++;
    CHECK(!function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
    number->stable_id = number_id;
    CHECK(function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
    PublishedNode *selection = published_node_for(ctx, targets[4]);
    CHECK(selection != NULL && selection->unit_enum_selection != NULL);
    if (selection != NULL && selection->unit_enum_selection != NULL) {
        SemanticPublishedUnitEnumSelection *saved = selection->unit_enum_selection;
        selection->unit_enum_selection = NULL;
        bool empty = true;
        CHECK(!function_param_flow_consumer_empty_current(ctx, index->claims, targets[4], &empty));
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
        selection->unit_enum_selection = saved;
        CHECK(function_param_flow_consumer_empty_current(ctx, index->claims, targets[4], &empty) && empty);
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
    }
    for (size_t t = 0; t < 2; t++) for (unsigned mode = 0; mode < 3; mode++) {
        unsigned effects[2] = {0, 0};
        uint64_t outputs = 0;
        FunctionParamFlowBackingFact partial = {.origin_count = mode == 0 ? 2 : 1,
            .origin_words = mode == 1 ? 2 : 1, .destination_count = 1,
            .destination_words = 1, .effects = effects, .outputs = &outputs,
            .claims = index->claims, .epoch = 1, .complete = true};
        ReturnedBacking state = {.store = store, .lookup = {ctx, program}, .decl = decl,
            .fact = &partial, .loan_seed = mode == 2 ? targets[t] : NULL};
        backing_consumers(&state, targets[t]);
        if (mode < 2) CHECK(!partial.complete);
        else CHECK(partial.complete && state.loan_seed_seen && state.loan_work != 0 && partial.node_count != 0);
        for (size_t n = 0; n < partial.node_count; n++) free(partial.nodes[n].origins);
        free(partial.nodes); free(partial.bindings); free(partial.pending_relations);
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_c_empty_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Probe(inout output:String,text:String,flag:Bool)->Void{"
        "if !flag||flag{output=text;}if StringLength(text)>0{output=text;}"
        "if (1 as Long)>0L{output=text;}}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 0), *body = ast_func_body(decl);
    ASTNode *binary = ast_if_condition(ast_block_statement(body, 0));
    ASTNode *unary = ast_binary_left(binary), *leaf = ast_unary_operand(unary);
    ASTNode *with_call = ast_if_condition(ast_block_statement(body, 1));
    ASTNode *with_cast = ast_if_condition(ast_block_statement(body, 2));
    unsigned baseline = function_param_flow_summary_for_param(ctx, decl, 1);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (index == NULL) goto cleanup;
    ASTNode *targets[] = {binary, unary, leaf, with_call, with_cast,
        ast_binary_left(with_call), ast_binary_left(with_cast)};
    size_t ordinals[7];
    for (size_t t = 0; t < 7; t++) {
        ordinals[t] = SIZE_MAX;
        for (size_t c = 0; c < index->claims->count; c++)
            if (index->claims->rows[c].node == targets[t]) ordinals[t] = c;
        CHECK(ordinals[t] < index->claims->count);
        if (ordinals[t] == SIZE_MAX) goto cleanup;
        CHECK(index->claims->rows[ordinals[t]].consumer_empty == (t < 3));
        CHECK(function_param_flow_consumer_empty_derive(ctx, index->claims, ordinals[t]) == (t < 3));
        bool empty = t >= 3;
        bool queried = function_param_flow_consumer_empty_current(ctx, index->claims, targets[t], &empty);
        if (t == 0 || t == 1 || t == 3 || t == 4) {
            CHECK(queried);
            CHECK(empty == (t < 3));
        } else CHECK(!queried);
    }
    ASTNode foreign = *binary;
    bool missing_empty = true;
    CHECK(!function_param_flow_consumer_empty_current(ctx, index->claims, NULL, &missing_empty));
    CHECK(!function_param_flow_consumer_empty_current(ctx, index->claims, &foreign, &missing_empty));
    for (size_t t = 0; t < 7; t++) {
        FunctionParamFlowIndexClaim *row = &index->claims->rows[ordinals[t]];
        bool saved = row->consumer_empty;
        row->consumer_empty = !saved;
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
        row->consumer_empty = saved;
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
    }
    FunctionParamFlowIndexClaim *parent = &index->claims->rows[ordinals[0]];
    FunctionParamFlowIndexClaim saved_parent = *parent;
    size_t saved_child = parent->child_claim_ordinals[0];
    ASTNode *saved_child_node = parent->children[0];
    for (unsigned fault = 0; fault < 7; fault++) {
        if (fault == 0) parent->child_claim_ordinals = NULL;
        if (fault == 1) parent->child_claim_ordinals[0] = SIZE_MAX;
        if (fault == 2) parent->child_claim_ordinals[0] = ordinals[2];
        if (fault == 3) parent->child_count--;
        if (fault == 4) parent->snapshot_semantic_complete = false;
        if (fault == 5) parent->children = NULL;
        if (fault == 6) parent->children[0] = ast_binary_right(binary);
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
        *parent = saved_parent;
        parent->child_claim_ordinals[0] = saved_child;
        parent->children[0] = saved_child_node;
        CHECK(function_param_flow_index_claims_current(ctx, index));
    }
    FunctionParamFlowIndexClaim *child = &index->claims->rows[ordinals[1]];
    FunctionParamFlowIndexClaim saved_child_row = *child;
    FunctionParamFlowIndexClaim saved_leaf_row = index->claims->rows[ordinals[2]];
    ASTNode saved_binary = *binary, saved_unary = *unary, saved_leaf = *leaf;
    PublishedNode *value = published_node_for(ctx, leaf);
    CHECK(value != NULL);
    if (value == NULL) goto cleanup;
    PublishedNode saved_value = *value;
    for (unsigned fault = 0; fault < 13; fault++) {
        if (fault == 0) child->snapshot_complete = false;
        if (fault == 1) child->semantic_promoted = false;
        if (fault == 2) binary->data.binary.left = NULL;
        if (fault == 3) unary->data.unary.operand = NULL;
        if (fault == 4) leaf->stable_id++;
        if (fault == 5) leaf->data.identifier.name = "other";
        if (fault == 6) ast_identifier_set_binding_syntax_id(leaf, 0);
        if (fault == 7) value->value_present = false;
        if (fault == 8) value->value_type = TYPE_STRING;
        if (fault == 9) value->meta = !saved_value.meta;
        if (fault == 10) child->snapshot_semantic_complete = false;
        if (fault == 11) child->id++;
        if (fault == 12) index->claims->rows[ordinals[2]].slot_token_operand = true;
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == SLOT_PARAM_SUMMARY_ALL);
        *binary = saved_binary; *unary = saved_unary; *leaf = saved_leaf;
        *value = saved_value; *child = saved_child_row;
        index->claims->rows[ordinals[2]] = saved_leaf_row;
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
    }
    /* A completed raw syntax snapshot is not an eager leaf receipt. */
    FunctionParamFlowProgramPointIndex staged = {.dependency_body = body,
        .dependency_body_id = ast_node_stable_id(body),
        .dependency_root_count = ast_block_statement_count(body)};
    CHECK(function_param_flow_syntax_claims_snapshot(ctx, &staged.claims, body));
    for (size_t c = 0; c < staged.claims->count; c++) CHECK(!staged.claims->rows[c].consumer_empty);
    bool empty = true;
    bool raw = function_param_flow_consumer_empty_current(ctx, staged.claims, binary, &empty);
    CHECK(!raw || !empty);
    CHECK(function_param_flow_index_claims_snapshot(ctx, &staged, body));
    CHECK(function_param_flow_consumer_empty_current(ctx, staged.claims, binary, &empty) && empty);
    function_param_flow_program_point_index_destroy(&staged);
    /* Raw topology controls do not claim these edited trees were typechecked. */
    for (unsigned shape = 0; shape < 4; shape++) {
        FunctionParamFlowProgramPointIndex probe = {.dependency_body = body,
            .dependency_body_id = ast_node_stable_id(body),
            .dependency_root_count = ast_block_statement_count(body)};
        if (shape == 0) binary->data.binary.left = NULL;
        if (shape == 1) unary->data.unary.operand = NULL;
        if (shape == 2) binary->data.binary.left = ast_binary_right(binary);
        if (shape == 3) binary->data.binary.left = binary;
        bool built = function_param_flow_index_claims_snapshot(ctx, &probe, body);
        empty = true;
        bool issued = built && function_param_flow_consumer_empty_current(ctx, probe.claims, binary, &empty);
        if (shape < 2) CHECK(!issued || !empty);
        if (shape == 2) CHECK(issued && empty);
        if (shape == 3) CHECK(!built && !issued);
        *binary = saved_binary; *unary = saved_unary;
        function_param_flow_program_point_index_destroy(&probe);
    }
    for (unsigned mode = 0; mode < 3; mode++) {
        unsigned effects[2] = {0, 0};
        uint64_t outputs = 0;
        FunctionParamFlowBackingFact partial = {.origin_count = mode == 0 ? 2 : 1,
            .origin_words = mode == 1 ? 2 : 1, .destination_count = 1,
            .destination_words = 1, .effects = effects, .outputs = &outputs,
            .claims = index->claims, .epoch = 1, .complete = true};
        ReturnedBacking state = {.store = store, .lookup = {ctx, program}, .decl = decl,
            .fact = &partial, .loan_seed = mode == 2 ? binary : NULL};
        backing_consumers(&state, binary);
        if (mode < 2) CHECK(!partial.complete);
        else CHECK(partial.complete && state.loan_seed_seen && state.loan_work != 0 && partial.node_count != 0);
        for (size_t n = 0; n < partial.node_count; n++) free(partial.nodes[n].origins);
        free(partial.nodes); free(partial.bindings); free(partial.pending_relations);
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_control_navigation_preparation(void)
{
    Lexer *lexer = lexer_create(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{"
        "var first:String=\"\";var second:String=\"\";"
        "while flag{if flag{first=second;break;}else{second=text;continue;}}"
        "output=first;return true;}");
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
    CHECK((mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (index == NULL || index->backing_frames == NULL || index->backing_frames[1] == NULL) {
        CHECK(false); goto cleanup;
    }
    FunctionParamFlowBackingFact *frame = index->backing_frames[1];
    CHECK(frame->complete && !frame->pending && frame->epoch > 1);
    CHECK(frame->outputs != NULL && (frame->outputs[0] & UINT64_C(1)) != 0);
    /* A normal formal evaluation consumes the owner's prepared data order.
     * Retaining wrapper rows but waiving their charge fails this oracle too. */
    size_t control_rows = 0;
    for (size_t n = 0; n < frame->node_count; n++) {
        ASTNodeType shape = frame->nodes[n].shape;
        if (shape == AST_BLOCK || shape == AST_IF_STMT || shape == AST_WHILE_LOOP ||
            shape == AST_FOR_LOOP || shape == AST_WITH_STMT || shape == AST_DEFER_STMT ||
            shape == AST_BREAK || shape == AST_CONTINUE) control_rows++;
    }
    if (control_rows != 0)
        fprintf(stderr, "control navigation retained %zu per-frame wrapper rows\n", control_rows);
    CHECK(control_rows == 0);
    CHECK(function_param_flow_backing_rows_current(store, decl));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_control_navigation_topology(void)
{
    for (unsigned cycle = 0; cycle < 2; cycle++) {
        Lexer *lexer = lexer_create(
            "func Probe(inout output:String,text:String,flag:Bool)->Void{"
            "if flag{output=text;}else{output=text;}}");
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *decl = ast_program_statement(program, 0);
        ASTNode *branch = ast_block_statement(ast_func_body(decl), 0);
        ASTNode *then_body = ast_if_then_branch(branch), *else_body = ast_if_else_branch(branch);
        if (!cycle) {
            FunctionParamFlowProgramPointIndex empty = {0};
            CHECK(!function_param_flow_index_claims_snapshot(ctx, &empty, NULL));
            CHECK(empty.claims == NULL && empty.control_projection == NULL);
            empty.dependency_body = ast_func_body(decl);
            empty.dependency_body_id = ast_node_stable_id(empty.dependency_body);
            CHECK(!function_param_flow_index_claims_snapshot(ctx, &empty, decl));
            empty.dependency_body_id++;
            CHECK(!function_param_flow_index_claims_snapshot(ctx, &empty, empty.dependency_body));
            CHECK(empty.claims == NULL && empty.control_projection == NULL);
            function_param_flow_program_point_index_destroy(&empty);
        }
        function_param_flow_summary_store_destroy(ctx);
        /* Reuse a checked block on both edges; do not typecheck a forged DAG.
         * The fresh owner snapshot must handle an already completed child. */
        if (cycle) branch->data.if_stmt.then_branch = branch;
        else branch->data.if_stmt.else_branch = then_body;
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        FunctionParamFlowProgramPointIndex *index = NULL;
        FunctionParamFlowIndexClaims *cyclic_claims = NULL;
        bool built = cycle
            ? function_param_flow_syntax_claims_snapshot(ctx, &cyclic_claims, ast_func_body(decl))
            : store != NULL && function_param_flow_program_point_index_build(store, decl, &index);
        CHECK(built == !cycle);
        if (!cycle && built) {
            unsigned effects = 0;
            bool outputs[3] = {false, false, false}, pending = true;
            CHECK(function_param_flow_returned_backing_effects(store, decl, 1,
                &effects, outputs, 3, &pending, false));
            CHECK(!pending && outputs[0] && !outputs[1] && !outputs[2]);
            CHECK((effects & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
            CHECK(function_param_flow_backing_rows_current(store, decl));
        }
        branch->data.if_stmt.then_branch = then_body;
        branch->data.if_stmt.else_branch = else_body;
        function_param_flow_claims_destroy(cyclic_claims);
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_control_navigation_body_boundary(void)
{
    Fixture f = fixture();
    ASTNode *body = ast_func_body(f.reader), *returned = ast_block_statement(body, 0);
    /* Source typechecking does not issue a top-body execution row. Exercise
     * that optional owner-protocol fact through the pending producer API;
     * this is not source admission or a MIR validity claim. */
    CHECK(semantic_param_publication_record_execution(f.ctx, body, false));
    CHECK(semantic_param_publication_record_execution(f.ctx, returned, true));
    CHECK(semantic_param_publication_record_value(f.ctx, f.map_call, TYPE_INT, false));
    CHECK(register_map(&f));
    CHECK(register_reader(&f));
    CHECK(register_caller(&f));
    CHECK(register_body_dependency(&f));
    CHECK(semantic_param_publication_finalize(f.ctx));
    bool reachable = true;
    CHECK(semantic_param_publication_execution(f.ctx, body, &reachable) && !reachable);
    CHECK(semantic_param_publication_execution(f.ctx, returned, &reachable) && reachable);
    SlotFunctionLookup lookup = {f.ctx, f.program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, f.reader, &index));
    if (index != NULL) {
        unsigned effects = 0;
        uint64_t outputs = 0;
        FunctionParamFlowBackingFact partial = {.origin_count = 1, .origin_words = 1,
            .destination_count = 1, .destination_words = 1, .effects = &effects,
            .outputs = &outputs, .claims = index->claims, .epoch = 1, .complete = true};
        ReturnedBacking state = {.store = store, .lookup = lookup, .decl = f.reader,
            .fact = &partial, .checking_declared_result = true};
        CHECK(backing_binding(&state, ast_func_param_stable_id(ast_func_param(f.reader, 0)), true) != NULL);
        function_param_flow_backing_control_roots(&state, index->control_projection);
        bool returned_seen = false, call_seen = false;
        for (size_t n = 0; n < partial.node_count; n++) {
            returned_seen |= partial.nodes[n].node == returned;
            call_seen |= partial.nodes[n].node == f.map_call;
            free(partial.nodes[n].origins);
        }
        CHECK(partial.complete && state.result_return_seen && returned_seen && call_seen);
        CHECK(effects == 0 && outputs == 0);
        for (size_t b = 0; b < partial.binding_count; b++) {
            free(partial.bindings[b].origins); free(partial.bindings[b].destinations);
        }
        free(partial.nodes); free(partial.bindings); free(partial.pending_relations);
    }
    dispose(&f);
}

static void checked_control_navigation_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Probe(inout output:String,text:String,flag:Bool)->Bool{"
        "if flag{output=text;}else{output=\"fresh\";}return true;"
        "if flag{while flag{output=text;}}}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 0);
    unsigned baseline = function_param_flow_summary_for_param(ctx, decl, 1);
    CHECK((baseline & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
    if (index == NULL) goto cleanup;
    FunctionParamFlowControlProjection *control = index->control_projection;
    CHECK(control != NULL && control->ready && control->step_count >= 2 &&
        control->steps != NULL && control->root_offsets != NULL && control->root_count == 3);
    if (control == NULL || !control->ready || control->step_count < 2 || control->steps == NULL ||
        control->root_offsets == NULL || control->root_count != 3) goto cleanup;
    CHECK(control->root_offsets[0] == 0 && control->root_offsets[3] == control->step_count &&
        control->root_offsets[2] == control->root_offsets[3]);
    FunctionParamFlowControlProjection saved = *control;
    unsigned char first[sizeof(*control->steps)], second[sizeof(*control->steps)];
    memcpy(first, &control->steps[0], sizeof(first));
    memcpy(second, &control->steps[1], sizeof(second));
    size_t offsets[4];
    memcpy(offsets, control->root_offsets, sizeof(offsets));
    size_t root_claim = control->root_claim_ordinals[0];
    for (unsigned fault = 0; fault < 21; fault++) {
        if (fault == 0) control->ready = false;
        if (fault == 1) control->steps = NULL;
        if (fault == 2) control->step_count = 0;
        if (fault == 3) control->step_count++;
        if (fault == 4) control->root_offsets = NULL;
        if (fault == 5) control->root_offsets[0] = 1;
        if (fault == 6) control->root_offsets[3] = control->step_count + 1;
        if (fault == 7) control->root_offsets[1] = control->step_count + 1;
        if (fault == 8) control->steps[0].node = NULL;
        if (fault == 9) control->steps[0].syntax_id++;
        if (fault == 10) control->steps[0].claim_ordinal = SIZE_MAX;
        if (fault == 11) {
            memcpy(&control->steps[0], second, sizeof(second));
            memcpy(&control->steps[1], first, sizeof(first));
        }
        if (fault == 12) {
            control->step_count--;
            control->root_offsets[2] = control->step_count;
            control->root_offsets[3] = control->step_count;
        }
        if (fault == 13) control->root_offsets[1]--;
        if (fault == 14) control->steps[0].root_ordinal++;
        if (fault == 15) control->root_count--;
        if (fault == 16) index->control_projection = NULL;
        if (fault == 17) control->projected_claim_count--;
        if (fault == 18) control->root_claim_ordinals = NULL;
        if (fault == 19) control->root_claim_ordinals[0] = SIZE_MAX;
        if (fault == 20) control->root_claim_ordinals[0] = control->root_claim_ordinals[1];
        FunctionParamFlowProgramPointIndex *rechecked = NULL;
        CHECK(!function_param_flow_program_point_index_build(store, decl, &rechecked));
        CHECK(!function_param_flow_backing_rows_current(store, decl));
        index->control_projection = control;
        *control = saved;
        memcpy(&control->steps[0], first, sizeof(first));
        memcpy(&control->steps[1], second, sizeof(second));
        memcpy(control->root_offsets, offsets, sizeof(offsets));
        control->root_claim_ordinals[0] = root_claim;
        CHECK(function_param_flow_program_point_index_build(store, decl, &rechecked));
        CHECK(function_param_flow_backing_rows_current(store, decl));
        CHECK(function_param_flow_summary_for_param(ctx, decl, 1) == baseline);
    }
    CHECK(root_claim < index->claims->count);
    if (root_claim < index->claims->count) {
        FunctionParamFlowIndexClaim saved_root = index->claims->rows[root_claim];
        CHECK(saved_root.control_outgoing_count != 0 && saved_root.control_incoming_count != 0);
        for (unsigned fault = 0; fault < 3; fault++) {
            if (fault == 0) index->claims->rows[root_claim].control_projected = false;
            if (fault == 1) index->claims->rows[root_claim].control_incoming_count = 0;
            if (fault == 2) index->claims->rows[root_claim].control_outgoing_count--;
            FunctionParamFlowProgramPointIndex *rechecked = NULL;
            CHECK(!function_param_flow_program_point_index_build(store, decl, &rechecked));
            CHECK(!function_param_flow_backing_rows_current(store, decl));
            index->claims->rows[root_claim] = saved_root;
            CHECK(function_param_flow_program_point_index_build(store, decl, &rechecked));
            CHECK(function_param_flow_backing_rows_current(store, decl));
        }
    }
    size_t claim_at = control->steps[0].claim_ordinal;
    CHECK(claim_at < index->claims->count);
    if (claim_at < index->claims->count) {
        FunctionParamFlowIndexClaim saved_claim = index->claims->rows[claim_at];
        for (unsigned fault = 0; fault < 2; fault++) {
            if (fault == 0) index->claims->rows[claim_at].control_projected = false;
            if (fault == 1) index->claims->rows[claim_at].control_step_ordinal = SIZE_MAX;
            FunctionParamFlowProgramPointIndex *rechecked = NULL;
            CHECK(!function_param_flow_program_point_index_build(store, decl, &rechecked));
            CHECK(!function_param_flow_backing_rows_current(store, decl));
            index->claims->rows[claim_at] = saved_claim;
            CHECK(function_param_flow_program_point_index_build(store, decl, &rechecked));
            CHECK(function_param_flow_backing_rows_current(store, decl));
        }
    }
    ASTNode *dead = ast_block_statement(ast_func_body(decl), 2);
    PublishedNode *execution = NULL;
    for (size_t p = 0; p < ctx->function_param_publications->node_capacity; p++)
        if (ctx->function_param_publications->nodes[p].node == dead)
            execution = &ctx->function_param_publications->nodes[p];
    CHECK(execution != NULL && execution->execution_present && !execution->reachable);
    if (execution != NULL) {
        PublishedNode saved_execution = *execution;
        for (unsigned fault = 0; fault < 2; fault++) {
            if (fault == 0) execution->reachable = true;
            if (fault == 1) execution->execution_present = false;
            FunctionParamFlowProgramPointIndex *rechecked = NULL;
            CHECK(!function_param_flow_program_point_index_build(store, decl, &rechecked));
            CHECK(!function_param_flow_backing_rows_current(store, decl));
            *execution = saved_execution;
            CHECK(function_param_flow_program_point_index_build(store, decl, &rechecked));
            CHECK(function_param_flow_backing_rows_current(store, decl));
        }
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_control_navigation_root_sequence(void)
{
    for (unsigned mode = 0; mode < 3; mode++) {
        const char *source = mode == 0
            ? "func Probe(text:String,flag:Bool)->String{if flag{let first=text;let sibling=\"sibling\";}return \"next\";}"
            : mode == 1
                ? "func Probe(text:String,callback:func(String)->String)->String{if true{callback(text);Log(\"tail\");}return \"next\";}"
                : "func Probe()->Array<Int>{if !false||true{let rows:Array<Int> = [];return rows;}return [];}";
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
        ASTNode *body = ast_func_body(decl);
        ASTNode *branch = ast_block_statement(body, 0);
        ASTNode *inside = ast_if_then_branch(branch);
        ASTNode *sibling = ast_block_statement(inside, 1);
        ASTNode *next_root = ast_block_statement(body, 1);
        SlotFunctionLookup lookup = {ctx, program};
        FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
        FunctionParamFlowProgramPointIndex *index = NULL;
        CHECK(store != NULL && function_param_flow_program_point_index_build(store, decl, &index));
        if (index == NULL || index->control_projection == NULL) goto cleanup;
        unsigned effects = 0;
        uint64_t outputs = 0;
        FunctionParamFlowBackingFact partial = {.origin_count = 1, .origin_words = 1,
            .destination_count = 1, .destination_words = 1, .effects = &effects,
            .outputs = &outputs, .claims = index->claims, .epoch = 1, .complete = true};
        ReturnedBacking state = {.store = store, .lookup = lookup, .decl = decl,
            .fact = &partial, .checking_declared_result = mode == 2};
        /* Missing formal storage is deliberate: the first LET must fail but
         * its same-root sibling must still run. A later top-level root must not. */
        function_param_flow_backing_control_roots(&state, index->control_projection);
        bool saw_sibling = false, saw_next = false;
        size_t control_rows = 0;
        for (size_t n = 0; n < partial.node_count; n++) {
            saw_sibling |= partial.nodes[n].node == sibling;
            saw_next |= partial.nodes[n].node == next_root;
            ASTNodeType shape = partial.nodes[n].shape;
            if (shape == AST_BLOCK || shape == AST_IF_STMT ||
                (mode == 2 && (shape == AST_BINARY || shape == AST_UNARY))) control_rows++;
        }
        CHECK(control_rows == 0);
        if (mode == 0) CHECK(!partial.complete && !partial.analysis_unsupported && saw_sibling && !saw_next);
        if (mode == 1) CHECK(!partial.complete && partial.analysis_unsupported && !saw_sibling && !saw_next);
        if (mode == 2) CHECK(partial.complete && !partial.analysis_unsupported && state.result_return_seen && saw_sibling);
        for (size_t n = 0; n < partial.node_count; n++) free(partial.nodes[n].origins);
        for (size_t b = 0; b < partial.binding_count; b++) {
            free(partial.bindings[b].origins); free(partial.bindings[b].destinations);
        }
        free(partial.nodes); free(partial.bindings); free(partial.pending_relations);
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_backing_input_frames(void)
{
    for (unsigned reverse = 0; reverse < 2; reverse++) {
        Lexer *lexer = lexer_create("func Choose(inout output:String,left:String,right:String)->Void{if !false||true{output=left;}}func Main()->Void{var output=\"\";Choose(output,\"left\",\"right\");}");
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
        ASTNode *source = ast_assignment_value(ast_block_statement(
            ast_if_then_branch(ast_block_statement(ast_func_body(decl), 0)), 0));
        CHECK(source != NULL && source->type == AST_IDENTIFIER);
        if (source == NULL || source->type != AST_IDENTIFIER) goto cleanup;
        uint32_t source_id = source->stable_id;
        uint32_t source_binding = ast_identifier_binding_syntax_id(source);
        CHECK(source_id != 0 && source_binding == ast_func_param_stable_id(ast_func_param(decl, 1)));
        source->stable_id = 0;
        CHECK(!function_param_flow_backing_rows_current(store, decl));
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
        source->stable_id = source_id;
        ast_identifier_set_binding_syntax_id(source, 0);
        CHECK(!function_param_flow_backing_rows_current(store, decl));
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
        ast_identifier_set_binding_syntax_id(source, source_binding);
        PublishedNode *source_value = NULL;
        for (size_t p = 0; p < ctx->function_param_publications->node_capacity; p++)
            if (ctx->function_param_publications->nodes[p].node == source)
                source_value = &ctx->function_param_publications->nodes[p];
        CHECK(source_value != NULL && source_value->value_present);
        if (source_value == NULL) goto cleanup;
        source_value->value_present = false;
        CHECK(!function_param_flow_backing_rows_current(store, decl));
        CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
        source_value->value_present = true;
        CHECK(function_param_flow_backing_rows_current(store, decl));
        CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        CHECK(frame->binding_count != 0 && frame->epoch < SIZE_MAX);
        if (frame->binding_count == 0 || frame->epoch == SIZE_MAX) goto cleanup;
        for (unsigned kind = 0; kind < 2; kind++) {
            size_t *stamp = kind == 0 ? &frame->bindings[0].origin_read_epoch :
                &frame->bindings[0].destination_read_epoch;
            size_t saved = *stamp;
            *stamp = 0;
            CHECK(function_param_flow_backing_rows_current(store, decl));
            *stamp = frame->epoch + 1;
            CHECK(!function_param_flow_backing_rows_current(store, decl));
            CHECK(!function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent));
            *stamp = saved;
            CHECK(function_param_flow_backing_rows_current(store, decl));
            CHECK(function_param_flow_output_dependency_for_call(&lookup, call, 1, 0, &dependent) && dependent);
        }
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
    Lexer *lexer = lexer_create(
        "func Build(input:Array<String>)->Array<String>{"
        "let owned_rows:Array<String> = [\"known\",Concat(\"local\",\" value\")];"
        "ArrayPush(owned_rows,input[0]);return owned_rows;}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(!admitted && ctx->has_error &&
        context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
    bool inputs_ready = parsed && semantic_param_publication_inputs_ready(ctx);
    CHECK(inputs_ready);
    if (!parsed || admitted || !inputs_ready) {
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
        return;
    }
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *local = ast_block_statement(ast_func_body(decl), 0);
    ASTNode *initial = ast_let_initializer(local);
    ASTNode *call = ast_block_statement(ast_func_body(decl), 1);
    ASTNode *child = ast_array_literal_element(initial, 1);
    ASTNode *target = ast_call_callee(call);
    CHECK(ast_array_literal_count(initial) == 2 && child != NULL && target != NULL);
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
    if (published != NULL && initial_value != NULL && index != NULL && child != NULL && target != NULL) {
        PublishedNode saved = *published, saved_initial = *initial_value;
        ASTNode saved_call = *call;
        uint32_t child_id = ast_node_stable_id(child), target_id = ast_node_stable_id(target);
        size_t initial_count = ast_array_literal_count(initial);
        for (unsigned fault = 0; fault < 15; fault++) {
            if (fault == 0) published->private_exclusive = false;
            if (fault == 1) published->private_escaped = true;
            if (fault == 2) published->private_pending = true;
            if (fault == 3) local->data.let_decl.is_alias = true;
            if (fault == 4) child->stable_id++;
            if (fault == 5) initial->data.array_literal.count = initial_count - 1;
            if (fault == 6) initial_value->string_array_type = NULL;
            if (fault == 7) published->private_binding_id++;
            if (fault == 8) published->private_initializer_id++;
            if (fault == 9) published->caller_id++;
            if (fault == 10) call->data.call.arg_count = 1;
            if (fault == 11) published->stdlib = false;
            if (fault == 12) target->stable_id++;
            if (fault == 13) published->deferred_syntax = NULL;
            if (fault == 14) published->private_binding_type_id++;
            CHECK(!semantic_param_publication_private_array_push(ctx, call, decl));
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            *published = saved; *initial_value = saved_initial; *call = saved_call;
            local->data.let_decl.is_alias = false;
            initial->data.array_literal.count = initial_count;
            child->stable_id = child_id; target->stable_id = target_id;
            CHECK(semantic_param_publication_private_array_push(ctx, call, decl));
            CHECK(function_param_flow_index_claims_current(ctx, index));
        }
        size_t annotation_entry = 0;
        bool annotation_found = false;
        for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
            if (ctx->type_resolution_metadata.keys[i] == ast_let_type(local)) {
                annotation_entry = i;
                annotation_found = true;
                break;
            }
        }
        CHECK(annotation_found);
        if (annotation_found) {
            void *saved_type = ctx->type_resolution_metadata.values[annotation_entry];
            for (unsigned fault = 0; fault < 2; fault++) {
                ctx->type_resolution_metadata.values[annotation_entry] =
                    fault == 0 ? NULL : TYPE_INT;
                CHECK(!semantic_param_publication_private_array_push(ctx, call, decl));
                CHECK(!function_param_flow_index_claims_current(ctx, index));
                ctx->type_resolution_metadata.values[annotation_entry] = saved_type;
                CHECK(semantic_param_publication_private_array_push(ctx, call, decl));
                CHECK(function_param_flow_index_claims_current(ctx, index));
            }
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

/* The private Int push must remain a current dependency through both nominal
 * result frames, rather than merely making the direct ArrayPush query green. */
static void checked_private_physical_array_nested_result_currency(void)
{
    const char *source =
        "struct Rows{ids:Array<Int>;}"
        "struct Arena{rows:Rows;}"
        "struct Sequence{ok:Bool;roots:Array<Int>;arena:Arena;ordinals:Array<Int>;}"
        "func Nested(count:Int)->Rows{let ids:Array<Int> = [];let row:Int=0;"
        "while row<count{ArrayPush(ids,row);row=row+1;}return Rows(ids);}"
        "func OuterEmpty()->Sequence{let roots:Array<Int> = [];"
        "let ordinals:Array<Int> = [];"
        "return Sequence(true,roots,Arena(Nested(0)),ordinals);}"
        "func Error(input:Sequence)->Sequence{let failed:Sequence=OuterEmpty();"
        "failed.ok=false;return failed;}"
        "func Probe(ref label:String)->Option<Int>{let input:Sequence=OuterEmpty();"
        "let failed:Sequence=Error(input);"
        "return Some(ArrayLength(failed.roots)+ArrayLength(failed.arena.rows.ids)+"
        "ArrayLength(failed.ordinals)+StringLength(label));}"
        "func Main()->Void{let grown:Rows=Nested(1);Log(ArrayLength(grown.ids));"
        "let label:String=Concat(\"heap\",\"\");Log(UnwrapOption(Probe(label)));}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) goto cleanup;

    ASTNode *nested = ast_program_statement(program, 3);
    ASTNode *outer = ast_program_statement(program, 4);
    ASTNode *error = ast_program_statement(program, 5);
    ASTNode *probe = ast_program_statement(program, 6);
    ASTNode *local = ast_block_statement(ast_func_body(nested), 0);
    ASTNode *initial = ast_let_initializer(local);
    ASTNode *loop = ast_block_statement(ast_func_body(nested), 2);
    ASTNode *push = ast_block_statement(ast_while_body(loop), 0);
    ASTNode *payload = ast_call_argument(push, 1);
    ASTNode *target = ast_call_callee(push);
    ASTNode *rows_ctor = ast_return_value(ast_block_statement(ast_func_body(nested), 3));
    ASTNode *sequence_ctor = ast_return_value(ast_block_statement(ast_func_body(outer), 2));
    ASTNode *arena_ctor = ast_call_argument(sequence_ctor, 2);
    ASTNode *nested_call = ast_call_argument(arena_ctor, 0);
    ASTNode *outer_call = ast_let_initializer(ast_block_statement(ast_func_body(error), 0));
    CHECK(nested != NULL && outer != NULL && error != NULL && probe != NULL &&
        local != NULL && initial != NULL && loop != NULL && push != NULL &&
        payload != NULL && target != NULL && rows_ctor != NULL &&
        nested_call != NULL && outer_call != NULL);
    if (nested == NULL || outer == NULL || error == NULL || probe == NULL ||
        local == NULL || initial == NULL || loop == NULL || push == NULL ||
        payload == NULL || target == NULL || rows_ctor == NULL ||
        nested_call == NULL || outer_call == NULL) goto cleanup;
    CHECK(ast_array_literal_count(initial) == 0 &&
        semantic_param_publication_private_array_push(ctx, push, nested));
    SlotFunctionLookup lookup = {ctx, program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    CHECK(store != NULL);
    if (store == NULL) goto cleanup;
    unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
    CHECK(baseline != SLOT_PARAM_SUMMARY_ALL &&
        (baseline & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0);
    CHECK(function_param_flow_no_store_graph(store, nested));
    CHECK(function_param_flow_no_store_graph(store, outer));
    const Type *result_type = NULL;
    CHECK(semantic_param_publication_declared_result(ctx, nested_call, &result_type) == nested &&
        result_type != NULL);
    CHECK(semantic_param_publication_declared_result(ctx, outer_call, &result_type) == outer &&
        result_type != NULL);
    CHECK(function_param_flow_backing_rows_current(store, probe));
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(function_param_flow_program_point_index_build(store, nested, &index) &&
        index != NULL && function_param_flow_index_claims_current(ctx, index));
    CHECK(function_param_flow_program_point_index_build(store, outer, &index) &&
        index != NULL && index->declared_result != NULL &&
        index->declared_result->epoch != 0 && index->declared_result->complete &&
        !index->declared_result->pending);
    CHECK(function_param_flow_program_point_index_build(store, nested, &index) &&
        index != NULL && index->declared_result != NULL &&
        index->declared_result->epoch != 0 && index->declared_result->complete &&
        !index->declared_result->pending);
    if (index == NULL || index->function_decl != nested) goto cleanup;
    size_t nested_index_position = (size_t)(index - store->program_point_indexes);

    PublishedNode *row = published_node_for(ctx, push);
    PublishedNode *initial_value = published_node_for(ctx, initial);
    PublishedNode *receiver_value = published_node_for(ctx, ast_call_argument(push, 0));
    PublishedNode *payload_value = published_node_for(ctx, payload);
    CHECK(row != NULL && initial_value != NULL && receiver_value != NULL &&
        payload_value != NULL && row->argument_count == 2);
    if (row == NULL || initial_value == NULL || receiver_value == NULL ||
        payload_value == NULL || row->argument_count != 2) goto cleanup;
    PublishedNode saved_row = *row, saved_initial = *initial_value;
    PublishedNode saved_receiver = *receiver_value, saved_payload = *payload_value;
    ASTNode saved_call = *push;
    uint32_t payload_id = ast_node_stable_id(payload);
    uint32_t target_id = ast_node_stable_id(target);
    for (unsigned fault = 0; fault < 17; fault++) {
        if (fault == 0) row->private_exclusive = false;
        if (fault == 1) row->private_escaped = true;
        if (fault == 2) row->private_pending = true;
        if (fault == 3) local->data.let_decl.is_alias = true;
        if (fault == 4) payload->stable_id++;
        if (fault == 5) initial_value->value_type = TYPE_STRING;
        if (fault == 6) receiver_value->value_type = TYPE_STRING;
        if (fault == 7) payload_value->value_type = TYPE_STRING;
        if (fault == 8) payload_value->physical_scalar = false;
        if (fault == 9) row->private_binding_id++;
        if (fault == 10) row->private_initializer_id++;
        if (fault == 11) row->private_binding_type_id++;
        if (fault == 12) row->caller_id++;
        if (fault == 13) push->data.call.arg_count = 1;
        if (fault == 14) row->stdlib = false;
        if (fault == 15) target->stable_id++;
        if (fault == 16) row->deferred_syntax = NULL;
        CHECK(!semantic_param_publication_private_array_push(ctx, push, nested));
        CHECK(!function_param_flow_index_claims_current(ctx,
            &store->program_point_indexes[nested_index_position]));
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        *row = saved_row; *initial_value = saved_initial;
        *receiver_value = saved_receiver; *payload_value = saved_payload;
        *push = saved_call; local->data.let_decl.is_alias = false;
        payload->stable_id = payload_id; target->stable_id = target_id;
        CHECK(semantic_param_publication_private_array_push(ctx, push, nested));
        CHECK(function_param_flow_program_point_index_build(store, nested, &index) &&
            index != NULL && function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_backing_rows_current(store, probe));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
    }

    size_t annotation_entry = 0;
    bool annotation_found = false;
    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++)
        if (ctx->type_resolution_metadata.keys[i] == ast_let_type(local)) {
            annotation_entry = i; annotation_found = true; break;
        }
    CHECK(annotation_found);
    if (annotation_found) {
        void *saved_type = ctx->type_resolution_metadata.values[annotation_entry];
        ctx->type_resolution_metadata.values[annotation_entry] = TYPE_STRING;
        CHECK(!semantic_param_publication_private_array_push(ctx, push, nested));
        CHECK(!function_param_flow_index_claims_current(ctx,
            &store->program_point_indexes[nested_index_position]));
        CHECK(!function_param_flow_backing_rows_current(store, probe));
        ctx->type_resolution_metadata.values[annotation_entry] = saved_type;
        CHECK(semantic_param_publication_private_array_push(ctx, push, nested));
        CHECK(function_param_flow_backing_rows_current(store, probe));
    }

    const Type *nominal = semantic_param_publication_value(ctx, rows_ctor).type;
    const Type *array_int = saved_row.arguments[0].type;
    CHECK(nominal != NULL && array_int != NULL);
    if (nominal != NULL && array_int != NULL) {
        for (unsigned fault = 0; fault < 2; fault++) {
            Type forged_array = *array_int;
            Type *element[1] = {(Type *)(fault == 0 ? TYPE_ALLOCATOR : nominal)};
            forged_array.name = fault == 0 ? "Array<Allocator>" : "Array<Rows>";
            forged_array.data.constructed.args = element;
            row->arguments[0].type = &forged_array;
            CHECK(!semantic_param_publication_private_array_push(ctx, push, nested));
            CHECK(!function_param_flow_index_claims_current(ctx,
                &store->program_point_indexes[nested_index_position]));
            CHECK(!function_param_flow_backing_rows_current(store, probe));
            row->arguments[0].type = array_int;
            CHECK(semantic_param_publication_private_array_push(ctx, push, nested));
            CHECK(function_param_flow_backing_rows_current(store, probe));
        }
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_array_alias_birth_before(void)
{
    Type *arguments[] = {TYPE_INT};
    Type *array_type = type_create_constructed(TYPE_ARRAY, arguments, 1);
    Symbol *raw = symbol_create_variable("raw_rows", array_type, 1, 1);
    Symbol *scalar = symbol_create_variable("raw_count", TYPE_INT, 1, 1);
    CHECK(raw != NULL && scalar != NULL);
    if (raw != NULL) {
        if (!raw->has_escaped_array_storage)
            fprintf(stderr, "Array alias birth raw default still grants unescaped storage\n");
        CHECK(raw->has_escaped_array_storage && !raw->has_exclusive_array_storage
            && !raw->has_pending_inout_preservation);
    }
    if (scalar != NULL)
        CHECK(!scalar->has_escaped_array_storage && !scalar->has_exclusive_array_storage
            && !scalar->has_pending_inout_preservation);
    symbol_destroy(raw); symbol_destroy(scalar);
    const char *sources[] = {
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let rows:Array<Int> = [7];let(alias,marker)=(rows,0);"
        "Log(Consume(alias));Log(ArrayLength(rows));}",
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let rows:Array<Int> = [7];let pair=(rows,0);"
        "Log(Consume(rows));let(alias,marker)=pair;Log(ArrayLength(alias));}",
        "struct Box{rows:Array<Int>}func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let box:Box=Box([7]);let alias=box.rows;"
        "Log(Consume(alias));Log(ArrayLength(box.rows));}",
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let nested:Array<Array<Int>> = [[7],[0]];"
        "let(alias,marker)=nested;Log(Consume(alias));Log(ArrayLength(nested[0]));}",
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let nested:Array<Array<Int>> = [[7],[0]];let view=nested.Slice(0,2);"
        "let(alias,marker)=view;Log(Consume(alias));Log(ArrayLength(nested[0]));}",
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let wrapped:Option<Array<Int>> = Some([7]);"
        "match wrapped{case Some(alias):Log(Consume(alias));case None:Log(0);}"
        "match wrapped{case Some(rows):Log(ArrayLength(rows));case None:Log(0);}}",
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let nested:Array<Array<Int>> = [[7]];let alias=nested[0];"
        "Log(Consume(alias));Log(ArrayLength(nested[0]));}"
    };
    for (size_t path = 0; path < sizeof(sources) / sizeof(sources[0]); path++) {
        Lexer *lexer = lexer_create(sources[path]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool admitted = parsed && type_check_program(program, ctx);
        if (admitted) fprintf(stderr, "Array alias birth unsafe source path=%zu still admitted\n", path);
        CHECK(!admitted && ctx->has_error && ctx->diagnostic_count != 0);
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_string_member_alias_birth(void)
{
    const char *sources[] = {
        "struct State{rows:Array<String>}"
        "func Consume(own rows:Array<String>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let state:State=State([\"row\"]);let otherRoot:State=state;"
        "let rows:Array<String> = state.rows;Log(Consume(rows));Log(ArrayLength(otherRoot.rows));}",
        "struct State{rows:Array<String>;other:Array<String>}"
        "func Consume(own rows:Array<String>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let source:Array<String> = [\"row\"];let state:State=State(source,source);"
        "let rows:Array<String> = state.rows;Log(Consume(rows));Log(ArrayLength(state.other));}"
    };
    for (size_t path = 0; path < 2; path++) {
        Lexer *lexer = lexer_create(sources[path]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool admitted = parsed && type_check_program(program, ctx);
        if (admitted) fprintf(stderr, "String member alias birth unsafe source path=%zu still admitted\n", path);
        CHECK(!admitted && ctx->has_error && ctx->diagnostic_count != 0);
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    reached_producer_source(
        "func Consume(own rows:Array<String>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let rows:Array<String> = [\"row\"];Log(Consume(rows));}", true);
}

static void checked_array_result_borrowed_formals(void)
{
    const char *rejected[] = {
        "struct State{rows:Array<String>}func Extract(state:State)->Array<String>{return state.rows;}",
        "struct State{rows:Array<String>}func Extract(inout state:State)->Array<String>{return state.rows;}",
        "struct State{rows:Array<Int>}func Extract(state:State)->Array<Int>{return state.rows;}",
        "struct State{rows:Array<Int>}func Extract(inout state:State)->Array<Int>{let rows=state.rows;return rows;}",
        "struct State{rows:Array<String>}func Extract(state:State)->Array<String>{return state.rows;}"
        "func Forward(state:State)->Array<String>{return Extract(state);}",
        "struct State{rows:Array<Int>}func Extract(state:State,flag:Bool)->Array<Int>{"
        "if flag{return [1];}return state.rows;}"
    };
    for (size_t path = 0; path < sizeof(rejected) / sizeof(rejected[0]); path++) {
        Lexer *lexer = lexer_create(rejected[path]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (typed) fprintf(stderr, "Array result borrowed formal path=%zu still admitted\n", path);
        CHECK(!typed && context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
    reached_producer_source(
        "struct State{rows:Array<Int>}func Fresh(state:State)->Array<Int>{return [ArrayLength(state.rows)];}"
        "func Main()->Void{let state=State([7]);let rows=Fresh(state);Log(ArrayLength(rows));}", true);
    reached_producer_source(
        "func Fresh(ref values:Array<Int>)->Array<Int>{return [values[0]];}"
        "func Main()->Void{let values:Array<Int> = [7];let rows=Fresh(values);Log(ArrayLength(rows));}", true);
    reached_producer_source(
        "func Factory(flag:Bool)->Array<Int>{if flag{return [1];}return [2];}"
        "func Main()->Void{let rows=Factory(true);ArrayDrop(rows);}", true);
    reached_producer_source(
        "func Relay(own rows:Array<Int>,flag:Bool)->Array<Int>{"
        "if flag{return rows;}return rows;}"
        "func Main()->Void{let rows:Array<Int> = [7];let moved=Relay(rows,true);ArrayDrop(moved);}", true);
    reached_producer_source(
        "func Build(count:Int)->Array<Int>{let rows:Array<Int> = [];"
        "for i in 0..count{ArrayPush(rows,i);}return rows;}"
        "func Main()->Void{let rows=Build(2);ArrayDrop(rows);}", true);
    reached_producer_source(
        "func Relay(own rows:Array<Int>,flag:Bool)->Array<Int>{"
        "if flag{return rows;}return rows;}"
        "func Main()->Void{let rows:Array<Int> = [7];let moved=Relay(rows,true);"
        "Log(ArrayLength(rows));ArrayDrop(moved);}", false);
    /* Genuine transfer is a required positive, not a reason to reclassify every
     * CALL as exclusive. A missing return-storage receipt must remain visible. */
    reached_producer_source(
        "func Relay(own rows:Array<Int>)->Array<Int>{return rows;}"
        "func Main()->Void{let rows:Array<Int> = [7];let moved=Relay(rows);ArrayDrop(moved);}", true);
}

static void checked_physical_hof_array_result_currency(void)
{
    const char *sources[] = {
        "func Transform(value:Int)->Int{return value+1;}"
        "func MakeRows(xs:Array<Int>)->Array<Int>{return ArrayMap(xs,Transform);}"
        "func Main()->Void{let source:Array<Int> = [1,2];let rows=MakeRows(source);ArrayDrop(rows);}",
        "func Keep(value:Int)->Bool{return value>0;}"
        "func MakeRows(xs:Array<Int>)->Array<Int>{return ArrayFilter(xs,Keep);}"
        "func Main()->Void{let source:Array<Int> = [1,2];let rows=MakeRows(source);ArrayDrop(rows);}"
    };
    for (size_t shape = 0; shape < sizeof(sources) / sizeof(sources[0]); shape++) {
        Lexer *lexer = lexer_create(sources[shape]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool admitted = parsed && type_check_program(program, ctx);
        CHECK(admitted && !ctx->has_error);
        if (!admitted) goto cleanup;
        ASTNode *factory = ast_program_statement(program, 1);
        ASTNode *main_decl = ast_program_statement(program, 2);
        ASTNode *hof = ast_return_value(ast_block_statement(ast_func_body(factory), 0));
        ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 1));
        ASTNode *target = ast_call_callee(hof);
        ASTNode *first = ast_call_argument(hof, 0);
        ASTNode *callback = ast_call_argument(hof, 1);
        PublishedNode *row = published_node_for(ctx, hof);
        const Type *result_type = NULL;
        CHECK(hof != NULL && hof->type == AST_CALL && target != NULL &&
            target->type == AST_IDENTIFIER && first != NULL && callback != NULL &&
            row != NULL && row->arguments != NULL && row->argument_count == 2);
        if (hof == NULL || target == NULL || first == NULL || callback == NULL ||
            row == NULL || row->arguments == NULL || row->argument_count != 2) goto cleanup;
        CHECK(semantic_array_storage_independent_physical_hof_result(hof, ctx));
        CHECK(semantic_param_publication_declared_result(ctx, call, &result_type) == factory &&
            result_type != NULL);
        CHECK(semantic_array_storage_function_result_owned(ctx, factory, result_type));
        CHECK(semantic_array_storage_owned_result_current(ctx, call));
        PublishedNode saved_row = *row;
        PublishedCallArgument saved_first = row->arguments[0], saved_callback = row->arguments[1];
        ASTNode saved_hof = *hof, saved_target = *target;
        uint32_t first_id = first->stable_id, callback_id = callback->stable_id;
        for (unsigned fault = 0; fault < 12; fault++) {
            if (fault == 0) row->value_present = false;
            if (fault == 1) row->stdlib = false;
            if (fault == 2) row->kind_present = false;
            if (fault == 3) row->builtin = BUILTIN_CLONE;
            if (fault == 4) target->data.identifier.name = "Other";
            if (fault == 5) target->stable_id++;
            if (fault == 6) hof->stable_id++;
            if (fault == 7) first->stable_id++;
            if (fault == 8) row->arguments[1].type = TYPE_INT;
            if (fault == 9) hof->data.call.arg_count = 1;
            if (fault == 10) row->arguments[0].type = TYPE_STRING;
            if (fault == 11) callback->stable_id++;
            CHECK(!semantic_array_storage_independent_physical_hof_result(hof, ctx));
            CHECK(!semantic_array_storage_function_result_owned(ctx, factory, result_type));
            CHECK(!semantic_array_storage_owned_result_current(ctx, call));
            *row = saved_row;
            row->arguments[0] = saved_first; row->arguments[1] = saved_callback;
            *hof = saved_hof; *target = saved_target;
            first->stable_id = first_id; callback->stable_id = callback_id;
            CHECK(semantic_array_storage_independent_physical_hof_result(hof, ctx));
            CHECK(semantic_array_storage_function_result_owned(ctx, factory, result_type));
            CHECK(semantic_array_storage_owned_result_current(ctx, call));
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_owned_array_result_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Factory(flag:Bool)->Array<Int>{if flag{return [1];}return [2];}"
        "func Main()->Void{let rows=Factory(true);ArrayDrop(rows);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted && !ctx->has_error);
    if (!admitted) goto cleanup;

    ASTNode *factory = ast_program_statement(program, 0);
    ASTNode *main_decl = ast_program_statement(program, 1);
    ASTNode *branch = ast_block_statement(ast_func_body(factory), 0);
    ASTNode *then_return = ast_block_statement(ast_if_then_branch(branch), 0);
    ASTNode *else_return = ast_block_statement(ast_func_body(factory), 1);
    ASTNode *then_value = ast_return_value(then_return);
    ASTNode *else_value = ast_return_value(else_return);
    ASTNode *header = ast_func_return_type(factory);
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 0));
    const Type *result_type = NULL;
    CHECK(then_value != NULL && else_value != NULL && header != NULL && call != NULL);
    if (then_value == NULL || else_value == NULL || header == NULL || call == NULL)
        goto cleanup;
    CHECK(semantic_param_publication_declared_result(ctx, call, &result_type) == factory
        && result_type != NULL);
    CHECK(semantic_array_storage_function_result_owned(ctx, factory, result_type));
    CHECK(semantic_array_storage_owned_result_current(ctx, call));

    PublishedNode *return_value = NULL;
    for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++)
        if (ctx->function_param_publications->nodes[i].node == then_value)
            return_value = &ctx->function_param_publications->nodes[i];
    CHECK(return_value != NULL && return_value->value_present);
    if (return_value != NULL) {
        PublishedNode saved = *return_value;
        ASTNode saved_return = *then_return;
        uint32_t selected_id = ast_call_semantic_callee_decl_id(call);
        uint32_t child_id = ast_node_stable_id(then_value);
        uint32_t header_id = ast_node_stable_id(header);
        size_t call_arity = ast_call_arg_count(call);
        for (unsigned fault = 0; fault < 8; fault++) {
            if (fault == 0) ast_call_set_semantic_callee_decl_id(call, 0);
            if (fault == 1) return_value->value_present = false;
            if (fault == 2) return_value->meta = true;
            if (fault == 3) return_value->value_type = TYPE_STRING;
            if (fault == 4) then_value->stable_id++;
            if (fault == 5) then_return->data.return_stmt.value = else_value;
            if (fault == 6) header->stable_id++;
            if (fault == 7) call->data.call.arg_count = 0;
            bool current = semantic_array_storage_owned_result_current(ctx, call);
            if (current)
                fprintf(stderr, "owned Array result fault=%u unexpectedly current\n", fault);
            CHECK(!current);
            *return_value = saved;
            *then_return = saved_return;
            then_value->stable_id = child_id;
            header->stable_id = header_id;
            call->data.call.arg_count = call_arity;
            CHECK(ast_call_set_semantic_callee_decl_id(call, selected_id));
            CHECK(semantic_array_storage_owned_result_current(ctx, call));
        }
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_owned_array_string_result_currency(void)
{
    Lexer *lexer = lexer_create(
        "func OwnedRows()->Array<String>{let rows:Array<String> = [];"
        "ArrayPushOwnedString(rows,Concat(\"owned\",\" value\"));return rows;}"
        "func Main()->Void{let rows=OwnedRows();ArrayDropOwnedStrings(rows);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted && !ctx->has_error);
    if (!admitted) goto cleanup;
    ASTNode *factory = ast_program_statement(program, 0);
    ASTNode *main_decl = ast_program_statement(program, 1);
    ASTNode *local = ast_block_statement(ast_func_body(factory), 0);
    ASTNode *return_stmt = ast_block_statement(ast_func_body(factory), 2);
    ASTNode *return_value = ast_return_value(return_stmt);
    ASTNode *header = ast_func_return_type(factory);
    ASTNode *call = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 0));
    PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(factory), ast_node_stable_id(local));
    CHECK(fact != NULL && return_value != NULL && header != NULL && call != NULL);
    if (fact == NULL || return_value == NULL || header == NULL || call == NULL) goto cleanup;
    CHECK(fact->return_elements_owned);
    CHECK(semantic_array_storage_owned_result_current(ctx, call));
    CHECK(semantic_array_storage_owned_string_result_current(ctx, call));
    PgyCollectionOwnershipFact saved = *fact;
    uint32_t return_id = return_value->stable_id, header_id = header->stable_id;
    uint32_t selected_id = ast_call_semantic_callee_decl_id(call);
    for (unsigned fault = 0; fault < 7; fault++) {
        if (fault == 0) fact->return_elements_owned = false;
        if (fault == 1) fact->return_elements_source_id++;
        if (fault == 2) fact->function_syntax_id++;
        if (fault == 3) return_value->stable_id++;
        if (fault == 4) header->stable_id++;
        if (fault == 5) CHECK(ast_call_set_semantic_callee_decl_id(call, 0));
        if (fault == 6) {
            fact->return_elements_source = call;
            fact->return_elements_source_id = ast_node_stable_id(call);
        }
        CHECK(!semantic_array_storage_owned_string_result_current(ctx, call));
        if (fault == 0) CHECK(semantic_array_storage_owned_result_current(ctx, call));
        *fact = saved;
        return_value->stable_id = return_id;
        header->stable_id = header_id;
        CHECK(ast_call_set_semantic_callee_decl_id(call, selected_id));
        CHECK(semantic_array_storage_owned_string_result_current(ctx, call));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_owned_array_builtin_element_currency(void)
{
    const char *sources[] = {
        "func Main()->Void{let source:Array<String> = [\"original\"];"
        "let rows=Clone(source);ArrayDropOwnedStrings(rows);}",
        "func Main()->Void{let map:HashMap<String,Int> = MapNew<String,Int>();"
        "MapSet(map,\"key\",1);let rows=MapKeys(map);ArrayDropOwnedStrings(rows);}",
    };
    for (unsigned route = 0; route < 2; route++) {
        Lexer *lexer = lexer_create(sources[route]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool admitted = parsed && type_check_program(program, ctx);
        CHECK(admitted && !ctx->has_error);
        if (!admitted) goto cleanup;
        ASTNode *main_decl = ast_program_statement(program, 0);
        ASTNode *local = ast_block_statement(ast_func_body(main_decl), route == 0 ? 1 : 2);
        ASTNode *drop = ast_block_statement(ast_func_body(main_decl), route == 0 ? 2 : 3);
        ASTNode *call = ast_let_initializer(local);
        ASTNode *receiver = ast_call_argument(drop, 0);
        ASTNode *target = ast_call_callee(call);
        PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
            semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(main_decl), ast_node_stable_id(local));
        CHECK(call != NULL && target != NULL && receiver != NULL && fact != NULL);
        if (call == NULL || target == NULL || receiver == NULL || fact == NULL) goto cleanup;
        ArrayStorageReturnElementReceipt receipt = {ast_node_stable_id(main_decl),
            ast_node_stable_id(local), true, call, ast_node_stable_id(call)};
        CHECK(fact->return_elements_owned && fact->return_elements_source == call
            && fact->return_elements_source_id == ast_node_stable_id(call));
        CHECK(semantic_array_storage_owned_result_current(ctx, call));
        CHECK(semantic_array_storage_owned_string_result_current(ctx, call));
        CHECK(semantic_array_storage_element_use_current(receiver, ctx, &receipt));
        PgyCollectionOwnershipFact saved_fact = *fact;
        for (unsigned fault = 0; fault < 2; fault++) {
            if (fault == 0) fact->return_elements_owned = false;
            else fact->return_elements_source_id++;
            CHECK(!semantic_array_storage_element_use_current(receiver, ctx, &receipt));
            *fact = saved_fact;
            CHECK(semantic_array_storage_element_use_current(receiver, ctx, &receipt));
        }
        PublishedNode *row = NULL;
        for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++)
            if (ctx->function_param_publications->nodes[i].node == call)
                row = &ctx->function_param_publications->nodes[i];
        CHECK(row != NULL);
        if (row != NULL) {
            PublishedNode saved_row = *row;
            ASTNode saved_call = *call;
            char *target_name = target->data.identifier.name;
            uint32_t target_id = target->stable_id;
            for (unsigned fault = 0; fault < 10; fault++) {
                if (fault == 0) CHECK(ast_call_set_semantic_callee_is_stdlib(call, false));
                if (fault == 1) CHECK(ast_call_set_semantic_callee_builtin_kind(call,
                    route == 0 ? BUILTIN_NOT_BUILTIN : BUILTIN_CLONE));
                if (fault == 2) CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(main_decl)));
                if (fault == 3) CHECK(ast_call_set_semantic_callee_value_binding_id(call, 1));
                if (fault == 4) target->data.identifier.name = "Impostor";
                if (fault == 5) target->stable_id++;
                if (fault == 6) call->stable_id++;
                if (fault == 7) row->stdlib = false;
                if (fault == 8) row->builtin = route == 0 ? BUILTIN_NOT_BUILTIN : BUILTIN_CLONE;
                if (fault == 9) {
                    CHECK(ast_call_set_semantic_callee_is_stdlib(call, false));
                    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(main_decl)));
                }
                CHECK(!semantic_array_storage_owned_string_result_current(ctx, call));
                *row = saved_row;
                *call = saved_call;
                target->data.identifier.name = target_name;
                target->stable_id = target_id;
                CHECK(semantic_array_storage_owned_string_result_current(ctx, call));
            }
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_owned_array_nested_return_currency(void)
{
    Lexer *lexer = lexer_create(
        "func OwnedRows()->Array<String>{let rows:Array<String> = [];"
        "ArrayPushOwnedString(rows,Concat(\"owned\",\" value\"));return rows;}"
        "func Forward()->Array<String>{return OwnedRows();}"
        "func Main()->Void{let rows=Forward();ArrayDropOwnedStrings(rows);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted && !ctx->has_error);
    if (!admitted) goto cleanup;
    ASTNode *owner = ast_program_statement(program, 0);
    ASTNode *forward = ast_program_statement(program, 1);
    ASTNode *main_decl = ast_program_statement(program, 2);
    ASTNode *forward_return = ast_block_statement(ast_func_body(forward), 0);
    ASTNode *inner = ast_return_value(forward_return);
    ASTNode *outer = ast_let_initializer(ast_block_statement(ast_func_body(main_decl), 0));
    ASTNode *forward_header = ast_func_return_type(forward);
    ASTNode *owner_header = ast_func_return_type(owner);
    CHECK(inner != NULL && outer != NULL && forward_header != NULL && owner_header != NULL);
    if (inner == NULL || outer == NULL || forward_header == NULL || owner_header == NULL)
        goto cleanup;
    CHECK(semantic_array_storage_owned_string_result_current(ctx, inner));
    CHECK(semantic_array_storage_owned_string_result_current(ctx, outer));
    ASTNode saved_return = *forward_return;
    uint32_t inner_id = inner->stable_id, forward_header_id = forward_header->stable_id;
    uint32_t owner_header_id = owner_header->stable_id;
    uint32_t callee_id = ast_call_semantic_callee_decl_id(inner);
    for (unsigned fault = 0; fault < 5; fault++) {
        if (fault == 0) inner->stable_id++;
        if (fault == 1) CHECK(ast_call_set_semantic_callee_decl_id(inner, 0));
        if (fault == 2) forward_return->data.return_stmt.value = outer;
        if (fault == 3) forward_header->stable_id++;
        if (fault == 4) owner_header->stable_id++;
        CHECK(!semantic_array_storage_owned_string_result_current(ctx, outer));
        *forward_return = saved_return;
        inner->stable_id = inner_id;
        forward_header->stable_id = forward_header_id;
        owner_header->stable_id = owner_header_id;
        CHECK(ast_call_set_semantic_callee_decl_id(inner, callee_id));
        CHECK(semantic_array_storage_owned_string_result_current(ctx, outer));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_array_birth_initializer_states(void)
{
    const char *sources[] = {
        "func Use()->Void{let rows:Array<Int> = [7];}",
        "func Factory()->Array<Int>{return [7];}func Use()->Void{let rows=Factory();}",
        "func Use(ref map:HashMap<String,Int>)->Void{let rows=MapKeys(map);}",
        "func Use()->Void{let source:Array<String> = [\"a\"];let rows=Clone(source);}"
    };
    for (size_t route = 0; route < 4; route++) {
        Lexer *lexer = lexer_create(sources[route]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool typed = parsed && type_check_program(program, ctx);
        if (!typed) {
            fprintf(stderr, "Array birth initializer route=%zu unexpectedly refused\n", route);
            for (size_t d = 0; d < ctx->diagnostic_count; d++)
                fprintf(stderr, "Array birth initializer diagnostic: %s\n", ctx->diagnostics[d]->message);
        }
        CHECK(typed);
        if (typed) {
            ASTNode *decl = ast_program_statement(program, route == 1 ? 1 : 0);
            ASTNode *local = ast_block_statement(ast_func_body(decl), route == 3 ? 1 : 0);
            ASTNode *initial = ast_let_initializer(local);
            SemanticPublishedValue value = semantic_param_publication_value(ctx, initial);
            CHECK(value.ok && type_is_constructed_named(value.type, "Array"));
            if (value.ok) {
                Symbol *binding = symbol_create_variable("rows", (Type *)value.type, 1, 1);
                CHECK(binding != NULL && binding->has_escaped_array_storage);
                if (binding != NULL) {
                    binding->decl_syntax_id = ast_node_stable_id(local);
                    ASTNode *saved_function = ctx->current_function_decl;
                    ctx->current_function_decl = decl;
                    semantic_array_storage_initialize(binding, initial, ctx);
                    CHECK(!binding->has_escaped_array_storage
                        && binding->has_exclusive_array_storage
                        && binding->has_pending_array_result == (route != 0)
                        && !binding->has_pending_inout_preservation);
                    if (route != 0)
                        CHECK(semantic_array_storage_owned_result_current(ctx, initial));
                    ctx->current_function_decl = saved_function;
                    symbol_destroy(binding);
                }
            }
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_array_birth_preserved_sources(void)
{
    const char *sources[] = {
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Forward(own rows:Array<Int>)->Int{return Consume(rows);}"
        "func Main()->Void{let rows:Array<Int> = [7];Log(Forward(rows));}",
        "func InspectLength(ref rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Mutate(inout rows:Array<Int>)->Void{ArrayPush(rows,2);}"
        "func Main()->Void{var rows:Array<Int> = [1];Log(InspectLength(rows));Mutate(rows);ArrayDrop(rows);}",
        "func Main()->Void{let rows:Array<String> = [];ArrayPushOwnedString(rows,Concat(\"a\",\"b\"));"
        "ArrayDropOwnedStrings(rows);}",
        "func Main()->Void{let map:HashMap<String,Int> = MapNew();MapSet(map,\"a\",1);"
        "let rows=MapKeys(map);let moved=rows;ArrayDropOwnedStrings(moved);}",
        "func Main()->Void{let borrowed:Array<String> = [\"a\"];let rows=Clone(borrowed);"
        "ArrayDropOwnedStrings(rows);}",
        /* Existing UNKNOWN call-return admission, not physical-result ownership proof. */
        "func Factory()->Array<Int>{return [7];}"
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let rows=Factory();Log(Consume(rows));}",
        /* Semantic positive only: the separate source gate records the existing
         * Unknown tuple initializer backend limitation rather than executing it. */
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Main()->Void{let(rows,marker)=([7],0);Log(Consume(rows));Log(marker);}"
    };
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        Lexer *lexer = lexer_create(sources[i]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        bool admitted = parsed && type_check_program(program, ctx);
        if (!admitted) {
            fprintf(stderr, "Array birth preserved source=%zu unexpectedly refused\n", i);
            for (size_t d = 0; d < ctx->diagnostic_count; d++)
                fprintf(stderr, "Array birth preserved diagnostic: %s\n", ctx->diagnostics[d]->message);
        }
        CHECK(admitted && !ctx->has_error);
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_owned_actual_final_currency_fault(unsigned fault)
{
    Lexer *lexer = lexer_create(
        "func Inspect(ref rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Consume(own rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Use()->Void{let rows:Array<Int> = [1];Inspect(rows);Consume(rows);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) goto cleanup;
    ASTNode *callee = ast_program_statement(program, 1);
    ASTNode *caller = ast_program_statement(program, 2);
    ASTNode *reader = ast_block_statement(ast_func_body(caller), 1);
    ASTNode *call = ast_block_statement(ast_func_body(caller), 2);
    ASTNode *argument = ast_call_argument(call, 0);
    FuncParam *formal = ast_func_param(callee, 0);
    PublishedNode *actual = NULL, *invocation = NULL;
    for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++) {
        PublishedNode *row = &ctx->function_param_publications->nodes[i];
        if (row->node == argument) actual = row;
        if (row->node == call) invocation = row;
    }
    CHECK(argument != NULL && formal != NULL && formal->type != NULL
        && actual != NULL && invocation != NULL);
    if (argument == NULL || formal == NULL || formal->type == NULL
        || actual == NULL || invocation == NULL) goto cleanup;
    CHECK(semantic_collection_owned_element_requirements_finalize(ctx));
    CHECK(ctx->diagnostic_count == 0);
    ASTNode saved_call = *call, saved_argument = *argument;
    ASTNode saved_type = *formal->type, foreign_argument = *argument, foreign_type = *formal->type;
    FuncParam saved_formal = *formal;
    PublishedNode saved_actual = *actual;
    uint32_t binding = ast_identifier_binding_syntax_id(argument);
    uint32_t reader_id = ast_node_stable_id(reader);
    {
        if (fault == 0) call->stable_id++;
        if (fault == 1) call->data.call.arguments[0] = &foreign_argument;
        if (fault == 2) ast_call_set_semantic_callee_decl_id(call, 0);
        if (fault == 3) formal->stable_id++;
        if (fault == 4) formal->mode = PARAM_MODE_REF;
        if (fault == 5) formal->type = &foreign_type;
        if (fault == 6) formal->type->stable_id++;
        if (fault == 7) actual->value_type = TYPE_INT;
        if (fault == 8) actual->value_present = false;
        if (fault == 9) ast_identifier_set_binding_syntax_id(argument, binding + 1);
        size_t diagnostics = ctx->diagnostic_count;
        bool current = semantic_collection_owned_element_requirements_finalize(ctx);
        if (current) fprintf(stderr, "Own actual final currency fault=%u not refused\n", fault);
        CHECK(!current && ctx->diagnostic_count > diagnostics);
        if (fault < 4) {
            CHECK(ast_node_stable_id(reader) == reader_id
                && ast_node_stable_id(argument) == saved_argument.stable_id
                && ast_identifier_binding_syntax_id(argument) == binding);
        }
        call->data.call.arguments[0] = argument;
        *call = saved_call; *argument = saved_argument;
        *formal = saved_formal; *formal->type = saved_type; *actual = saved_actual;
        ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(callee));
        ast_identifier_set_binding_syntax_id(argument, binding);
        CHECK(semantic_collection_owned_element_requirements_finalize(ctx));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_owned_actual_final_currency(void)
{
    for (unsigned fault = 0; fault < 10; fault++)
        checked_owned_actual_final_currency_fault(fault);
}

static void checked_collection_exact_replay(const char *source, bool birth, bool retired)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) goto cleanup;
    ASTNode *decl = ast_program_statement(program, 0);
    ASTNode *local = ast_block_statement(ast_func_body(decl), 0);
    ASTNode *initializer = ast_let_initializer(local);
    ASTNode *call = ast_block_statement(ast_func_body(decl), 1);
    ASTNode *receiver = ast_call_argument(call, 0);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, receiver);
    PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(decl), ast_node_stable_id(local));
    CHECK(value.ok && value.type != NULL && fact != NULL);
    if (!value.ok || fact == NULL) goto cleanup;
    Symbol symbol = {0};
    symbol.name = (char *)ast_identifier_name(receiver); symbol.kind = SYMBOL_VARIABLE;
    symbol.decl_syntax_id = ast_node_stable_id(local); symbol.type = (Type *)value.type;
    Symbol *symbols[] = {&symbol};
    Scope scope = {0}; scope.symbols = symbols; scope.symbol_count = 1;
    Scope *saved_scope = ctx->scope;
    ASTNode *saved_function = ctx->current_function_decl;
    ctx->scope = &scope; ctx->current_function_decl = decl;
    uint32_t kind = 0, binding = 0, seed = 0;
    CHECK(ast_call_semantic_collection_effect(call, &kind, &binding, &seed));
    CHECK(binding == symbol.decl_syntax_id && fact->disposition ==
        (retired ? PGY_COLLECTION_DISPOSITION_RETIRED : PGY_COLLECTION_DISPOSITION_LIVE));
    CHECK(!ast_call_set_semantic_collection_effect(call, kind, binding, seed));
    CHECK(semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    CHECK(!semantic_collection_record_call_effect_from(call, receiver,
        kind == PGY_COLLECTION_EFFECT_DROP ? PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
            : PGY_COLLECTION_EFFECT_DROP, seed, ctx));
    CHECK(!semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed + 1, ctx));
    uint32_t receiver_binding = ast_identifier_binding_syntax_id(receiver);
    ast_identifier_set_binding_syntax_id(receiver, receiver_binding + 1);
    CHECK(!semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    ast_identifier_set_binding_syntax_id(receiver, receiver_binding);
    symbol.decl_syntax_id++;
    CHECK(!semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    symbol.decl_syntax_id--;
    symbol.type = TYPE_INT;
    CHECK(!semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    symbol.type = (Type *)value.type;
    CHECK(semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    PgyCollectionOwnershipFact saved = *fact;
    fact->disposition = retired ? PGY_COLLECTION_DISPOSITION_LIVE : PGY_COLLECTION_DISPOSITION_RETIRED;
    CHECK(!semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    *fact = saved;
    CHECK(semantic_collection_record_call_effect_from(call, receiver,
        (PgyCollectionOwnershipEffectKind)kind, seed, ctx));
    if (birth) {
        size_t count = ctx->collection_ownership_fact_count;
        CHECK(semantic_collection_ownership_initialize_binding(&symbol, initializer, value.type, ctx));
        CHECK(ctx->collection_ownership_fact_count == count);
        for (unsigned fault = 0; fault < 5; fault++) {
            if (fault == 0) fact->origin_syntax_id++;
            if (fault == 1) fact->source_binding_syntax_id++;
            if (fault == 2) fact->element_ownership = PGY_STRING_ARRAY_BORROWED_ELEMENTS;
            if (fault == 3) fact->origin = PGY_COLLECTION_ORIGIN_CLONE;
            if (fault == 4) fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
            CHECK(!semantic_collection_ownership_initialize_binding(&symbol, initializer, value.type, ctx));
            CHECK(ctx->collection_ownership_fact_count == count);
            *fact = saved;
            CHECK(semantic_collection_ownership_initialize_binding(&symbol, initializer, value.type, ctx));
        }
        uint32_t initializer_id = initializer->stable_id;
        initializer->stable_id++;
        CHECK(!semantic_collection_ownership_initialize_binding(&symbol, initializer, value.type, ctx));
        initializer->stable_id = initializer_id;
    }
    ctx->scope = saved_scope; ctx->current_function_decl = saved_function;
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_collection_loop_replay(void)
{
    checked_collection_exact_replay(
        "func Use()->Void{let rows:Array<String> = [];"
        "ArrayPushOwnedString(rows,Concat(\"a\",\"b\"));}", true, false);
    checked_collection_exact_replay(
        "func Use()->Void{let rows:Array<String> = [];ArrayDropOwnedStrings(rows);}", true, false);
    checked_collection_exact_replay(
        "func Use(ref values:HashMap<String,Int>)->Void{let rows=MapKeys(values);"
        "ArrayDropOwnedStrings(rows);}", false, true);
}

static void checked_array_storage_pending_currency(const char *source, bool moved)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (!admitted) goto cleanup;
    ASTNode *callee = ast_program_statement(program, 0);
    ASTNode *caller = ast_program_statement(program, 1);
    ASTNode *local = ast_block_statement(ast_func_body(caller), 0);
    ASTNode *call = ast_block_statement(ast_func_body(caller), 1);
    ASTNode *argument = ast_call_argument(call, 0);
    uint32_t binding = ast_node_stable_id(local);
    size_t sequence = semantic_array_storage_pending_sequence(ctx);
    CHECK(sequence > 0 && binding != 0 && argument != NULL);
    CHECK(semantic_array_storage_pending_preserved(ctx, binding, sequence));
    if (moved) {
        ASTNode *destination = ast_block_statement(ast_func_body(caller), 2);
        const PgyCollectionOwnershipFact *from = semantic_collection_ownership_fact_find(
            ctx, ast_node_stable_id(caller), binding);
        const PgyCollectionOwnershipFact *to = semantic_collection_ownership_fact_find(
            ctx, ast_node_stable_id(caller), ast_node_stable_id(destination));
        CHECK(sequence >= 2 && destination != NULL && destination->type == AST_LET_DECL);
        CHECK(from != NULL && from->disposition == PGY_COLLECTION_DISPOSITION_RETIRED);
        CHECK(to != NULL && to->source_binding_syntax_id == binding
            && to->origin == PGY_COLLECTION_ORIGIN_BINDING
            && to->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT);
    }
    PublishedNode *actual = NULL, *invocation = NULL;
    for (size_t i = 0; i < ctx->function_param_publications->node_capacity; i++) {
        PublishedNode *row = &ctx->function_param_publications->nodes[i];
        if (row->node == argument) actual = row;
        if (row->node == call) invocation = row;
    }
    CHECK(actual != NULL && invocation != NULL && invocation->arguments != NULL);
    if (actual == NULL || invocation == NULL || invocation->arguments == NULL
        || argument == NULL) goto cleanup;
    ASTNode saved_call = *call, saved_argument = *argument, saved_callee = *callee;
    PublishedNode saved_actual = *actual, saved_invocation = *invocation;
    ASTNode foreign_argument = *argument;
    for (unsigned fault = 0; fault < 11; fault++) {
        if (fault == 0) call->stable_id++;
        if (fault == 1) callee->stable_id++;
        if (fault == 2) argument->stable_id++;
        if (fault == 3) ast_call_set_semantic_callee_decl_id(call, 0);
        if (fault == 4) ast_identifier_set_binding_syntax_id(argument, 0);
        if (fault == 5) actual->value_present = false;
        if (fault == 6) actual->value_type = TYPE_INT;
        if (fault == 7) invocation->call_present = false;
        if (fault == 8) invocation->declaration = 0;
        if (fault == 9) invocation->arguments[0].binding++;
        if (fault == 10) call->data.call.arguments[0] = &foreign_argument;
        CHECK(!semantic_array_storage_pending_preserved(ctx, binding, sequence));
        if (fault == 9) invocation->arguments[0].binding--;
        call->data.call.arguments[0] = argument;
        *call = saved_call; *argument = saved_argument; *callee = saved_callee;
        *actual = saved_actual; *invocation = saved_invocation;
        ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(callee));
        ast_identifier_set_binding_syntax_id(argument, binding);
        CHECK(semantic_array_storage_pending_preserved(ctx, binding, sequence));
    }
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    size_t index = SIZE_MAX;
    for (size_t i = 0; store != NULL && i < store->count; i++)
        if (store->entries[i].function_decl == callee && store->entries[i].param_index == 0)
            index = i;
    CHECK(index != SIZE_MAX && !store->solving);
    if (index == SIZE_MAX) goto cleanup;
    FunctionParamFlowSummaryEntry saved_entry = store->entries[index];
    CHECK(saved_entry.state == FUNCTION_PARAM_FLOW_COMPLETE
        && !saved_entry.array_storage_unproved && !saved_entry.analysis_unsupported);
    for (unsigned fault = 0; fault < 5; fault++) {
        if (fault == 0) store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        if (fault == 1) store->entries[index].analysis_unsupported = true;
        if (fault == 2) store->entries[index].array_storage_unproved = true;
        if (fault == 3) store->entries[index].output_dependencies_ready = false;
        if (fault == 4) store->entries[index].output_producer = FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED;
        bool preserved = semantic_array_storage_pending_preserved(ctx, binding, sequence);
        if (preserved)
            fprintf(stderr, "Array pending currency fault=%u saved producer=%u ready=%d state=%u unproved=%d\n",
                fault, (unsigned)saved_entry.output_producer,
                (int)saved_entry.output_dependencies_ready, (unsigned)saved_entry.state,
                (int)saved_entry.array_storage_unproved);
        CHECK(!preserved);
        store->entries[index] = saved_entry;
        CHECK(semantic_array_storage_pending_preserved(ctx, binding, sequence));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_array_storage_pending_release(void)
{
    checked_array_storage_pending_currency(
        "func Inspect(ref rows:Array<Int>)->Int{return ArrayLength(rows);}"
        "func Use()->Void{let rows:Array<Int> = [];Inspect(rows);ArrayDrop(rows);}", false);
    checked_array_storage_pending_currency(
        "func Inspect(ref rows:Array<String>)->Int{return ArrayLength(rows);}"
        "func Use(ref values:HashMap<String,Int>)->Void{let rows=MapKeys(values);"
        "Inspect(rows);ArrayDropOwnedStrings(rows);}", false);
    checked_array_storage_pending_currency(
        "func Inspect(rows:Array<String>)->Int{return ArrayLength(rows);}"
        "func Use(ref values:HashMap<String,Int>)->Void{let rows=MapKeys(values);"
        "Inspect(rows);let moved=rows;ArrayDropOwnedStrings(moved);}", true);
    /* Finalized call-proof currency only. The separate source negative owns
     * evidence that OwnedPush registered and consumed its pending prefix. */
    checked_array_storage_pending_currency(
        "func Inspect(ref rows:Array<String>)->Int{return ArrayLength(rows);}"
        "func Use(ref values:HashMap<String,Int>)->Void{let rows=MapKeys(values);"
        "Inspect(rows);ArrayPushOwnedString(rows,Concat(\"owned\",\" value\"));}", false);
}

static void checked_deferred_string_snapshot_currency(void)
{
    SemanticContext *ctx = semantic_context_create();
    Symbol first = {.name = "first", .kind = SYMBOL_VARIABLE, .type = TYPE_STRING,
        .decl_syntax_id = 11, .indexed_string_borrow_invalidated = false};
    Symbol second = {.name = "second", .kind = SYMBOL_VARIABLE, .type = TYPE_STRING,
        .decl_syntax_id = 12, .indexed_string_borrow_invalidated = true};
    Symbol *bindings[] = {&first, &second};
    Scope scope = {.kind = SCOPE_FUNCTION, .symbols = bindings, .symbol_count = 2};
    Scope *saved_scope = ctx->scope;
    ctx->scope = &scope;
    resource_flow_universe_begin(ctx);
    ResourceConsumeSnapshot snapshot = snapshot_resource_states(ctx);
    CHECK(snapshot.valid && resource_snapshot_capture_deferred_strings(&snapshot, ctx));
    CHECK(snapshot.deferred_strings_captured && snapshot.deferred_string_count == 2);
    if (!snapshot.deferred_strings_captured || snapshot.deferred_string_count != 2)
        goto cleanup;
    first.indexed_string_borrow_invalidated = true;
    second.indexed_string_borrow_invalidated = false;
    CHECK(resource_snapshot_restore_deferred_strings(&snapshot, ctx));
    CHECK(!first.indexed_string_borrow_invalidated && second.indexed_string_borrow_invalidated);
    DeferredStringInvalidationState saved = snapshot.deferred_strings[0];
    for (unsigned fault = 0; fault < 7; fault++) {
        first.indexed_string_borrow_invalidated = true;
        second.indexed_string_borrow_invalidated = false;
        if (fault == 0) snapshot.deferred_strings[0].symbol = &second;
        if (fault == 1) snapshot.deferred_strings[0].declaration_id = second.decl_syntax_id;
        if (fault == 2) snapshot.deferred_strings[0].declaration_id = 0;
        if (fault == 3) snapshot.deferred_strings[0].symbol = NULL;
        if (fault == 4) first.type = TYPE_INT;
        if (fault == 5) snapshot.deferred_string_count++;
        if (fault == 6) snapshot.deferred_strings_captured = false;
        CHECK(!resource_snapshot_restore_deferred_strings(&snapshot, ctx));
        CHECK(first.indexed_string_borrow_invalidated && !second.indexed_string_borrow_invalidated);
        snapshot.deferred_strings[0] = saved;
        first.type = TYPE_STRING;
        snapshot.deferred_string_count = 2;
        snapshot.deferred_strings_captured = true;
        CHECK(resource_snapshot_restore_deferred_strings(&snapshot, ctx));
        CHECK(!first.indexed_string_borrow_invalidated && second.indexed_string_borrow_invalidated);
    }
cleanup:
    destroy_resource_snapshot(&snapshot);
    resource_flow_universe_end(ctx);
    ctx->scope = saved_scope;
    semantic_context_destroy(ctx);
}

static void checked_deferred_row_source_currency(bool owner_release)
{
    const char *source = owner_release
        ? "func Main()->Void{var rows:Array<String> = [];let other:Array<String> = [];"
          "defer{ArrayDropOwnedStrings(rows);};rows=other;}"
        : "func Main()->Void{var text:String=\"seed\";let other:String=\"value\";"
          "defer{Log(text);};text=other;}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    Scope *saved_scope = ctx->scope;
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    if (!parsed) goto cleanup;
    CHECK(ast_assign_stable_ids(program));
    ASTNode *routine = ast_program_statement(program, 0);
    ASTNode *first_decl = ast_block_statement(ast_func_body(routine), 0);
    ASTNode *second_decl = ast_block_statement(ast_func_body(routine), 1);
    ASTNode *deferred = ast_block_statement(ast_func_body(routine), 2);
    ASTNode *body = ast_defer_body(deferred);
    ASTNode *call = ast_block_statement(body, 0);
    ASTNode *use = ast_call_argument(call, 0);
    ASTNode *assignment = ast_block_statement(ast_func_body(routine), 3);
    ASTNode *value = ast_assignment_value(assignment);
    ASTNode *target = ast_assignment_target(assignment);
    uint32_t first_id = ast_node_stable_id(first_decl);
    uint32_t second_id = ast_node_stable_id(second_decl);
    CHECK(use != NULL && value != NULL && target != NULL && first_id != 0 && second_id != 0);
    if (use == NULL || value == NULL || target == NULL || first_id == 0 || second_id == 0)
        goto cleanup;
    Type *elements[] = {TYPE_STRING};
    Type *array_type = type_create_constructed(TYPE_ARRAY, elements, 1);
    Type *value_type = owner_release ? array_type : TYPE_STRING;
    Symbol first = {.name = owner_release ? "rows" : "text", .kind = SYMBOL_VARIABLE,
        .type = value_type, .decl_syntax_id = first_id,
        .collection_storage_source_syntax_id = owner_release ? first_id : 0};
    Symbol second = {.name = "other", .kind = SYMBOL_VARIABLE,
        .type = value_type, .decl_syntax_id = second_id,
        .collection_storage_source_syntax_id = owner_release ? second_id : 0,
        .indexed_string_borrow_source_syntax_id = owner_release ? 0 : second_id};
    Symbol *bindings[] = {&first, &second};
    Scope scope = {.kind = SCOPE_FUNCTION, .symbols = bindings, .symbol_count = 2};
    ctx->scope = &scope;
    ctx->program_root = program;
    ctx->current_function_decl = routine;
    ctx->indexed_string_next_defer_sequence = 1;
    ast_identifier_set_binding_syntax_id(use, first_id);
    ast_identifier_set_binding_syntax_id(value, second_id);
    ast_identifier_set_binding_syntax_id(target, first_id);
    CHECK(semantic_param_publication_begin(ctx));
    CHECK(semantic_param_publication_record_value(ctx, use, value_type, false));
    CHECK(semantic_param_publication_record_value(ctx, value, value_type, false));
    CHECK(semantic_indexed_string_borrow_publish_source(use, ctx));
    CHECK(semantic_indexed_string_borrow_publish_source(value, ctx));
    uint32_t initial = owner_release ? first_id : 0;
    IndexedStringDeferredRead row = {.function = routine, .body = body, .use = use,
        .bound = use, .function_id = ast_node_stable_id(routine),
        .body_id = ast_node_stable_id(body), .use_id = ast_node_stable_id(use),
        .use_kind = (uint32_t)use->type, .bound_id = ast_node_stable_id(use),
        .binding_id = first_id, .source_id = initial, .issued_source_id = initial,
        .sequence = 1, .owner_release = owner_release};
    CHECK(semantic_array_storage_deferred_read_current(&row, ctx));
    CHECK(semantic_array_storage_deferred_read_assignment(&row, assignment, value, second_id, ctx));
    CHECK(row.assignment_count == 1
        && row.source_id == (owner_release ? UINT32_MAX : second_id));
    if (row.assignment_count != 1) goto release;
    IndexedStringDeferredRead saved = row;
    IndexedStringDeferredAssignment saved_event = row.assignments[0];
    uint32_t use_id = ast_node_stable_id(use);
    for (unsigned fault = 0; fault < 14; fault++) {
        if (fault == 0) row.issued_source_id = owner_release ? 0 : second_id;
        if (fault == 1) row.issued_source_id = UINT32_MAX;
        if (fault == 2) row.source_id = 0;
        if (fault == 3) row.source_id = first_id;
        if (fault == 4) row.assignments[0].source_id = 0;
        if (fault == 5) row.assignments[0].source_id = first_id;
        if (fault == 6) row.assignments[0].value_id++;
        if (fault == 7) row.assignments[0].binding_id = second_id;
        if (fault == 8) row.assignment_count = 0;
        if (fault == 9) row.assignments = NULL;
        if (fault == 10) use->stable_id++;
        if (fault == 11) row.assignments[0].target_id++;
        if (fault == 12) row.assignments[0].ordinal++;
        if (fault == 13) row.assignments[0].receipt_sequence++;
        CHECK(!semantic_array_storage_deferred_read_current(&row, ctx));
        row = saved;
        row.assignments[0] = saved_event;
        use->stable_id = use_id;
        CHECK(semantic_array_storage_deferred_read_current(&row, ctx));
    }
release:
    free(row.assignments);
cleanup:
    ctx->scope = saved_scope;
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_array_storage_path_snapshots(void)
{
    SemanticContext *ctx = semantic_context_create();
    Type *element[] = {TYPE_INT};
    Type *array = type_create_constructed(TYPE_ARRAY, element, 1);
    Symbol values[10] = {0};
    Symbol *bindings[10];
    char names[10][16];
    Scope scope = {0};
    Scope *saved_scope = ctx->scope;
    for (size_t i = 0; i < 10; i++) {
        snprintf(names[i], sizeof(names[i]), "path_rows_%zu", i);
        values[i].name = names[i]; values[i].kind = SYMBOL_VARIABLE;
        values[i].type = array; values[i].decl_syntax_id = (uint32_t)i + 1;
        values[i].has_exclusive_array_storage = true;
        bindings[i] = &values[i];
    }
    /* Existing ownership classification tracks both local and formal Arrays. */
    values[9].is_parameter = true;
    scope.symbols = bindings; scope.symbol_count = 10;
    ctx->scope = &scope;
    resource_flow_universe_begin(ctx);
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    fprintf(stderr, "Array path-state snapshot valid=%d count=%zu capacity=%zu\n",
        (int)base.valid, base.count, base.capacity);
    CHECK(base.valid && base.count == 10 && base.capacity >= 10);
    CHECK(base.array_storage_states != NULL);
    if (!base.valid || base.count != 10 || base.array_storage_states == NULL)
        goto cleanup;
    ResourceConsumeSnapshot copy = copy_resource_snapshot(&base);
    CHECK(copy.valid && copy.array_storage_states != base.array_storage_states);
    if (!copy.valid || copy.array_storage_states == NULL)
        goto cleanup;
    CHECK(resource_snapshots_equal(&base, &copy));
    CHECK(resource_snapshot_availability_equal(&base, &copy));
    for (size_t bit = 0; bit < 4; bit++) {
        copy.array_storage_states[0] = base.array_storage_states[0];
        if (bit == 0) copy.array_storage_states[0].exclusive = false;
        if (bit == 1) copy.array_storage_states[0].escaped = true;
        if (bit == 2) copy.array_storage_states[0].pending = true;
        if (bit == 3) copy.array_storage_states[0].result_pending = true;
        CHECK(!resource_snapshots_equal(&base, &copy));
        CHECK(!resource_snapshot_availability_equal(&base, &copy));
    }
    copy.array_storage_states[0] = (ArrayStoragePathState){false, true, true, true};
    ASTNode loop = {0};
    loop.type = AST_WHILE_LOOP;
    FlowFlags replayed = FLOW_NONE;
    loop_flow_summary_begin_function(ctx);
    loop_flow_summary_record(ctx, &loop, &base, &copy, 0, 0, FLOW_FALLTHROUGH);
    CHECK(loop_flow_summary_try_apply(ctx, &loop, &base, 0, &replayed));
    CHECK(replayed == FLOW_FALLTHROUGH && !values[0].has_exclusive_array_storage
        && values[0].has_escaped_array_storage
        && values[0].has_pending_inout_preservation
        && values[0].has_pending_array_result);
    for (size_t bit = 0; bit < 4; bit++) {
        ArrayStoragePathState saved = base.array_storage_states[0];
        if (bit == 0) base.array_storage_states[0].exclusive = false;
        if (bit == 1) base.array_storage_states[0].escaped = true;
        if (bit == 2) base.array_storage_states[0].pending = true;
        if (bit == 3) base.array_storage_states[0].result_pending = true;
        CHECK(!loop_flow_summary_try_apply(ctx, &loop, &base, 0, &replayed));
        base.array_storage_states[0] = saved;
    }
    loop_flow_summary_end_function(ctx);
    for (size_t parallel = 0; parallel < 2; parallel++) {
        ResourceConsumeSnapshot joined = copy_resource_snapshot(&base);
        if (parallel) merge_resource_states_parallel(&joined, &copy);
        else merge_resource_states_or(&joined, &copy);
        CHECK(joined.valid && !joined.array_storage_states[0].exclusive
            && joined.array_storage_states[0].escaped
            && joined.array_storage_states[0].pending
            && joined.array_storage_states[0].result_pending);
        destroy_resource_snapshot(&joined);
    }
    values[0].has_exclusive_array_storage = false;
    values[0].has_escaped_array_storage = true;
    values[0].has_pending_inout_preservation = true;
    values[0].has_pending_array_result = true;
    restore_resource_states(&base);
    CHECK(values[0].has_exclusive_array_storage
        && !values[0].has_escaped_array_storage
        && !values[0].has_pending_inout_preservation
        && !values[0].has_pending_array_result);
    Symbol replacement = values[0];
    replacement.has_exclusive_array_storage = false;
    replacement.has_escaped_array_storage = true;
    replacement.has_pending_inout_preservation = true;
    replacement.has_pending_array_result = true;
    bindings[0] = &replacement;
    restore_resource_states_for_context(&base, ctx);
    CHECK(replacement.has_exclusive_array_storage
        && !replacement.has_escaped_array_storage
        && !replacement.has_pending_inout_preservation
        && !replacement.has_pending_array_result);
    CHECK(base.symbols[0] == &values[0]);
    CHECK(base.symbol_indices[0] != RESOURCE_FLOW_INDEX_NONE);
    ArrayStoragePathState *saved_column = copy.array_storage_states;
    copy.array_storage_states = NULL;
    CHECK(!resource_snapshots_equal(&base, &copy));
    CHECK(!resource_snapshot_availability_equal(&base, &copy));
    ResourceConsumeSnapshot refused = copy_resource_snapshot(&copy);
    CHECK(!refused.valid);
    destroy_resource_snapshot(&refused);
    ResourceConsumeSnapshot joined = copy_resource_snapshot(&base);
    merge_resource_states_or(&joined, &copy);
    CHECK(!joined.valid);
    destroy_resource_snapshot(&joined);
    joined = copy_resource_snapshot(&base);
    merge_resource_states_parallel(&joined, &copy);
    CHECK(!joined.valid);
    destroy_resource_snapshot(&joined);
    restore_resource_states(&copy);
    CHECK(!values[0].has_exclusive_array_storage
        && values[0].has_escaped_array_storage
        && values[0].has_pending_inout_preservation
        && values[0].has_pending_array_result);
    restore_resource_states_for_context(&copy, ctx);
    CHECK(!replacement.has_exclusive_array_storage
        && replacement.has_escaped_array_storage
        && replacement.has_pending_inout_preservation
        && replacement.has_pending_array_result);
    CHECK(!values[9].has_exclusive_array_storage
        && values[9].has_escaped_array_storage
        && values[9].has_pending_inout_preservation);
    copy.array_storage_states = saved_column;
    bindings[0] = &values[0];
    destroy_resource_snapshot(&copy);
cleanup:
    destroy_resource_snapshot(&base);
    resource_flow_universe_end(ctx);
    ctx->scope = saved_scope;
    semantic_context_destroy(ctx);
}

static void checked_array_storage_live_arm(const char *source, size_t push_statement)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (admitted) {
        ASTNode *decl = ast_program_statement(program, 1);
        ASTNode *push = ast_block_statement(ast_func_body(decl), push_statement);
        CHECK(push != NULL && push->type == AST_CALL);
        bool private_push = semantic_param_publication_private_array_push(ctx, push, decl);
        if (private_push)
            fprintf(stderr, "Array path-state unexpected live-arm private grant: %s\n", source);
        CHECK(!private_push);
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_array_storage_live_paths(void)
{
    checked_array_storage_live_arm(
        "struct Rows{items:Array<Int>}func Build(flag:Bool)->Rows{"
        "let rows:Array<Int> = [];if flag{let held=Rows(rows);}"
        "ArrayPush(rows,1);return Rows(rows);}", 2);
    checked_array_storage_live_arm(
        "struct Rows{items:Array<Int>}func Build(flag:Bool)->Rows{"
        "let rows:Array<Int> = [];if flag{return Rows(rows);}else{let held=Rows(rows);}"
        "ArrayPush(rows,1);return Rows(rows);}", 2);
    checked_array_storage_live_arm(
        "struct Rows{items:Array<Int>}func Build(flag:Bool)->Rows{"
        "let rows:Array<Int> = [];while flag{let held=Rows(rows);break;}"
        "ArrayPush(rows,1);return Rows(rows);}", 2);
    checked_array_storage_live_arm(
        "struct Rows{items:Array<Int>}func Build(flag:Bool)->Rows{"
        "let rows:Array<Int> = [];var go=flag;while go{let held=Rows(rows);go=false;continue;}"
        "ArrayPush(rows,1);return Rows(rows);}", 3);
    checked_array_storage_live_arm(
        "struct Rows{items:Array<Int>}func Build(flag:Bool)->Rows{"
        "let rows:Array<Int> = [];for i in 0..2{let held=Rows(rows);continue;}"
        "ArrayPush(rows,1);return Rows(rows);}", 2);
    checked_array_storage_live_arm(
        "struct Rows{items:Array<Int>}func Build(flag:Bool)->Rows{"
        "let rows:Array<Int> = [];if ArrayLength(Rows(rows).items)>0{}"
        "ArrayPush(rows,1);return Rows(rows);}", 2);
}

static void checked_private_array_push_terminating_arm(void)
{
    Lexer *lexer = lexer_create(
        "struct Rows{items:Array<Int>}"
        "func Build(flag:Bool)->Rows{let rows:Array<Int> = [];"
        "if flag{return Rows(rows);}ArrayPush(rows,1);return Rows(rows);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool admitted = parsed && type_check_program(program, ctx);
    CHECK(admitted);
    if (admitted) {
        ASTNode *decl = ast_program_statement(program, 1);
        ASTNode *push = ast_block_statement(ast_func_body(decl), 2);
        CHECK(push != NULL && push->type == AST_CALL);
        bool private_push = semantic_param_publication_private_array_push(ctx, push, decl);
        if (!private_push)
            fprintf(stderr, "Array path-state retained terminating-arm escape before private push\n");
        CHECK(private_push);
        CHECK(function_param_flow_summary_for_param(ctx, decl, 0) == 0);
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
        "extern \"C\"{func Hold(rows:Array<String>)->Void;}func Build(input:Array<String>)->Int{let owned_rows:Array<String> = [];Hold(owned_rows);ArrayPush(owned_rows,input[0]);return ArrayLength(owned_rows);}", 1, 2, false);
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

/* Compare the two production collector modes on actual typed nested calls.
 * A returned scalar must not hide effects of evaluating its arguments. */
static void checked_nested_slot_effect_facets(void)
{
    const char *sources[] = {
        "func Effect(ref slot:Slot<Int>)->Int{return Read(slot);}func Probe(ref slot:Slot<Int>)->Void{if Effect(slot)>0{Log(1);}}",
        "func Effect(ref slot:Slot<Int>)->Bool{Write(slot,7);return true;}func Probe(ref slot:Slot<Int>)->Void{if Effect(slot)==true{Log(1);}}",
        /* Release belongs to a lexical Slot binding, not an owned formal.
         * Its valid nested statement baseline is separate from own-call escape. */
        "func Effect()->Bool{return true;}func Probe()->Void{let slot:Slot<Int> =ClaimSlot();if true{Release(slot);}}",
        /* Ordinary returned String loans are admitted for a read-only outer
         * consumer; anchored Slot return refusal is a separate boundary. */
        "func Effect(ref slot:String)->String{return slot;}func Probe(ref slot:String)->Void{Log(Effect(slot));}",
        "func Effect(own slot:Slot<Int>)->Bool{return true;}func Probe(ref slot:Slot<Int>)->Void{if Effect(slot)==true{Log(1);}}",
        /* A ref Slot cannot be sent by a helper at all. Use a lexical owned
         * source so this facet tests nested channel evaluation, not rejection
         * before the wrapper's typed publication exists. */
        "func Effect()->Bool{return true;}func Probe()->Void{let slot:Slot<Int> =ClaimSlot();let channel:Channel<Slot<Int>> =Channel(1);if true{channel<-slot;}}"
    };
    const unsigned expected_access[] = {SLOT_ACCESS_READ, SLOT_ACCESS_WRITE, SLOT_ACCESS_RELEASE, 0, 0, 0};
    const unsigned expected_escape[] = {0, 0, 0, SLOT_ESCAPE_RETURN, SLOT_ESCAPE_CALL, SLOT_ESCAPE_CHANNEL};
    for (size_t effect = 0; effect < sizeof(sources) / sizeof(sources[0]); effect++) {
        Lexer *lexer = lexer_create(sources[effect]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        if (program == NULL || parser_has_error(parser)) goto cleanup;
        bool typed = type_check_program(program, ctx);
        if (typed != (effect != 4))
            fprintf(stderr, "nested slot facet baseline effect=%zu typed=%d source=%s\n", effect, typed, sources[effect]);
        CHECK(typed == (effect != 4));
        if (effect == 4) CHECK(context_has_diagnostic(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE));
        CHECK(semantic_param_publication_inputs_ready(ctx));
        ASTNode *probe = ast_program_statement(program, 1);
        ASTNode *body = ast_func_body(probe);
        SlotFunctionLookup lookup = {ctx, program};
        SlotSummaryOrigin origin = {.function_decl = probe, .param_index = 0, .param_name = "slot"};
        SlotAccessEntry *accesses = NULL;
        SlotEscapeEntry *escapes = NULL;
        size_t ac = 0, az = 0, ec = 0, ez = 0;
        bool failed = false;
        bool access_ready = collect_slot_accesses(body, &accesses, &ac, &az, &lookup, &failed);
        if (!access_ready || failed)
            fprintf(stderr, "nested slot facet access effect=%zu ready=%d failed=%d source=%s\n", effect, access_ready, failed, sources[effect]);
        CHECK(access_ready);
        CHECK(!failed);
        unsigned full_access = 0, full_escape = 0;
        for (size_t i = 0; i < ac; i++)
            if (strcmp(accesses[i].name, "slot") == 0) full_access |= accesses[i].mask;
        failed = false;
        bool escape_ready = collect_slot_escapes(body, &escapes, &ec, &ez, &lookup, &origin, NULL, &failed);
        if (!escape_ready || failed)
            fprintf(stderr, "nested slot facet escape effect=%zu ready=%d failed=%d source=%s\n", effect, escape_ready, failed, sources[effect]);
        CHECK(escape_ready);
        CHECK(!failed);
        for (size_t i = 0; i < ec; i++)
            if (strcmp(escapes[i].name, "slot") == 0) full_escape |= escapes[i].mask;
        if (full_access != expected_access[effect] || full_escape != expected_escape[effect])
            fprintf(stderr, "nested slot facet projection effect=%zu access=%u expected-access=%u escape=%u expected-escape=%u source=%s\n",
                effect, full_access, expected_access[effect], full_escape, expected_escape[effect], sources[effect]);
        CHECK(full_access == expected_access[effect]);
        CHECK(full_escape == expected_escape[effect]);
        CHECK(slot_access_mask_for_named_symbol(body, "slot", &lookup) == full_access);
        failed = false;
        CHECK(slot_escape_mask_in_program(body, "slot", &lookup, &origin, &failed) == full_escape && !failed);
        unsigned summary = slot_param_summary_in_program_points(&body, 1, "slot", &lookup, &origin);
        unsigned expected = 0;
        if ((expected_access[effect] & SLOT_ACCESS_READ) != 0) expected |= SLOT_PARAM_SUMMARY_READ;
        if ((expected_access[effect] & SLOT_ACCESS_WRITE) != 0) expected |= SLOT_PARAM_SUMMARY_WRITE;
        if ((expected_access[effect] & SLOT_ACCESS_RELEASE) != 0) expected |= SLOT_PARAM_SUMMARY_RELEASE;
        if ((expected_escape[effect] & SLOT_ESCAPE_RETURN) != 0) expected |= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
        if ((expected_escape[effect] & SLOT_ESCAPE_CALL) != 0) expected |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
        if ((expected_escape[effect] & SLOT_ESCAPE_CHANNEL) != 0) expected |= SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
        CHECK(summary == expected);
        free(accesses); free(escapes);
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

/* Mutations keep the parent node alive: original topology and publication
 * must still own its current evaluation children, including zero-bit reads. */
static void checked_wrapper_child_currency(void)
{
    const char *sources[] = {
        "func Probe(ref slot:Slot<Int>)->Bool{return Read(slot)>0;}",
        "func Probe(ref slot:Slot<Int>)->Int{let values:Array<Int> =[Read(slot),0];return ArrayLength(values);}",
        "func Probe(ref slot:Slot<Int>)->Bool{return Read(slot) is Int;}",
        "func Probe(ref slot:Slot<Int>)->Void{match Some(Read(slot)){case Some(value):Log(value);case None:Log(0);}}",
        "func Probe(ref slot:Slot<Int>)->Void{match 1{case 1 if Read(slot)>0:Log(1);default:Log(0);}}",
        /* Direct owner/HIR behavior only: MIR emission for a general runtime
         * call pattern is a separate, currently unsupported artifact route. */
        "func Probe(ref slot:Slot<Int>)->Void{match 1{case Read(slot):Log(1);default:Log(0);}}",
        "func Probe(ref slot:Slot<Int>)->Void{transaction{compensate Log(Read(slot));fail \"rollback\";}}"
    };
    for (size_t shape = 0; shape < sizeof(sources) / sizeof(sources[0]); shape++) {
        Lexer *lexer = lexer_create(sources[shape]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        if (program == NULL || parser_has_error(parser)) goto cleanup;
        CHECK(type_check_program(program, ctx));
        if (ctx->has_error) goto cleanup;
        ASTNode *probe = ast_program_statement(program, 0);
        ASTNode *statement = ast_block_statement(ast_func_body(probe), 0);
        ASTNode *parent = shape == 0 || shape == 2 ? ast_return_value(statement)
            : shape == 1 ? ast_let_initializer(statement) : statement;
        ASTNode **edge = shape == 0 ? &parent->data.binary.left
            : shape == 1 ? &parent->data.array_literal.elements[0]
            : shape == 2 ? &parent->data.type_test.operand
            : shape == 3 ? &parent->data.match_stmt.subject
            : shape == 4 ? &ast_match_case_at(parent, 0)->data.match_case.guard
            : shape == 5 ? &ast_match_case_at(parent, 0)->data.match_case.pattern
            : &parent->data.transaction_block.compensations[0];
        ASTNode *saved_child = *edge;
        FunctionParamFlowProgramPointIndex *index = NULL, *rechecked = NULL;
        unsigned baseline = function_param_flow_summary_for_param(ctx, probe, 0);
        CHECK(baseline == SLOT_PARAM_SUMMARY_READ);
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, probe, &index));
        if (index == NULL) goto cleanup;
        CHECK(function_param_flow_index_claims_current(ctx, index));
        SlotFunctionLookup lookup = {ctx, program};
        CHECK(slot_access_mask_for_named_symbol(ast_func_body(probe), "slot", &lookup) == SLOT_ACCESS_READ);
        SlotAccessEntry *accesses = NULL;
        size_t count = 0, capacity = 0;
        bool failed = false;
        CHECK(collect_slot_accesses(ast_func_body(probe), &accesses, &count, &capacity, &lookup, &failed) && !failed);
        unsigned full = 0;
        for (size_t i = 0; i < count; i++)
            if (strcmp(accesses[i].name, "slot") == 0) full |= accesses[i].mask;
        CHECK(full == SLOT_ACCESS_READ);
        free(accesses);
        *edge = NULL;
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        CHECK(!function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, probe, &rechecked));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
        *edge = saved_child;
        CHECK(function_param_flow_index_claims_current(ctx, index));
        CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
        uint32_t saved_id = saved_child->stable_id;
        saved_child->stable_id = 0;
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        saved_child->stable_id = saved_id;
        CHECK(function_param_flow_index_claims_current(ctx, index));
        ASTNodeType saved_shape = parent->type;
        parent->type = AST_NUMBER;
        CHECK(!function_param_flow_index_claims_current(ctx, index));
        parent->type = saved_shape;
        CHECK(function_param_flow_index_claims_current(ctx, index));
        if (shape == 0 || shape == 1) {
            ASTNode *other = shape == 0 ? ast_binary_right(parent) : ast_array_literal_element(parent, 1);
            *edge = other;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
            *edge = saved_child;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
        }
        if (shape == 1) {
            parent->data.array_literal.count--;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            parent->data.array_literal.count++;
            CHECK(function_param_flow_index_claims_current(ctx, index));
        }
        if (shape >= 3 && shape <= 5) {
            ASTNode *arm = ast_match_case_at(parent, 0);
            ASTNode *pattern = arm->data.match_case.pattern;
            arm->data.match_case.pattern = NULL;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            arm->data.match_case.pattern = pattern;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            size_t count = parent->data.match_stmt.case_count;
            parent->data.match_stmt.case_count--;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            parent->data.match_stmt.case_count = count;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            size_t pattern_count = arm->data.match_case.pattern_count;
            arm->data.match_case.pattern_count = 0;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            arm->data.match_case.pattern_count = pattern_count;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            ASTNode *alternative = arm->data.match_case.patterns[0];
            arm->data.match_case.patterns[0] = NULL;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            arm->data.match_case.patterns[0] = alternative;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            int family = arm->data.match_case.semantic_subject_family;
            arm->data.match_case.semantic_subject_family = family + 1;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            arm->data.match_case.semantic_subject_family = family;
            CHECK(function_param_flow_index_claims_current(ctx, index));
        }
        if (shape == 2) {
            char *type_name = parent->data.type_test.target_type;
            parent->data.type_test.target_type = "Bool";
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            parent->data.type_test.target_type = type_name;
            CHECK(function_param_flow_index_claims_current(ctx, index));
        }
        if (shape == 6) {
            size_t compensation_count = parent->data.transaction_block.compensation_count;
            parent->data.transaction_block.compensation_count = 0;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
            parent->data.transaction_block.compensation_count = compensation_count;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
        }
        PublishedNode *publication = NULL;
        FunctionParamPublicationStore *store = ctx->function_param_publications;
        for (size_t i = 0; store != NULL && i < store->node_capacity; i++)
            if (store->nodes[i].node == saved_child) publication = &store->nodes[i];
        CHECK(publication != NULL && publication->value_present);
        if (publication != NULL) {
            bool present = publication->value_present;
            publication->value_present = false;
            CHECK(!function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
            publication->value_present = present;
            CHECK(function_param_flow_index_claims_current(ctx, index));
            CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
        }
        ASTNode *read = shape == 3 || shape == 6 ? ast_call_argument(saved_child, 0)
            : shape == 4 ? ast_binary_left(saved_child) : saved_child;
        CHECK(read != NULL && read->type == AST_CALL);
        if (read != NULL && read->type == AST_CALL) {
            ASTNode *argument = ast_call_argument(read, 0), *target = ast_call_callee(read);
            uint32_t binding = ast_identifier_binding_syntax_id(argument);
            uint32_t target_id = target->stable_id, callee_id = read->data.call.semantic_callee_decl_id;
            for (unsigned fault = 0; fault < 3; fault++) {
                if (fault == 0) ast_identifier_set_binding_syntax_id(argument, binding + 1);
                if (fault == 1) target->stable_id++;
                if (fault == 2) read->data.call.semantic_callee_decl_id++;
                CHECK(!function_param_flow_index_claims_current(ctx, index));
                CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == SLOT_PARAM_SUMMARY_ALL);
                ast_identifier_set_binding_syntax_id(argument, binding);
                target->stable_id = target_id;
                read->data.call.semantic_callee_decl_id = callee_id;
                CHECK(function_param_flow_index_claims_current(ctx, index));
                CHECK(function_param_flow_summary_for_param(ctx, probe, 0) == baseline);
            }
        }
        CHECK(function_param_flow_program_point_index_build(ctx->function_param_flow_summaries, probe, &rechecked) && rechecked == index);
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_deferred_capture_not_eager(void)
{
    /* Raw owner protocol: an unsupported lambda is not an eager execution
     * container. No typed or source-admission claim is made by this probe. */
    Lexer *lexer = lexer_create("func Probe(ref slot:Slot<Int>)->Void{let deferred:func()->Int=()=>Read(slot);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    if (program != NULL && !parser_has_error(parser)) {
        CHECK(ast_assign_stable_ids(program));
        ASTNode *lambda = ast_let_initializer(ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 0));
        CHECK(lambda != NULL && lambda->type == AST_LAMBDA_EXPR);
        CHECK(slot_access_mask_for_named_symbol(lambda, "slot", NULL) ==
            (SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE));
        SlotAccessEntry *accesses = NULL;
        SlotEscapeEntry *escapes = NULL;
        size_t count = 0, capacity = 0;
        bool failed = false;
        CHECK(!collect_slot_accesses(lambda, &accesses, &count, &capacity, NULL, &failed) && failed);
        free(accesses);
        count = 0; capacity = 0; failed = false;
        CHECK(!collect_slot_escapes(lambda, &escapes, &count, &capacity, NULL, NULL, NULL, &failed) && failed);
        free(escapes);
        failed = false;
        CHECK(slot_escape_mask_in_program(lambda, "slot", NULL, NULL, &failed) ==
            (SLOT_ESCAPE_RETURN | SLOT_ESCAPE_CALL | SLOT_ESCAPE_CHANNEL) && failed);
    }
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_deferred_publication_currency(void)
{
    const char *sources[] = {
        "func Main()->Void{let value:Int=7;let delayed:func()->Int=()=>value;Log(delayed());}",
        "func Probe(text:String)->Void{let delayed:func()->String=()=>text;}func Main()->Void{}",
        "func Main()->Void{async{}}"
    };
    for (size_t kind = 0; kind < sizeof(sources) / sizeof(sources[0]); kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        CHECK(program != NULL && !parser_has_error(parser));
        if (program == NULL || parser_has_error(parser)) goto cleanup;
        bool typed = type_check_program(program, ctx);
        if (!typed) fprintf(stderr, "deferred receipt baseline kind=%zu source=%s\n", kind, sources[kind]);
        CHECK(typed);
        if (!typed) goto cleanup;
        ASTNode *routine = ast_program_statement(program, 0);
        ASTNode *statement = ast_block_statement(ast_func_body(routine), kind == 0 ? 1 : 0);
        ASTNode *boundary = kind == 2 ? statement : ast_let_initializer(statement);
        SemanticPublishedDeferred view;
        bool ready = semantic_param_publication_query_deferred(ctx, boundary, &view);
        if (!ready) fprintf(stderr, "deferred receipt query baseline kind=%zu source=%s\n", kind, sources[kind]);
        CHECK(ready);
        if (!ready) goto cleanup;
        CHECK(view.capture_count == (kind == 2 ? 0u : 1u));
        SlotFunctionLookup lookup = {ctx, program};
        SlotSummaryOrigin origin = {.function_decl = routine, .param_index = 0, .param_name = "text"};
        bool failed = false;
        unsigned expected = kind == 1 ? SLOT_ESCAPE_CALL : SLOT_ESCAPE_NONE;
        CHECK(slot_escape_mask_in_program(boundary, kind == 0 ? "value" : "text", &lookup, &origin, &failed) == expected && !failed);
        /* Creating the capture reads its source; executing the deferred body
         * is not part of this access query. String capture still retains CALL. */
        CHECK(slot_access_mask_for_named_symbol(boundary, kind == 0 ? "value" : "text", &lookup) ==
            (kind == 2 ? 0u : SLOT_ACCESS_READ));
        if (kind == 1)
            CHECK((function_param_flow_summary_for_param(ctx, routine, 0) & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0);
        PublishedNode *row = NULL;
        FunctionParamPublicationStore *store = ctx->function_param_publications;
        for (size_t i = 0; store != NULL && i < store->node_capacity; i++)
            if (store->nodes[i].node == boundary) row = &store->nodes[i];
        CHECK(row != NULL);
        if (row == NULL) goto cleanup;
        PublishedNode saved = *row;
        for (unsigned fault = 0; fault < (kind == 2 ? 4u : 6u); fault++) {
            if (fault == 0) row->deferred_scan = false;
            if (fault == 1) row->deferred_checked = false;
            if (fault == 2) row->deferred_shape = AST_NUMBER;
            if (fault == 3) row->deferred_syntax = NULL;
            if (fault == 4) row->value_present = false;
            if (fault == 5) row->deferred_capture_count = 0;
            bool accepted = semantic_param_publication_query_deferred(ctx, boundary, &view);
            if (accepted) fprintf(stderr, "deferred receipt fault accepted kind=%zu fault=%u\n", kind, fault);
            CHECK(!accepted);
            failed = false;
            CHECK(slot_escape_mask_in_program(boundary, kind == 0 ? "value" : "text", &lookup, &origin, &failed) ==
                (SLOT_ESCAPE_RETURN | SLOT_ESCAPE_CALL | SLOT_ESCAPE_CHANNEL) && failed);
            *row = saved;
            CHECK(semantic_param_publication_query_deferred(ctx, boundary, &view));
            failed = false;
            CHECK(slot_escape_mask_in_program(boundary, kind == 0 ? "value" : "text", &lookup, &origin, &failed) == expected && !failed);
        }
        if (kind != 2 && row->deferred_capture_count == 1) {
            PublishedDeferredCapture *capture = &row->deferred_captures[0];
            PublishedDeferredCapture original = *capture;
            ASTNode *source = (ASTNode *)capture->source;
            uint32_t source_id = source->stable_id, binding = ast_identifier_binding_syntax_id(source);
            PublishedNode *source_row = NULL;
            for (size_t i = 0; i < store->node_capacity; i++)
                if (store->nodes[i].node == source) source_row = &store->nodes[i];
            CHECK(source_row != NULL && source_row->value_present);
            for (unsigned fault = 0; fault < 16; fault++) {
                if (fault == 15 && source_row == NULL) continue;
                if (fault == 0) capture->source_id++;
                if (fault == 1) capture->binding_id++;
                if (fault == 2) capture->mode = LAMBDA_CAPTURE_REF;
                if (fault == 3) capture->type = TYPE_BOOL;
                if (fault == 4) capture->copy_escape = !capture->copy_escape;
                if (fault == 5) capture->category++;
                if (fault == 6) capture->name = "foreign";
                if (fault == 7) capture->type_name = "Bool";
                if (fault == 8) capture->source = NULL;
                if (fault == 9) source->stable_id = 0;
                if (fault == 10) ast_identifier_set_binding_syntax_id(source, binding + 1);
                if (fault == 11) row->deferred_captures = NULL;
                if (fault == 12) store->program_id++;
                if (fault == 13) capture->type = NULL;
                if (fault == 14) capture->type_name = NULL;
                if (fault == 15) source_row->value_present = false;
                bool accepted = semantic_param_publication_query_deferred(ctx, boundary, &view);
                if (accepted) fprintf(stderr, "deferred capture fault accepted kind=%zu fault=%u\n", kind, fault);
                CHECK(!accepted);
                *capture = original;
                source->stable_id = source_id;
                ast_identifier_set_binding_syntax_id(source, binding);
                row->deferred_captures = saved.deferred_captures;
                store->program_id = ast_node_stable_id(program);
                if (source_row != NULL) source_row->value_present = true;
                CHECK(semantic_param_publication_query_deferred(ctx, boundary, &view));
            }
        }
cleanup:
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_transaction_compensation_identity(void)
{
    Lexer *lexer = lexer_create(
        "func Main()->Void{transaction{compensate Log(Concat(\"roll\",\"back\"));fail \"x\";}}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    CHECK(program != NULL && !parser_has_error(parser));
    if (program != NULL && !parser_has_error(parser)) {
        CHECK(ast_assign_stable_ids(program));
        ASTNode *transaction = ast_block_statement(ast_func_body(ast_program_statement(program, 0)), 0);
        ASTNode *handler = transaction->data.transaction_block.compensations[0];
        ASTNode *nested = ast_call_argument(handler, 0);
        ASTNode *nodes[] = {transaction, ast_transaction_block_body(transaction),
            handler, ast_call_callee(handler), nested, ast_call_callee(nested),
            ast_call_argument(nested, 0), ast_call_argument(nested, 1)};
        uint32_t original[sizeof(nodes) / sizeof(nodes[0])];
        for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
            original[i] = ast_node_stable_id(nodes[i]);
            CHECK(original[i] != 0);
            for (size_t j = 0; j < i; j++) CHECK(original[i] != original[j]);
        }
        CHECK(ast_complete_stable_ids(program));
        for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++)
            CHECK(ast_node_stable_id(nodes[i]) == original[i]);
        CHECK(ast_assign_stable_ids(program));
        for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++)
            CHECK(ast_node_stable_id(nodes[i]) == original[i]);
    }
    ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_transaction_failure_states(void)
{
    const struct {
        const char *name, *source, *error;
    } cases[] = {
        {"literal failure", "func Main()->Void{transaction{compensate Log(\"rollback\");fail \"x\";}Log(\"after\");}", NULL},
        {"dead owned handler restores normal", "func Take(own slot:Slot<Int>)->Void{}func Main()->Void{let slot:Slot<Int> =ClaimSlot();transaction{compensate Take(slot);}Log(Read(slot));}", NULL},
        {"unreachable failure is not an input", "func Take(own slot:Slot<Int>)->Void{}func Main()->Void{let slot:Slot<Int> =ClaimSlot();transaction{compensate Take(slot);if false{fail \"x\";}}Log(Read(slot));}", NULL},
        {"body-local not visible", "func Main()->Void{transaction{let hidden:Int=7;compensate Log(hidden);fail \"x\";}}", PGY_CODE_SEM_UNDEFINED_SYMBOL},
        {"outer identity survives body scope", "func Main()->Void{let text:String=\"outer\";transaction{let inner:Int=7;compensate Log(text);fail \"x\";}}", NULL},
        {"consumed failure input", "func Take(own slot:Slot<Int>)->Void{}func Main()->Void{let slot:Slot<Int> =ClaimSlot();transaction{compensate Log(Read(slot));Take(slot);fail \"x\";}}", PGY_CODE_SEM_SLOT_RELEASED},
        {"failure alternatives joined", "func Take(own slot:Slot<Int>)->Void{}func Probe(condition:Bool)->Void{let slot:Slot<Int> =ClaimSlot();transaction{compensate Log(Read(slot));if condition{Take(slot);fail \"a\";}else{fail \"b\";}}}func Main()->Void{}", PGY_CODE_SEM_SLOT_RELEASED},
        {"failure is not routine return", "func Probe()->Int{transaction{compensate Log(\"rollback\");fail \"x\";}}func Main()->Void{}", PGY_CODE_SEM_MISSING_RETURN},
        {"return bypasses failure", "func Probe()->Void{transaction{compensate Log(\"rollback\");return;}}func Main()->Void{}", NULL},
        {"while failure reaches transaction", "func Main()->Void{transaction{compensate Log(\"rollback\");while true{fail \"x\";}}Log(\"after\");}", NULL},
        {"for failure reaches transaction", "func Main()->Void{transaction{compensate Log(\"rollback\");for i in 0..1{fail \"x\";}}Log(\"after\");}", NULL},
        {"nested failure is local", "func Main()->Void{transaction{compensate Log(\"outer\");transaction{compensate Log(\"inner\");fail \"inner\";}Log(\"continued\");fail \"outer\";}}", NULL},
        {"reverse handler state", "func Take(own slot:Slot<Int>)->Void{}func Main()->Void{let slot:Slot<Int> =ClaimSlot();transaction{compensate Log(Read(slot));compensate Take(slot);fail \"x\";}}", PGY_CODE_SEM_SLOT_RELEASED},
        {"failure poststate joins normal", "func Take(own slot:Slot<Int>)->Void{}func Probe(condition:Bool)->Void{let slot:Slot<Int> =ClaimSlot();transaction{compensate Take(slot);if condition{fail \"x\";}}Log(Read(slot));}func Main()->Void{}", PGY_CODE_SEM_SLOT_RELEASED},
        {"lambda cannot fail caller transaction", "func Main()->Void{transaction{let delayed:func()->Void=()=>{fail \"not caller\";};fail \"outer\";}}", PGY_CODE_SEM_TRANSACTION_CONTROL_INVALID},
        {"lambda internal transaction", "func Main()->Void{transaction{let delayed:func()->Void=()=>{transaction{compensate Log(\"inner\");fail \"inner\";}};fail \"outer\";}}", NULL},
        {"defer cannot fail caller transaction", "func Main()->Void{transaction{defer{fail \"not caller\";};fail \"outer\";}}", PGY_CODE_SEM_TRANSACTION_CONTROL_INVALID},
        {"defer internal transaction", "func Main()->Void{transaction{defer{transaction{compensate Log(\"inner\");fail \"inner\";}};fail \"outer\";}}", NULL},
        {"async cannot fail caller transaction", "func Main()->Void{transaction{async{fail \"not caller\";}fail \"outer\";}}", PGY_CODE_SEM_TRANSACTION_CONTROL_INVALID},
        {"async internal transaction", "func Main()->Void{transaction{async{transaction{compensate Log(\"inner\");fail \"inner\";}}fail \"outer\";}}", NULL},
        {"task cannot fail caller transaction", "func Main()->Void{transaction{parallel{{fail \"not caller\";}{Log(1);}}fail \"outer\";}}", PGY_CODE_SEM_TRANSACTION_CONTROL_INVALID},
        {"task internal transaction", "func Main()->Void{transaction{parallel{{transaction{compensate Log(\"inner\");fail \"inner\";}}{Log(1);}}fail \"outer\";}}", NULL},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Lexer *lexer = lexer_create(cases[i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        bool parsed = program != NULL && !parser_has_error(parser);
        if (!parsed) fprintf(stderr, "transaction fixture parse failed: %s\n", cases[i].name);
        CHECK(parsed);
        if (parsed) {
            bool typed = type_check_program(program, ctx);
            const Diagnostic *first = NULL;
            for (size_t d = 0; d < ctx->diagnostic_count; d++)
                if (ctx->diagnostics[d]->level == DIAG_ERROR && first == NULL)
                    first = ctx->diagnostics[d];
            bool expected = cases[i].error == NULL ? typed :
                !typed && first != NULL && first->code != NULL && strcmp(first->code, cases[i].error) == 0;
            if (!expected) {
                fprintf(stderr, "transaction fixture %s typed=%d first=%s message=%s\n",
                    cases[i].name, typed, first == NULL || first->code == NULL ? "<none>" : first->code,
                    first == NULL || first->message == NULL ? "<none>" : first->message);
            }
            CHECK(expected);
            CHECK(ctx->transaction_flow == NULL && ctx->transaction_depth == 0);
        }
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

static void checked_transaction_function_boundary(void)
{
    /* Exercise the existing named-function owner with an active caller frame.
     * Routine-local function declarations are not a supported flow-dispatch
     * route, so their source admission is deliberately not claimed here. */
    const char *sources[] = {
        "func Worker()->Void{fail \"not caller\";}func Main()->Void{}",
        "func Worker()->Void{transaction{compensate Log(\"inner\");fail \"inner\";}}func Main()->Void{}"
    };
    for (size_t kind = 0; kind < sizeof(sources) / sizeof(sources[0]); kind++) {
        Lexer *lexer = lexer_create(sources[kind]);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TransactionFlowContext caller = {.outer_scope = ctx->scope};
        ctx->transaction_flow = &caller;
        ctx->transaction_depth = 1;
        bool parsed = program != NULL && !parser_has_error(parser);
        CHECK(parsed);
        if (parsed) {
            bool typed = type_check_program(program, ctx);
            const Diagnostic *first = NULL;
            for (size_t d = 0; d < ctx->diagnostic_count; d++)
                if (ctx->diagnostics[d]->level == DIAG_ERROR && first == NULL)
                    first = ctx->diagnostics[d];
            bool expected = kind == 1 ? typed : !typed && first != NULL && first->code != NULL &&
                strcmp(first->code, PGY_CODE_SEM_TRANSACTION_CONTROL_INVALID) == 0;
            if (!expected) fprintf(stderr, "named transaction boundary kind=%zu typed=%d first=%s\n",
                kind, typed, first == NULL || first->code == NULL ? "<none>" : first->code);
            CHECK(expected);
            CHECK(ctx->transaction_flow == &caller && ctx->transaction_depth == 1);
            CHECK(!caller.has_failures);
        }
        ctx->transaction_flow = NULL;
        ctx->transaction_depth = 0;
        destroy_resource_snapshot(&caller.failures);
        semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
    }
}

/* A typed projection path and a nonlocal-input proof are distinct receipts.
 * Neither makes the indexed String payload owned or source-independent. */
static void checked_nonlocal_projection_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct TextNode{type_name:String;}struct Arena{atom_table:Array<String>;}"
        "func Project(nodes:Array<TextNode>)->Arena{"
        "let atom_table:Array<String> = [\"\"];let i:Int=0;"
        "while i<ArrayLength(nodes){if nodes[i].type_name!=\"\"{"
        "ArrayPush(atom_table,nodes[i].type_name);}i=i+1;}"
        "return Arena(atom_table);}"
        "func Main()->Void{let name=Concat(\"Type\",\"Name\");"
        "let nodes:Array<TextNode> = [TextNode(name)];let arena=Project(nodes);"
        "Log(arena.atom_table[1]);Log(nodes[0].type_name);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *text_node = ast_program_statement(program, 0);
    ASTNode *project = ast_program_statement(program, 2);
    ASTNode *loop = ast_block_statement(ast_func_body(project), 2);
    ASTNode *branch = loop != NULL ? ast_block_statement(ast_while_body(loop), 0) : NULL;
    ASTNode *push = branch != NULL ? ast_block_statement(ast_if_then_branch(branch), 0) : NULL;
    ASTNode *member = push != NULL ? ast_call_argument(push, 1) : NULL;
    ASTNode *array = member != NULL ? ast_member_object(member) : NULL;
    ASTNode *root = array != NULL ? ast_array_access_array(array) : NULL;
    ASTNode *index = array != NULL ? ast_array_access_index(array) : NULL;
    size_t field_count = 0;
    ClassField **fields = text_node != NULL ? ast_class_fields(text_node, &field_count) : NULL;
    CHECK(member != NULL && member->type == AST_MEMBER_ACCESS &&
        array != NULL && array->type == AST_ARRAY_ACCESS &&
        root != NULL && root->type == AST_IDENTIFIER &&
        index != NULL && index->type == AST_IDENTIFIER &&
        fields != NULL && field_count == 1 && fields[0] != NULL);
    if (member == NULL || array == NULL || root == NULL || index == NULL ||
        fields == NULL || field_count != 1 || fields[0] == NULL) goto cleanup;
    ASTNode *caller = NULL, *found_root = NULL;
    CHECK(semantic_param_publication_projection_root_current(ctx, member,
        &caller, &found_root) && caller == project && found_root == root);
    CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
    uint32_t source_id = 0;
    CHECK(semantic_indexed_string_borrow_source_current(member, ctx, &source_id) &&
        source_id == UINT32_MAX);
    uint32_t owned_producer = 0;
    CHECK(!semantic_collection_owned_string_call_result(member, ctx, &owned_producer));
    SemanticPublishedValue value = semantic_param_publication_value(ctx, member);
    CHECK(value.ok && !value.meta && type_equals(value.type, TYPE_STRING));
    uint32_t saved_id = member->stable_id;
    member->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    member->stable_id = saved_id;
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    saved_id = root->stable_id;
    root->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    root->stable_id = saved_id;
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    uint32_t saved_binding = ast_identifier_binding_syntax_id(root);
    ast_identifier_set_binding_syntax_id(root, 0);
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    ast_identifier_set_binding_syntax_id(root, saved_binding);
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    saved_id = index->stable_id;
    index->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    index->stable_id = saved_id;
    saved_binding = ast_identifier_binding_syntax_id(index);
    ast_identifier_set_binding_syntax_id(index, 0);
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    ast_identifier_set_binding_syntax_id(index, saved_binding);
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    char *saved_name = member->data.member.name;
    member->data.member.name = "other";
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    member->data.member.name = saved_name;
    saved_name = fields[0]->name;
    fields[0]->name = "other";
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    fields[0]->name = saved_name;
    saved_id = fields[0]->stable_id;
    fields[0]->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    fields[0]->stable_id = saved_id;
    ASTNode *field_type = fields[0]->type;
    saved_id = field_type->stable_id;
    field_type->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    field_type->stable_id = saved_id;
    saved_name = field_type->data.type.name;
    field_type->data.type.name = "Int";
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    field_type->data.type.name = saved_name;
    ASTNode *saved_receiver = member->data.member.object;
    member->data.member.object = NULL;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    member->data.member.object = saved_receiver;
    ASTNode *saved_index = array->data.array_access.index;
    array->data.array_access.index = NULL;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    array->data.array_access.index = saved_index;
    PublishedNode *member_row = published_node_for(ctx, member);
    CHECK(member_row != NULL && member_row->projection.present);
    if (member_row != NULL) {
        const Type *field_type_ref = member_row->projection.field_type;
        member_row->projection.field_type = TYPE_INT;
        CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
        member_row->projection.field_type = field_type_ref;
        uint32_t field_id = member_row->projection.field_id;
        member_row->projection.field_id++;
        CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
        member_row->projection.field_id = field_id;
    }
    CHECK(semantic_param_publication_projection_root_current(ctx, member,
        &caller, &found_root) && caller == project && found_root == root);
    CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    FunctionParamFlowSummaryEntry *entry = NULL;
    for (size_t n = 0; flow != NULL && n < flow->count; n++)
        if (flow->entries[n].function_decl == project && flow->entries[n].param_index == 0)
            entry = &flow->entries[n];
    CHECK(entry != NULL && entry->state == FUNCTION_PARAM_FLOW_COMPLETE);
    unsigned original_mask = entry != NULL ? entry->mask : 0;
    CHECK(flow != NULL && function_param_flow_no_store_graph(flow, project));
    if (entry != NULL) {
        FunctionParamFlowSummaryState state = entry->state;
        entry->state = FUNCTION_PARAM_FLOW_COMPUTING;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        entry->state = state;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        unsigned forbidden[] = {SLOT_PARAM_SUMMARY_WRITE,
            SLOT_PARAM_SUMMARY_CALL_ESCAPE, (unsigned)SLOT_PARAM_SUMMARY_ALL + 1u};
        for (size_t f = 0; f < sizeof(forbidden) / sizeof(forbidden[0]); f++) {
            entry->mask = original_mask | forbidden[f];
            CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
            entry->mask = original_mask;
            CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        }
    }
    FunctionParamFlowProgramPointIndex *point = NULL;
    CHECK(flow != NULL && function_param_flow_program_point_index_build(flow, project, &point));
    CHECK(point != NULL && point->backing_frames != NULL &&
        point->backing_frame_count != 0 && point->backing_frames[0] != NULL);
    if (point != NULL && point->backing_frames != NULL &&
        point->backing_frame_count != 0 && point->backing_frames[0] != NULL) {
        FunctionParamFlowBackingFact *frame = point->backing_frames[0];
        CHECK(frame->epoch != 0 && frame->private_stores_closed);
        size_t epoch = frame->epoch;
        frame->epoch = 0;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->epoch = epoch;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        bool private_stores_closed = frame->private_stores_closed;
        frame->private_stores_closed = false;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->private_stores_closed = private_stores_closed;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        bool complete = frame->complete;
        frame->complete = false;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->complete = complete;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        bool pending = frame->pending;
        frame->pending = true;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->pending = pending;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        uint32_t input_id = frame->input_id;
        frame->input_id++;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->input_id = input_id;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        size_t input_ordinal = frame->input_ordinal;
        frame->input_ordinal = SIZE_MAX;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->input_ordinal = input_ordinal;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        FunctionParamFlowIndexClaims *claims = frame->claims;
        frame->claims = NULL;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        frame->claims = claims;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
        uint32_t body_id = point->dependency_body_id;
        point->dependency_body_id++;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        CHECK(!function_param_flow_no_store_graph(flow, project));
        point->dependency_body_id = body_id;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
    }
    ASTNode *push_receiver = ast_call_argument(push, 0);
    CHECK(push_receiver != NULL && push_receiver->type == AST_IDENTIFIER);
    if (push_receiver != NULL && push_receiver->type == AST_IDENTIFIER) {
        uint32_t receiver_binding = ast_identifier_binding_syntax_id(push_receiver);
        unsigned mask = entry != NULL ? entry->mask : 0;
        bool storage_unproved = entry != NULL && entry->array_storage_unproved;
        ast_identifier_set_binding_syntax_id(push_receiver,
            ast_func_param_stable_id(ast_func_param(project, 0)));
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        CHECK(entry == NULL || (entry->mask == mask &&
            entry->array_storage_unproved == storage_unproved));
        ast_identifier_set_binding_syntax_id(push_receiver, receiver_binding);
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
    }
    ASTNode *push_target = ast_call_callee(push);
    CHECK(push_target != NULL && push_target->type == AST_IDENTIFIER);
    if (push_target != NULL && push_target->type == AST_IDENTIFIER) {
        char *target_name = push_target->data.identifier.name;
        push_target->data.identifier.name = "Unknown";
        CHECK(!function_param_flow_nonlocal_input_current(ctx, project, 0));
        push_target->data.identifier.name = target_name;
        CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
    }
    CHECK(semantic_indexed_string_borrow_source_current(member, ctx, &source_id) &&
        source_id == UINT32_MAX);
    CHECK(!semantic_collection_owned_string_call_result(member, ctx, &owned_producer));
    SemanticPublishedValue restored = semantic_param_publication_value(ctx, member);
    CHECK(restored.ok && restored.origin_ready == value.origin_ready &&
        restored.copy_escape == value.copy_escape);
    CHECK(entry != NULL && entry->mask == original_mask);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_compound_projection_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct TextNode{type_name:String;}struct Arena{atom_table:Array<String>;}"
        "func Project(nodes:Array<TextNode>)->Arena{"
        "let atom_table:Array<String> = [\"\"];let i:Int=0;"
        "while i<ArrayLength(nodes){if nodes[i+0].type_name!=\"\"{"
        "ArrayPush(atom_table,nodes[i+0].type_name);}i=i+1;}"
        "return Arena(atom_table);}"
        "func Main()->Void{let name=Concat(\"Type\",\"Name\");"
        "let nodes:Array<TextNode> = [TextNode(name)];let arena=Project(nodes);"
        "Log(arena.atom_table[1]);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *project = ast_program_statement(program, 2);
    ASTNode *loop = ast_block_statement(ast_func_body(project), 2);
    ASTNode *branch = loop != NULL ? ast_block_statement(ast_while_body(loop), 0) : NULL;
    ASTNode *push = branch != NULL ? ast_block_statement(ast_if_then_branch(branch), 0) : NULL;
    ASTNode *member = push != NULL ? ast_call_argument(push, 1) : NULL;
    ASTNode *array = member != NULL ? ast_member_object(member) : NULL;
    ASTNode *index = array != NULL ? ast_array_access_index(array) : NULL;
    ASTNode *left = index != NULL ? ast_binary_left(index) : NULL;
    ASTNode *right = index != NULL ? ast_binary_right(index) : NULL;
    ASTNode *root = array != NULL ? ast_array_access_array(array) : NULL;
    CHECK(member != NULL && member->type == AST_MEMBER_ACCESS &&
        index != NULL && index->type == AST_BINARY &&
        left != NULL && left->type == AST_IDENTIFIER &&
        right != NULL && right->type == AST_NUMBER && root != NULL);
    if (member == NULL || index == NULL || left == NULL || right == NULL || root == NULL)
        goto cleanup;
    ASTNode *caller = NULL, *found_root = NULL;
    CHECK(semantic_param_publication_projection_root_current(ctx, member,
        &caller, &found_root) && caller == project && found_root == root);
    CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
    uint32_t source_id = 0;
    CHECK(semantic_indexed_string_borrow_source_current(member, ctx, &source_id) &&
        source_id == UINT32_MAX);
    uint32_t child_id = right->stable_id;
    right->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    right->stable_id = child_id;
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    child_id = left->stable_id;
    left->stable_id++;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    left->stable_id = child_id;
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    ASTNode *saved_right = index->data.binary.right;
    index->data.binary.right = left;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    index->data.binary.right = saved_right;
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    PgyTokenType op = index->data.binary.op.type;
    index->data.binary.op.type = TOKEN_MINUS;
    CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    index->data.binary.op.type = op;
    CHECK(semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
    PublishedNode *index_row = published_node_for(ctx, index);
    CHECK(index_row != NULL);
    if (index_row != NULL) {
        const Type *index_type = index_row->value_type;
        index_row->value_type = TYPE_STRING;
        CHECK(!semantic_param_publication_projection_root_current(ctx, member, &caller, &found_root));
        index_row->value_type = index_type;
    }
    CHECK(semantic_param_publication_projection_root_current(ctx, member,
        &caller, &found_root) && caller == project && found_root == root);
    CHECK(function_param_flow_nonlocal_input_current(ctx, project, 0));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_physical_array_mutation_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct Rows{ids:Array<Int>;}"
        "func ParentRows()->Rows{let ids:Array<Int> = [1,2];"
        "ArraySet(ids,0,3);ArrayPop(ids);return Rows(ids);}"
        "func Main()->Void{let rows=ParentRows();Log(rows.ids[0]);"
        "Log(ArrayLength(rows.ids));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *parent = ast_program_statement(program, 1);
    ASTNode *main_decl = ast_program_statement(program, 2);
    ASTNode *local = ast_block_statement(ast_func_body(parent), 0);
    ASTNode *initial = ast_let_initializer(local);
    ASTNode *calls[] = {ast_block_statement(ast_func_body(parent), 1),
        ast_block_statement(ast_func_body(parent), 2)};
    CHECK(local != NULL && initial != NULL && initial->type == AST_ARRAY_LITERAL &&
        calls[0] != NULL && calls[0]->type == AST_CALL &&
        calls[1] != NULL && calls[1]->type == AST_CALL);
    if (local == NULL || initial == NULL || calls[0] == NULL || calls[1] == NULL)
        goto cleanup;
    for (size_t kind = 0; kind < 2; kind++) {
        ASTNode *call = calls[kind];
        ASTNode *receiver = ast_call_argument(call, 0);
        ASTNode *target = ast_call_callee(call);
        PublishedNode *row = published_node_for(ctx, call);
        CHECK(receiver != NULL && target != NULL && row != NULL &&
            row->private_physical_mutation && row->deferred_syntax != NULL &&
            row->argument_count == (kind == 0 ? 3u : 1u));
        if (receiver == NULL || target == NULL || row == NULL) continue;
        CHECK(semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
        CHECK(!semantic_param_publication_private_physical_array_mutation(ctx, call, main_decl));
        PublishedNode saved = *row;
        uint32_t receiver_binding = ast_identifier_binding_syntax_id(receiver);
        char *target_name = target->data.identifier.name;
        bool alias = ast_let_is_alias(local);
        for (unsigned fault = 0; fault < 12; fault++) {
            if (fault == 0) row->private_physical_mutation = false;
            if (fault == 1) row->private_exclusive = false;
            if (fault == 2) row->private_escaped = true;
            if (fault == 3) row->private_pending = true;
            if (fault == 4) row->private_binding_id++;
            if (fault == 5) row->private_binding_type_id++;
            if (fault == 6) row->private_initializer_id++;
            if (fault == 7) row->deferred_syntax = NULL;
            if (fault == 8) row->caller_id++;
            if (fault == 9) ast_identifier_set_binding_syntax_id(receiver, 0);
            if (fault == 10) target->data.identifier.name = "Unknown";
            if (fault == 11) local->data.let_decl.is_alias = true;
            CHECK(!semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
            *row = saved;
            ast_identifier_set_binding_syntax_id(receiver, receiver_binding);
            target->data.identifier.name = target_name;
            local->data.let_decl.is_alias = alias;
            CHECK(semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
        }
        if (kind == 0) {
            ASTNode *index = ast_call_argument(call, 1);
            ASTNode *payload = ast_call_argument(call, 2);
            PublishedNode *index_row = published_node_for(ctx, index);
            PublishedNode *payload_row = published_node_for(ctx, payload);
            CHECK(index != NULL && payload != NULL && index_row != NULL && payload_row != NULL);
            if (index != NULL && payload != NULL && index_row != NULL && payload_row != NULL) {
                uint32_t index_id = index->stable_id;
                index->stable_id++;
                CHECK(!semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
                index->stable_id = index_id;
                CHECK(semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
                const Type *index_type = index_row->value_type;
                index_row->value_type = TYPE_STRING;
                CHECK(!semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
                index_row->value_type = index_type;
                const Type *payload_type = payload_row->value_type;
                payload_row->value_type = TYPE_STRING;
                CHECK(!semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
                payload_row->value_type = payload_type;
                CHECK(semantic_param_publication_private_physical_array_mutation(ctx, call, parent));
            }
        }
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_string_field_return_nonlocal_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct TextNode{name:String;payload:String;}"
        "struct Arena{atom_table:Array<String>;}"
        "func AtomFor(node:TextNode)->String{"
        "if node.name!=\"\"{return node.name;}return node.payload;}"
        "func Project(nodes:Array<TextNode>)->Arena{"
        "let atom_table:Array<String> = [\"\"];let i:Int=0;"
        "while i<ArrayLength(nodes){let atom_text:String=AtomFor(nodes[i]);"
        "if atom_text!=\"\"{ArrayPush(atom_table,atom_text);}i=i+1;}"
        "return Arena(atom_table);}"
        "func Main()->Void{let name=Concat(\"N\",\"ame\");"
        "let nodes:Array<TextNode> = [TextNode(name,\"\")];"
        "let arena=Project(nodes);Log(arena.atom_table[1]);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *atom_for = ast_program_statement(program, 2);
    ASTNode *project = ast_program_statement(program, 3);
    ASTNode *loop = ast_block_statement(ast_func_body(project), 2);
    ASTNode *named = loop != NULL ? ast_block_statement(ast_while_body(loop), 0) : NULL;
    ASTNode *call = named != NULL ? ast_let_initializer(named) : NULL;
    ASTNode *actual = call != NULL ? ast_call_argument(call, 0) : NULL;
    ASTNode *first_return = ast_block_statement(ast_if_then_branch(
        ast_block_statement(ast_func_body(atom_for), 0)), 0);
    ASTNode *second_return = ast_block_statement(ast_func_body(atom_for), 1);
    ASTNode *first_field = first_return != NULL ? ast_return_value(first_return) : NULL;
    ASTNode *second_field = second_return != NULL ? ast_return_value(second_return) : NULL;
    CHECK(atom_for != NULL && project != NULL && call != NULL && call->type == AST_CALL &&
        actual != NULL && actual->type == AST_ARRAY_ACCESS &&
        first_field != NULL && first_field->type == AST_MEMBER_ACCESS &&
        second_field != NULL && second_field->type == AST_MEMBER_ACCESS);
    if (atom_for == NULL || project == NULL || call == NULL || actual == NULL ||
        first_field == NULL || second_field == NULL) goto cleanup;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    CHECK(store != NULL);
    if (store == NULL) goto cleanup;
    unsigned mask = function_param_flow_summary_for_param(ctx, project, 0);
    CHECK(mask != SLOT_PARAM_SUMMARY_ALL && function_param_flow_nonlocal_input_current(ctx, project, 0));
    /* The existing declared-result owner, not this query, produces the frame. */
    FunctionParamFlowBackingFact *result = function_param_flow_declared_result_frame_current(store, call, NULL);
    CHECK(result != NULL && result->epoch != 0 && result->complete && !result->pending);
    if (result == NULL) goto cleanup;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    PublishedNode *call_row = published_node_for(ctx, call);
    CHECK(call_row != NULL && call_row->indexed_string_source_present &&
        call_row->indexed_string_source_id == UINT32_MAX);
    uint32_t owned_producer = 0;
    CHECK(!semantic_collection_owned_string_call_result(call, ctx, &owned_producer));
    FunctionParamFlowSummaryEntry *entry = NULL;
    FunctionParamFlowProgramPointIndex *index = NULL;
    for (size_t e = 0; e < store->count; e++)
        if (store->entries[e].function_decl == atom_for && store->entries[e].param_index == 0)
            entry = &store->entries[e];
    for (size_t i = 0; i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == atom_for)
            index = &store->program_point_indexes[i];
    CHECK(entry != NULL && index != NULL && index->declared_result == result &&
        index->backing_frames != NULL && index->backing_frame_count != 0 &&
        index->backing_frames[0] != NULL);
    if (entry == NULL || index == NULL || index->backing_frames == NULL ||
        index->backing_frame_count == 0 || index->backing_frames[0] == NULL) goto cleanup;
    uint32_t call_id = call->stable_id;
    call->stable_id++;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    call->stable_id = call_id;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    uint32_t actual_id = actual->stable_id;
    actual->stable_id++;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    actual->stable_id = actual_id;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    uint32_t callee_id = ast_call_semantic_callee_decl_id(call);
    CHECK(ast_call_set_semantic_callee_decl_id(call, ast_node_stable_id(project)));
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(call, callee_id));
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    ASTNode *fields[] = {first_field, second_field};
    for (size_t f = 0; f < 2; f++) {
        uint32_t field_id = fields[f]->stable_id;
        fields[f]->stable_id++;
        CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
        fields[f]->stable_id = field_id;
        CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    }
    unsigned original_mask = entry->mask;
    entry->mask &= ~SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    entry->mask = original_mask;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    entry->mask = original_mask | ((unsigned)SLOT_PARAM_SUMMARY_ALL + 1u);
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    entry->mask = original_mask;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    bool output_ready = entry->output_dependencies_ready;
    entry->output_dependencies_ready = false;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    entry->output_dependencies_ready = output_ready;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    CHECK(entry->output_dependencies != NULL && entry->output_count == 1);
    if (entry->output_dependencies != NULL && entry->output_count == 1) {
        bool dependency = entry->output_dependencies[0];
        entry->output_dependencies[0] = true;
        CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
        entry->output_dependencies[0] = dependency;
        CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    }
    size_t epoch = result->epoch;
    result->epoch = 0;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    result->epoch = epoch;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    bool complete = result->complete;
    result->complete = false;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    result->complete = complete;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    FunctionParamFlowBackingFact *normal = index->backing_frames[0];
    complete = normal->complete;
    normal->complete = false;
    CHECK(!function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    normal->complete = complete;
    CHECK(function_param_flow_string_result_nonlocal_current(ctx, call, project, 0));
    CHECK(call_row != NULL && call_row->indexed_string_source_id == UINT32_MAX);
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_scalar_noframe_nonlocal_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Direct(value:Int)->Int{return value+1;}"
        "func StringPush(value:Int)->Int{let texts:Array<String> = [\"\"];"
        "ArrayPush(texts,ToString(value));return value;}"
        "func Main()->Void{Log(Direct(3));Log(StringPush(3));}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *direct = ast_program_statement(program, 0);
    ASTNode *string_push = ast_program_statement(program, 1);
    ASTNode *binary = ast_return_value(ast_block_statement(ast_func_body(direct), 0));
    ASTNode *push = ast_block_statement(ast_func_body(string_push), 1);
    CHECK(binary != NULL && binary->type == AST_BINARY &&
        push != NULL && push->type == AST_CALL);
    if (binary == NULL || push == NULL) goto cleanup;
    unsigned mask = function_param_flow_summary_for_param(ctx, direct, 0);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    CHECK(store != NULL);
    if (store == NULL) goto cleanup;
    CHECK(mask != SLOT_PARAM_SUMMARY_ALL);
    FunctionParamFlowProgramPointIndex *point = NULL;
    CHECK(function_param_flow_program_point_index_build(store, direct, &point) && point != NULL);
    CHECK(point != NULL && (point->backing_frames == NULL ||
        point->backing_frame_count == 0 || point->backing_frames[0] == NULL));
    CHECK(function_param_flow_nonlocal_input_current(ctx, direct, 0));
    CHECK(!function_param_flow_nonlocal_input_current(ctx, string_push, 0));
    CHECK(!semantic_param_publication_private_physical_array_mutation(ctx, push, string_push));
    FunctionParamFlowSummaryEntry *entry = NULL;
    for (size_t e = 0; e < store->count; e++)
        if (store->entries[e].function_decl == direct && store->entries[e].param_index == 0)
            entry = &store->entries[e];
    CHECK(entry != NULL && function_param_flow_physical_result_header_current(ctx, direct));
    if (entry != NULL) {
        FunctionParamFlowOutputProducer producer = entry->output_producer;
        entry->output_producer = FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, direct, 0));
        entry->output_producer = producer;
        CHECK(function_param_flow_nonlocal_input_current(ctx, direct, 0));
        FunctionParamFlowSummaryState state = entry->state;
        entry->state = FUNCTION_PARAM_FLOW_COMPUTING;
        CHECK(!function_param_flow_nonlocal_input_current(ctx, direct, 0));
        entry->state = state;
        CHECK(function_param_flow_nonlocal_input_current(ctx, direct, 0));
    }
    ASTNode *header = ast_func_return_type(direct);
    CHECK(header != NULL);
    if (header != NULL) {
        uint32_t header_id = header->stable_id;
        header->stable_id++;
        CHECK(!function_param_flow_physical_result_header_current(ctx, direct));
        CHECK(!function_param_flow_nonlocal_input_current(ctx, direct, 0));
        header->stable_id = header_id;
        CHECK(function_param_flow_physical_result_header_current(ctx, direct));
        CHECK(function_param_flow_nonlocal_input_current(ctx, direct, 0));
        size_t metadata_at = 0;
        while (metadata_at < ctx->type_resolution_metadata.count &&
            ctx->type_resolution_metadata.keys[metadata_at] != header) metadata_at++;
        CHECK(metadata_at < ctx->type_resolution_metadata.count);
        if (metadata_at < ctx->type_resolution_metadata.count) {
            void *saved = ctx->type_resolution_metadata.values[metadata_at];
            ctx->type_resolution_metadata.values[metadata_at] = NULL;
            CHECK(!function_param_flow_physical_result_header_current(ctx, direct));
            CHECK(!function_param_flow_nonlocal_input_current(ctx, direct, 0));
            ctx->type_resolution_metadata.values[metadata_at] = TYPE_VOID;
            CHECK(!function_param_flow_physical_result_header_current(ctx, direct));
            CHECK(!function_param_flow_nonlocal_input_current(ctx, direct, 0));
            ctx->type_resolution_metadata.values[metadata_at] = saved;
            CHECK(function_param_flow_physical_result_header_current(ctx, direct));
            CHECK(function_param_flow_nonlocal_input_current(ctx, direct, 0));
        }
    }
    PgyTokenType op = binary->data.binary.op.type;
    binary->data.binary.op.type = TOKEN_MINUS;
    CHECK(!function_param_flow_nonlocal_input_current(ctx, direct, 0));
    binary->data.binary.op.type = op;
    CHECK(function_param_flow_nonlocal_input_current(ctx, direct, 0));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_physical_result_nonlocal_currency(void)
{
    Lexer *lexer = lexer_create(
        "func Fresh(value:Int)->Array<Int>{return [value];}"
        "func Copy(ref values:Array<Int>)->Array<Int>{return Fresh(values[0]);}"
        "func Main()->Void{let values:Array<Int> = [7];"
        "let rows:Array<Int> = Copy(values);Log(rows[0]);Log(values[0]);}");
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed);
    if (!typed) goto cleanup;
    ASTNode *fresh = ast_program_statement(program, 0);
    ASTNode *copy = ast_program_statement(program, 1);
    ASTNode *fresh_call = ast_return_value(ast_block_statement(ast_func_body(copy), 0));
    ASTNode *actual = fresh_call != NULL ? ast_call_argument(fresh_call, 0) : NULL;
    ASTNode *root = actual != NULL ? ast_array_access_array(actual) : NULL;
    CHECK(fresh_call != NULL && fresh_call->type == AST_CALL &&
        actual != NULL && actual->type == AST_ARRAY_ACCESS &&
        root != NULL && root->type == AST_IDENTIFIER);
    if (fresh_call == NULL || actual == NULL || root == NULL) goto cleanup;
    ASTNode *caller = NULL, *found_root = NULL;
    CHECK(semantic_param_publication_projection_root_current(ctx, actual,
        &caller, &found_root) && caller == copy && found_root == root);
    CHECK(semantic_array_storage_owned_result_current(ctx, fresh_call));
    /* The callee's completed formal summary is the physical-input premise. */
    unsigned fresh_mask = function_param_flow_summary_for_param(ctx, fresh, 0);
    CHECK(fresh_mask != SLOT_PARAM_SUMMARY_ALL);
    CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    unsigned mask = function_param_flow_summary_for_param(ctx, copy, 0);
    bool nonlocal = function_param_flow_nonlocal_input_current(ctx, copy, 0);
    if (mask == SLOT_PARAM_SUMMARY_ALL || !nonlocal) {
        FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
        for (size_t e = 0; store != NULL && e < store->count; e++) {
            FunctionParamFlowSummaryEntry *entry = &store->entries[e];
            if (entry->function_decl == copy && entry->param_index == 0) {
                fprintf(stderr, "physical result input: returned=%u entry=%u state=%u producer=%u "
                    "output_ready=%d unproved=%d pending=%d nonlocal=%d\n",
                    mask, entry->mask, (unsigned)entry->state, (unsigned)entry->output_producer,
                    (int)entry->output_dependencies_ready, (int)entry->returned_backing_unproved,
                    (int)entry->returned_backing_pending, (int)nonlocal);
                break;
            }
        }
    }
    CHECK(mask != SLOT_PARAM_SUMMARY_ALL);
    CHECK(nonlocal);
    uint32_t call_id = fresh_call->stable_id;
    fresh_call->stable_id++;
    CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    CHECK(!function_param_flow_nonlocal_input_current(ctx, copy, 0));
    CHECK(!semantic_array_storage_owned_result_current(ctx, fresh_call));
    fresh_call->stable_id = call_id;
    CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    CHECK(function_param_flow_nonlocal_input_current(ctx, copy, 0));
    uint32_t actual_id = actual->stable_id;
    actual->stable_id++;
    CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    CHECK(!function_param_flow_nonlocal_input_current(ctx, copy, 0));
    actual->stable_id = actual_id;
    CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    CHECK(function_param_flow_nonlocal_input_current(ctx, copy, 0));
    uint32_t callee_id = ast_call_semantic_callee_decl_id(fresh_call);
    CHECK(ast_call_set_semantic_callee_decl_id(fresh_call, ast_node_stable_id(copy)));
    CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    CHECK(!function_param_flow_nonlocal_input_current(ctx, copy, 0));
    CHECK(ast_call_set_semantic_callee_decl_id(fresh_call, callee_id));
    CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    CHECK(function_param_flow_nonlocal_input_current(ctx, copy, 0));
    PublishedNode *fresh_row = published_node_for(ctx, fresh_call);
    CHECK(fresh_row != NULL);
    if (fresh_row != NULL) {
        bool value_present = fresh_row->value_present;
        const Type *value_type = fresh_row->value_type;
        fresh_row->value_present = false;
        CHECK(!semantic_array_storage_owned_result_current(ctx, fresh_call));
        CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
        fresh_row->value_present = value_present;
        CHECK(semantic_array_storage_owned_result_current(ctx, fresh_call));
        CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
        fresh_row->value_type = TYPE_STRING;
        CHECK(!semantic_array_storage_owned_result_current(ctx, fresh_call));
        CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
        fresh_row->value_type = value_type;
        CHECK(semantic_array_storage_owned_result_current(ctx, fresh_call));
        CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    }
    PendingRoutine *fresh_routine = NULL;
    for (size_t i = 0; i < ctx->function_param_publications->routine_count; i++)
        if (ctx->function_param_publications->routines[i].decl == fresh)
            fresh_routine = &ctx->function_param_publications->routines[i];
    CHECK(fresh_routine != NULL && fresh_routine->normal_exit_closed);
    if (fresh_routine != NULL) {
        bool normal_exit_closed = fresh_routine->normal_exit_closed;
        fresh_routine->normal_exit_closed = false;
        CHECK(!semantic_array_storage_owned_result_current(ctx, fresh_call));
        CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
        fresh_routine->normal_exit_closed = normal_exit_closed;
        CHECK(semantic_array_storage_owned_result_current(ctx, fresh_call));
        CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
    }
    PublishedNode *actual_row = published_node_for(ctx, actual);
    CHECK(actual_row != NULL);
    if (actual_row != NULL) {
        bool physical = actual_row->physical_scalar;
        actual_row->physical_scalar = false;
        CHECK(!function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
        CHECK(!function_param_flow_nonlocal_input_current(ctx, copy, 0));
        actual_row->physical_scalar = physical;
        CHECK(function_param_flow_nonlocal_physical_input_current(ctx, fresh_call, 0));
        CHECK(function_param_flow_nonlocal_input_current(ctx, copy, 0));
    }
    uint32_t root_binding = ast_identifier_binding_syntax_id(root);
    ast_identifier_set_binding_syntax_id(root, 0);
    CHECK(!function_param_flow_nonlocal_input_current(ctx, copy, 0));
    ast_identifier_set_binding_syntax_id(root, root_binding);
    CHECK(function_param_flow_nonlocal_input_current(ctx, copy, 0));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_string_push_currency(void)
{
    Lexer *lexer = lexer_create(
        "struct TextNode{name:String;payload:String;}"
        "struct Arena{atom_table:Array<String>;provenance_texts:Array<String>;}"
        "func AtomFor(node:TextNode)->String{"
        "if node.name!=\"\"{return node.name;}return node.payload;}"
        "func Project(nodes:Array<TextNode>)->Arena{"
        "let provenance_texts:Array<String> = [];"
        "let atom_table:Array<String> = [\"\"];"
        "ArrayPush(atom_table,nodes[0].name);"
        "let atom_text:String=AtomFor(nodes[0]);"
        "ArrayPush(atom_table,atom_text);"
        "return Arena(atom_table,provenance_texts);}"
        "func Main()->Void{let name=Concat(\"N\",\"ame\");"
        "let nodes:Array<TextNode> = [TextNode(name,\"\")];"
        "let arena=Project(nodes);Log(arena.atom_table[1]);}"
    );
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (!typed) goto cleanup;
    ASTNode *atom_for = ast_program_statement(program, 2);
    ASTNode *project = ast_program_statement(program, 3);
    ASTNode *body = project == NULL ? NULL : ast_func_body(project);
    ASTNode *provenance = body == NULL ? NULL : ast_block_statement(body, 0);
    ASTNode *atom_table = body == NULL ? NULL : ast_block_statement(body, 1);
    ASTNode *direct = body == NULL ? NULL : ast_block_statement(body, 2);
    ASTNode *named_let = body == NULL ? NULL : ast_block_statement(body, 3);
    ASTNode *named = body == NULL ? NULL : ast_block_statement(body, 4);
    ASTNode *source = direct == NULL ? NULL : ast_call_argument(direct, 1);
    ASTNode *receiver = direct == NULL ? NULL : ast_call_argument(direct, 0);
    ASTNode *named_source = named == NULL ? NULL : ast_call_argument(named, 1);
    ASTNode *dependency = named_let == NULL ? NULL : ast_let_initializer(named_let);
    ASTNode *actual = dependency == NULL ? NULL : ast_call_argument(dependency, 0);
    CHECK(atom_for != NULL && project != NULL && provenance != NULL && atom_table != NULL &&
        direct != NULL && direct->type == AST_CALL && named_let != NULL &&
        named != NULL && named->type == AST_CALL && source != NULL &&
        source->type == AST_MEMBER_ACCESS && receiver != NULL &&
        named_source != NULL && named_source->type == AST_IDENTIFIER &&
        dependency != NULL && dependency->type == AST_CALL &&
        actual != NULL && actual->type == AST_ARRAY_ACCESS);
    if (atom_for == NULL || project == NULL || provenance == NULL || atom_table == NULL ||
        direct == NULL || named_let == NULL || named == NULL || source == NULL || receiver == NULL ||
        named_source == NULL || dependency == NULL || actual == NULL) goto cleanup;
    FunctionParamPublicationStore *publication = ctx->function_param_publications;
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    CHECK(publication != NULL && flow != NULL);
    if (publication == NULL || flow == NULL) goto cleanup;
    PublishedNode *direct_row = published_node_for(ctx, direct);
    PublishedNode *named_row = published_node_for(ctx, named);
    PublishedNode *source_row = published_node_for(ctx, source);
    PublishedNode *named_source_row = published_node_for(ctx, named_source);
    PublishedNode *let_row = published_node_for(ctx, named_let);
    PublishedNode *actual_row = published_node_for(ctx, actual);
    CHECK(direct_row != NULL && named_row != NULL && source_row != NULL &&
        named_source_row != NULL && let_row != NULL && actual_row != NULL);
    if (direct_row == NULL || named_row == NULL || source_row == NULL ||
        named_source_row == NULL || let_row == NULL || actual_row == NULL) goto cleanup;
    CHECK(direct_row->private_string_push != NULL && named_row->private_string_push != NULL &&
        publication->private_string_push_count == 2);
    CHECK(source_row->indexed_string_source_present && source_row->indexed_string_source_id == UINT32_MAX &&
        named_source_row->indexed_string_source_present && named_source_row->indexed_string_source_id == UINT32_MAX);
    uint32_t direct_kind = 0, direct_receiver = 0, direct_source = UINT32_MAX;
    uint32_t named_kind = 0, named_receiver = 0, named_effect_source = UINT32_MAX;
    CHECK(ast_call_semantic_collection_effect(direct, &direct_kind, &direct_receiver, &direct_source) &&
        ast_call_semantic_collection_effect(named, &named_kind, &named_receiver, &named_effect_source) &&
        direct_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        named_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        direct_receiver == ast_node_stable_id(atom_table) &&
        named_receiver == direct_receiver && direct_source == 0 && named_effect_source == 0);
    CHECK(!semantic_collection_owned_string_call_result(dependency, ctx, NULL));
    PgyCollectionOwnershipFact *atom_fact = (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(
            ctx, ast_node_stable_id(project), ast_node_stable_id(atom_table));
    PgyCollectionOwnershipFact *risk = (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(project), ast_node_stable_id(provenance));
    CHECK(atom_fact != NULL && atom_fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS &&
        !atom_fact->return_elements_owned && atom_fact->return_elements_source == NULL &&
        atom_fact->return_elements_source_id == 0);
    CHECK(risk != NULL && risk->element_ownership != PGY_STRING_ARRAY_BORROWED_ELEMENTS);
    unsigned mask = function_param_flow_summary_for_param(ctx, project, 0);
    CHECK(mask != SLOT_PARAM_SUMMARY_ALL && function_param_flow_nonlocal_input_current(ctx, project, 0));
    FunctionParamFlowBackingFact *result = function_param_flow_declared_result_frame_current(flow, dependency, NULL);
    CHECK(result != NULL && result->epoch != 0 && result->complete && !result->pending);
    FunctionParamFlowProgramPointIndex *project_index = NULL;
    CHECK(function_param_flow_program_point_index_build(flow, project, &project_index));
    FunctionParamFlowBackingFact *normal = project_index != NULL && project_index->backing_frames != NULL &&
        project_index->backing_frame_count != 0 ? project_index->backing_frames[0] : NULL;
    CHECK(normal != NULL && normal->epoch != 0 && normal->private_stores_closed);
    CHECK(semantic_indexed_string_private_push_current(ctx, direct));
    CHECK(semantic_indexed_string_private_push_current(ctx, named));
    CHECK(semantic_indexed_string_borrow_finalize(ctx));
    if (risk == NULL || atom_fact == NULL || result == NULL || normal == NULL) goto cleanup;
    /* A query can recover after restoring its row. Finalization records an
     * error, so each diagnostic must be discarded before checking recovery. */
    for (unsigned fault = 0; fault < 15; fault++) {
        uint32_t direct_id = direct->stable_id, source_id = source->stable_id;
        uint32_t receiver_id = receiver->stable_id;
        uint32_t projected_field = source_row->projection.field_id;
        uint32_t callee_id = ast_call_semantic_callee_decl_id(dependency);
        uint32_t initializer_id = let_row->indexed_string_let.initializer_id;
        uint32_t projected_index = actual_row->projection.index_id;
        uint32_t risk_binding = risk->binding_syntax_id;
        PgyCollectionOwnershipFact saved_atom_fact = *atom_fact;
        size_t epoch = result->epoch, count = publication->private_string_push_count;
        bool private_closed = normal->private_stores_closed;
        PendingPrivateStringPush *receipt = direct_row->private_string_push;
        if (fault == 0) direct->stable_id++;
        if (fault == 1) source->stable_id++;
        if (fault == 2) receiver->stable_id++;
        if (fault == 3) source_row->projection.field_id++;
        if (fault == 4) ast_call_set_semantic_callee_decl_id(dependency, ast_node_stable_id(project));
        if (fault == 5) let_row->indexed_string_let.initializer_id++;
        if (fault == 6) actual_row->projection.index_id++;
        if (fault == 7) result->epoch = 0;
        if (fault == 8) normal->private_stores_closed = false;
        if (fault == 9) direct_row->private_string_push = NULL;
        if (fault == 10) publication->private_string_push_count++;
        if (fault == 11) risk->binding_syntax_id++;
        if (fault == 12) atom_fact->return_elements_owned = true;
        if (fault == 13) atom_fact->return_elements_source = atom_table;
        if (fault == 14) atom_fact->return_elements_source_id = ast_node_stable_id(atom_table);
        bool direct_current = semantic_indexed_string_private_push_current(ctx, direct);
        bool named_current = semantic_indexed_string_private_push_current(ctx, named);
        /* Both receipts consume the caller's current claims. Altering either
         * projection or selected call can withdraw that shared proof. */
        bool expected_direct = fault == 5 || fault == 10;
        bool expected_named = fault == 9 || fault == 10;
        if (direct_current != expected_direct || named_current != expected_named)
            fprintf(stderr, "private String push fault=%u direct=%d named=%d expected=%d,%d\n",
                fault, (int)direct_current, (int)named_current,
                (int)expected_direct, (int)expected_named);
        CHECK(direct_current == expected_direct && named_current == expected_named);
        size_t diagnostics = ctx->diagnostic_count;
        CHECK(!semantic_indexed_string_borrow_finalize(ctx));
        CHECK(ctx->has_error && ctx->diagnostic_count == diagnostics + 1 &&
            ctx->diagnostics[diagnostics] != NULL &&
            ctx->diagnostics[diagnostics]->code != NULL &&
            strcmp(ctx->diagnostics[diagnostics]->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0);
        direct->stable_id = direct_id; source->stable_id = source_id;
        receiver->stable_id = receiver_id;
        source_row->projection.field_id = projected_field;
        ast_call_set_semantic_callee_decl_id(dependency, callee_id);
        let_row->indexed_string_let.initializer_id = initializer_id;
        actual_row->projection.index_id = projected_index;
        risk->binding_syntax_id = risk_binding;
        *atom_fact = saved_atom_fact;
        result->epoch = epoch; normal->private_stores_closed = private_closed;
        direct_row->private_string_push = receipt;
        publication->private_string_push_count = count;
        for (size_t d = diagnostics; d < ctx->diagnostic_count; d++) {
            free(ctx->diagnostics[d]->message);
            diag_payload_snapshot_destroy(ctx->diagnostics[d]->payload);
            free(ctx->diagnostics[d]);
            ctx->diagnostics[d] = NULL;
        }
        ctx->diagnostic_count = diagnostics;
        ctx->has_error = false;
        bool restored_direct = semantic_indexed_string_private_push_current(ctx, direct);
        bool restored_named = semantic_indexed_string_private_push_current(ctx, named);
        bool restored_final = semantic_indexed_string_borrow_finalize(ctx);
        if (!restored_direct || !restored_named || !restored_final)
            fprintf(stderr, "private String push restore fault=%u direct=%d named=%d finalize=%d\n",
                fault, (int)restored_direct, (int)restored_named, (int)restored_final);
        CHECK(restored_direct && restored_named && restored_final);
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_string_empty_receiver_currency(bool named)
{
    Lexer *lexer = lexer_create(named
        ? "struct TextNode{type_name:String;}struct Arena{atom_table:Array<String>;}"
          "func AtomFor(node:TextNode)->String{return node.type_name;}"
          "func Project(nodes:Array<TextNode>)->Arena{"
          "let atom_table:Array<String> = [];"
          "let atom_text:String=AtomFor(nodes[0]);ArrayPush(atom_table,atom_text);"
          "return Arena(atom_table);}"
          "func Main()->Void{let name=Concat(\"Type\",\"Name\");"
          "let nodes:Array<TextNode> = [TextNode(name)];"
          "let arena=Project(nodes);Log(arena.atom_table[0]);}"
        : "struct TextNode{type_name:String;}struct Arena{atom_table:Array<String>;}"
          "func Project(nodes:Array<TextNode>)->Arena{"
          "let atom_table:Array<String> = [];"
          "ArrayPush(atom_table,nodes[0].type_name);return Arena(atom_table);}"
          "func Main()->Void{let name=Concat(\"Type\",\"Name\");"
          "let nodes:Array<TextNode> = [TextNode(name)];"
          "let arena=Project(nodes);Log(arena.atom_table[0]);}"
    );
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (!typed) goto cleanup;
    ASTNode *project = ast_program_statement(program, named ? 3 : 2);
    ASTNode *body = project == NULL ? NULL : ast_func_body(project);
    ASTNode *binding = body == NULL ? NULL : ast_block_statement(body, 0);
    ASTNode *push = body == NULL ? NULL : ast_block_statement(body, named ? 2 : 1);
    ASTNode *receiver = push == NULL ? NULL : ast_call_argument(push, 0);
    ASTNode *source = push == NULL ? NULL : ast_call_argument(push, 1);
    ASTNode *dependency = named && body != NULL
        ? ast_let_initializer(ast_block_statement(body, 1)) : NULL;
    CHECK(project != NULL && binding != NULL && push != NULL && push->type == AST_CALL &&
        receiver != NULL && source != NULL && (!named || dependency != NULL));
    if (project == NULL || binding == NULL || push == NULL || receiver == NULL || source == NULL ||
        (named && dependency == NULL)) goto cleanup;
    uint32_t binding_id = ast_node_stable_id(binding);
    PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(project), binding_id);
    CHECK(fact != NULL && binding_id != 0 && fact->binding_syntax_id == binding_id &&
        fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL &&
        fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN &&
        fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE &&
        !fact->return_elements_owned && fact->return_elements_source == NULL &&
        fact->return_elements_source_id == 0 &&
        ast_identifier_binding_syntax_id(receiver) == binding_id);
    uint32_t effect_kind = 0, effect_receiver = 0, effect_source = UINT32_MAX;
    CHECK(ast_call_semantic_collection_effect(push, &effect_kind, &effect_receiver, &effect_source) &&
        effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        effect_receiver == binding_id && effect_source == 0);
    CHECK(published_node_for(ctx, push) != NULL &&
        published_node_for(ctx, push)->private_string_push != NULL);
    unsigned mask = function_param_flow_summary_for_param(ctx, project, 0);
    CHECK(mask != SLOT_PARAM_SUMMARY_ALL);
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    FunctionParamFlowBackingFact *result = named && flow != NULL
        ? function_param_flow_declared_result_frame_current(flow, dependency, NULL) : NULL;
    CHECK(!named || (result != NULL && result->epoch != 0 && result->complete && !result->pending));
    if (named) CHECK(!function_param_flow_string_result_independent_current(ctx, dependency));
    FunctionParamFlowProgramPointIndex *index = NULL;
    CHECK(flow != NULL && function_param_flow_program_point_index_build(flow, project, &index));
    FunctionParamFlowIndexClaim *claim = NULL;
    if (index != NULL && index->claims != NULL)
        for (size_t i = 0; i < index->claims->count; i++)
            if (index->claims->rows[i].node == push) claim = &index->claims->rows[i];
    CHECK(claim != NULL && claim->collection_effect_present &&
        claim->collection_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        claim->collection_effect_receiver == binding_id && claim->collection_effect_source == 0);
    CHECK(function_param_flow_private_string_receiver_current(ctx, project, binding_id));
    CHECK(semantic_indexed_string_private_push_current(ctx, push));
    CHECK(semantic_indexed_string_borrow_finalize(ctx));
    if (fact == NULL || claim == NULL) goto cleanup;
    /* The receiver is the risk binding itself. Its current history and the
     * native/AST effect tuple must both survive final replay. */
    for (unsigned fault = 0; fault < 20; fault++) {
        PgyCollectionOwnershipFact saved_fact = *fact;
        bool saved_effect_present = push->data.call.semantic_collection_effect_set;
        uint32_t saved_kind = push->data.call.semantic_collection_effect_kind;
        uint32_t saved_receiver = push->data.call.semantic_collection_receiver_binding_id;
        uint32_t saved_source = push->data.call.semantic_collection_source_binding_id;
        bool saved_claim_present = claim->collection_effect_present;
        uint32_t saved_claim_kind = claim->collection_effect_kind;
        uint32_t saved_claim_receiver = claim->collection_effect_receiver;
        uint32_t saved_claim_source = claim->collection_effect_source;
        if (fault == 0) memset(fact, 0, sizeof(*fact));
        if (fault == 1) fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
        if (fault == 2) fact->element_ownership = PGY_STRING_ARRAY_OWNED_ELEMENTS;
        if (fault == 3) fact->element_ownership = PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT;
        if (fault == 4) fact->origin = PGY_COLLECTION_ORIGIN_MAP_KEYS;
        if (fault == 5) fact->origin_syntax_id++;
        if (fault == 6) fact->source_binding_syntax_id++;
        if (fault == 7) fact->binding_syntax_id++;
        if (fault == 8) fact->function_syntax_id++;
        if (fault == 9) push->data.call.semantic_collection_effect_set = false;
        if (fault == 10) push->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
        if (fault == 11) push->data.call.semantic_collection_receiver_binding_id++;
        if (fault == 12) push->data.call.semantic_collection_source_binding_id++;
        if (fault == 13) claim->collection_effect_present = false;
        if (fault == 14) claim->collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
        if (fault == 15) claim->collection_effect_receiver++;
        if (fault == 16) claim->collection_effect_source++;
        if (fault == 17) fact->return_elements_owned = true;
        if (fault == 18) fact->return_elements_source = binding;
        if (fault == 19) fact->return_elements_source_id = binding_id;
        if (fault < 17)
            CHECK(!function_param_flow_private_string_receiver_current(ctx, project, binding_id));
        CHECK(!semantic_indexed_string_private_push_current(ctx, push));
        size_t diagnostics = ctx->diagnostic_count;
        CHECK(!semantic_indexed_string_borrow_finalize(ctx));
        CHECK(ctx->has_error && ctx->diagnostic_count == diagnostics + 1 &&
            ctx->diagnostics[diagnostics] != NULL && ctx->diagnostics[diagnostics]->code != NULL &&
            strcmp(ctx->diagnostics[diagnostics]->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0);
        *fact = saved_fact;
        push->data.call.semantic_collection_effect_set = saved_effect_present;
        push->data.call.semantic_collection_effect_kind = saved_kind;
        push->data.call.semantic_collection_receiver_binding_id = saved_receiver;
        push->data.call.semantic_collection_source_binding_id = saved_source;
        claim->collection_effect_present = saved_claim_present;
        claim->collection_effect_kind = saved_claim_kind;
        claim->collection_effect_receiver = saved_claim_receiver;
        claim->collection_effect_source = saved_claim_source;
        for (size_t d = diagnostics; d < ctx->diagnostic_count; d++) {
            free(ctx->diagnostics[d]->message);
            diag_payload_snapshot_destroy(ctx->diagnostics[d]->payload);
            free(ctx->diagnostics[d]);
            ctx->diagnostics[d] = NULL;
        }
        ctx->diagnostic_count = diagnostics;
        ctx->has_error = false;
        CHECK(function_param_flow_private_string_receiver_current(ctx, project, binding_id));
        CHECK(semantic_indexed_string_private_push_current(ctx, push));
        CHECK(semantic_indexed_string_borrow_finalize(ctx));
    }
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_string_borrowed_without_receipt(void)
{
    Lexer *lexer = lexer_create(
        "func Main()->Void{let rows:Array<String> = [\"\"];"
        "ArrayPush(rows,\"next\");Log(rows[1]);}"
    );
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (typed) {
        ASTNode *main_decl = ast_program_statement(program, 0);
        ASTNode *body = main_decl == NULL ? NULL : ast_func_body(main_decl);
        ASTNode *push = body == NULL ? NULL : ast_block_statement(body, 1);
        PublishedNode *row = push == NULL ? NULL : published_node_for(ctx, push);
        CHECK(push != NULL && push->type == AST_CALL && row != NULL &&
            row->private_string_push == NULL &&
            !ast_call_semantic_collection_effect(push, NULL, NULL, NULL));
    }
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

static void checked_private_string_independent_result_currency(bool borrowed_receiver)
{
    Lexer *lexer = lexer_create(borrowed_receiver
        ? "struct TextNode{type_name:String;}"
          "struct Arena{atom_table:Array<String>;provenance_texts:Array<String>;}"
          "func Fresh(node:TextNode)->String{return Concat(\"fresh\",\" value\");}"
          "func Project(nodes:Array<TextNode>)->Arena{"
          "let provenance_texts:Array<String> = [];"
          "let atom_table:Array<String> = [\"\"];"
          "ArrayPush(atom_table,Fresh(nodes[0]));"
          "return Arena(atom_table,provenance_texts);}"
          "func Main()->Void{let nodes:Array<TextNode> = [TextNode(\"unused\")];"
          "let arena=Project(nodes);Log(arena.atom_table[1]);}"
        : "struct TextNode{type_name:String;}struct Arena{atom_table:Array<String>;}"
          "func Fresh(node:TextNode)->String{return Concat(\"fresh\",\" value\");}"
          "func Project(nodes:Array<TextNode>)->Arena{let atom_table:Array<String> = [];"
          "ArrayPush(atom_table,Fresh(nodes[0]));return Arena(atom_table);}"
          "func Main()->Void{let nodes:Array<TextNode> = [TextNode(\"unused\")];"
          "let arena=Project(nodes);Log(arena.atom_table[0]);}"
    );
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticContext *ctx = semantic_context_create();
    bool parsed = program != NULL && !parser_has_error(parser);
    CHECK(parsed);
    bool typed = parsed && type_check_program(program, ctx);
    CHECK(typed && !ctx->has_error);
    if (!typed) goto cleanup;
    ASTNode *fresh = ast_program_statement(program, 2);
    ASTNode *project = ast_program_statement(program, 3);
    ASTNode *body = project == NULL ? NULL : ast_func_body(project);
    ASTNode *risk_binding = borrowed_receiver && body != NULL ? ast_block_statement(body, 0) : NULL;
    ASTNode *receiver_binding = body == NULL ? NULL : ast_block_statement(body, borrowed_receiver ? 1 : 0);
    ASTNode *push = body == NULL ? NULL : ast_block_statement(body, borrowed_receiver ? 2 : 1);
    ASTNode *call = push == NULL ? NULL : ast_call_argument(push, 1);
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    CHECK(fresh != NULL && project != NULL && receiver_binding != NULL &&
        (!borrowed_receiver || risk_binding != NULL) && push != NULL &&
        push->type == AST_CALL && call != NULL && call->type == AST_CALL && flow != NULL);
    if (fresh == NULL || project == NULL || receiver_binding == NULL ||
        (borrowed_receiver && risk_binding == NULL) || push == NULL || call == NULL || flow == NULL)
        goto cleanup;
    uint32_t receiver_id = ast_node_stable_id(receiver_binding);
    PgyCollectionOwnershipFact *receiver_fact = (PgyCollectionOwnershipFact *)
        semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(project), receiver_id);
    const PgyCollectionOwnershipFact *risk_fact = borrowed_receiver
        ? semantic_collection_ownership_fact_find(ctx, ast_node_stable_id(project),
            ast_node_stable_id(risk_binding)) : receiver_fact;
    CHECK(receiver_fact != NULL && risk_fact != NULL &&
        receiver_fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE &&
        receiver_fact->origin == (borrowed_receiver
            ? PGY_COLLECTION_ORIGIN_BORROWED_LITERAL : PGY_COLLECTION_ORIGIN_EMPTY_LITERAL) &&
        receiver_fact->element_ownership == (borrowed_receiver
            ? PGY_STRING_ARRAY_BORROWED_ELEMENTS : PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN) &&
        !receiver_fact->return_elements_owned && receiver_fact->return_elements_source == NULL &&
        receiver_fact->return_elements_source_id == 0 &&
        risk_fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE &&
        risk_fact->element_ownership != PGY_STRING_ARRAY_BORROWED_ELEMENTS &&
        risk_fact->binding_syntax_id == (borrowed_receiver
            ? ast_node_stable_id(risk_binding) : receiver_id));
    if (receiver_fact == NULL || risk_fact == NULL) goto cleanup;
    uint32_t effect_kind = 0, effect_receiver = 0, effect_source = UINT32_MAX;
    CHECK(ast_call_semantic_collection_effect(push, &effect_kind, &effect_receiver, &effect_source) &&
        effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        effect_receiver == receiver_id && effect_source == 0);
    PublishedNode *push_row = published_node_for(ctx, push);
    CHECK(push_row != NULL && push_row->private_string_push != NULL);
    unsigned mask = function_param_flow_summary_for_param(ctx, fresh, 0);
    CHECK(mask != SLOT_PARAM_SUMMARY_ALL && (mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0);
    FunctionParamFlowBackingFact *result = function_param_flow_declared_result_frame_current(flow, call, NULL);
    CHECK(result != NULL && result->epoch != 0 && result->complete && !result->pending);
    if (result == NULL) goto cleanup;
    size_t ready_count = flow->count, ready_index_count = flow->program_point_index_count;
    size_t ready_epoch = flow->demand_epoch, ready_evaluations = flow->body_evaluations;
    size_t ready_result_epoch = result->epoch;
    FunctionParamFlowSummaryEntry *ready_entries = flow->entries;
    FunctionParamFlowProgramPointIndex *ready_indexes = flow->program_point_indexes;
    uint32_t source = UINT32_MAX;
    CHECK(function_param_flow_string_result_independent_current(ctx, call));
    CHECK(semantic_indexed_string_borrow_source_current(call, ctx, &source) && source == 0);
    CHECK(semantic_indexed_string_private_push_current(ctx, push));
    CHECK(semantic_indexed_string_borrow_finalize(ctx));
    CHECK(flow->count == ready_count && flow->program_point_index_count == ready_index_count &&
        flow->demand_epoch == ready_epoch && flow->body_evaluations == ready_evaluations &&
        flow->entries == ready_entries && flow->program_point_indexes == ready_indexes &&
        result->epoch == ready_result_epoch);
    size_t fresh_entry = function_param_flow_find(flow, ast_node_stable_id(fresh), 0);
    CHECK(fresh_entry != SIZE_MAX && flow->entries[fresh_entry].function_decl == fresh &&
        flow->entries[fresh_entry].function_id == ast_node_stable_id(fresh) &&
        flow->entries[fresh_entry].param_index == 0 &&
        flow->entries[fresh_entry].state == FUNCTION_PARAM_FLOW_COMPLETE &&
        flow->hash != NULL && flow->hash_capacity != 0);
    if (fresh_entry == SIZE_MAX || flow->hash == NULL || flow->hash_capacity == 0) goto cleanup;
    /* A current query must never demand a replacement for a withdrawn callee
     * summary. The hash fault uses a copy, so even an erroneous rebuild cannot
     * free the original table before this fixture reports and stops. */
    for (unsigned fault = 0; fault < 5; fault++) {
        FunctionParamFlowSummaryEntry *entry = &flow->entries[fresh_entry];
        FunctionParamFlowSummaryState saved_state = entry->state;
        ASTNode *saved_decl = entry->function_decl;
        uint32_t saved_function_id = entry->function_id;
        size_t saved_param = entry->param_index;
        size_t *saved_hash = flow->hash;
        size_t saved_hash_capacity = flow->hash_capacity;
        if (fault == 0) entry->state = FUNCTION_PARAM_FLOW_UNSEEN;
        if (fault == 1) entry->function_decl = project;
        if (fault == 2) entry->function_id++;
        if (fault == 3) entry->param_index = SIZE_MAX;
        if (fault == 4) {
            CHECK(saved_hash_capacity <= SIZE_MAX / sizeof(size_t));
            if (saved_hash_capacity > SIZE_MAX / sizeof(size_t)) goto cleanup;
            size_t *shadow = malloc(saved_hash_capacity * sizeof(size_t));
            CHECK(shadow != NULL);
            if (shadow == NULL) goto cleanup;
            memcpy(shadow, saved_hash, saved_hash_capacity * sizeof(size_t));
            size_t slot = SIZE_MAX;
            for (size_t i = 0; i < saved_hash_capacity; i++)
                if (shadow[i] == fresh_entry + 1) slot = i;
            CHECK(slot != SIZE_MAX);
            if (slot == SIZE_MAX) { free(shadow); goto cleanup; }
            shadow[slot] = 0;
            flow->hash = shadow;
        }
        size_t *active_hash = flow->hash;
        bool current = function_param_flow_string_result_independent_current(ctx, call);
        bool push_current = semantic_indexed_string_private_push_current(ctx, push);
        size_t diagnostics = ctx->diagnostic_count;
        bool finalized = semantic_indexed_string_borrow_finalize(ctx);
        FunctionParamFlowBackingFact *current_result = NULL;
        for (size_t i = 0; i < flow->program_point_index_count; i++)
            if (flow->program_point_indexes[i].function_decl == fresh)
                current_result = flow->program_point_indexes[i].declared_result;
        bool unchanged = flow->count == ready_count &&
            flow->program_point_index_count == ready_index_count &&
            flow->demand_epoch == ready_epoch &&
            flow->body_evaluations == ready_evaluations &&
            flow->entries == ready_entries &&
            flow->program_point_indexes == ready_indexes &&
            flow->hash == active_hash && flow->hash_capacity == saved_hash_capacity &&
            current_result == result && current_result != NULL &&
            current_result->epoch == ready_result_epoch;
        bool refused = !current && !push_current && !finalized && ctx->has_error &&
            ctx->diagnostic_count == diagnostics + 1 &&
            ctx->diagnostics[diagnostics] != NULL &&
            ctx->diagnostics[diagnostics]->code != NULL &&
            strcmp(ctx->diagnostics[diagnostics]->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0;
        if (!unchanged || !refused)
            fprintf(stderr, "independent summary fault=%u borrowed=%d current=%d push=%d final=%d "
                "count=%zu/%zu index=%zu/%zu epoch=%zu/%zu eval=%zu/%zu frame=%d\n",
                fault, (int)borrowed_receiver, (int)current, (int)push_current, (int)finalized,
                flow->count, ready_count, flow->program_point_index_count, ready_index_count,
                flow->demand_epoch, ready_epoch, flow->body_evaluations, ready_evaluations,
                (int)(current_result == result && current_result != NULL &&
                    current_result->epoch == ready_result_epoch));
        CHECK(unchanged && refused);
        if (fault == 4) {
            if (flow->hash != saved_hash) free(flow->hash);
            flow->hash = saved_hash;
            flow->hash_capacity = saved_hash_capacity;
        }
        if (unchanged) {
            entry = &flow->entries[fresh_entry];
            entry->state = saved_state;
            entry->function_decl = saved_decl;
            entry->function_id = saved_function_id;
            entry->param_index = saved_param;
        }
        for (size_t d = diagnostics; d < ctx->diagnostic_count; d++) {
            free(ctx->diagnostics[d]->message);
            diag_payload_snapshot_destroy(ctx->diagnostics[d]->payload);
            free(ctx->diagnostics[d]);
            ctx->diagnostics[d] = NULL;
        }
        ctx->diagnostic_count = diagnostics;
        ctx->has_error = false;
        if (!unchanged || !refused) goto cleanup;
        CHECK(function_param_flow_string_result_independent_current(ctx, call));
        CHECK(semantic_indexed_string_private_push_current(ctx, push));
        CHECK(semantic_indexed_string_borrow_finalize(ctx));
        CHECK(flow->count == ready_count && flow->program_point_index_count == ready_index_count &&
            flow->demand_epoch == ready_epoch && flow->body_evaluations == ready_evaluations &&
            flow->entries == ready_entries && flow->program_point_indexes == ready_indexes &&
            result->epoch == ready_result_epoch);
    }
    size_t index_slot = SIZE_MAX, claim_slot = SIZE_MAX;
    for (size_t i = 0; i < flow->program_point_index_count; i++)
        if (flow->program_point_indexes[i].function_decl == project) index_slot = i;
    CHECK(index_slot != SIZE_MAX && flow->program_point_indexes[index_slot].claims != NULL);
    if (index_slot == SIZE_MAX || flow->program_point_indexes[index_slot].claims == NULL) goto cleanup;
    FunctionParamFlowIndexClaims *claims = flow->program_point_indexes[index_slot].claims;
    for (size_t i = 0; i < claims->count; i++)
        if (claims->rows[i].node == push) claim_slot = i;
    CHECK(claim_slot != SIZE_MAX && claims->rows[claim_slot].collection_effect_present &&
        claims->rows[claim_slot].collection_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        claims->rows[claim_slot].collection_effect_receiver == receiver_id &&
        claims->rows[claim_slot].collection_effect_source == 0);
    if (claim_slot == SIZE_MAX) goto cleanup;
    /* The independent source remains source-zero, but it still owes the
     * exact receiver mutation at final consumption. Neither query may heal
     * withdrawn caller claims or a withdrawn declared-result epoch. */
    for (unsigned fault = 0; fault < 14; fault++) {
        FunctionParamFlowProgramPointIndex *index = &flow->program_point_indexes[index_slot];
        FunctionParamFlowIndexClaim *claim = &claims->rows[claim_slot];
        bool saved_effect_present = push->data.call.semantic_collection_effect_set;
        uint32_t saved_kind = push->data.call.semantic_collection_effect_kind;
        uint32_t saved_receiver = push->data.call.semantic_collection_receiver_binding_id;
        uint32_t saved_source = push->data.call.semantic_collection_source_binding_id;
        bool saved_claim_present = claim->collection_effect_present;
        uint32_t saved_claim_kind = claim->collection_effect_kind;
        uint32_t saved_claim_receiver = claim->collection_effect_receiver;
        uint32_t saved_claim_source = claim->collection_effect_source;
        uint32_t saved_index_id = index->function_id;
        size_t saved_epoch = result->epoch;
        PgyCollectionOwnershipFact saved_receiver_fact = *receiver_fact;
        if (fault == 0) push->data.call.semantic_collection_effect_set = false;
        if (fault == 1) push->data.call.semantic_collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
        if (fault == 2) push->data.call.semantic_collection_receiver_binding_id++;
        if (fault == 3) push->data.call.semantic_collection_source_binding_id++;
        if (fault == 4) claim->collection_effect_present = false;
        if (fault == 5) claim->collection_effect_kind = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
        if (fault == 6) claim->collection_effect_receiver++;
        if (fault == 7) claim->collection_effect_source++;
        if (fault == 8) index->function_id = 0;
        if (fault == 9) index->claims = NULL;
        if (fault == 10) result->epoch = 0;
        if (fault == 11) receiver_fact->return_elements_owned = true;
        if (fault == 12) receiver_fact->return_elements_source = receiver_binding;
        if (fault == 13) receiver_fact->return_elements_source_id = receiver_id;
        bool current = semantic_indexed_string_private_push_current(ctx, push);
        if (current) fprintf(stderr, "independent private String fault=%u borrowed=%d remained current\n",
            fault, (int)borrowed_receiver);
        CHECK(!current);
        size_t diagnostics = ctx->diagnostic_count;
        CHECK(!semantic_indexed_string_borrow_finalize(ctx));
        CHECK(ctx->has_error && ctx->diagnostic_count == diagnostics + 1 &&
            ctx->diagnostics[diagnostics] != NULL && ctx->diagnostics[diagnostics]->code != NULL &&
            strcmp(ctx->diagnostics[diagnostics]->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0);
        index = &flow->program_point_indexes[index_slot];
        index->function_id = saved_index_id;
        index->claims = claims;
        claim = &claims->rows[claim_slot];
        push->data.call.semantic_collection_effect_set = saved_effect_present;
        push->data.call.semantic_collection_effect_kind = saved_kind;
        push->data.call.semantic_collection_receiver_binding_id = saved_receiver;
        push->data.call.semantic_collection_source_binding_id = saved_source;
        claim->collection_effect_present = saved_claim_present;
        claim->collection_effect_kind = saved_claim_kind;
        claim->collection_effect_receiver = saved_claim_receiver;
        claim->collection_effect_source = saved_claim_source;
        result->epoch = saved_epoch;
        *receiver_fact = saved_receiver_fact;
        for (size_t d = diagnostics; d < ctx->diagnostic_count; d++) {
            free(ctx->diagnostics[d]->message);
            diag_payload_snapshot_destroy(ctx->diagnostics[d]->payload);
            free(ctx->diagnostics[d]);
            ctx->diagnostics[d] = NULL;
        }
        ctx->diagnostic_count = diagnostics;
        ctx->has_error = false;
        CHECK(semantic_indexed_string_private_push_current(ctx, push));
        CHECK(semantic_indexed_string_borrow_finalize(ctx));
    }
    size_t epoch = result->epoch;
    result->epoch = 0;
    CHECK(!function_param_flow_string_result_independent_current(ctx, call));
    CHECK(!semantic_indexed_string_private_push_current(ctx, push));
    size_t diagnostics = ctx->diagnostic_count;
    CHECK(!semantic_indexed_string_borrow_finalize(ctx));
    CHECK(ctx->has_error && ctx->diagnostic_count == diagnostics + 1 &&
        ctx->diagnostics[diagnostics] != NULL && ctx->diagnostics[diagnostics]->code != NULL &&
        strcmp(ctx->diagnostics[diagnostics]->code, PGY_CODE_SEM_BORROW_ESCAPE) == 0);
    result->epoch = epoch;
    for (size_t d = diagnostics; d < ctx->diagnostic_count; d++) {
        free(ctx->diagnostics[d]->message);
        diag_payload_snapshot_destroy(ctx->diagnostics[d]->payload);
        free(ctx->diagnostics[d]);
        ctx->diagnostics[d] = NULL;
    }
    ctx->diagnostic_count = diagnostics;
    ctx->has_error = false;
    CHECK(function_param_flow_string_result_independent_current(ctx, call));
    CHECK(semantic_indexed_string_private_push_current(ctx, push));
    CHECK(semantic_indexed_string_borrow_finalize(ctx));
cleanup:
    semantic_context_destroy(ctx); ast_destroy(program); parser_destroy(parser); lexer_destroy(lexer);
}

int main(void)
{
    checked_unit_enum_selection_currency();
    checked_unit_enum_selection_boundaries();
    checked_owned_string_result_receipt_currency();
    checked_local_owner_constructor_currency();
    checked_typed_string_result_constructor_currency();
    checked_selected_domain_callable_identity();
    checked_selected_namespace_callable_identity();
    checked_selected_host_method_formal_currency();
    checked_readonly_receiver_result_currency();
    checked_finish_independence_currency();
    checked_local_owner_constructor_nested_effect();
    checked_local_owner_map_constructor_currency();
    checked_declared_result_currency();
    checked_named_string_result_source_currency();
    checked_nonlocal_projection_currency();
    checked_compound_projection_currency();
    checked_private_physical_array_mutation_currency();
    checked_string_field_return_nonlocal_currency();
    checked_scalar_noframe_nonlocal_currency();
    checked_physical_result_nonlocal_currency();
    checked_private_string_push_currency();
    checked_private_string_empty_receiver_currency(false);
    checked_private_string_empty_receiver_currency(true);
    checked_private_string_borrowed_without_receipt();
    checked_private_string_independent_result_currency(false);
    checked_private_string_independent_result_currency(true);
    checked_named_string_shallow_retention_currency();
    checked_result_argument_currency();
    checked_witnessed_physical_call_alias_currency();
    checked_slot_physical_scalar_forgery();
    checked_witnessed_physical_recursive_alias_currency();
    checked_mixed_ready_pending_physical_alias();
    checked_physical_array_element_destination();
    checked_descriptor_array_element_escape();
    checked_array_element_call_children_eager();
    checked_declared_physical_result_currency();
    checked_declared_array_result_currency();
    checked_declared_result_unclosed();
    checked_transaction_function_boundary();
    checked_transaction_compensation_identity();
    checked_transaction_failure_states();
    checked_deferred_publication_currency();
    checked_deferred_capture_not_eager();
    checked_nested_slot_effect_facets();
    checked_wrapper_child_currency();
    checked_named_escape_projection();
    checked_failed_no_store_selection_epoch();
    checked_admitted_no_store_proof();
    checked_return_loan_currency();
    checked_pure_literal_backing_currency();
    checked_identifier_consumer_currency();
    checked_local_alias_return_backing();
    checked_call_result_field_destination();
    checked_default_formal_replacement();
    checked_recursive_alias_return_backing();
    checked_conservative_callback_backing();
    checked_builtin_string_concat_receipt();
    checked_indexed_string_concat_nested_call_currency();
    checked_closed_scalar_return_receipt();
    /* A physical outer result does not erase retention of an inner loan. */
    checked_physical_return_retaining_effects("struct TextRecord{text:String} extern \"C\"{func Hold(text:String)->Void;} func Echo(ref text:String)->String{return text;} func Probe(ref record:TextRecord,ignored:String)->Int{Hold(Echo(record.text));return 1;}", 3, SLOT_PARAM_SUMMARY_CALL_ESCAPE, false);
    /* The late SCC edge must retain CALL after the scalar RETURN join. */
    reached_producer_source("struct TextRecord{text:String} extern \"C\"{func Hold(text:String)->Void;} func First(text:String)->Int{if StringLength(text)>0{return Second(text);}return 1;} func Second(text:String)->Int{Hold(text);return First(text);} func Probe(ref record:TextRecord)->Int{return First(record.text);}", false);
    checked_return_loan_alias_currency();
    checked_backing_binding_growth();
    checked_c_empty_preparation();
    checked_selector_c_empty_preparation();
    checked_selector_c_unreachable();
    checked_selector_c_empty_currency();
    checked_c_empty_currency();
    checked_control_navigation_preparation();
    checked_control_navigation_topology();
    checked_control_navigation_body_boundary();
    checked_control_navigation_currency();
    checked_control_navigation_root_sequence();
    checked_defer_return_backing();
    checked_backing_input_frames();
    checked_private_array_push_opaque_boundaries();
    checked_private_array_push_terminating_arm();
    checked_array_storage_path_snapshots();
    checked_array_storage_pending_release();
    checked_deferred_string_snapshot_currency();
    checked_deferred_row_source_currency(false);
    checked_deferred_row_source_currency(true);
    checked_collection_loop_replay();
    checked_owned_actual_final_currency();
    checked_array_alias_birth_before();
    checked_array_birth_preserved_sources();
    checked_array_birth_initializer_states();
    checked_string_member_alias_birth();
    checked_array_result_borrowed_formals();
    checked_owned_array_result_currency();
    checked_physical_hof_array_result_currency();
    checked_owned_array_string_result_currency();
    checked_owned_array_builtin_element_currency();
    checked_owned_array_nested_return_currency();
    checked_array_storage_live_paths();
    checked_private_local_array_push();
    checked_private_physical_array_nested_result_currency();
    checked_empty_array_no_output();
    checked_static_string_backing();
    checked_nested_call_currentness();
    checked_tuple_literal_type_receipt();
    checked_secure_view_call_tuple();
    checked_nominal_result_relations();
    checked_selected_string_field_constructor_currency();
    checked_indexed_string_field_recipe_currency();
    checked_nominal_result_growth();
    checked_nominal_unsupported_producers();
    checked_unmodelled_map_class_result();
    type_system_init();
    checked_some_unwrap_result_relations();
    checked_nested_option_result_relations();
    checked_string_map_get_result_relation();
    checked_result_relation_reallocation();
    checked_string_map_get_payload_source();
    checked_callable_flow_projection_authority();
    checked_cast_and_consumed_edges();
    checked_cast_after_prior_ownership_error();
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
    checked_output_graph("func Probe(text:String)->Bool{let line=ReadLine();return StringLength(text)>0;}", 0, false);
    checked_output_graph("extern \"C\"{func Opaque()->Int;} func Probe(text:String)->Bool{return StringLength(text)>Opaque();}", 1, false);
    checked_output_graph("func Probe(text:String)->Bool{let indirect:func()->Int=()=>1;return StringLength(text)>indirect();}", 0, false);
    checked_output_graph("extern \"C\"{func Opaque(value:Int)->Int;} func Probe(text:String)->Bool{return StringLength(text)>Opaque(1);}", 1, false);
    checked_output_graph("func Probe(text:String)->Bool{let indirect:func(Int)->Int=(value:Int)=>value+StringLength(text);return StringLength(text)>indirect(1);}", 0, false);
    checked_output_graph("class Box{let amount:Int;func Add(self,value:Int)->Int{return self.amount+value;}} func Probe(text:String,box:Box)->Bool{return StringLength(text)>box.Add(1);}", 1, true);
    checked_output_graph("class Box{let amount:Int;func Add(self,value:Int)->Int{return self.amount+value;}} extern \"C\"{func Opaque()->Int;} func Build()->Box{let n=Opaque();return Box(n);} func Probe(text:String)->Bool{return StringLength(text)>Build().Add(1);}", 3, false);
    checked_output_graph("struct Cell{text:String} func Retain(inout output:Cell,text:String)->Int{output.text=text;return 1;} func Probe(inout output:Cell,text:String)->Bool{return StringLength(text)>Retain(output,text);}", 2, false);
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
    checked_stdlib_prefix_retention_currency();
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
    /* The completed recursive row now proves destination=text; the caller still cannot retain its borrowed source. */
    retained_inout_source("func SetText(inout destination:String,ref text:String)->Void{if StringLength(text)>0{SetText(destination,text);}destination=text;}", alias_store, true);
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
    checked_allocating_reader("func Main()->Void { let text = StringTrim(\" heap text \"); Log(text); }");
    checked_reader_reserved_name("func CharAtN(input:String,count:Int,index:Int)->String{return input;} func Main()->Void{let text=CharAtN(\"abc\",3,1);}");
    checked_reader_reserved_name("func SubstringWithLen(input:String,count:Int,start:Int,length:Int)->String{return input;} func Main()->Void{let text=SubstringWithLen(\"abc\",3,0,2);}");
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=StringIndexOf(record.text,\"x\");}", false);
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=StringContains(record.text,\"x\");}", false);
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=StringReplace(record.text,\"x\",\"y\");}", true);
    checked_input_reader("struct TextRecord{ text:String } func Inspect(ref record:TextRecord)->Void{let result=Split(record.text,\"x\");}", true);
    checked_input_reader("struct TextRecord{ text:String, rows:HashMap<String,Int> } func Inspect(ref record:TextRecord)->Void{let result=MapHas(record.rows,record.text);}", false);
    checked_input_reader("struct Wrapped{value:Option<String>} func Inspect(ref record:Wrapped)->Void{let result=IsSome(record.value);}", false);
    checked_input_reader("struct TextRecord{text:String} func Inspect(ref record:TextRecord)->Void{let result=ToInt(record.text);}", false);
    checked_input_reader("struct TextRecord{text:String} func Inspect(ref record:TextRecord)->Void{let result=StringTrim(record.text);}", false);
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
