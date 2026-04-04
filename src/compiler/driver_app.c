/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../semantic/semantic.h"
#include "hir.h"
#include "module_loader.h"
#include "path_utils.h"
#include "llvm_runner.h"
#include "c_runner.h"

/* Path utilities are now in path_utils.h/c */

static int
run_token_dump(const char *source, const char *path)
{
    Lexer *lexer = lexer_create(source);
    if (lexer == NULL) {
        fprintf(stderr, "pgy: lexer init failed for '%s'\n", path);
        return 1;
    }

    printf("=== tokens: %s ===\n", path);
    int n = 0;
    Token tok;
    do {
        tok = lexer_next_token(lexer);
        printf("%4d  ", ++n);
        token_print(&tok);
        if (tok.type == TOKEN_ERROR) {
            fprintf(stderr, "pgy: lex error: %s\n", lexer_get_error(lexer));
            lexer_destroy(lexer);
            return 1;
        }
    } while (tok.type != TOKEN_EOF);

    printf("  %d tokens total\n", n);
    lexer_destroy(lexer);
    return 0;
}

static bool
scaffold_has_suffix(const char *value, const char *suffix)
{
    size_t value_len;
    size_t suffix_len;

    if (value == NULL || suffix == NULL)
        return false;

    value_len = strlen(value);
    suffix_len = strlen(suffix);
    if (value_len < suffix_len)
        return false;
    return strcmp(value + value_len - suffix_len, suffix) == 0;
}

static char *
scaffold_base_name_dup(const char *path)
{
    const char *base = path;
    const char *slash;
    const char *dot;
    size_t len;

    if (path == NULL)
        return NULL;

    slash = strrchr(path, '/');
    if (slash != NULL && slash[1] != '\0')
        base = slash + 1;
    dot = strrchr(base, '.');
    len = (dot != NULL && dot > base) ? (size_t)(dot - base) : strlen(base);
    return pergyra_strndup(base, len);
}

static int
scaffold_mkdir_p(const char *path)
{
    char *buf;
    size_t len;

    if (path == NULL || *path == '\0')
        return 0;

    len = strlen(path);
    buf = pergyra_strdup(path);
    if (buf == NULL)
        return 1;

    for (size_t i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (buf[0] != '\0' && mkdir(buf, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "pgy: failed to create directory '%s': %s\n",
                    buf, strerror(errno));
                free(buf);
                return 1;
            }
            buf[i] = '/';
        }
    }

    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "pgy: failed to create directory '%s': %s\n",
            buf, strerror(errno));
        free(buf);
        return 1;
    }

    free(buf);
    return 0;
}

static int
scaffold_ensure_parent_dir(const char *path)
{
    char *dir;
    char *slash;
    int rc;

    if (path == NULL)
        return 1;

    dir = pergyra_strdup(path);
    if (dir == NULL)
        return 1;

    slash = strrchr(dir, '/');
    if (slash == NULL) {
        free(dir);
        return 0;
    }
    *slash = '\0';
    rc = scaffold_mkdir_p(dir);
    free(dir);
    return rc;
}

static int
scaffold_write_file(const char *path, const char *content)
{
    FILE *fp;

    if (path == NULL || content == NULL)
        return 1;
    if (scaffold_ensure_parent_dir(path) != 0)
        return 1;

    fp = fopen(path, "rb");
    if (fp != NULL) {
        fclose(fp);
        fprintf(stderr, "pgy: refusing to overwrite existing file '%s'\n", path);
        return 1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "pgy: failed to write '%s': %s\n", path, strerror(errno));
        return 1;
    }

    if (fputs(content, fp) == EOF) {
        fprintf(stderr, "pgy: failed to write '%s'\n", path);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    return 0;
}

static char *
scaffold_file_path_dup(const char *target)
{
    size_t len;
    char *path;

    if (target == NULL)
        return NULL;
    if (scaffold_has_suffix(target, ".pgy"))
        return pergyra_strdup(target);

    len = strlen(target) + 5;
    path = malloc(len);
    if (path == NULL)
        return NULL;
    snprintf(path, len, "%s.pgy", target);
    return path;
}

