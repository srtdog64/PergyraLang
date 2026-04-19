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
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "../common/string_compat.h"
#include "../lexer/lexer.h"
#include "../semantic/semantic.h"
#include "dir.h"
#include "rir.h"
#include "mir.h"
#include "hir.h"
#include "module_loader.h"
#include "path_utils.h"
#include "llvm_runner.h"
#include "c_runner.h"

/* Path utilities are now in path_utils.h/c */

static double
driver_now_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
#endif
}

static void
driver_debug_stage(const char *stage)
{
    if (stage != NULL && getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL)
        fprintf(stderr, "[driver stage] %s\n", stage);
}

/* Emit a JSON-escaped string including surrounding quotes. Handles ASCII
 * control characters, backslash, and double-quote; non-ASCII bytes pass
 * through unchanged (source assumed UTF-8). */
static void
driver_json_emit_string(FILE *out, const char *s)
{
    fputc('"', out);
    if (s == NULL) {
        fputc('"', out);
        return;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        unsigned char c = *p;
        switch (c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b", out);  break;
        case '\f': fputs("\\f", out);  break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (c < 0x20)
                fprintf(out, "\\u%04x", c);
            else
                fputc((int)c, out);
        }
    }
    fputc('"', out);
}

/* Attempt to pull "line N, column M" or "line N" out of a message string
 * for best-effort structured location. Returns true on match and fills out
 * *line / *column. Column is 0 when not present. Unchanged when no match. */
static bool
driver_extract_line_col(const char *s, unsigned *line, unsigned *column)
{
    if (s == NULL || line == NULL)
        return false;
    const char *p = strstr(s, "line ");
    if (p == NULL)
        return false;
    p += 5;
    char *endp = NULL;
    unsigned long l = strtoul(p, &endp, 10);
    if (endp == p)
        return false;
    *line = (unsigned)l;
    if (column != NULL) {
        *column = 0;
        const char *q = strstr(endp, "column ");
        if (q != NULL) {
            q += 7;
            unsigned long c = strtoul(q, &endp, 10);
            if (endp != q)
                *column = (unsigned)c;
        }
    }
    return true;
}

/* Emit a single-entry JSON diagnostic array to stderr. Used for error sites
 * that occur outside the SemanticResult accumulator (module loader, parser
 * wrapper, backend codegen, linker). Severity defaults to "error".
 * `stage` is a short tag like "module_load", "parse", "backend_c",
 * "backend_llvm", "link". `code`, `cause_ir`, and `fix_source` are all
 * optional routing tags and omitted from the JSON when NULL. Location is
 * best-effort extracted from the message text (parser errors include
 * "at line N, column M"). */
void
driver_emit_single_diag_json_full(const char *stage, const char *code,
                                   const char *cause_ir,
                                   const char *fix_source,
                                   const char *message)
{
    FILE *out = stderr;
    unsigned line = 0, column = 0;
    bool have_loc = driver_extract_line_col(message, &line, &column);

    fputs("[{\"severity\":\"error\",\"stage\":", out);
    driver_json_emit_string(out, stage != NULL ? stage : "unknown");
    if (code != NULL) {
        fputs(",\"code\":", out);
        driver_json_emit_string(out, code);
    }
    if (cause_ir != NULL) {
        fputs(",\"cause_ir\":", out);
        driver_json_emit_string(out, cause_ir);
    }
    if (fix_source != NULL) {
        fputs(",\"fix_source\":", out);
        driver_json_emit_string(out, fix_source);
    }
    if (have_loc) {
        fprintf(out, ",\"location\":{\"line\":%u,\"column\":%u}", line, column);
    } else {
        fputs(",\"location\":null", out);
    }
    fputs(",\"message\":", out);
    driver_json_emit_string(out, message != NULL ? message : "");
    fputs("}]\n", out);
}

void
driver_emit_single_diag_json_with_code(const char *stage, const char *code,
                                        const char *message)
{
    driver_emit_single_diag_json_full(stage, code, NULL, NULL, message);
}

void
driver_emit_single_diag_json(const char *stage, const char *message)
{
    driver_emit_single_diag_json_full(stage, NULL, NULL, NULL, message);
}

