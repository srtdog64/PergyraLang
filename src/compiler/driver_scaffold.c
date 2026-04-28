/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_scaffold.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "../common/string_compat.h"
#include "path_utils.h"

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