static int
scaffold_subject_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "subject %s\n"
        "{\n"
        "    let state: Int;\n"
        "\n"
        "    func Tick(self) -> Void\n"
        "    {\n"
        "        state = state + 1;\n"
        "    }\n"
        "\n"
        "    action Step(self) -> Void\n"
        "    {\n"
        "        Tick();\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded subject -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_vessel_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "vessel %s\n"
        "{\n"
        "    current: Int;\n"
        "\n"
        "    func Reset(self) -> Void\n"
        "    {\n"
        "        current = 0;\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded vessel -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_object_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "object %s\n"
        "{\n"
        "    label: String;\n"
        "    value: Int;\n"
        "\n"
        "    func Summary(self) -> String\n"
        "    {\n"
        "        return label + \":\" + ToString(value);\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded object -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_dto_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[1024];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "dto %s\n"
        "{\n"
        "    label: String;\n"
        "    value: Int;\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded dto -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_zone_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "zone %s\n"
        "{\n"
        "    shared tick: Int = 0\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        tick = tick + 1;\n"
        "        Log(ToString(tick));\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded zone -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_world_file(const char *target)
{
    char *path = scaffold_file_path_dup(target);
    char *name = scaffold_base_name_dup(target);
    char content[2048];
    int rc;

    if (path == NULL || name == NULL) {
        free(path);
        free(name);
        return 1;
    }

    snprintf(content, sizeof(content),
        "world %s\n"
        "{\n"
        "    shared tick: Int = 0\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        tick = tick + 1;\n"
        "        Log(ToString(tick));\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded world -> %s\n", path);
    free(path);
    free(name);
    return rc;
}

static int
scaffold_simulator_dir(const char *target)
{
    char *dir = pergyra_strdup(target);
    char *hosts_path = NULL;
    char *main_path = NULL;
    char *name = NULL;
    char hosts_content[8192];
    char main_content[4096];
    int rc = 1;

    if (dir == NULL)
        return 1;
    if (scaffold_mkdir_p(dir) != 0)
        goto cleanup;

    hosts_path = path_join_dup(dir, "hosts.pgy");
    main_path = path_join_dup(dir, "main.pgy");
    name = scaffold_base_name_dup(dir);
    if (hosts_path == NULL || main_path == NULL || name == NULL)
        goto cleanup;

    snprintf(hosts_content, sizeof(hosts_content),
        "vessel Cycle\n"
        "{\n"
        "    age: Int;\n"
        "\n"
        "    func Advance(self) -> Void\n"
        "    {\n"
        "        age = age + 1;\n"
        "    }\n"
        "}\n"
        "\n"
        "subject Creature\n"
        "{\n"
        "    let name: String;\n"
        "    let energy: Int;\n"
        "    vessel cycle: Cycle;\n"
        "\n"
        "    func Rest(self) -> Void\n"
        "    {\n"
        "        energy = energy + 1;\n"
        "        cycle.Advance();\n"
        "    }\n"
        "\n"
        "    func Status(self) -> String\n"
        "    {\n"
        "        return name;\n"
        "    }\n"
        "}\n"
        "\n"
        "object CreatureView\n"
        "{\n"
        "    name: String;\n"
        "    energy: Int;\n"
        "}\n"
        "\n"
        "dto CreaturePacket\n"
        "{\n"
        "    name: String;\n"
        "    energy: Int;\n"
        "}\n"
        "\n"
        "zone Habitat\n"
        "{\n"
        "    subject slot creature: Creature\n"
        "    object slot view: CreatureView\n"
        "    dto slot packet: CreaturePacket\n"
        "    authority creature\n"
        "    refresh view from creature by creature\n"
        "    publish packet from creature by creature\n"
        "    shared day: Int = 0\n"
        "\n"
        "    func Tick(self) -> Void\n"
        "    {\n"
        "        day = day + 1;\n"
        "        creature.Rest();\n"
        "        Log(creature.Status());\n"
        "    }\n"
        "}\n"
        "\n"
        "world %sWorld\n"
        "{\n"
        "    zone habitat: Habitat\n"
        "    shared tick: Int = 0\n"
        "    state habitatLive: zone habitat\n"
        "    activate habitat\n"
        "\n"
        "    func Tick(self) -> Void\n"
        "    {\n"
        "        habitat.Tick();\n"
        "        tick = tick + 1;\n"
        "        Log(\"world.tick\");\n"
        "    }\n"
        "\n"
        "    func Save(self, path: String) -> Void\n"
        "    {\n"
        "        WriteFile(path, \"simulator-ready\");\n"
        "    }\n"
        "}\n",
        name);

    snprintf(main_content, sizeof(main_content),
        "import \"hosts.pgy\";\n"
        "\n"
        "func Main() -> Void\n"
        "{\n"
        "    let habitat = Habitat(Creature(\"Fox\", 5, Cycle(0)));\n"
        "    let sim = %sWorld(habitat);\n"
        "    sim.Tick();\n"
        "    sim.Tick();\n"
        "    sim.Save(\"results.txt\");\n"
        "}\n",
        name);

    if (scaffold_write_file(hosts_path, hosts_content) != 0)
        goto cleanup;
    if (scaffold_write_file(main_path, main_content) != 0)
        goto cleanup;

    printf("pgy: scaffolded simulator -> %s\n", dir);
    rc = 0;

cleanup:
    free(dir);
    free(hosts_path);
    free(main_path);
    free(name);
    return rc;
}

