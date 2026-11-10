/* Landing reloads must consume the contents parsed for their own generation,
 * including empty imports: path and syntax-node cardinality are not identity. */
#include "compiler/module_loader.h"
#include "compiler/path_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(condition) do { checks++; if (!(condition)) { failures++; \
    fprintf(stderr, "source snapshot line %d: %s\n", __LINE__, #condition); \
} } while (0)

static bool write_source(const char *path, const char *text)
{
    FILE *out = fopen(path, "wb");
    if (out == NULL) return false;
    size_t length = strlen(text);
    bool ok = fwrite(text, 1, length, out) == length;
    return fclose(out) == 0 && ok;
}

static void compare_snapshot_reload(const char *path,
                                    PgySourceModuleGraph *original,
                                    uint32_t original_ceiling)
{
    (void)path;
    char *error = NULL;
    ASTNode *ast = module_loader_reload_program_from_graph(
        original, &error);
    CHECK(ast != NULL && error == NULL);
    if (ast != NULL)
        CHECK(ast_stable_id_ceiling(ast) == original_ceiling);
    ast_destroy(ast);
    free(error);
}

static void expect_live_load_fails(const char *path)
{
    PgySourceModuleGraph *graph = NULL;
    char *error = NULL;
    ASTNode *ast = module_loader_load_program_with_graph(path, &graph, &error);
    CHECK(ast == NULL && error != NULL);
    ast_destroy(ast);
    module_loader_destroy_graph(graph);
    free(error);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: module_source_snapshot_unit <fresh-test-directory>\n");
        return 2;
    }
    const char *root_text = "import \"part.pgy\"; import \"empty.pgy\"; "
        "func Main() -> Int { return Imported() + 1; }\n";
    const char *part_text = "export func Imported() -> Int { return 1; }\n";
    char *root = path_join_dup(argv[1], "main.pgy");
    char *part = path_join_dup(argv[1], "part.pgy");
    char *empty = path_join_dup(argv[1], "empty.pgy");
    char *alternate = path_join_dup(argv[1], "else.pgy");
    if (root == NULL || part == NULL || empty == NULL || alternate == NULL) {
        free(root); free(part); free(empty); free(alternate);
        return 2;
    }
    CHECK(write_source(root, root_text));
    CHECK(write_source(part, part_text));
    CHECK(write_source(empty, "// first\n"));
    CHECK(write_source(alternate, part_text));
    PgySourceModuleGraph *original = NULL;
    char *error = NULL;
    ASTNode *ast = module_loader_load_program_with_graph(root, &original, &error);
    CHECK(ast != NULL && error == NULL);
    if (ast == NULL) goto cleanup;
    uint32_t ceiling = ast_stable_id_ceiling(ast);
    CHECK(original->inputs.count == 3);
    CHECK(module_loader_validate_graph(original, &error));
    ast_destroy(ast);
    ast = NULL;
    compare_snapshot_reload(root, original, ceiling);

    /* A landing generation is sealed at its initial bytes. Invalid live
     * replacements prove the replay has no filesystem fallback, while a new
     * compilation still observes and rejects those replacements. */
    CHECK(write_source(root, "func Main(\n"));
    compare_snapshot_reload(root, original, ceiling);
    expect_live_load_fails(root);
    CHECK(write_source(root, root_text));
    CHECK(write_source(part, "export func Imported(\n"));
    compare_snapshot_reload(root, original, ceiling);
    expect_live_load_fails(root);
    CHECK(write_source(part, part_text));
    CHECK(write_source(empty, "func Empty(\n"));
    compare_snapshot_reload(root, original, ceiling);
    expect_live_load_fails(root);
    CHECK(write_source(empty, "// first\n"));
    compare_snapshot_reload(root, original, ceiling);

    /* Replay resolves imports from the retained canonical path set as well as
     * from its retained bytes. Removing a live import cannot redirect or
     * block this generation; a fresh compilation still observes the loss. */
    CHECK(remove(part) == 0);
    compare_snapshot_reload(root, original, ceiling);
    expect_live_load_fails(root);
    CHECK(write_source(part, part_text));

    PgySourceModuleGraph missing = *original;
    missing.inputs = (ImportSourceSnapshot){0};
    CHECK(!module_loader_validate_graph(&missing, &error));
    free(error);
    error = NULL;
    ast = module_loader_reload_program_from_graph(&missing, &error);
    CHECK(ast == NULL && error != NULL);
    ast_destroy(ast);
    ast = NULL;
    free(error);
    error = NULL;

    PgySourceModuleGraph truncated = *original;
    truncated.inputs.count--;
    ast = module_loader_reload_program_from_graph(&truncated, &error);
    CHECK(ast == NULL && error != NULL);
    ast_destroy(ast);
    ast = NULL;
    free(error);
    error = NULL;

    ImportSourceInput *duplicate_rows = calloc(
        original->inputs.count + 1, sizeof(*duplicate_rows));
    CHECK(duplicate_rows != NULL);
    if (duplicate_rows != NULL) {
        memcpy(duplicate_rows, original->inputs.rows,
            original->inputs.count * sizeof(*duplicate_rows));
        duplicate_rows[original->inputs.count] = original->inputs.rows[0];
        PgySourceModuleGraph duplicate = *original;
        duplicate.inputs.rows = duplicate_rows;
        duplicate.inputs.count = original->inputs.count + 1;
        duplicate.inputs.capacity = duplicate.inputs.count;
        ast = module_loader_reload_program_from_graph(&duplicate, &error);
        CHECK(ast == NULL && error != NULL);
        ast_destroy(ast);
        ast = NULL;
        free(error);
        error = NULL;
        free(duplicate_rows);
    }

    const char *retained_path = original->inputs.rows[0].canonical_path;
    ast = import_resolver_load_program_with_snapshot(root, &error, &original->inputs);
    CHECK(ast == NULL && error != NULL);
    CHECK(original->inputs.count == 3
        && original->inputs.rows[0].canonical_path == retained_path);
    free(error);
    error = NULL;

    /* A failed imported parse retires every buffer already retained. */
    CHECK(write_source(part, "export func Imported(\n"));
    ImportSourceSnapshot failed = {0};
    ast = import_resolver_load_program_with_snapshot(root, &error, &failed);
    CHECK(ast == NULL && error != NULL);
    CHECK(failed.rows == NULL && failed.count == 0 && failed.capacity == 0);
    free(error);
    error = NULL;
    CHECK(write_source(part, part_text));
    ast = module_loader_load_program_with_graph(root, NULL, &error);
    CHECK(ast != NULL && error == NULL);

cleanup:
    ast_destroy(ast);
    module_loader_destroy_graph(original);
    free(error);
    free(root); free(part); free(empty); free(alternate);
    printf("MODULE SOURCE SNAPSHOT: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
