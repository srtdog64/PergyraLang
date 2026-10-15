/* Receiver admission only. Altered MIR facts are never emitted or executed.
 * Callee shadow/operation permission and OOM execution are outside this unit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "semantic/type_system.h"
#include "compiler/hir.h"
#include "compiler/dir.h"
#include "compiler/rir.h"
#include "compiler/mir.h"
#include "compiler/mir_decl_headers.h"
#include "codegen/llvm_internal.h"
#include "codegen/llvm_internal_api.h"
#include "codegen/llvm_inventory_decl_lookup.h"
#include "codegen/llvm_expr_call_collections_extended.h"

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "LLVM receiver fault line %d: %s\n", __LINE__, #x); } } while (0)

typedef struct Fixture {
    Lexer *lexer;
    Parser *parser;
    ASTNode *program, *record_decl, *receiver, *leaf;
    SemanticResult *semantic;
    HIRProgram *hir;
    DIRProgram *dir;
    RIRProgram *rir;
    MIRProgram *mir;
    MIRDeclHeader *inner;
    MIRDeclField *field;
    MIRRoutine *routine;
} Fixture;

static bool lower_fixture(Fixture *f, const char *key)
{
    char source[768];
    snprintf(source, sizeof(source),
        "struct MapMetadataInner { entries: HashMap<%s, Int>; }"
        "struct MapMetadataOuter { inner: MapMetadataInner; }"
        "func Main() -> Void { let seed: HashMap<%s, Int> = MapNew();"
        " let record: MapMetadataOuter = MapMetadataOuter(MapMetadataInner(seed));"
        " let value = MapGet(record.inner.entries, %s); Log(value); }",
        key, key, strcmp(key, "String") == 0 ? "\"key\"" : "4");
    f->lexer = lexer_create(source);
    if (f->lexer == NULL) return false;
    f->parser = parser_create(f->lexer);
    if (f->parser == NULL) return false;
    f->program = parser_parse_program(f->parser);
    if (f->program == NULL || parser_has_error(f->parser)) return false;
    f->semantic = semantic_analyze(f->program);
    if (f->semantic == NULL || !f->semantic->success) return false;
    char *error = NULL;
    f->hir = hir_lower_with_semantic_facts(f->semantic, NULL, &error);
    free(error); error = NULL;
    if (f->hir == NULL) return false;
    f->dir = dir_lower_with_hir_facts(f->semantic->annotated_ast, f->hir, &error);
    free(error); error = NULL;
    f->rir = rir_lower(f->semantic->annotated_ast, &error);
    free(error); error = NULL;
    if (f->dir == NULL || f->rir == NULL) return false;
    bool ready = rir_enrich_with_hir_flow(f->rir, f->hir, &error)
        && dir_validate(f->dir, &error);
    free(error); error = NULL;
    if (!ready) return false;
    MIRLowerRequest request;
    mir_lower_request_init(&request, f->hir, f->rir, f->semantic);
    mir_lower_request_bind_dir(&request, f->dir);
    f->mir = mir_lower(&request, &error);
    free(error);
    if (f->mir == NULL || !mir_validate(f->mir, NULL)) return false;
    ASTNode *main_decl = ast_program_statement(f->program, 2);
    f->record_decl = ast_block_statement(ast_func_body(main_decl), 1);
    ASTNode *value_decl = ast_block_statement(ast_func_body(main_decl), 2);
    f->receiver = ast_call_argument(ast_let_initializer(value_decl), 0);
    f->leaf = ast_call_argument(ast_call_argument(ast_let_initializer(f->record_decl), 0), 0);
    for (size_t row = 0; row < f->mir->decl_header_count; row++) {
        MIRDeclHeader *header = &f->mir->decl_headers[row];
        if (strcmp(mir_decl_header_name(header), "MapMetadataInner") == 0) {
            f->inner = header;
            if (header->field_metadata_count == 1) f->field = &header->field_metadata[0];
        }
    }
    for (size_t row = 0; row < f->mir->routine_count; row++)
        if (strcmp(f->mir->routines[row].name, "Main") == 0) f->routine = &f->mir->routines[row];
    return f->inner != NULL && f->field != NULL && f->routine != NULL
        && f->receiver != NULL && f->receiver->type == AST_MEMBER_ACCESS
        && f->leaf != NULL && f->leaf->type == AST_IDENTIFIER;
}

static void dispose_fixture(Fixture *f)
{
    mir_destroy(f->mir);
    rir_destroy(f->rir);
    dir_destroy(f->dir);
    hir_destroy(f->hir);
    /* TypeRegistry remains alive until every IR/context borrower is gone. */
    semantic_result_destroy(f->semantic);
    ast_destroy(f->program);
    parser_destroy(f->parser);
    lexer_destroy(f->lexer);
}