static int
scaffold_project_dir(const char *target)
{
    char *dir = pergyra_strdup(target);
    char *domain_path = NULL;
    char *main_path = NULL;
    char *name = NULL;
    char domain_content[6144];
    char main_content[2048];
    int rc = 1;

    if (dir == NULL)
        return 1;
    if (scaffold_mkdir_p(dir) != 0)
        goto cleanup;

    domain_path = path_join_dup(dir, "domain.pgy");
    main_path = path_join_dup(dir, "main.pgy");
    name = scaffold_base_name_dup(dir);
    if (domain_path == NULL || main_path == NULL || name == NULL)
        goto cleanup;

    snprintf(domain_content, sizeof(domain_content),
        "vessel Health\n"
        "{\n"
        "    current: Int;\n"
        "\n"
        "    func Heal(self, amount: Int) -> Void\n"
        "    {\n"
        "        current = current + amount;\n"
        "    }\n"
        "}\n"
        "\n"
        "subject Actor\n"
        "{\n"
        "    let name: String;\n"
        "    vessel health: Health;\n"
        "\n"
        "    func Tick(self) -> Void\n"
        "    {\n"
        "        health.Heal(1);\n"
        "    }\n"
        "\n"
        "    action Step(self) -> Void\n"
        "    {\n"
        "        Tick();\n"
        "    }\n"
        "}\n"
        "\n"
        "object ActorView\n"
        "{\n"
        "    name: String;\n"
        "}\n"
        "\n"
        "zone MainZone\n"
        "{\n"
        "    subject slot unit: Actor\n"
        "    object slot view: ActorView\n"
        "    authority unit\n"
        "    refresh view from unit by unit\n"
        "    shared tick: Int = 0\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        unit.Step();\n"
        "        tick = tick + 1;\n"
        "        Log(unit.name);\n"
        "    }\n"
        "}\n"
        "\n"
        "world %sWorld\n"
        "{\n"
        "    zone main: MainZone\n"
        "    shared tick: Int = 0\n"
        "    activate main\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        main.Step();\n"
        "        tick = tick + 1;\n"
        "    }\n"
        "}\n",
        name);

    snprintf(main_content, sizeof(main_content),
        "import \"domain.pgy\";\n"
        "\n"
        "func Main() -> Void\n"
        "{\n"
        "    let zone = MainZone(Actor(\"hero\", Health(10)));\n"
        "    let app = %sWorld(zone);\n"
        "    app.Step();\n"
        "    app.Step();\n"
        "}\n",
        name);

    if (scaffold_write_file(domain_path, domain_content) != 0)
        goto cleanup;
    if (scaffold_write_file(main_path, main_content) != 0)
        goto cleanup;

    printf("pgy: scaffolded project -> %s\n", dir);
    rc = 0;

cleanup:
    free(dir);
    free(domain_path);
    free(main_path);
    free(name);
    return rc;
}