/* Mid-pipeline stage failure helper. Routes to JSON when --error-format=json
 * is active (so downstream consumers get a parseable array for HIR/DIR/
 * RIR/MIR lowering/validation failures), otherwise keeps the legacy
 * free-text line. `stage` is the JSON stage tag ("hir_lower", "mir_
 * validate", ...); `description` is the human-readable prefix used in
 * text mode ("HIR lowering failed"); `detail` is the underlying error
 * string (may be NULL → treated as "out of memory"). */
static void
driver_emit_stage_fail(const DriverFlags *flags, const char *stage,
                       const char *description, const char *detail)
{
    const char *msg = (detail != NULL) ? detail : "out of memory";
    if (flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
        driver_emit_single_diag_json(stage, msg);
    } else {
        fprintf(stderr, "pgy: %s: %s\n", description, msg);
    }
}

const char *
driver_route_stage(const char *default_stage, const char *code)
{
    if (code == NULL)
        return default_stage;
    if (strncmp(code, "PGY_MIR_", 8) == 0)
        return "mir_validation";
    if (strncmp(code, "PGY_C_", 6) == 0)
        return "c_codegen";
    if (strncmp(code, "PGY_LLVM_", 9) == 0)
        return "llvm_codegen";
    if (strncmp(code, "PGY_SEM_", 8) == 0)
        return "semantic";
    if (strncmp(code, "PGY_PARSE_", 10) == 0)
        return "parse";
    /* Unknown prefix: runner's default_stage wins — avoids silent mis-routing
     * if prefix taxonomy is extended later. */
    return default_stage;
}

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

#ifdef _WIN32
#define PGY_MKDIR(path_) _mkdir(path_)
#else
#define PGY_MKDIR(path_) mkdir((path_), 0777)
#endif

    for (size_t i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (buf[0] != '\0' && PGY_MKDIR(buf) != 0 && errno != EEXIST) {
                fprintf(stderr, "pgy: failed to create directory '%s': %s\n",
                    buf, strerror(errno));
                free(buf);
                return 1;
            }
            buf[i] = '/';
        }
    }

    if (PGY_MKDIR(buf) != 0 && errno != EEXIST) {
        fprintf(stderr, "pgy: failed to create directory '%s': %s\n",
            buf, strerror(errno));
        free(buf);
        return 1;
    }