static LLVMGenCtx *context_for_fixture(Fixture *f)
{
    LLVMGenCtx *ctx = llvm_ctx_create("map-receiver-admission");
    if (ctx == NULL) return NULL;
    ctx->mir = f->mir;
    llvm_register_nominal_decl(ctx, ast_program_statement(f->program, 0));
    llvm_register_nominal_decl(ctx, ast_program_statement(f->program, 1));
    LLVMTypeRef fn_type = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, "probe_storage", fn_type);
    LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, block);
    llvm_scope_push(ctx);
    bool record_found = false, seed_found = false;
    for (size_t row = 0; !ctx->has_error && row < f->routine->source_local_type_count; row++) {
        const MIRSourceLocalType *local = &f->routine->source_local_types[row];
        if (strcmp(local->name, "record") != 0 && strcmp(local->name, "seed") != 0) continue;
        LLVMTypeRef type = pergyra_type_to_llvm(ctx, local->type_name);
        if (type == NULL || ctx->has_error) break;
        LLVMValueRef storage = LLVMBuildAlloca(ctx->builder, type, local->name);
        llvm_scope_declare(ctx, local->name, storage, type);
        if (strcmp(local->name, "record") == 0) {
            record_found = local->binding_syntax_id == ast_node_stable_id(f->record_decl);
            llvm_register_var_class(ctx, local->name, local->type_name);
        } else {
            char key[256], value[256];
            seed_found = llvm_constructed_arg_name_copy(local->type_name, 0, key, sizeof(key))
                && llvm_constructed_arg_name_copy(local->type_name, 1, value, sizeof(value));
            if (seed_found) llvm_register_map_var(ctx, local->name, key, value);
        }
    }
    if (ctx->has_error || !record_found || !seed_found) {
        llvm_ctx_destroy(ctx);
        return NULL;
    }
    return ctx;
}

static void field_faults(Fixture *f, const char *key)
{
    for (unsigned fault = 0; fault < 9; fault++) {
        CHECK(mir_validate(f->mir, NULL));
        LLVMGenCtx *ctx = context_for_fixture(f);
        LLVMMapReceiverMetadata snapshot;
        bool baseline = ctx != NULL && llvm_map_receiver_metadata_ready(ctx, f->receiver, &snapshot)
            && strcmp(snapshot.key_type, key) == 0 && strcmp(snapshot.value_type, "Int") == 0
            && snapshot.value_llvm_type == ctx->type_i32 && !ctx->has_error;
        CHECK(baseline);
        if (!baseline) { llvm_ctx_destroy(ctx); continue; }
        uint32_t saved_id = f->field->source_syntax_id;
        char *saved_type = f->field->type_name;
        size_t saved_count = f->inner->field_metadata_count;
        switch (fault) {
        case 0: f->inner->field_metadata_count = 0; break;
        case 1: f->field->source_syntax_id = 0; break;
        case 2: f->field->type_name = NULL; break;
        case 3: f->field->type_name = "HashMap<String, Unknown>"; break;
        case 4: f->field->type_name = "HashMap<String>"; break;
        case 5: f->field->type_name = "HashMap<String, Int, Int>"; break;
        case 6: f->field->type_name = "Array<Int>"; break;
        case 7: f->field->type_name = "HashMap<Float, Int>"; break;
        case 8: f->field->type_name = strcmp(key, "String") == 0
                    ? "HashMap<String, Long>" : "HashMap<Int, Long>"; break;
        }
        LLVMMapReceiverMetadata altered;
        if (fault == 8) {
            char *error = NULL;
            CHECK(!mir_validate(f->mir, &error) && error != NULL
                && strstr(error, "type metadata drift") != NULL);
            free(error);
            /* Deliberately bypassing MIR admission here exposes the physical
             * boundary: opaque pointers erase Int/Long element layout. This
             * acceptance is not production permission for a semantic swap. */
            CHECK(llvm_hashmap_struct_type(ctx, "Int")
                == llvm_hashmap_struct_type(ctx, "Long"));
            CHECK(llvm_map_receiver_metadata_ready(ctx, f->receiver, &altered)
                && strcmp(altered.value_type, "Long") == 0
                && altered.value_llvm_type == ctx->type_i64 && !ctx->has_error);
        } else {
            CHECK(!llvm_map_receiver_metadata_ready(ctx, f->receiver, &altered)
                && ctx->has_error && ctx->error_code != NULL
                && strcmp(ctx->error_code, "PGY_LLVM_TYPE_UNSUPPORTED") == 0);
        }
        f->field->source_syntax_id = saved_id;
        f->field->type_name = saved_type;
        f->inner->field_metadata_count = saved_count;
        CHECK(mir_validate(f->mir, NULL));
        llvm_ctx_destroy(ctx);
    }
}