int
driver_run_scaffold_command(int argc, char *argv[])
{
    const char *verb;
    const char *kind;
    const char *target;

    verb = (argc > 0) ? argv[0] : "scaffold";
    if (argc < 3) {
        fprintf(stderr,
            "Usage:\n"
            "  pgy scaffold <subject|vessel|object|dto|zone|world|simulator|project> <target>\n"
            "  pgy new <project-dir>\n");
        return 1;
    }

    kind = argv[1];
    target = argv[2];

    if (strcmp(kind, "subject") == 0)
        return scaffold_subject_file(target);
    if (strcmp(kind, "vessel") == 0)
        return scaffold_vessel_file(target);
    if (strcmp(kind, "object") == 0)
        return scaffold_object_file(target);
    if (strcmp(kind, "dto") == 0)
        return scaffold_dto_file(target);
    if (strcmp(kind, "zone") == 0)
        return scaffold_zone_file(target);
    if (strcmp(kind, "world") == 0)
        return scaffold_world_file(target);
    if (strcmp(kind, "simulator") == 0)
        return scaffold_simulator_dir(target);
    if (strcmp(kind, "project") == 0)
        return scaffold_project_dir(target);

    fprintf(stderr, "pgy: unknown scaffold kind '%s' for '%s'\n", kind, verb);
    return 1;
}

int
driver_run_pipeline(const DriverFlags *flags)
{
    ASTNode *ast = NULL;
    SemanticResult *sem = NULL;
    HIRProgram *hir = NULL;
    int exit_code = 1;
    char *load_error = NULL;
    char *hir_error = NULL;

    if (flags->dump_tokens) {
        char *source = path_read_file(flags->source_path);
        if (source == NULL)
            return 1;
        int rc = run_token_dump(source, flags->source_path);
        free(source);
        return rc;
    }

    if (flags->verbose)
        printf("pgy: loading modules\n");

    ast = module_loader_load_program(flags->source_path, &load_error);
    if (ast == NULL) {
        fprintf(stderr, "pgy: %s\n",
                load_error != NULL ? load_error : "module loading failed");
        goto cleanup;
    }

    if (flags->dump_ast) {
        ast_print(ast, 0);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    sem = semantic_analyze(ast);
    if (sem == NULL) {
        fprintf(stderr, "pgy: out of memory during semantic analysis\n");
        goto cleanup;
    }

    semantic_result_print(sem);
    if (!sem->success) {
        fprintf(stderr, "pgy: %zu error(s) — aborting\n", sem->error_count);
        goto cleanup;
    }

    hir = hir_lower(sem->annotated_ast, &hir_error);
    if (hir == NULL) {
        fprintf(stderr, "pgy: HIR lowering failed: %s\n",
                hir_error != NULL ? hir_error : "out of memory");
        goto cleanup;
    }

    if (flags->dump_hir) {
        hir_dump(hir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    /* Dispatch to backend runner */
    if (flags->backend == BACKEND_LLVM && !flags->emit_c_only) {
        exit_code = llvm_runner_execute(flags, hir);
    } else {
        exit_code = c_runner_execute(flags, hir);
    }

cleanup:
    free(load_error);
    free(hir_error);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    return exit_code;
}

void
driver_print_usage(void)
{
    printf(
        "Usage:\n"
        "  pgy <source.pgy>              compile to native binary\n"
        "  pgy <source.pgy> -o <out>     name the emitted native binary\n"
        "  pgy <source.pgy> --emit-c     stop after generating C\n"
        "  pgy <source.pgy> --emit-c -o <out.c>\n"
        "  pgy <source.pgy> --emit-llvm -o <out.ll>\n"
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy scaffold <kind> <target> create starter files\n"
        "  pgy new <project-dir>         scaffold a starter project\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump merged/normalized AST\n"
        "  pgy --hir    <source.pgy>     dump lowered HIR summary\n"
#ifdef PGY_LLVM_ENABLED
        "  default backend: LLVM\n"
        "  pgy <source.pgy> --backend=llvm   use LLVM native backend\n"
#else
        "  default backend: C\n"
#endif
#ifdef PGY_LLVM_ENABLED
        "  pgy <source.pgy> --emit-llvm      emit LLVM IR text\n"
#endif
        "  pgy --repl                    interactive REPL\n"
        "  pgy --help\n");
}