#undef PGY_MKDIR

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
scaffold_class_file(const char *target)
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
        "class %s\n"
        "{\n"
        "    let label: String;\n"
        "    let bonus: Int;\n"
        "\n"
        "    func Summary(self) -> String\n"
        "    {\n"
        "        return label + \" (+\" + ToString(bonus) + \")\";\n"
        "    }\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded class -> %s\n", path);
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
        "tobject %s\n"
        "{\n"
        "    label: String;\n"
        "    value: Int;\n"
        "}\n",
        name);

    rc = scaffold_write_file(path, content);
    if (rc == 0)
        printf("pgy: scaffolded tobject -> %s\n", path);
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
        "// supporting hosts behind the contract:\n"
        "// - subject: active host / who performs the contract\n"
        "// - class: passive tool or thing with hosted func\n"
        "// - object: passive view/state target\n"
        "// - tobject: boundary packet\n"
        "\n"
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
        "class ToolCard\n"
        "{\n"
        "    let label: String;\n"
        "    let recovery: Int;\n"
        "\n"
        "    func Summary(self) -> String\n"
        "    {\n"
        "        return label + \" (+\" + ToString(recovery) + \")\";\n"
        "    }\n"
        "}\n"
        "\n"
        "subject Creature\n"
        "{\n"
        "    let name: String;\n"
        "    let energy: Int;\n"
        "    let tool: ToolCard;\n"
        "    vessel cycle: Cycle;\n"
        "\n"
        "    func Rest(self) -> Void\n"
        "    {\n"
        "        energy = energy + tool.recovery;\n"
        "        cycle.Advance();\n"
        "    }\n"
        "\n"
        "    func Status(self) -> String\n"
        "    {\n"
        "        return name + \" using \" + tool.Summary();\n"
        "    }\n"
        "}\n"
        "\n"
        "object CreatureView\n"
        "{\n"
        "    name: String;\n"
        "    energy: Int;\n"
        "    tool: ToolCard;\n"
        "}\n"
        "\n"
        "tobject CreaturePacket\n"
        "{\n"
        "    name: String;\n"
        "    energy: Int;\n"
        "    tool: ToolCard;\n"
        "}\n"
        "\n"
        "zone Habitat\n"
        "{\n"
        "    subject slot creature: Creature\n"
        "    object slot view: CreatureView\n"
        "    tobject slot packet: CreaturePacket\n"
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
        "    let habitat = Habitat(Creature(\"Fox\", 5, ToolCard(\"Camp Tea\", 1), Cycle(0)));\n"
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
    char *subject_path = NULL;
    char *zone_path = NULL;
    char *intent_path = NULL;
    char *world_path = NULL;
    char *main_path = NULL;
    char *name = NULL;
    char subject_content[4096];
    char zone_content[3072];
    char intent_content[3072];
    char world_content[3072];
    char main_content[2048];
    int rc = 1;

    if (dir == NULL)
        return 1;
    if (scaffold_mkdir_p(dir) != 0)
        goto cleanup;

    subject_path = path_join_dup(dir, "subjects/unit.pgy");
    zone_path = path_join_dup(dir, "zones/main.pgy");
    intent_path = path_join_dup(dir, "intents/recover_unit.pgy");
    world_path = path_join_dup(dir, "world.pgy");
    main_path = path_join_dup(dir, "main.pgy");
    name = scaffold_base_name_dup(dir);
    if (subject_path == NULL || zone_path == NULL || intent_path == NULL ||
        world_path == NULL || main_path == NULL || name == NULL)
        goto cleanup;

    snprintf(subject_content, sizeof(subject_content),
        "// supporting host behind intent/world/zone\n"
        "// subject = who performs the contract\n"
        "// class   = what the subject uses\n"
        "// object  = passive projected state\n"
        "// vessel  = internal passive state holder\n"
        "\n"
        "vessel Health\n"
        "{\n"
        "    current: Int;\n"
        "\n"
        "    func Restore(self, amount: Int) -> Void\n"
        "    {\n"
        "        current = current + amount;\n"
        "    }\n"
        "}\n"
        "\n"
        "class Tool\n"
        "{\n"
        "    let label: String;\n"
        "    let recovery: Int;\n"
        "\n"
        "    func Summary(self) -> String\n"
        "    {\n"
        "        return label + \" (+\" + ToString(recovery) + \")\";\n"
        "    }\n"
        "}\n"
        "\n"
        "subject Unit\n"
        "{\n"
        "    let name: String;\n"
        "    let tool: Tool;\n"
        "    vessel health: Health;\n"
        "\n"
        "    func HealUp(self) -> Void\n"
        "    {\n"
        "        health.Restore(tool.recovery);\n"
        "    }\n"
        "\n"
        "    action Recover(self) -> Void\n"
        "    {\n"
        "        HealUp();\n"
        "    }\n"
        "}\n"
        "\n"
        "object UnitView\n"
        "{\n"
        "    name: String;\n"
        "    tool: Tool;\n"
        "}\n");

    snprintf(zone_content, sizeof(zone_content),
        "// scene boundary for RecoverUnit intent\n"
        "import \"../subjects/unit.pgy\";\n"
        "\n"
        "zone MainZone\n"
        "{\n"
        "    subject slot unit: Unit\n"
        "    object slot view: UnitView\n"
        "    authority unit\n"
        "    refresh view from unit by unit\n"
        "    shared tick: Int = 0\n"
        "\n"
        "    func Snapshot(self) -> String\n"
        "    {\n"
        "        return view.name + \" hp=\" + ToString(unit.health.current);\n"
        "    }\n"
        "}\n");

    snprintf(intent_content, sizeof(intent_content),
        "// start here: user-facing contract first\n"
        "import \"../zones/main.pgy\";\n"
        "\n"
        "intent RecoverUnit(main: MainZone, unit: Unit)\n"
        "{\n"
        "    exclusive;\n"
        "    priority: 10;\n"
        "\n"
        "    step Recover\n"
        "    {\n"
        "        where: MainZone;\n"
        "        using: main;\n"
        "        who: unit;\n"
        "        on: unit.Recover();\n"
        "        post: main.unit.health.current == unit.health.current;\n"
        "        expect: main.view.name == unit.name;\n"
        "    }\n"
        "\n"
        "    success: unit.health.current > 0;\n"
        "    failure: false;\n"
        "}\n");

    snprintf(world_content, sizeof(world_content),
        "// execution / trust boundary for the project\n"
        "import \"intents/recover_unit.pgy\";\n"
        "\n"
        "world %sWorld\n"
        "{\n"
        "    zone main: MainZone\n"
        "    shared tick: Int = 0\n"
        "    activate main\n"
        "\n"
        "    func Step(self) -> Void\n"
        "    {\n"
        "        Log(\"[Intent] RecoverUnit=\" + ToString(RecoverUnit(main, main.unit)));\n"
        "        Log(main.Snapshot());\n"
        "        tick = tick + 1;\n"
        "    }\n"
        "}\n"
        "\n"
        "func Open%sWorld() -> %sWorld\n"
        "{\n"
        "    let zone = MainZone(Unit(\"hero\", Tool(\"Bandage\", 1), Health(10)));\n"
        "    return %sWorld(zone);\n"
        "}\n",
        name,
        name,
        name,
        name);

    snprintf(main_content, sizeof(main_content),
        "import \"world.pgy\";\n"
        "\n"
        "func Main() -> Void\n"
        "{\n"
        "    let app = Open%sWorld();\n"
        "    app.Step();\n"
        "    app.Step();\n"
        "}\n",
        name);

    if (scaffold_write_file(subject_path, subject_content) != 0)
        goto cleanup;
    if (scaffold_write_file(zone_path, zone_content) != 0)
        goto cleanup;
    if (scaffold_write_file(intent_path, intent_content) != 0)
        goto cleanup;
    if (scaffold_write_file(world_path, world_content) != 0)
        goto cleanup;
    if (scaffold_write_file(main_path, main_content) != 0)
        goto cleanup;

    printf("pgy: scaffolded project -> %s\n", dir);
    rc = 0;