static void physical_field_fault(Fixture *f)
{
    CHECK(mir_validate(f->mir, NULL));
    LLVMGenCtx *ctx = context_for_fixture(f);
    LLVMMapReceiverMetadata snapshot;
    bool baseline = ctx != NULL
        && llvm_map_receiver_metadata_ready(ctx, f->receiver, &snapshot)
        && !ctx->has_error;
    CHECK(baseline);
    if (!baseline) { llvm_ctx_destroy(ctx); return; }
    LLVMClassTypeEntry *inner = llvm_lookup_class(ctx, "MapMetadataInner");
    int ordinal = inner != NULL ? llvm_class_field_ordinal(inner, "entries") : -1;
    bool field_ready = ordinal >= 0 && ordinal < inner->field_count;
    CHECK(field_ready);
    if (field_ready) {
        LLVMTypeRef saved = inner->fields[ordinal].field_type;
        inner->fields[ordinal].field_type = ctx->type_i32;
        LLVMMapReceiverMetadata altered;
        CHECK(!llvm_map_receiver_metadata_ready(ctx, f->receiver, &altered)
            && ctx->has_error && ctx->error_code != NULL
            && strcmp(ctx->error_code, "PGY_LLVM_TYPE_UNSUPPORTED") == 0);
        inner->fields[ordinal].field_type = saved;
        CHECK(mir_validate(f->mir, NULL));
    }
    llvm_ctx_destroy(ctx);
}

static void snapshot_growth(Fixture *f, const char *key)
{
    CHECK(mir_validate(f->mir, NULL));
    LLVMGenCtx *ctx = context_for_fixture(f);
    LLVMMapReceiverMetadata member, leaf;
    bool baseline = ctx != NULL && llvm_map_receiver_metadata_ready(ctx, f->receiver, &member)
        && llvm_map_receiver_metadata_ready(ctx, f->leaf, &leaf);
    CHECK(baseline);
    if (baseline) {
        pgy_arena_destroy(&ctx->scratch);
        pgy_arena_init_named(&ctx->scratch, 0, "llvm-backend-scratch");
        CHECK(pgy_arena_alloc(&ctx->scratch, 32768) != NULL);
        for (unsigned row = 0; row < 128; row++) {
            char name[32]; snprintf(name, sizeof(name), "growth_%u", row);
            LLVMValueRef storage = LLVMBuildAlloca(ctx->builder, ctx->type_i32, name);
            llvm_scope_declare(ctx, name, storage, ctx->type_i32);
        }
        CHECK(!ctx->has_error && strcmp(member.key_type, key) == 0
            && strcmp(member.value_type, "Int") == 0 && member.value_llvm_type == ctx->type_i32
            && strcmp(leaf.key_type, key) == 0 && strcmp(leaf.value_type, "Int") == 0);
        LLVMMapReceiverMetadata after;
        CHECK(llvm_map_receiver_metadata_ready(ctx, f->receiver, &after)
            && strcmp(after.key_type, key) == 0);
    }
    llvm_ctx_destroy(ctx);
}

int main(void)
{
    type_system_init();
    const char *keys[] = {"String", "Int"};
    for (size_t family = 0; family < 2; family++) {
        Fixture f = {0};
        bool ready = lower_fixture(&f, keys[family]);
        CHECK(ready);
        if (ready) { field_faults(&f, keys[family]); physical_field_fault(&f); snapshot_growth(&f, keys[family]); }
        dispose_fixture(&f);
    }
    type_system_cleanup();
    printf("LLVM MAP RECEIVER METADATA: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
