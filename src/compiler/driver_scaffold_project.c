/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "driver_scaffold_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "path_utils.h"
#include "pkg_manifest.h"

int
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
    name = scaffold_identifier_name_dup(dir);
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

int
scaffold_project_dir(const char *target)
{
    char *dir = pergyra_strdup(target);
    char *subject_path = NULL;
    char *zone_path = NULL;
    char *intent_path = NULL;
    char *world_path = NULL;
    char *main_path = NULL;
    char *manifest_path = NULL;
    char *lock_path = NULL;
    char *name = NULL;
    char *package_name = NULL;
    char subject_content[4096];
    char zone_content[3072];
    char intent_content[3072];
    char world_content[3072];
    char main_content[2048];
    char manifest_content[3072];
    char lock_content[1024];
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
    manifest_path = path_join_dup(dir, "pgy.toml");
    lock_path = path_join_dup(dir, "pgy.lock");
    name = scaffold_identifier_name_dup(dir);
    package_name = scaffold_base_name_dup(dir);
    if (subject_path == NULL || zone_path == NULL || intent_path == NULL ||
        world_path == NULL || main_path == NULL || manifest_path == NULL ||
        lock_path == NULL || name == NULL || package_name == NULL)
        goto cleanup;
    if (!pgy_package_manifest_package_name_is_valid(package_name)) {
        fprintf(stderr,
            "pgy: package name must use only letters, digits, '.', '_', or '-'\n");
        goto cleanup;
    }

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
        "export intent RecoverUnit(main: MainZone, unit: Unit)\n"
        "{\n"
        "    exclusive;\n"
        "    priority: 10;\n"
        "\n"
        "    step Recover\n"
        "    {\n"
        "        where: MainZone;\n"
        "        using: main;\n"
        "        authorized by: unit;\n"
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
        "}\n"
        "\n",
        name);

    snprintf(manifest_content, sizeof(manifest_content),
        "[seashell]\n"
        "schema = \"pgy.seashell.v1\"\n"
        "format = \"toml-subset\"\n"
        "\n"
        "[package]\n"
        "name = \"%s\"\n"
        "version = \"0.1.0\"\n"
        "pergyra = \"1.0\"\n"
        "edition = \"2026\"\n"
        "\n"
        "[targets.app]\n"
        "main = \"main.pgy\"\n"
        "\n"
        "[targets.test]\n"
        "main = \"main.pgy\"\n"
        "\n"
        "[dependencies]\n"
        "# Dependency version solving is out-of-beta; use file imports for now.\n"
        "\n"
        "[dev-dependencies]\n"
        "\n"
        "[effects]\n"
        "requires = []\n"
        "\n"
        "[authority]\n"
        "requires = []\n"
        "\n"
        "[capabilities]\n"
        "allow = []\n"
        "deny = []\n"
        "\n"
        "[build]\n"
        "backend = \"c\"\n"
        "deterministic = true\n",
        package_name);

    snprintf(lock_content, sizeof(lock_content),
        "# This file is generated by pgy package.\n"
        "# It records the deterministic package graph accepted by the beta package owner.\n"
        "\n"
        "[seashell]\n"
        "schema = \"pgy.seashell.lock.v1\"\n"
        "manifest_schema = \"pgy.seashell.v1\"\n"
        "\n"
        "[[package]]\n"
        "name = \"%s\"\n"
        "version = \"0.1.0\"\n"
        "source = \"path:.\"\n"
        "entry = \"main.pgy\"\n"
        "backend = \"c\"\n"
        "deterministic = true\n",
        package_name);

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
    if (scaffold_write_file(manifest_path, manifest_content) != 0)
        goto cleanup;
    if (scaffold_write_file(lock_path, lock_content) != 0)
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
    free(manifest_path);
    free(lock_path);
    free(name);
    free(package_name);
    return rc;
}