cleanup:
    free(dir);
    free(subject_path);
    free(zone_path);
    free(intent_path);
    free(world_path);
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
            "  pgy scaffold <subject|class|vessel|object|tobject|zone|world|simulator|project> <target>\n"
            "  pgy new <project-dir>\n"
            "\n"
            "Project design order:\n"
            "  intent -> world -> zone -> subject\n"
            "\n"
            "Host kinds:\n"
            "  subject  = active host / who performs the contract\n"
            "  class    = passive tool or thing with hosted func\n"
            "  object   = passive view or state target\n"
            "  tobject  = transfer object (boundary data)\n");
        return 1;
    }

    kind = argv[1];
    target = argv[2];

    if (strcmp(kind, "subject") == 0)
        return scaffold_subject_file(target);
    if (strcmp(kind, "class") == 0)
        return scaffold_class_file(target);
    if (strcmp(kind, "vessel") == 0)
        return scaffold_vessel_file(target);
    if (strcmp(kind, "object") == 0)
        return scaffold_object_file(target);
    if (strcmp(kind, "tobject") == 0)
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
    return driver_run_pipeline_timed(flags, NULL);
}

int
driver_run_pipeline_timed(const DriverFlags *flags, DriverPhaseTimings *timings)
{
    ASTNode *ast = NULL;
    SemanticResult *sem = NULL;
    DIRProgram *dir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    HIRProgram *hir = NULL;
    CompilerIRBundle bundle;
    int exit_code = 1;
    char *load_error = NULL;
    char *hir_error = NULL;
    double phase_start = 0.0;
    double total_start = driver_now_seconds();
    memset(&bundle, 0, sizeof(bundle));
    if (timings != NULL)
        memset(timings, 0, sizeof(*timings));

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

    driver_debug_stage("module_load");
    phase_start = driver_now_seconds();
    ast = module_loader_load_program(flags->source_path, &load_error);
    if (timings != NULL)
        timings->module_load = driver_now_seconds() - phase_start;
    if (ast == NULL) {
        const char *msg = load_error != NULL ? load_error : "module loading failed";
        if (flags->diag_format == DIAG_FORMAT_JSON) {
            /* Distinguish parse errors from other module-load failures so AI
             * consumers can route syntax vs I/O vs resolution issues. Module
             * loader prefixes parse errors with "parse error in '<path>':". */
            const char *stage =
                (strncmp(msg, "parse error in", 14) == 0) ? "parse"
                                                          : "module_load";
            driver_emit_single_diag_json(stage, msg);
        } else {
            fprintf(stderr, "pgy: %s\n", msg);
        }
        goto cleanup;
    }

    if (flags->dump_ast) {
        ast_print(ast, 0);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->verbose)
        printf("pgy: semantic analysis\n");

    driver_debug_stage("semantic");
    phase_start = driver_now_seconds();
    sem = semantic_analyze(ast);
    if (timings != NULL)
        timings->semantic = driver_now_seconds() - phase_start;
    if (sem == NULL) {
        fprintf(stderr, "pgy: out of memory during semantic analysis\n");
        goto cleanup;
    }

    /* Text mode: print diagnostics now (legacy UX preserves mid-run warnings).
     * JSON mode: defer so exactly one JSON array lands on stderr per run.
     * Backend runners emit their own error array on failure; we emit the
     * semantic array only at terminal points (semantic-fail here, or the
     * pipeline-success site near the end of this function). */
    if (flags == NULL || flags->diag_format != DIAG_FORMAT_JSON) {
        semantic_result_print(sem);
    }
    if (!sem->success) {
        if (flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
            semantic_result_print_json(sem);  /* terminal: error array */
        } else {
            fprintf(stderr, "pgy: %zu error(s) — aborting\n", sem->error_count);
        }
        goto cleanup;
    }

    driver_debug_stage("hir_lower");
    phase_start = driver_now_seconds();
    hir = hir_lower(sem->annotated_ast, &hir_error);
    if (timings != NULL)
        timings->hir_lower = driver_now_seconds() - phase_start;
    if (hir == NULL) {
        driver_emit_stage_fail(flags, "hir_lower",
            "HIR lowering failed", hir_error);
        goto cleanup;
    }

    driver_debug_stage("dir_lower");
    phase_start = driver_now_seconds();
    dir = dir_lower(sem->annotated_ast, &hir_error);
    if (timings != NULL)
        timings->dir_lower = driver_now_seconds() - phase_start;
    if (dir == NULL) {
        driver_emit_stage_fail(flags, "dir_lower",
            "DIR lowering failed", hir_error);
        goto cleanup;
    }
    driver_debug_stage("dir_validate");
    phase_start = driver_now_seconds();
    if (!dir_validate(dir, &hir_error)) {
        if (timings != NULL)
            timings->dir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "dir_validate",
            "DIR validation failed",
            hir_error != NULL ? hir_error : "invalid DIR");
        goto cleanup;
    }
    if (timings != NULL)
        timings->dir_validate = driver_now_seconds() - phase_start;

    driver_debug_stage("rir_lower");
    phase_start = driver_now_seconds();
    rir = rir_lower(sem->annotated_ast, &hir_error);
    if (timings != NULL)
        timings->rir_lower = driver_now_seconds() - phase_start;
    if (rir == NULL) {
        driver_emit_stage_fail(flags, "rir_lower",
            "RIR lowering failed", hir_error);
        goto cleanup;
    }
    driver_debug_stage("rir_enrich");
    phase_start = driver_now_seconds();
    if (!rir_enrich_with_hir_flow(rir, hir, &hir_error)) {
        if (timings != NULL)
            timings->rir_enrich = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "rir_enrich",
            "RIR flow enrichment failed", hir_error);
        goto cleanup;
    }
    if (timings != NULL)
        timings->rir_enrich = driver_now_seconds() - phase_start;
    driver_debug_stage("rir_validate");
    phase_start = driver_now_seconds();
    if (!rir_validate(rir, &hir_error)) {
        if (timings != NULL)
            timings->rir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "rir_validate",
            "RIR validation failed",
            hir_error != NULL ? hir_error : "invalid RIR");
        goto cleanup;
    }
    if (timings != NULL)
        timings->rir_validate = driver_now_seconds() - phase_start;
    driver_debug_stage("rir_dir_validate");
    phase_start = driver_now_seconds();
    if (!rir_validate_against_dir(rir, dir, &hir_error)) {
        if (timings != NULL)
            timings->rir_dir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "rir_dir_validate",
            "RIR/DIR validation failed",
            hir_error != NULL ? hir_error : "invalid RIR/DIR contract");
        goto cleanup;
    }
    if (timings != NULL)
        timings->rir_dir_validate = driver_now_seconds() - phase_start;

    driver_debug_stage("mir_lower");
    phase_start = driver_now_seconds();
    mir = mir_lower(hir, rir, &hir_error);
    if (timings != NULL)
        timings->mir_lower = driver_now_seconds() - phase_start;
    if (mir == NULL) {
        driver_emit_stage_fail(flags, "mir_lower",
            "MIR lowering failed", hir_error);
        goto cleanup;
    }
    driver_debug_stage("mir_validate");
    phase_start = driver_now_seconds();
    if (!mir_validate(mir, &hir_error)) {
        if (timings != NULL)
            timings->mir_validate = driver_now_seconds() - phase_start;
        driver_emit_stage_fail(flags, "mir_validate",
            "MIR validation failed",
            hir_error != NULL ? hir_error : "invalid MIR");
        goto cleanup;
    }
    if (timings != NULL)
        timings->mir_validate = driver_now_seconds() - phase_start;

    bundle.hir = hir;
    bundle.dir = dir;
    bundle.rir = rir;
    bundle.mir = mir;

    if (flags->dump_dir) {
        dir_dump(dir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_rir) {
        rir_dump(rir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_mir) {
        mir_dump(mir, stdout);
        exit_code = 0;
        goto cleanup;
    }

    if (flags->dump_hir) {
        hir_dump_mode(hir, stdout, flags->hir_dump_mode);
        exit_code = 0;
        goto cleanup;
    }

    /* Dispatch to backend runner */
    driver_debug_stage(flags->backend == BACKEND_LLVM && !flags->emit_c_only
                       ? "backend_llvm"
                       : "backend_c");
    phase_start = driver_now_seconds();
    if (flags->backend == BACKEND_LLVM && !flags->emit_c_only) {
        CompilerBackendTimings backend_timings = {0};
        exit_code = llvm_runner_execute(flags, &bundle,
                                        timings != NULL ? &backend_timings : NULL);
        if (timings != NULL) {
            timings->backend_codegen = backend_timings.codegen;
            timings->backend_native_compile = backend_timings.native_compile;
            timings->backend_link = backend_timings.link;
        }
    } else {
        CompilerBackendTimings backend_timings = {0};
        exit_code = c_runner_execute(flags, &bundle,
                                     timings != NULL ? &backend_timings : NULL);
        if (timings != NULL) {
            timings->backend_codegen = backend_timings.codegen;
            timings->backend_native_compile = backend_timings.native_compile;
            timings->backend_link = backend_timings.link;
        }
    }
    if (timings != NULL)
        timings->backend = driver_now_seconds() - phase_start;

    /* Terminal semantic-JSON emit for the "full success" path. Backend
     * runners emit their own error array on failure (exit_code != 0) and
     * we skip here to avoid a second JSON array on stderr. HIR/DIR/RIR/MIR
     * mid-pipeline failures also keep exit_code == 1 (initial value) so
     * this guard correctly suppresses emit for those too — they are a
     * separate pre-existing gap tracked outside this fix. */
    if (sem != NULL && exit_code == 0
        && flags != NULL && flags->diag_format == DIAG_FORMAT_JSON) {
        semantic_result_print_json(sem);
    }

cleanup:
    if (timings != NULL)
        timings->total = driver_now_seconds() - total_start;
    free(load_error);
    free(hir_error);
    dir_destroy(dir);
    mir_destroy(mir);
    rir_destroy(rir);
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
#ifdef PGY_LLVM_ENABLED
        "  (LLVM + --run): if -o ends with .o/.obj, executable target becomes .exe on Windows\n"
#endif
        "  pgy <source.pgy> --run        compile + run\n"
        "  pgy <source.pgy> --opt=dev|release   (default: release)\n"
        "  pgy <source.pgy> --error-format=json|text  (default: text; json for structured tooling)\n"
        "  pgy scaffold <kind> <target> create starter files\n"
        "  pgy new <project-dir>         scaffold a starter project\n"
        "\n"
        "Project design order:\n"
        "  intent -> world -> zone -> subject\n"
        "\n"
        "Host scaffold kinds:\n"
        "  subject  active host / who performs the contract\n"
        "  class    passive tool or thing with hosted func\n"
        "  object   passive view or state target\n"
        "  tobject  boundary packet\n"
        "  pgy --tokens <source.pgy>     dump token stream\n"
        "  pgy --ast    <source.pgy>     dump merged/normalized AST\n"
        "  pgy --dir    <source.pgy>     dump lowered DIR summary\n"
        "  pgy --rir    <source.pgy>     dump lowered RIR summary\n"
        "  pgy --mir    <source.pgy>     dump lowered MIR summary\n"
        "  pgy --hir     <source.pgy>     dump lowered HIR summary\n"
        "  pgy --hir-cfg <source.pgy>     dump HIR CFG view\n"
        "  pgy --hir-dom <source.pgy>     dump HIR dominance view\n"
        "  pgy --hir-ssa <source.pgy>     dump HIR SSA-prep view\n"
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
